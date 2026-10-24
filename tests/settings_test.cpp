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
    Check(!s.enabled && s.exposure==0 && s.contrast==2 && s.saturation==0 && s.sharpen==0 && s.gamma==0.5f && s.temperature==100,"Finite values, bounds and malformed fallback");
    s.exposure=-0.75f;s.contrast=1.25f;s.saturation=1.5f;s.sharpen=0.25f;s.gamma=1.75f;s.temperature=-35;
    Check(s.Save(path),"Atomic save to Unicode path");
    auto loaded=Settings::Load(path);
    Check(!loaded.enabled && loaded.exposure==s.exposure && loaded.contrast==s.contrast &&
        loaded.saturation==s.saturation && loaded.sharpen==s.sharpen && loaded.gamma==s.gamma && loaded.temperature==s.temperature,"Own format roundtrip");
    s.exposure=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject invalid runtime values");
    loaded=Settings::Load(path);Check(loaded.exposure==-0.75f,"Failed save preserves previous config");
    Check(!s.Save(path/"missing"/"CGE.ini"),"Report unwritable path");

    Check(Settings{}.Neutral(),"Neutral defaults");
    auto changed=Settings{};changed.gamma=1.1f;Check(!changed.Neutral(),"Gamma activates rendering");
    changed=Settings{};changed.temperature=1;Check(!changed.Neutral(),"Temperature activates rendering");
    s=Settings{};s.gamma=std::numeric_limits<float>::quiet_NaN();Check(!s.Save(path),"Reject non-finite gamma");
    s=Settings{};s.temperature=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject non-finite temperature");
    {std::ofstream file(path);file<<"[Color]\nExposureStops=-1\n";}
    auto legacy=Settings::Load(path);
    Check(legacy.gamma==1 && legacy.temperature==0,"Older INI files keep neutral new controls");
    {std::ofstream file(path);file<<"[Input]\nToggleKey=122.5\n[Color]\nGamma=nan\nTemperature=inf\n";}
    auto invalid=Settings::Load(path);
    Check(invalid.gamma==1 && invalid.temperature==0,"Malformed additions retain defaults");

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

    {std::ofstream file(path);file<<"[Profiles]\nSeparate=true\n[Exterior]\nExposureStops=-0.7\nGamma=1.3\n[Interior]\nExposureStops=0.2\n";}
    auto legacy02=Settings::Load(path);
    Check(legacy02.exterior==legacy02.exteriorNight && legacy02.exterior.exposure==-0.7f && legacy02.interior.exposure==0.2f,"0.2 outdoor profile migrates to identical day and night values");
    legacy02.exteriorNight.exposure=1.5f;legacy02.dayHour=7;legacy02.nightHour=21;
    Check(legacy02.Save(path),"Save distinct night values and schedule");
    auto dayNight=Settings::Load(path);
    Check(dayNight.exterior.exposure==-0.7f && dayNight.exteriorNight.exposure==1.5f && dayNight.dayHour==7 && dayNight.nightHour==21,"Day/night profiles and schedule roundtrip");
    {std::ofstream file(path);file<<"[ExteriorNight]\nExposureStops=2\nGamma=nan\n[ExteriorDay]\nGamma=1.6\n[Profiles]\nDayHour=99\nNightHour=-1\n";}
    auto schedule=Settings::Load(path);
    Check(schedule.exteriorNight.gamma==1.6f && schedule.exteriorNight.exposure==2 && schedule.dayHour==11 && schedule.nightHour==13,"Night fields inherit day values independent of section order; schedule is bounded");
    dayNight.exteriorNight.gamma=std::numeric_limits<float>::quiet_NaN();
    Check(!dayNight.Save(path),"Non-finite night profile is rejected");
    dayNight=Settings{};dayNight.dayHour=std::numeric_limits<float>::infinity();
    Check(!dayNight.Save(path),"Non-finite schedule is rejected");

    Settings motion;motion.motion.enabled=true;motion.motion.strength=1.25f;motion.motion.radius=37;motion.motion.samples=16;
    Check(motion.Save(path),"Save motion blur settings");
    auto motionReload=Settings::Load(path);
    Check(motionReload.motion.enabled && motionReload.motion.strength==1.25f && motionReload.motion.radius==37 && motionReload.motion.samples==16,"Motion blur settings persist across reloads");
    {std::ofstream file(path);file<<"[MotionBlur]\nEnabled=true\nStrength=99\nMaxRadius=-5\nSamples=17\n";}
    auto motionBounds=Settings::Load(path);
    Check(motionBounds.motion.enabled && motionBounds.motion.strength==2 && motionBounds.motion.radius==1 && motionBounds.motion.samples==12,"Motion bounds and invalid quality are handled");
    motion.motion.strength=std::numeric_limits<float>::quiet_NaN();
    Check(!motion.Save(path),"Invalid motion values cannot overwrite settings");
    {std::ofstream file(path);file<<"[Color]\nGamma=1.2\n";}
    Check(!Settings::Load(path).motion.enabled,"Older configurations keep motion blur disabled");
    Settings bloom;bloom.bloom={true,0.4f,0.75f,48};bloom.SetSeparateProfiles(true);
    bloom.interior.bloom={false,0.2f,0.25f,16};bloom.exteriorNight.bloom={true,0.8f,0.5f,64};
    Check(bloom.Save(path),"Save per-profile bloom");
    auto reloadedBloom=Settings::Load(path);
    Check(reloadedBloom.bloom==bloom.bloom && reloadedBloom.interior.bloom==bloom.interior.bloom &&
        reloadedBloom.exteriorNight.bloom==bloom.exteriorNight.bloom,"All bloom profile values persist");
    {std::ofstream file(path);file<<"[ExteriorNight]\nBloomIntensity=0.7\n[ExteriorDay]\nBloomRadius=24\n[Bloom]\nEnabled=true\nThreshold=0.8\n";}
    auto inheritedBloom=Settings::Load(path);
    Check(inheritedBloom.exteriorNight.bloom.enabled && inheritedBloom.exteriorNight.bloom.radius==24 &&
        inheritedBloom.exteriorNight.bloom.threshold==0.8f && inheritedBloom.exteriorNight.bloom.intensity==0.7f,"Bloom inheritance is independent of section order");
    {std::ofstream file(path);file<<"[Bloom]\nIntensity=nan\nThreshold=99\nRadius=-1\n";}
    auto boundedBloom=Settings::Load(path);
    Check(boundedBloom.bloom.intensity==0.15f && boundedBloom.bloom.threshold==1 && boundedBloom.bloom.radius==1,"Bloom input validation");
    bloom.interior.bloom.intensity=std::numeric_limits<float>::quiet_NaN();
    Check(!bloom.Save(path),"Invalid bloom cannot overwrite saved settings");
    {std::ofstream file(path);file<<"[Color]\nGamma=1.2\n";}
    Check(!Settings::Load(path).bloom.enabled,"0.3 settings keep bloom disabled");
    std::puts("PASS: CGE settings bounds, finite values, persistence and failure handling.");
}
