#include "address_library.h"
#include <fstream>
#include <array>
#include <algorithm>
#include <charconv>
#include <limits>
namespace cge {
namespace {
struct Reader {
    std::ifstream file;
    bool good{true};
    template<class T>T Get() {
        T value{};
        if(!file.read(reinterpret_cast<char*>(&value),sizeof(value)))good=false;
        return value;
    }
    std::uint64_t Compact(unsigned code,std::uint64_t previous) {
        if(code==0)return Get<std::uint64_t>();
        if(code==6)return Get<std::uint16_t>();
        if(code==7)return Get<std::uint32_t>();
        const auto amount=code==1?1ull:code<=3?Get<std::uint8_t>():Get<std::uint16_t>();
        if(code==3 || code==5) {
            if(previous<amount){good=false;return 0;}
            return previous-amount;
        }
        if(previous>std::numeric_limits<std::uint64_t>::max()-amount){good=false;return 0;}
        return previous+amount;
    }
};

}
bool AddressLibrary::Load(const std::filesystem::path& path,const runtime::Layout& layout,std::string& error) {
    entries_.clear();
    auto fail=[&](const char* reason){entries_.clear();error=reason;return false;};
    Reader reader;reader.file.open(path,std::ios::binary|std::ios::ate);
    if(!reader.file)return fail("Matching Address Library file is missing");
    const auto length=reader.file.tellg();reader.file.seekg(0);
    if(length<=0 || length>256*1024*1024)return fail("Address Library size is invalid");
    auto add=[&](std::uint64_t id,std::uint64_t offset) {
        if(id>UINT32_MAX || offset>UINT32_MAX)return false;
        entries_.emplace_back(static_cast<std::uint32_t>(id),static_cast<std::uint32_t>(offset));return true;
    };
    {
        const auto format=reader.Get<std::uint32_t>();
        const unsigned expected=layout.version>=runtime::Pack(7,99)?5:layout.family==runtime::Family::SE?1:2;
        if(format!=expected)return fail("Address Library encoding does not match the runtime");
        const std::array<unsigned,4> expectedVersion={1u,(layout.version>>16)&255u,(layout.version>>4)&4095u,0u};
        for(auto value:expectedVersion)if(reader.Get<std::uint32_t>()!=value)return fail("Address Library executable version mismatch");
        std::string name;
        if(format==5) {
            std::array<char,64> bytes{};
            if(!reader.file.read(bytes.data(),bytes.size()))return fail("Truncated Address Library name");
            const auto end=std::find(bytes.begin(),bytes.end(),'\0');
            if(end==bytes.end())return fail("Unterminated Address Library name");
            name.assign(bytes.begin(),end);
        }else {
            const auto size=reader.Get<std::uint32_t>();
            if(!size || size>128)return fail("Invalid Address Library name length");
            name.resize(size);
            if(!reader.file.read(name.data(),size))return fail("Truncated Address Library name");
            if(name.back()=='\0')name.pop_back();
        }
        if(name!="SkyrimSE.exe")return fail("Address Library executable name mismatch");
        if(reader.Get<std::uint32_t>()!=8)return fail("Address Library pointer size is not x64");
        if(format==5)reader.Get<std::uint32_t>();
        const auto count=reader.Get<std::uint32_t>();
        if(!reader.good || !count || count>16000000)return fail("Address Library entry count is invalid");
        if(format==5) {
            if(length!=std::streamoff(96ull+4ull*count))return fail("Address Library dense body length mismatch");
            for(std::uint32_t id=0;id<count;++id) {
                const auto offset=reader.Get<std::uint32_t>();
                if(offset)add(id,offset);
            }
        }else {
            if(std::streamoff(count)>length-reader.file.tellg())return fail("Address Library sparse body is truncated");
            std::uint64_t previousId{},previousOffset{};
            for(std::uint32_t i=0;i<count;++i) {
                const auto type=reader.Get<std::uint8_t>();
                const unsigned idCode=type&15u,offsetCode=(type>>4)&7u;
                if(idCode>7)return fail("Address Library entry encoding is invalid");
                const auto id=reader.Compact(idCode,previousId);
                const auto prior=(type&128u)?previousOffset/8:previousOffset;
                auto offset=reader.Compact(offsetCode,prior);
                if(type&128u) {
                    if(offset>UINT64_MAX/8)return fail("Address Library offset overflow");
                    offset*=8;
                }
                if(!reader.good || !add(id,offset))return fail("Invalid or truncated Address Library entry");
                previousId=id;previousOffset=offset;
            }
            if(reader.file.peek()!=std::char_traits<char>::eof())return fail("Unexpected Address Library sparse data");
        }
    }
    std::sort(entries_.begin(),entries_.end());
    for(std::size_t i=1;i<entries_.size();++i)if(entries_[i-1].first==entries_[i].first)return fail("Duplicate Address Library identifier");
    return true;
}
std::uint32_t AddressLibrary::Offset(std::uint32_t id) const noexcept {
    const auto entry=std::lower_bound(entries_.begin(),entries_.end(),id,[](const auto& a,std::uint32_t b){return a.first<b;});
    return entry!=entries_.end() && entry->first==id?entry->second:0;
}
}
