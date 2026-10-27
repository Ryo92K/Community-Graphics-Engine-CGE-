#include "weather_profiles.h"
#include "presets.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <cstdio>
#include <cstdlib>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<0.0001f;}
int main(int argc,char** argv) {
    Check(argc==2,"Fixture path");
    const auto path=std::filesystem::path(argv[1]);std::filesystem::create_directories(path);
    Settings s;ColorSettings base;base.exposure=0.3f;base.bloom={true,0.2f,0.5f,32};base.fog={true,0.3f,2000};
    WeatherTransition blend;auto rain=ObserveWeather(4,1,1,true);
    Check(blend.Update(base,s.weather,Environment::Exterior,rain,1)==base,"Existing configuration remains unchanged by default");
    s.weather.enabled=true;s.weather.transitionSeconds=0;
    auto wet=blend.Update(base,s.weather,Environment::Exterior,rain,0);
    Check(Near(wet.exposure,0.1f) && Near(wet.bloom.intensity,0.16f) && Near(wet.fog.density,0.405f),"Rain adjusts existing exterior values");
    Check(blend.Update(base,s.weather,Environment::Interior,rain,1)==base,"Interior is independent of weather");
    s.weather.profiles[0].exposure=0;s.weather.profiles[2].exposure=1;
    auto halfway=ObserveWeather(4,1,0.25f,true);
    auto mixed=blend.Update(base,s.weather,Environment::Exterior,halfway,0);
    Check(Near(mixed.exposure,0.55f),"Native previous/current weather percentages blend profiles");
    Check(blend.Update(base,s.weather,Environment::Exterior,{},0)==mixed,"Missing weather retains the last valid blend");
    auto invalid=halfway;invalid.weights[0]=-1;
    Check(blend.Update(base,s.weather,Environment::Exterior,invalid,0)==mixed,"Malformed weights cannot overwrite valid weather");
    for(unsigned flags=1;flags<=8;flags*=2)Check(ObserveWeather(flags,0,1,false).valid,"All four weather flags classified");
    Check(WeatherClass(12)==3 && WeatherClass(6)==2,"Precipitation takes priority over cloud flags");
    Check(!ObserveWeather(0,1,1,true).valid && !ObserveWeather(4,0,0.5f,true).valid,"Unknown classifications bypass new data");
    Check(!ObserveWeather(4,1,std::numeric_limits<float>::quiet_NaN(),true).valid &&
        !ObserveWeather(4,1,1.1f,true).valid,"Invalid native progress rejected");
    blend.Reset();s.weather.transitionSeconds=2;
    const auto initial=blend.Update(base,s.weather,Environment::Exterior,rain,0);
    Check(initial==base,"Enabling weather begins from neutral");
    const auto gradual=blend.Update(base,s.weather,Environment::Exterior,rain,0.1f);
    Check(gradual.exposure>base.exposure && gradual.exposure<base.exposure+1,"Real time transition avoids a sudden jump");
    Check(blend.Update(base,s.weather,Environment::Exterior,rain,-1)==gradual,"Negative delta cannot reverse transition");
    s.weather.enabled=false;
    Check(blend.Update(base,s.weather,Environment::Exterior,rain,0.1f).exposure<gradual.exposure,"Disabling weather blends back to base values");
    s.weather.enabled=true;s.weather.transitionSeconds=0;base.bloom.enabled=base.fog.enabled=false;
    auto disabled=blend.Update(base,s.weather,Environment::Exterior,rain,0);
    Check(!disabled.bloom.enabled && !disabled.fog.enabled && disabled.ssao==base.ssao && disabled.dof==base.dof,"Weather never enables disabled effects or alters unrelated effects");
    base.exposure=3;base.temperature=-100;s.weather.profiles[2].temperature=-100;
    auto clamped=blend.Update(base,s.weather,Environment::Exterior,rain,0);
    Check(clamped.exposure==3 && clamped.temperature==-100,"Combined adjustments stay in supported ranges");
    auto file=path/"weather.ini";
    s.weather.profiles[3]={0.4f,1.2f,0.8f,-6,0.7f,1.4f};
    Check(s.Save(file) && Settings::Load(file).weather==s.weather,"Every weather field roundtrips through INI");
    s.toggleScanCode=68;auto before=s.weather;
    Check(presets::Apply(1,s) && s.weather==before && s.toggleScanCode==68,"Visual presets preserve weather and custom hotkeys");
    s.weather.profiles[0].exposure=std::numeric_limits<float>::infinity();
    Check(!s.Save(file) && Settings::Load(file).weather==before,"Non-finite weather cannot replace a valid save");
    {std::ofstream out(file);out<<"[Weather]\nEnabled=true\nTransitionSeconds=-4\n[Weather.Rain]\nExposureOffset=99\nFogMultiplier=nan\n";}
    auto loaded=Settings::Load(file);
    Check(loaded.weather.enabled && loaded.weather.transitionSeconds==0 && loaded.weather.profiles[2].exposure==3 &&
        loaded.weather.profiles[2].fog==WeatherSettings{}.profiles[2].fog,"INI clamps ranges and rejects non-finite input");
    std::puts("Weather profiles passed");
}
