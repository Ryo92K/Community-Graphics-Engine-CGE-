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
    path/=L"CGE-\u03B3.ini";
    {std::ofstream f(path);f<<"[Color]\nExposureStops=nan\nContrast=50\nSaturation=-3\nGamma=0\nTemperature=1000\n[Input]\nToggleKey=117\n[Detail]\nSharpen=bad\n[CGE]\nEnabled=false\n";}
    auto s=Settings::Load(path);
    Check(!s.enabled && s.exposure==0 && s.contrast==2 && s.saturation==0 && s.sharpen==0 && s.gamma==0.5f && s.temperature==100 && s.toggleKey==119,"Finite values, bounds and malformed fallback");
    s.exposure=-0.75f;s.contrast=1.25f;s.saturation=1.5f;s.sharpen=0.25f;s.gamma=1.75f;s.temperature=-35;s.toggleKey=122;
    Check(s.Save(path),"Atomic save to Unicode path");
    auto loaded=Settings::Load(path);
    Check(!loaded.enabled && loaded.exposure==s.exposure && loaded.contrast==s.contrast &&
        loaded.saturation==s.saturation && loaded.sharpen==s.sharpen && loaded.gamma==s.gamma && loaded.temperature==s.temperature && loaded.toggleKey==122,"Own format roundtrip");
    s.exposure=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject invalid runtime values");
    loaded=Settings::Load(path);Check(loaded.exposure==-0.75f,"Failed save preserves previous config");
    Check(!s.Save(path/"missing"/"CGE.ini"),"Report unwritable path");

    Check(Settings{}.Neutral(),"Neutral defaults");
    auto changed=Settings{};changed.gamma=1.1f;Check(!changed.Neutral(),"Gamma activates rendering");
    changed=Settings{};changed.temperature=1;Check(!changed.Neutral(),"Temperature activates rendering");
    Check(!Settings::ValidToggleKey(117) && !Settings::ValidToggleKey(118) && !Settings::ValidToggleKey(121),"Reserved menu/save/reset keys rejected");
    Check(Settings::ValidToggleKey(119) && Settings::ValidToggleKey(122),"Default and custom function keys accepted");
    s=Settings{};s.gamma=std::numeric_limits<float>::quiet_NaN();Check(!s.Save(path),"Reject non-finite gamma");
    s=Settings{};s.temperature=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject non-finite temperature");
    s=Settings{};s.toggleKey=121;Check(!s.Save(path),"Reject shortcut collision");
    {std::ofstream file(path);file<<"[Color]\nExposureStops=-1\n";}
    auto legacy=Settings::Load(path);
    Check(legacy.gamma==1 && legacy.temperature==0 && legacy.toggleKey==119,"Older INI files keep neutral new controls");
    {std::ofstream file(path);file<<"[Input]\nToggleKey=122.5\n[Color]\nGamma=nan\nTemperature=inf\n";}
    auto invalid=Settings::Load(path);
    Check(invalid.gamma==1 && invalid.temperature==0 && invalid.toggleKey==119,"Malformed additions retain defaults");

    {std::ofstream file(path);file<<"[Color]\nExposureStops=-0.5\nGamma=1.2\n";}
    auto migrated=Settings::Load(path);
    Check(!migrated.separateProfiles && migrated.interior.exposure==-0.5f && migrated.exterior.gamma==1.2f,"Legacy settings seed both profiles without enabling them");
    migrated.exposure=-0.8f;migrated.SetSeparateProfiles(true);
    Check(migrated.interior.exposure==-0.8f && migrated.exterior.exposure==-0.8f,"First enable copies current shared values");
    migrated.interior.exposure=1;migrated.exterior.exposure=-1;migrated.transitionSeconds=2;
    Check(migrated.Save(path),"Save independent profiles");
    auto profiles=Settings::Load(path);
    Check(profiles.separateProfiles && profiles.interior.exposure==1 && profiles.exterior.exposure==-1 && profiles.transitionSeconds==2,"Profiles and transition duration roundtrip");
    profiles.SetSeparateProfiles(false);profiles.SetSeparateProfiles(true);
    Check(profiles.interior.exposure==1 && profiles.exterior.exposure==-1,"Switching shared mode preserves separate profiles");
    {std::ofstream file(path);file<<"[Interior]\nExposureStops=99\nGamma=nan\n[Profiles]\nSeparate=true\nTransitionSeconds=-1\n[Color]\nGamma=1.4\n";}
    auto bounded=Settings::Load(path);
    Check(bounded.interior.exposure==3 && bounded.interior.gamma==1.4f && bounded.exterior.gamma==1.4f && bounded.transitionSeconds==0,"Profile bounds, finite fallback and section-order independence");
    profiles.interior.gamma=std::numeric_limits<float>::quiet_NaN();
    Check(!profiles.Save(path),"Non-finite profile values cannot replace a valid file");
    profiles=Settings{};profiles.transitionSeconds=std::numeric_limits<float>::infinity();
    Check(!profiles.Save(path),"Non-finite transition duration cannot be saved");

    std::puts("PASS: CGE settings bounds, finite values, persistence and failure handling.");
}
