#include "address_library.h"
#include "skse_api.h"
#include "prologue.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <array>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
template<class T>void Put(std::ofstream& file,T value){file.write(reinterpret_cast<const char*>(&value),sizeof(value));}
void Header(std::ofstream& file,const runtime::Layout& layout,unsigned count,unsigned format=0) {
    Put(file,std::uint32_t(format?format:layout.family==runtime::Family::SE?1:2));
    Put(file,1u);Put(file,(layout.version>>16)&255u);Put(file,(layout.version>>4)&4095u);Put(file,0u);
    Put(file,12u);file.write("SkyrimSE.exe",12);Put(file,8u);Put(file,count);
}
int wmain(int argc,wchar_t** argv) {
    Check(argc>=2,"Fixture directory");std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    const auto se=*runtime::Select(runtime::Pack(5,97)),ae=*runtime::Select(runtime::Pack(6,1179));
    Check(se.LibraryName()=="version-1-5-97-0.bin" && ae.LibraryName()=="versionlib-1-6-1179-0.bin","Library names");
    Check(se.depthView==0x2040 && ae.depthView==0x20a0 &&
        runtime::Select(runtime::Pack(6,640))->depthView==0x2040,"Versioned depth layout");
    Check(!runtime::Select(runtime::Pack(4,15)) && !runtime::Select(runtime::Pack(7,105)) &&
        !runtime::Select(runtime::Pack(6,678)) && !runtime::Select(runtime::Pack(5,4)),"Unmapped runtimes refused");
    for(auto version:{0x02000010u,0x02000140u,0x02010050u,0x02020060u,0x02030010u}) {
        skse::Interface api{version,se.version,0,0};
        Check(skse::Compatible(&api),"Stable SKSE 2 ABI accepted");
        api.isEditor=1;Check(!skse::Compatible(&api),"Editor refused");
        api.isEditor=0;api.skseVersion=0x01070300;Check(!skse::Compatible(&api),"Classic SKSE ABI refused");
    }
    const std::array<unsigned char,15> prologue={0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x50};
    Check(PrologueLength(prologue)==15,"Whole AE instructions");
    const std::array<unsigned char,17> saved={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x83,0xec,0x20,0x90};
    Check(PrologueLength(saved)==16,"Register spill prologue without relative addressing");
    for(auto bytes:std::vector<std::vector<unsigned char>>{{0xe9,0,0,0,0},{0xff,0x25,0,0,0,0},{0x90},
        {0x48,0x8b,0x05,0,0,0,0},{0x48,0x83},{0x48,0x83,0xec,0xff}})Check(!PrologueLength(bytes),"Unsafe or incomplete prologue rejected");
    AddressLibrary library;std::string error;const auto fixture=root/"sparse.bin";
    for(const auto& layout:{se,ae}) {
        for(unsigned code=0;code<8;++code)for(bool scaled:{false,true}) {
            {
                std::ofstream file(fixture,std::ios::binary);Header(file,layout,2);
                file.put(0);Put(file,std::uint64_t(1000));Put(file,std::uint64_t(8000));
                file.put(static_cast<char>(code|(code<<4)|(scaled?128:0)));
                if(code==0){Put(file,std::uint64_t(2000));Put(file,std::uint64_t(scaled?2000:16000));}
                else if(code==2 || code==3){file.put(10);file.put(10);}
                else if(code==4 || code==5 || code==6){Put(file,std::uint16_t(100));Put(file,std::uint16_t(100));}
                else if(code==7){Put(file,std::uint32_t(2000));Put(file,std::uint32_t(scaled?2000:16000));}
            }
            Check(library.Load(fixture,layout,error),error.c_str());
            const unsigned id=code==0 || code==7?2000:code==1?1001:code==2?1010:code==3?990:code==4?1100:code==5?900:100;
            const unsigned base=scaled?1000:8000;
            unsigned offset=code==0 || code==7?(scaled?2000:16000):code==1?base+1:code==2?base+10:code==3?base-10:code==4?base+100:code==5?base-100:100;
            if(scaled)offset*=8;
            Check(library.Size()==2 && library.Offset(1000)==8000 && library.Offset(id)==offset,"Sparse compression modes");
        }
        {std::ofstream file(fixture,std::ios::binary);Header(file,layout,1);file.put(0);Put(file,std::uint64_t(55));}
        Check(!library.Load(fixture,layout,error) && !library.Size(),"Truncated table clears entries");
        {std::ofstream file(fixture,std::ios::binary);Header(file,layout,1);file.put(0xff);}
        Check(!library.Load(fixture,layout,error),"Unknown encoding refused");
        {std::ofstream file(fixture,std::ios::binary);Header(file,layout,1,5);}
        Check(!library.Load(fixture,layout,error),"Wrong encoding family refused");
        {std::ofstream file(fixture,std::ios::binary);Header(file,layout,2);for(unsigned i=0;i<2;++i){file.put(0);Put(file,std::uint64_t(55));Put(file,std::uint64_t(128));}}
        Check(!library.Load(fixture,layout,error),"Duplicate identifiers refused");
    }
    if(argc==3) {
        unsigned checked=0;
        for(const auto& file:std::filesystem::directory_iterator(argv[2])) {
            if(file.path().extension()!=".bin")continue;
            std::ifstream input(file.path(),std::ios::binary);std::array<std::uint32_t,5> header{};
            input.read(reinterpret_cast<char*>(header.data()),sizeof(header));
            const auto selected=runtime::Select(runtime::Pack(header[2],header[3]));
            Check(selected.has_value(),"Installed library runtime supported");
            if(!library.Load(file.path(),*selected,error)){std::printf("%ls\n",file.path().c_str());Check(false,error.c_str());}
            for(auto id:selected->ids)if(id)Check(library.Offset(id)!=0,"Required identifier exists in installed library");
            std::printf("PASS: %s, %zu addresses\n",selected->VersionString().c_str(),library.Size());++checked;
        }
        Check(checked>=23,"All provided runtime libraries checked");
    }
    std::puts("PASS: runtime selection, SKSE ABI, prologues, library formats and invalid data.");
}
