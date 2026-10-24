#include "settings.h"
#include <fstream>
#include <limits>
#include <cstdio>
#include <cstdlib>
using cge::Settings;
void Check(bool b,const char* text){if(!b){std::printf("FAIL: %s\n",text);std::exit(1);}}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"Fixture path");std::filesystem::path path(argv[1]);
    std::filesystem::create_directories(path);
    path/=L"réglages CGE.ini";
    {std::ofstream f(path);f<<"[Color]\nExposureStops=nan\nContrast=50\nSaturation=-3\n[Detail]\nSharpen=bad\n[CGE]\nEnabled=false\n";}
    auto s=Settings::Load(path);
    Check(!s.enabled && s.exposure==0 && s.contrast==2 && s.saturation==0 && s.sharpen==0,"Finite values, bounds and malformed fallback");
    s.exposure=-0.75f;s.contrast=1.25f;s.saturation=1.5f;s.sharpen=0.25f;
    Check(s.Save(path),"Atomic save to Unicode path");
    auto loaded=Settings::Load(path);
    Check(!loaded.enabled && loaded.exposure==s.exposure && loaded.contrast==s.contrast &&
        loaded.saturation==s.saturation && loaded.sharpen==s.sharpen,"Own format roundtrip");
    s.exposure=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject invalid runtime values");
    loaded=Settings::Load(path);Check(loaded.exposure==-0.75f,"Failed save preserves previous config");
    Check(!s.Save(path/"missing"/"CGE.ini"),"Report unwritable path");
    Check(Settings{}.Neutral(),"Neutral defaults");
    std::puts("PASS: CGE settings bounds, finite values, persistence and failure handling.");
}
