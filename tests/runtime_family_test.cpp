#include "skse_api.h"
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <vector>
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
int wmain(int argc,wchar_t** argv) {
    Check(argc==4,"DLL, family and fixture path");
    const unsigned minor=static_cast<unsigned>(wcstoul(argv[2],nullptr,10));
    Check(minor==5 || minor==6 || minor==7,"Known build family");
    const auto root=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(root);
    const auto file=root/"CGE.dll";std::filesystem::copy_file(argv[1],file,std::filesystem::copy_options::overwrite_existing);
    const auto module=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"Load specialized production DLL");
    const auto query=reinterpret_cast<bool(*)(const cge::skse::Interface*,cge::skse::Info*)>(GetProcAddress(module,"SKSEPlugin_Query"));
    const auto load=reinterpret_cast<bool(*)(const cge::skse::Interface*)>(GetProcAddress(module,"SKSEPlugin_Load"));
    const auto metadata=reinterpret_cast<const cge::skse::Metadata*>(GetProcAddress(module,"SKSEPlugin_Version"));
    Check(query && load && metadata,"SKSE exports");
    std::vector<std::uint32_t> expected;
    for(unsigned m:{5u,6u,7u})for(unsigned build:{3u,16u,23u,39u,50u,53u,62u,73u,80u,97u,317u,318u,323u,342u,353u,629u,640u,659u,1130u,1170u,1179u,99u,104u}) {
        const auto version=cge::runtime::Pack(m,build);
        if(!cge::runtime::Select(version))continue;
        cge::skse::Interface api{0x02030010,version,0,0};cge::skse::Info info{};
        const bool accepted=m==minor;
        Check(query(&api,&info)==accepted,"Query accepts only mapped runtimes in the compiled family");
        if(accepted)expected.push_back(version);
        else Check(!load(&api),"Wrong-family Load rejects before installing hooks");
    }
    for(unsigned build:{659u,1179u}) {
        const auto version=cge::runtime::Pack(6,build)|1u;
        cge::skse::Interface api{0x02020060,version,0,0};cge::skse::Info info{};
        Check(query(&api,&info)==(minor==6),"GOG identity belongs only to the 1.6 build");
        if(minor==6)expected.push_back(version);
        else Check(!load(&api),"Wrong-family GOG Load is rejected");
    }
    Check(expected.size()<16,"Compatibility list fits with a terminator");
    for(unsigned i=0;i<16;++i)Check(metadata->compatibleVersions[i]==(i<expected.size()?expected[i]:0),"Metadata exactly matches Query family and has a terminator");
    Check(FreeLibrary(module)!=FALSE,"Unload specialized DLL");
    std::puts("PASS: family-specific Query, Load and static SKSE metadata agree.");
}
