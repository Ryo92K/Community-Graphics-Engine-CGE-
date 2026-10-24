#include "skse_api.h"
#include <windows.h>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <string>
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 && argc!=4)return 1;
    auto root=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(root);
    auto dll=root/L"CGE.dll";std::filesystem::copy_file(argv[1],dll,std::filesystem::copy_options::overwrite_existing);
    HMODULE conflict{};
    if(argc==4) {
        auto proxy=root/L"d3d11.dll";
        std::filesystem::copy_file(argv[3],proxy,std::filesystem::copy_options::overwrite_existing);
        conflict=LoadLibraryExW(proxy.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!conflict)return 1;
    }
    auto module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return 1;
    auto load=reinterpret_cast<bool(*)(const cge::skse::Interface*)>(GetProcAddress(module,"SKSEPlugin_Load"));
    if(!load)return 1;
    cge::skse::Interface api{0x02030010,cge::skse::Runtime,0,0};
                                                                                     
    if(load(&api)){std::puts("FAIL: plugin reported ready without a creation hook");return 1;}
    if(argc==4) {
        std::ifstream file(root/"CGE.log");std::string text((std::istreambuf_iterator<char>(file)),{});
        if(text.find("non-system d3d11.dll")==std::string::npos)return 1;
    }
    FreeLibrary(module);if(conflict)FreeLibrary(conflict);
    std::puts("PASS: missing graphics import is rejected without claiming successful rendering.");
}
