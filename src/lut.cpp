#include "lut.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
namespace cge {
bool LoadCube(const std::filesystem::path& path,CubeLut& output,std::string& error) {
    output={};error.clear();
    auto fail=[&](const char* message){error=message;return false;};
    std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
    if(ec || bytes>32*1024*1024)return fail("LUT missing or exceeds 32 MiB");
    std::ifstream file(path,std::ios::binary);if(!file)return fail("LUT could not be opened");
    CubeLut result;std::string line;bool sizeSeen=false,minSeen=false,maxSeen=false,rangeSeen=false;
    unsigned lineNumber=0;std::size_t consumed=0;
    while(std::getline(file,line)) {
        consumed+=line.size()+1;
        if(consumed>32*1024*1024 || line.size()>4096)return fail("LUT file or line exceeds limits");
        if(++lineNumber==1 && line.starts_with("\xef\xbb\xbf"))line.erase(0,3);
        const auto comment=line.find('#');if(comment!=line.npos)line.resize(comment);
        std::istringstream stream(line);stream.imbue(std::locale::classic());std::string token,extra;
        if(!(stream>>token))continue;
        auto end=[&](){return !(stream>>extra);};
        auto triple=[&](std::array<float,3>& v) {
            return bool(stream>>v[0]>>v[1]>>v[2]) && end() &&
                std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x) && std::abs(x)<=65504;});
        };
        const bool header=token=="TITLE" || token=="LUT_3D_SIZE" || token=="DOMAIN_MIN" || token=="DOMAIN_MAX" || token=="LUT_3D_INPUT_RANGE";
        if(header && !result.values.empty())return fail("LUT header appears after table data");
        if(token=="TITLE")continue;
        if(token=="LUT_3D_SIZE") {
            if(sizeSeen || !(stream>>result.size) || !end() || result.size<2 || result.size>65)return fail("LUT_3D_SIZE must occur once and range from 2 to 65");
            sizeSeen=true;result.values.reserve(std::size_t(result.size)*result.size*result.size);continue;
        }
        if(token=="DOMAIN_MIN") {
            if(minSeen || rangeSeen || !triple(result.minimum))return fail("Invalid DOMAIN_MIN");
            minSeen=true;continue;
        }
        if(token=="DOMAIN_MAX") {
            if(maxSeen || rangeSeen || !triple(result.maximum))return fail("Invalid DOMAIN_MAX");
            maxSeen=true;continue;
        }
        if(token=="LUT_3D_INPUT_RANGE") {
            float low{},high{};
            if(rangeSeen || minSeen || maxSeen || !(stream>>low>>high) || !end() || !std::isfinite(low) || !std::isfinite(high) || std::abs(low)>65504 || std::abs(high)>65504)return fail("Invalid LUT_3D_INPUT_RANGE");
            result.minimum.fill(low);result.maximum.fill(high);rangeSeen=true;continue;
        }
        if(!sizeSeen)return fail("Expected a standalone LUT_3D_SIZE table");
        if(result.values.size()>=std::size_t(result.size)*result.size*result.size)return fail("Too many LUT rows");
        stream.clear();stream.str(line);std::array<float,3> rgb;
        if(!triple(rgb))return fail("Invalid LUT RGB row or unsupported header");
        result.values.push_back({rgb[0],rgb[1],rgb[2],1});
    }
    if(!file.eof())return fail("LUT read failed");
    if(!sizeSeen || result.values.size()!=std::size_t(result.size)*result.size*result.size)return fail("LUT row count does not match its size");
    for(unsigned i=0;i<3;++i)if(result.maximum[i]-result.minimum[i]<0.000001f)return fail("LUT input domain must increase on each axis");
    output=std::move(result);return true;
}
std::vector<std::string> ListLuts(const std::filesystem::path& directory) {
    std::vector<std::string> result;std::error_code ec;
    for(std::filesystem::directory_iterator i(directory,ec),end;i!=end && !ec;i.increment(ec)) {
        const auto native=i->path().filename().native();
        if(native.size()>128)continue;
        std::string name;name.reserve(native.size());
        for(const auto c:native) {
            if(c>127){name.clear();break;}
            name.push_back(static_cast<char>(c));
        }
        if(i->is_regular_file(ec) && !name.empty() && ValidLutName(name))result.push_back(name);
        if(result.size()==256)break;
    }
    std::sort(result.begin(),result.end());return result;
}
}
