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
        a.motion==b.motion && a.enabled==b.enabled && a.separateProfiles==b.separateProfiles &&
        a.profilesConfigured==b.profilesConfigured && a.transitionSeconds==b.transitionSeconds &&
        a.dayHour==b.dayHour && a.nightHour==b.nightHour;
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"Preset export directory");
    const auto root=std::filesystem::absolute(argv[1]);
    const auto choices=presets::List();Check(choices.size()==5,"Five presets retained");
    std::array<Settings,5> settings;
    for(std::size_t i=0;i<choices.size();++i) {
        auto& value=settings[i];value.vibrance={true,1};value.clarity={true,1};value.motion.enabled=true;
        Check(presets::Apply(i,value),"Apply preset");
        Check(value.enabled && value.separateProfiles && value.profilesConfigured && !value.motion.enabled,"Complete preset replaces previous settings");
        const ColorSettings* profiles[]={&value,&value.interior,&value.exterior,&value.exteriorNight};
        for(const auto* p:profiles) {
            Check(p->vibrance.enabled && p->vibrance.strength>0 && p->vibrance.strength<=0.35f,"Every profile uses restrained Vibrance");
            Check(p->clarity.enabled==(i!=4),"Soft Focus preserves softness; other presets use Clarity");
            Check(p->sharpen<=0.06f,"Sharpness reduced to balance the detail controls");
            Check(p->dof.enabled==(i==4),"Only Soft Focus uses depth of field");
        }
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
