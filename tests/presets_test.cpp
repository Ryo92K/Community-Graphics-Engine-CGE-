#include "presets.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <array>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
bool Same(const Settings& a,const Settings& b) {
    return static_cast<const ColorSettings&>(a)==static_cast<const ColorSettings&>(b) &&
        a.interior==b.interior && a.exterior==b.exterior && a.exteriorNight==b.exteriorNight &&
        a.motion==b.motion && a.showFps==b.showFps && a.enabled==b.enabled && a.separateProfiles==b.separateProfiles &&
        a.profilesConfigured==b.profilesConfigured && a.transitionSeconds==b.transitionSeconds &&
        a.dayHour==b.dayHour && a.nightHour==b.nightHour;
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"Preset export directory");
    const auto root=std::filesystem::absolute(argv[1]);
    const auto choices=presets::List();Check(choices.size()==5,"Five presets retained");
    std::array<Settings,5> settings;
    for(std::size_t i=0;i<choices.size();++i) {
        auto& value=settings[i];value.vibrance={true,1};value.clarity={true,1};value.dofAllowed=false;value.showFps=true;value.motion={true,1.7f,37,16};const auto savedMotion=value.motion;
        Check(presets::Apply(i,value),"Apply preset");
        Check(value.enabled && value.separateProfiles && value.profilesConfigured && value.motion==savedMotion && value.showFps && !value.dofAllowed,"Preset keeps motion settings and the global DOF switch");
        value.motion=MotionSettings{};value.dofAllowed=true;value.showFps=false;
        const ColorSettings* profiles[]={&value,&value.interior,&value.exterior,&value.exteriorNight};
        for(const auto* p:profiles) {
            Check(p->ssao.enabled && p->ssao.quality==0 && p->ssao.strength>0 && p->ssao.strength<=0.55f,"Every profile includes bounded SSAO in Balanced quality");
            if(i==1)Check(p->temperature>=-10 && p->splitToning.strength<=0.1f && p->liftGammaGain.gainBlue<=1.01f,"Nordic avoids stacked strong blue grading");
            if(i==3)Check(p->saturation>=1 && p->vibrance.strength>=0.12f,"Cinematic retains full color in every profile");
            Check(p->vibrance.enabled && p->vibrance.strength>0 && p->vibrance.strength<=0.5f,"Every profile has a bounded Vibrance setting");
            Check(p->clarity.enabled==(i!=4),"Soft Focus preserves softness; other presets use Clarity");
            Check(p->sharpen<=0.1f,"Sharpness stays bounded alongside Clarity");
            Check(p->dof.enabled==(i==4),"Only Soft Focus uses depth of field");
            Check(p->vignette.enabled==(i>=3) && p->filmGrain.enabled==(i>=3) &&
                p->chromaticAberration.enabled==(i>=3),"Lens and grain treatments reserved for cinematic presets");
            Check(p->debanding.enabled && p->debanding.threshold>=0.025f,"Presets use the new gradient smoothing controls");
            Check(p->liftGammaGain.enabled==(i>0) && p->lensDirt.enabled==(i>0),"Styled presets use RGB grading and responsive lens dirt");
            Check(p->lut.enabled==(i==2 || i==4) && (!p->lut.enabled || p->lut.file=="Warm.cube"),"Preset LUT references use a supplied non-neutral asset");
            Check(p->lensDistortion.enabled==(i>=3),"Cinematic and screenshot styles use mild distortion");
            Check(p->splitToning.enabled==(i>0),"Natural keeps neutral toning; styled presets use color separation");
            Check(p->filmGrain.strength<=0.1f && (!p->chromaticAberration.enabled || p->chromaticAberration.strength<=0.25f),"Grain and aberration remain restrained");

        }
        Check(value.exteriorNight.ssao.strength<value.exterior.ssao.strength,"Night uses gentler ambient occlusion");
        Check(value.exteriorNight.vibrance.strength<value.exterior.vibrance.strength,"Night uses less Vibrance");
        if(i!=4)Check(value.exteriorNight.clarity.strength<value.exterior.clarity.strength,"Night uses less Clarity");
        const auto path=root/choices[i].name/"SKSE"/"Plugins"/"CGE.ini";
        std::filesystem::create_directories(path.parent_path());
        Check(value.Save(path),"Export preset INI");
        Check(Same(value,Settings::Load(path)),"Exported INI matches built-in settings after reload");
    }
    Check(settings[2].vibrance.strength>settings[0].vibrance.strength &&
        settings[0].vibrance.strength>settings[3].vibrance.strength,"Fantasy and Cinematic retain distinct color intent");
    auto original=settings[0],unchanged=original;
    Check(!presets::Apply(choices.size(),unchanged) && Same(original,unchanged),"Invalid preset leaves current settings intact");
    std::puts("PASS: preset styles, detail/color balance, all profiles, export and reload.");
}
