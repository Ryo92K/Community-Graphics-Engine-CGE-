#include "profiles.h"
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<0.0001f;}
int main() {
    Settings s;s.separateProfiles=true;s.transitionSeconds=2;s.exterior.exposure=-1;s.interior.exposure=1;
    s.exterior.gamma=1;s.interior.gamma=2;s.exterior.temperature=-100;s.interior.temperature=100;
    ProfileTransition blend;
    Check(blend.Update(s,Environment::Unknown,0).Neutral(),"No cell uses shared values initially");
    Check(blend.Update(s,Environment::Exterior,0).exposure==-1,"First valid cell starts on its profile");
    auto start=blend.Update(s,Environment::Interior,0);
    Check(start.exposure==-1,"Crossing a doorway does not jump");
    Check(blend.Update(s,Environment::Interior,-1)==start,"Negative time does not move a running transition");
    Check(blend.Update(s,Environment::Interior,std::numeric_limits<float>::quiet_NaN())==start,"Non-finite time does not corrupt a running transition");
    auto middle=blend.Update(s,Environment::Interior,1);
    Check(Near(middle.exposure,0) && Near(middle.gamma,1.5f) && Near(middle.temperature,0),"All controls interpolate halfway");
    Check(blend.Update(s,Environment::Unknown,30)==middle,"Missing cell holds the displayed values through loading");
    Check(blend.Update(s,Environment::Exterior,0)==middle,"Reversing direction starts from the displayed value");
    Check(blend.Update(s,Environment::Exterior,2).exposure==-1,"Retargeted transition reaches its endpoint");
    s.separateProfiles=false;s.exposure=0.5f;
    Check(blend.Update(s,Environment::Exterior,0).exposure==-1,"Shared mode change also blends");
    Check(blend.Update(s,Environment::Exterior,2).exposure==0.5f,"Shared mode resolves to common values");
    s.exposure=0.8f;Check(blend.Update(s,Environment::Exterior,0).exposure==0.8f,"Editing an established profile applies immediately");
    s.separateProfiles=true;s.transitionSeconds=0;
    Check(blend.Update(s,Environment::Interior,0).exposure==1,"Zero duration switches immediately");
    blend.Reset();Check(blend.Update(s,Environment::Interior,0).exposure==1,"Reset removes state from previous sessions");
    Check(blend.Update(s,Environment::Interior,-10).exposure==1,"Negative delta cannot reverse a transition");
    Check(NightWeight(12)==0 && NightWeight(0)==1 && NightWeight(24)==1,"Day, night and midnight normalization");
    Check(Near(NightWeight(6),0.5f) && Near(NightWeight(20),0.5f),"Dawn and dusk are centered on schedule boundaries");
    Check(NightWeight(5.75f)>NightWeight(6) && NightWeight(6)>NightWeight(6.25f),"Dawn blend decreases continuously");
    Check(NightWeight(19.75f)<NightWeight(20) && NightWeight(20)<NightWeight(20.25f),"Dusk blend increases continuously");
    Check(Near(NightWeight(0,0,23),0.5f) && std::abs(NightWeight(23.999f,0,23)-NightWeight(0,0,23))<0.002f,"Midnight dawn wraps continuously");
    Settings clock;clock.SetSeparateProfiles(true);clock.exterior.exposure=-1;clock.exteriorNight.exposure=1;clock.interior.exposure=0.25f;clock.transitionSeconds=2;
    ProfileTransition time;
    Check(time.Update(clock,Environment::Exterior,0,12).exposure==-1,"Noon selects the day profile");
    Check(time.Update(clock,Environment::Exterior,0,23).exposure==-1,"Waiting across hours starts from the displayed value");
    Check(Near(time.Update(clock,Environment::Exterior,1,23).exposure,0),"Time jumps blend over the real-time duration");
    Check(time.Update(clock,Environment::Exterior,1,23).exposure==1,"Night transition completes");
    Check(time.Update(clock,Environment::Exterior,0,-1).exposure==1,"Missing clock retains the last valid hour");
    clock.transitionSeconds=0;
    Check(time.Update(clock,Environment::Interior,0,12).exposure==0.25f,"Interior ignores time of day");
    Check(time.Update(clock,Environment::Interior,0,23).exposure==0.25f,"Waiting indoors does not change the interior profile");
    time.Reset();
    Check(Near(time.Update(clock,Environment::Exterior,0,20).exposure,0),"Twilight mixes complete color profiles");
    clock.SetSeparateProfiles(false);clock.exposure=0.4f;
    Check(time.Update(clock,Environment::Exterior,0,0).exposure==0.4f,"Shared mode ignores the day/night schedule");
    Settings bloom;bloom.bloom={true,1,1,16};bloom.SetSeparateProfiles(true);bloom.transitionSeconds=2;
    bloom.interior.bloom.enabled=false;bloom.exteriorNight.bloom.intensity=2;
    ProfileTransition lights;
    Check(lights.Update(bloom,Environment::Exterior,0,12).bloom.intensity==1,"Bloom uses day profile");
    const auto halfLight=lights.Update(bloom,Environment::Interior,1,12).bloom;
    Check(halfLight.enabled && Near(halfLight.intensity,0.5f),"Bloom fades towards disabled interior");
    Check(lights.Update(bloom,Environment::Interior,1,12).bloom.intensity==0,"Bloom fade reaches zero");
    bloom.transitionSeconds=0;lights.Reset();
    Check(Near(lights.Update(bloom,Environment::Exterior,0,20).bloom.intensity,1.5f),"Bloom blends at dusk");
    Settings fog;fog.fog={true,0.6f,2000};fog.SetSeparateProfiles(true);fog.transitionSeconds=2;
    fog.interior.fog.enabled=false;fog.exteriorNight.fog={true,1,500};
    ProfileTransition mist;
    Check(mist.Update(fog,Environment::Exterior,0,12).fog.density==0.6f,"Fog uses exterior day profile");
    const auto midpoint=mist.Update(fog,Environment::Interior,1,12).fog;
    Check(midpoint.enabled && Near(midpoint.density,0.3f),"Fog fades towards a disabled profile");
    Check(mist.Update(fog,Environment::Interior,1,12).fog.density==0,"Fog fade reaches zero");
    fog.transitionSeconds=0;mist.Reset();
    const auto duskFog=mist.Update(fog,Environment::Exterior,0,20).fog;
    Check(Near(duskFog.density,0.8f) && Near(duskFog.startDistance,1250),"Fog density and distance blend at dusk");
    std::puts("PASS: profile selection, smooth transitions, retargeting, loading and shared mode.");
}
