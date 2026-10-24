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
    std::puts("PASS: profile selection, smooth transitions, retargeting, loading and shared mode.");
}
