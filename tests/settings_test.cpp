#include "finishing_settings.h"
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
    s.showFps=true;
    s.exposure=-0.75f;s.contrast=1.25f;s.saturation=1.5f;s.sharpen=0.25f;s.gamma=1.75f;s.temperature=-35;
    Check(s.Save(path),"Atomic save to Unicode path");
    auto loaded=Settings::Load(path);
    Check(loaded.showFps,"FPS setting persists");
    Check(!loaded.enabled && loaded.exposure==s.exposure && loaded.contrast==s.contrast &&
        loaded.saturation==s.saturation && loaded.sharpen==s.sharpen && loaded.gamma==s.gamma && loaded.temperature==s.temperature,"Own format roundtrip");
    s.exposure=std::numeric_limits<float>::infinity();Check(!s.Save(path),"Reject invalid runtime values");
    loaded=Settings::Load(path);Check(loaded.exposure==-0.75f,"Failed save preserves previous config");
    Check(!s.Save(path/"missing"/"CGE.ini"),"Report unwritable path");

    Check(Settings{}.toggleScanCode==66,"F8 is the default hotkey");
    for(const char* value:{"0","67","255","256","-1","66.5","nan","66junk"}) {
        {std::ofstream f(path);f<<"[Input]\nToggleScanCode="<<value<<"\n";}
        const auto keys=Settings::Load(path);
        const unsigned expected=std::string(value)=="0"?0:std::string(value)=="67"?67:std::string(value)=="255"?255:66;
        Check(keys.toggleScanCode==expected,"Hotkey scan codes require an integer in range");
        Check(keys.Save(path) && Settings::Load(path).toggleScanCode==expected,"Hotkey mapping persists");
    }
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
    Check(motionBounds.motion.enabled && motionBounds.motion.strength==10 && motionBounds.motion.radius==1 && motionBounds.motion.samples==12,"Motion bounds and invalid quality are handled");
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
    Settings fog;fog.fog={true,0.7f,3000};fog.SetSeparateProfiles(true);
    fog.interior.fog={false,0.2f,100};fog.exteriorNight.fog={true,0.8f,1200};
    Check(fog.Save(path),"Save per-profile fog");
    auto fogReload=Settings::Load(path);
    Check(fogReload.fog==fog.fog && fogReload.interior.fog==fog.interior.fog &&
        fogReload.exteriorNight.fog==fog.exteriorNight.fog,"Fog settings persist for all profiles");
    {std::ofstream file(path);file<<"[ExteriorNight]\nFogDensity=0.9\n[ExteriorDay]\nFogStartDistance=500\n[Fog]\nEnabled=true\nDensity=0.3\n";}
    auto fogInherited=Settings::Load(path);
    Check(fogInherited.exteriorNight.fog.enabled && fogInherited.exteriorNight.fog.density==0.9f &&
        fogInherited.exteriorNight.fog.startDistance==500,"Fog profile inheritance ignores section order");
    {std::ofstream file(path);file<<"[Fog]\nEnabled=true\nDensity=99\nStartDistance=-100\n";}
    auto fogBounds=Settings::Load(path);
    Check(fogBounds.fog.density==1 && fogBounds.fog.startDistance==0,"Fog input bounds");
    fog.interior.fog.density=std::numeric_limits<float>::quiet_NaN();
    Check(!fog.Save(path),"Non-finite fog cannot overwrite saved settings");
    {std::ofstream file(path);file<<"[Bloom]\nEnabled=true\nIntensity=0.4\n";}
    auto oldFog=Settings::Load(path);
    Check(!oldFog.fog.enabled && !oldFog.interior.fog.enabled && oldFog.bloom.enabled,"0.4 settings retain their look with fog disabled");
    Settings dof;dof.dof={true,1500,0.75f};dof.SetSeparateProfiles(true);
    dof.interior.dof={false,200,0.2f};dof.exteriorNight.dof={true,500,0.9f};
    Check(dof.Save(path),"Save depth of field");
    auto dofReload=Settings::Load(path);
    Check(dofReload.dof==dof.dof && dofReload.interior.dof==dof.interior.dof &&
        dofReload.exteriorNight.dof==dof.exteriorNight.dof,"Depth of field persists per profile");
    {std::ofstream file(path);file<<"[DepthOfField]\nEnabled=true\nFocusDistance=-1\nStrength=99\n";}
    auto boundedDof=Settings::Load(path);
    Check(boundedDof.dof.focusDistance==1 && boundedDof.dof.strength==1,"Depth of field bounds");
    {std::ofstream file(path);file<<"[ExteriorNight]\nDofStrength=0.8\n[ExteriorDay]\nDofFocusDistance=2500\n[DepthOfField]\nEnabled=true\n";}
    auto inheritedDof=Settings::Load(path);
    Check(inheritedDof.exteriorNight.dof.enabled && inheritedDof.exteriorNight.dof.focusDistance==2500 &&
        inheritedDof.exteriorNight.dof.strength==0.8f,"Depth of field profile inheritance is section-order independent");
    dof.interior.dof.focusDistance=std::numeric_limits<float>::quiet_NaN();
    Check(!dof.Save(path),"Invalid depth of field cannot overwrite saved settings");
    {std::ofstream file(path);file<<"[Fog]\nEnabled=true\nDensity=0.5\n";}
    auto oldDof=Settings::Load(path);
    Check(oldDof.fog.enabled && !oldDof.dof.enabled && !oldDof.interior.dof.enabled,"0.5.0 configuration keeps depth of field disabled");
    Settings clarity;clarity.clarity={true,0.7f};clarity.SetSeparateProfiles(true);
    clarity.interior.clarity={false,0.2f};clarity.exteriorNight.clarity={true,0.9f};
    Check(clarity.Save(path),"Save clarity");
    const auto clarityReload=Settings::Load(path);
    Check(clarityReload==clarity && clarityReload.interior==clarity.interior &&
        clarityReload.exterior==clarity.exterior && clarityReload.exteriorNight==clarity.exteriorNight,"Clarity round trip for all profiles");
    {std::ofstream file(path);file<<"[ExteriorNight]\nClarityStrength=0.8\n[ExteriorDay]\nClarityStrength=0.4\n[Clarity]\nEnabled=true\nStrength=0.3\n";}
    const auto inheritedClarity=Settings::Load(path);
    Check(inheritedClarity.interior.clarity.strength==0.3f && inheritedClarity.exterior.clarity.enabled &&
        inheritedClarity.exterior.clarity.strength==0.4f && inheritedClarity.exteriorNight.clarity.strength==0.8f,"Clarity inheritance ignores section order");
    {std::ofstream file(path);file<<"[Clarity]\nEnabled=true\nStrength=99\n[Interior]\nClarityStrength=-1\n[ExteriorNight]\nClarityStrength=nan\n";}
    const auto boundedClarity=Settings::Load(path);
    Check(boundedClarity.clarity.strength==1 && boundedClarity.interior.clarity.strength==0 &&
        boundedClarity.exteriorNight.clarity.strength==1,"Clarity bounds and finite values");
    clarity.interior.clarity.strength=std::numeric_limits<float>::quiet_NaN();
    const auto beforeInvalid=Settings::Load(path);
    Check(!clarity.Save(path) && Settings::Load(path).interior==beforeInvalid.interior,"Invalid clarity cannot replace existing settings");
    {std::ofstream file(path);file<<"[Clarity]\nStrength=nan\n";}
    Check(Settings::Load(path).clarity.strength==0.25f,"Non-finite clarity retains default");
    {std::ofstream file(path);file<<"[DepthOfField]\nEnabled=true\nStrength=0.75\n";}
    Check(!Settings::Load(path).clarity.enabled && Settings::Load(path).dof.enabled,"Old settings retain their appearance");
    {
    Settings vibrance;vibrance.vibrance={true,0.7f};vibrance.SetSeparateProfiles(true);
    vibrance.interior.vibrance={false,0.2f};vibrance.exteriorNight.vibrance={true,0.9f};
    Check(vibrance.Save(path),"Save vibrance");
    const auto vibranceReload=Settings::Load(path);
    Check(vibranceReload==vibrance && vibranceReload.interior==vibrance.interior &&
        vibranceReload.exterior==vibrance.exterior && vibranceReload.exteriorNight==vibrance.exteriorNight,"Vibrance round trip for all profiles");
    {std::ofstream file(path);file<<"[ExteriorNight]\nVibranceStrength=0.8\n[ExteriorDay]\nVibranceStrength=0.4\n[Vibrance]\nEnabled=true\nStrength=0.3\n";}
    const auto inheritedVibrance=Settings::Load(path);
    Check(inheritedVibrance.interior.vibrance.strength==0.3f && inheritedVibrance.exterior.vibrance.enabled &&
        inheritedVibrance.exterior.vibrance.strength==0.4f && inheritedVibrance.exteriorNight.vibrance.strength==0.8f,"Vibrance inheritance ignores section order");
    {std::ofstream file(path);file<<"[Vibrance]\nEnabled=true\nStrength=99\n[Interior]\nVibranceStrength=-1\n[ExteriorNight]\nVibranceStrength=nan\n";}
    const auto boundedVibrance=Settings::Load(path);
    Check(boundedVibrance.vibrance.strength==1 && boundedVibrance.interior.vibrance.strength==0 &&
        boundedVibrance.exteriorNight.vibrance.strength==1,"Vibrance bounds and finite values");
    vibrance.interior.vibrance.strength=std::numeric_limits<float>::quiet_NaN();
    const auto beforeInvalid=Settings::Load(path);
    Check(!vibrance.Save(path) && Settings::Load(path).interior==beforeInvalid.interior,"Invalid vibrance cannot replace existing settings");
    {std::ofstream file(path);file<<"[Vibrance]\nStrength=nan\n";}
    Check(Settings::Load(path).vibrance.strength==0.25f,"Non-finite vibrance retains default");
    {std::ofstream file(path);file<<"[DepthOfField]\nEnabled=true\nStrength=0.75\n";}
    Check(!Settings::Load(path).vibrance.enabled && Settings::Load(path).dof.enabled,"Old settings retain their appearance");
    }
    {
        Settings finishing;
        FinishingEffects(finishing,[](auto,bool& enabled){enabled=true;});
        FinishingParameters(finishing,[](auto,auto,auto,float& value,float low,float high,auto){value=low+(high-low)*0.3f;});
        finishing.SetSeparateProfiles(true);
        FinishingParameters(finishing.interior,[](auto,auto,auto,float& value,float low,float high,auto){value=low+(high-low)*0.2f;});
        FinishingParameters(finishing.exteriorNight,[](auto,auto,auto,float& value,float low,float high,auto){value=low+(high-low)*0.8f;});
        Check(finishing.Save(path),"Save four finishing effects");
        const auto loaded=Settings::Load(path);
        Check(loaded==finishing && loaded.interior==finishing.interior && loaded.exterior==finishing.exterior &&
            loaded.exteriorNight==finishing.exteriorNight,"All finishing fields and profiles survive reload");
        {std::ofstream file(path);file<<"[ExteriorNight]\nVignetteStrength=0.8\nFilmGrainSize=3\n[ExteriorDay]\nSplitToningShadowRed=0.6\n[Vignette]\nEnabled=true\n[SplitToning]\nEnabled=true\n[FilmGrain]\nEnabled=true\n";}
        const auto inherited=Settings::Load(path);
        Check(inherited.exteriorNight.vignette.enabled && inherited.exteriorNight.vignette.strength==0.8f &&
            inherited.exteriorNight.filmGrain.size==3 && inherited.exteriorNight.splitToning.shadowRed==0.6f,
            "Finishing inheritance ignores INI section order");
        {
            std::ofstream file(path);
            FinishingEffects(finishing,[&](const char* effect,bool){file<<"["<<effect<<"]\nEnabled=true\n";
                FinishingParameters(finishing,[&](const char* group,const char* key,auto,auto,auto,float high,auto) {
                    if(std::string_view(group)==effect)file<<key<<"="<<(high+999)<<"\n";
                });
            });
        }
        const auto bounded=Settings::Load(path);
        FinishingParameters(bounded,[](auto,auto,auto,float value,auto,float high,auto){Check(value==high,"Finishing INI fields clamp to upper bounds");});
        Check(finishing.Save(path),"Restore valid settings");
        unsigned finishingFieldCount=0;
        FinishingParameters(finishing,[&](auto,auto,auto,auto,auto,auto,auto){++finishingFieldCount;});
        for(unsigned invalidField=0;invalidField<finishingFieldCount;++invalidField) {
            auto invalid=finishing;unsigned index=0;
            FinishingParameters(invalid,[&](auto,auto,auto,float& value,auto,auto,auto) {
                if(index++==invalidField)value=std::numeric_limits<float>::quiet_NaN();
            });
            Check(!invalid.Save(path) && Settings::Load(path)==finishing,"Non-finite finishing value cannot overwrite saved INI");
        }
        {std::ofstream file(path);file<<"[Color]\nSaturation=1.2\n";}
        const auto old=Settings::Load(path);
        FinishingEffects(old,[](const char* effect,bool enabled){Check(std::string_view(effect)=="Sharpness"?enabled:!enabled,"Older configurations retain sharpness compatibility while optional effects stay disabled");});
    }
    {
        Settings autofocus;autofocus.dof={true,2500,0.6f,true,7,0.08f};autofocus.SetSeparateProfiles(true);
        autofocus.interior.dof.autofocus=false;autofocus.exteriorNight.dof.focusSpeed=3;
        Check(autofocus.Save(path),"Save autofocus configuration");
        const auto restored=Settings::Load(path);
        Check(restored.dof==autofocus.dof && restored.interior.dof==autofocus.interior.dof &&
            restored.exteriorNight.dof==autofocus.exteriorNight.dof,"Autofocus mode, speed and zone persist by profile");
        {std::ofstream f(path);f<<"[ExteriorNight]\nDofAutofocus=true\nDofFocusSpeed=3\n[DepthOfField]\nFocusRadius=0.08\n";}
        const auto inherited=Settings::Load(path);
        Check(inherited.exteriorNight.dof.autofocus && inherited.exteriorNight.dof.focusSpeed==3 &&
            inherited.exteriorNight.dof.focusRadius==0.08f,"Autofocus fields inherit independent of section order");
        {std::ofstream f(path);f<<"[DepthOfField]\nAutofocus=true\nFocusSpeed=999\nFocusRadius=-1\n";}
        Check(Settings::Load(path).dof.focusSpeed==20 && Settings::Load(path).dof.focusRadius==0,"Autofocus bounds");
        autofocus.dof.focusSpeed=std::numeric_limits<float>::quiet_NaN();Check(!autofocus.Save(path),"Non-finite autofocus cannot save");
        {std::ofstream f(path);f<<"[DepthOfField]\nFocusDistance=1500\n";}
        Check(!Settings::Load(path).dof.autofocus && !Settings::Load(path).debanding.enabled,"Old configuration keeps manual focus and debanding off");
    }
    {
        Settings grading;grading.liftGammaGain.enabled=true;grading.liftGammaGain.liftRed=0.08f;
        grading.liftGammaGain.gammaGreen=1.3f;grading.liftGammaGain.gainBlue=0.8f;grading.SetSeparateProfiles(true);
        grading.interior.liftGammaGain.enabled=false;grading.exteriorNight.liftGammaGain.strength=0.4f;
        Check(grading.Save(path),"Save RGB grading");
        const auto loaded=Settings::Load(path);
        Check(loaded==grading && loaded.interior==grading.interior && loaded.exterior==grading.exterior &&
            loaded.exteriorNight==grading.exteriorNight,"All RGB grading fields persist for every profile");
        {std::ofstream f(path);f<<"[ExteriorNight]\nLiftGammaGainGammaBlue=1.4\n[LiftGammaGain]\nEnabled=true\nLiftRed=0.05\n";}
        const auto inherited=Settings::Load(path);
        Check(inherited.exteriorNight.liftGammaGain.enabled && inherited.exteriorNight.liftGammaGain.gammaBlue==1.4f &&
            inherited.exteriorNight.liftGammaGain.liftRed==0.05f,"LGG inheritance ignores section order");
        {std::ofstream f(path);f<<"[Color]\nGamma=1.1\n";}
        Check(!Settings::Load(path).liftGammaGain.enabled,"Old settings keep new grading disabled");
    }
    {
        Settings lut;lut.lut={true,0.7f,"Warm.cube"};lut.SetSeparateProfiles(true);
        lut.interior.lut.file="Identity.cube";lut.exteriorNight.lut.strength=0.2f;
        Check(lut.Save(path),"Save LUT settings");const auto loadedLut=Settings::Load(path);
        Check(loadedLut.lut==lut.lut && loadedLut.interior.lut==lut.interior.lut && loadedLut.exterior.lut==lut.exterior.lut && loadedLut.exteriorNight.lut==lut.exteriorNight.lut,"LUT settings roundtrip in every profile");
        lut.lut.file="../escape.cube";Check(!lut.Save(path),"Reject unsafe LUT names on save");
        {std::ofstream f(path);f<<"[Interior]\nLUTFile=Identity.cube\n[LUT]\nFile=Warm.cube\nEnabled=true\nStrength=0.6\n";}
        const auto inherited=Settings::Load(path);
        Check(inherited.lut.file=="Warm.cube" && inherited.interior.lut.file=="Identity.cube" && inherited.interior.lut.enabled && inherited.exterior.lut.file=="Warm.cube","LUT section order and inherited profile defaults");
        {std::ofstream f(path);f<<"[LUT]\nFile=../escape.cube\n";}
        Check(Settings::Load(path).lut.file.empty(),"Ignore unsafe LUT paths on load");
    }
    {
        Settings dirt;dirt.lensDirt={true,0.7f};dirt.SetSeparateProfiles(true);
        dirt.interior.lensDirt.enabled=false;dirt.exteriorNight.lensDirt.strength=0.2f;
        Check(dirt.Save(path),"Save Lens Dirt settings");const auto loadedDirt=Settings::Load(path);
        Check(loadedDirt.lensDirt==dirt.lensDirt && loadedDirt.interior.lensDirt==dirt.interior.lensDirt && loadedDirt.exterior.lensDirt==dirt.exterior.lensDirt && loadedDirt.exteriorNight.lensDirt==dirt.exteriorNight.lensDirt,"Lens Dirt roundtrip in every profile");
        {std::ofstream f(path);f<<"[Interior]\nLensDirtStrength=0.25\n[LensDirt]\nEnabled=true\nStrength=0.6\n";}
        const auto inherited=Settings::Load(path);
        Check(inherited.interior.lensDirt.enabled && inherited.interior.lensDirt.strength==0.25f && inherited.exterior.lensDirt.strength==0.6f,"Lens Dirt inherits shared settings regardless of section order");
        {std::ofstream f(path);f<<"[Bloom]\nEnabled=true\n";}
        Check(!Settings::Load(path).lensDirt.enabled,"Older INIs leave Lens Dirt disabled");
    }
    {
        Settings s;s.lensDistortion={true,-0.6f};s.SetSeparateProfiles(true);
        s.interior.lensDistortion.enabled=false;s.exteriorNight.lensDistortion.amount=0.8f;
        Check(s.Save(path),"Save Lens Distortion");const auto loaded=Settings::Load(path);
        Check(loaded.lensDistortion==s.lensDistortion && loaded.interior.lensDistortion==s.interior.lensDistortion && loaded.exteriorNight.lensDistortion==s.exteriorNight.lensDistortion,"Distortion saves signed values per profile");
        {std::ofstream f(path);f<<"[Interior]\nLensDistortionAmount=-0.4\n[LensDistortion]\nEnabled=true\nAmount=99\n";}
        const auto inherited=Settings::Load(path);Check(inherited.lensDistortion.amount==1 && inherited.interior.lensDistortion.amount==-0.4f && inherited.interior.lensDistortion.enabled,"Distortion bounds and section-independent inheritance");
        Settings neutral;neutral.lensDistortion.enabled=true;neutral.lensDistortion.amount=0;Check(neutral.Neutral(),"Zero distortion is neutral");
    }
    {
        {std::ofstream f(path);f<<"[Detail]\nSharpen=0.5\n";}
        auto legacy=Settings::Load(path);Check(legacy.sharpnessEnabled && legacy.sharpen==0.5f,"Old sharpness settings remain active");
        legacy.sharpnessEnabled=false;legacy.SetSeparateProfiles(true);legacy.interior.sharpnessEnabled=true;legacy.debanding.threshold=0.05f;
        Check(legacy.Save(path),"Save independent sharpness switch and debanding threshold");const auto restored=Settings::Load(path);
        Check(!restored.sharpnessEnabled && restored.sharpen==0.5f && restored.interior.sharpnessEnabled && restored.debanding.threshold==0.05f,"New detail controls persist without discarding saved strength");
    }
    {
        Settings s;s.ssao={true,0.8f,95,0.1f,2,5000,2};s.ssao.lightProtection=0.35f;s.SetSeparateProfiles(true);
        s.interior.ssao.lightProtection=0.5f;s.exteriorNight.ssao.lightProtection=0;
        s.interior.ssao.enabled=false;s.exterior.ssao.quality=0;s.exteriorNight.ssao.strength=0.3f;
        Check(s.Save(path),"Save SSAO settings");const auto loaded=Settings::Load(path);
        Check(loaded.ssao==s.ssao && loaded.interior.ssao==s.interior.ssao && loaded.exterior.ssao==s.exterior.ssao && loaded.exteriorNight.ssao==s.exteriorNight.ssao,"SSAO fields roundtrip in every profile");
        s.ssao.radius=std::numeric_limits<float>::quiet_NaN();Check(!s.Save(path),"Reject non-finite SSAO settings");
        s.ssao.radius=60;s.ssao.quality=3;Check(!s.Save(path),"Reject invalid SSAO quality on save");
        {std::ofstream f(path);f<<"[Interior]\nSSAOQuality=0\nSSAORadius=85\n[SSAO]\nEnabled=true\nRadius=9999\nQuality=2\nBias=-1\n";}
        const auto bounded=Settings::Load(path);
        Check(bounded.ssao.radius==250 && bounded.ssao.bias==0.01f && bounded.ssao.quality==2 && bounded.interior.ssao.radius==85 && bounded.interior.ssao.quality==0 && bounded.interior.ssao.enabled,"SSAO bounds and section-independent inheritance");
        {std::ofstream f(path);f<<"[SSAO]\nQuality=1.5\nStrength=nan\n";}
        Check(Settings::Load(path).ssao==cge::SsaoSettings{},"Invalid SSAO values retain safe disabled defaults");
    }
    {
        Settings global;global.dofAllowed=false;global.dof.enabled=true;global.SetSeparateProfiles(true);
        global.motion.strength=10;Check(global.Save(path),"Save global DOF switch and intensity ten");
        auto restored=Settings::Load(path);
        Check(!restored.dofAllowed && restored.dof.enabled && restored.interior.dof.enabled && restored.motion.strength==10,"Global DOF off and intensity ten persist without discarding profile settings");
    }
    std::puts("PASS: CGE settings bounds, finite values, persistence and failure handling.");
}
