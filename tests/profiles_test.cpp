#include "finishing_settings.h"
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
    Settings dof;dof.dof={true,2000,0.6f};dof.SetSeparateProfiles(true);dof.transitionSeconds=2;
    dof.interior.dof={false,1000,0.5f};dof.exteriorNight.dof={true,500,1};
    ProfileTransition lens;
    Check(lens.Update(dof,Environment::Exterior,0,12).dof.focusDistance==2000,"Depth of field selects exterior day");
    const auto halfDof=lens.Update(dof,Environment::Interior,1,12).dof;
    Check(halfDof.enabled && Near(halfDof.strength,0.3f) && Near(halfDof.focusDistance,1500),"Focus and strength blend towards disabled interior");
    Check(lens.Update(dof,Environment::Interior,1,12).dof.strength==0,"Depth of field fades to zero");
    dof.transitionSeconds=0;lens.Reset();
    const auto duskDof=lens.Update(dof,Environment::Exterior,0,20).dof;
    Check(Near(duskDof.focusDistance,1250) && Near(duskDof.strength,0.8f),"Depth of field blends at dusk");
    Settings clarity;clarity.clarity={true,0.6f};clarity.SetSeparateProfiles(true);clarity.transitionSeconds=2;
    clarity.interior.clarity={false,0.5f};clarity.exteriorNight.clarity={true,1};
    ProfileTransition detail;
    Check(Near(detail.Update(clarity,Environment::Exterior,0,12).clarity.strength,0.6f),"Clarity selects day profile");
    const auto halfClarity=detail.Update(clarity,Environment::Interior,1,12).clarity;
    Check(halfClarity.enabled && Near(halfClarity.strength,0.3f),"Clarity fades towards disabled interior");
    Check(detail.Update(clarity,Environment::Interior,1,12).Neutral(),"Clarity fade reaches neutral");
    clarity.transitionSeconds=0;detail.Reset();
    Check(Near(detail.Update(clarity,Environment::Exterior,0,20).clarity.strength,0.8f),"Clarity blends at dusk");
    {
    Settings vibrance;vibrance.vibrance={true,0.6f};vibrance.SetSeparateProfiles(true);vibrance.transitionSeconds=2;
    vibrance.interior.vibrance={false,0.5f};vibrance.exteriorNight.vibrance={true,1};
    ProfileTransition detail;
    Check(Near(detail.Update(vibrance,Environment::Exterior,0,12).vibrance.strength,0.6f),"Vibrance selects day profile");
    const auto halfVibrance=detail.Update(vibrance,Environment::Interior,1,12).vibrance;
    Check(halfVibrance.enabled && Near(halfVibrance.strength,0.3f),"Vibrance fades towards disabled interior");
    Check(detail.Update(vibrance,Environment::Interior,1,12).Neutral(),"Vibrance fade reaches neutral");
    vibrance.transitionSeconds=0;detail.Reset();
    Check(Near(detail.Update(vibrance,Environment::Exterior,0,20).vibrance.strength,0.8f),"Vibrance blends at dusk");
    }
    {
        Settings finishing;
        FinishingEffects(finishing,[](auto,bool& enabled){enabled=true;});
        finishing.vignette.strength=finishing.filmGrain.strength=finishing.splitToning.strength=finishing.chromaticAberration.strength=0.8f;
        finishing.SetSeparateProfiles(true);finishing.transitionSeconds=2;
        FinishingEffects(finishing.interior,[](auto,bool& enabled){enabled=false;});
        finishing.interior.filmGrain.size=3;finishing.interior.splitToning.shadowRed=0.75f;
        ProfileTransition transition;
        transition.Update(finishing,Environment::Exterior,0,12);
        const auto half=transition.Update(finishing,Environment::Interior,1,12);
        Check(Near(half.vignette.strength,0.4f) && Near(half.filmGrain.strength,0.4f) &&
            Near(half.splitToning.strength,0.4f) && Near(half.chromaticAberration.strength,0.4f),"All finishing effects fade through disabled endpoints");
        Check(Near(half.filmGrain.size,2) && Near(half.splitToning.shadowRed,0.5f),"Grain size and palette transition smoothly");
        Check(transition.Update(finishing,Environment::Interior,1,12).Neutral(),"Disabled finishing profile reaches neutral output");
    }
    {
        Settings focus;focus.dof={true,1500,0.5f,false,2,0.02f};focus.debanding={true,0.8f,4};
        focus.SetSeparateProfiles(true);focus.exteriorNight.dof.autofocus=true;
        focus.exteriorNight.dof.focusSpeed=8;focus.exteriorNight.dof.focusRadius=0.1f;
        focus.exteriorNight.debanding.strength=0.2f;focus.exteriorNight.debanding.radius=8;
        focus.transitionSeconds=0;ProfileTransition blend;
        const auto dusk=blend.Update(focus,Environment::Exterior,0,20);
        Check(dusk.dof.autofocus && Near(dusk.dof.focusSpeed,5) && Near(dusk.dof.focusRadius,0.06f),"Autofocus mode and parameters blend at dusk");
        Check(Near(dusk.debanding.strength,0.5f) && Near(dusk.debanding.radius,6),"Debanding settings blend at dusk");
    }
    {
        Settings grading;grading.liftGammaGain.enabled=true;grading.liftGammaGain.liftRed=0.1f;
        grading.liftGammaGain.gammaGreen=1.2f;grading.liftGammaGain.gainBlue=0.8f;
        grading.SetSeparateProfiles(true);grading.transitionSeconds=2;
        grading.interior.liftGammaGain.enabled=false;ProfileTransition transition;
        transition.Update(grading,Environment::Exterior,0,12);
        const auto halfway=transition.Update(grading,Environment::Interior,1,12).liftGammaGain;
        Check(halfway.enabled && Near(halfway.strength,0.5f),"Grading fades through zero at a disabled profile");
        Check(transition.Update(grading,Environment::Interior,1,12).Neutral(),"Disabled grading transition reaches neutral");
        grading.exteriorNight.liftGammaGain.liftRed=-0.1f;grading.exteriorNight.liftGammaGain.gammaGreen=1.8f;
        grading.exteriorNight.liftGammaGain.gainBlue=1.2f;grading.transitionSeconds=0;transition.Reset();
        const auto dusk=transition.Update(grading,Environment::Exterior,0,20).liftGammaGain;
        Check(Near(dusk.liftRed,0) && Near(dusk.gammaGreen,1.5f) && Near(dusk.gainBlue,1),"Each RGB range interpolates at dusk");
    }
    {
        Settings s;s.lut={true,1,"Warm.cube"};s.SetSeparateProfiles(true);s.transitionSeconds=2;
        s.interior.lut.file="Identity.cube";ProfileTransition p;p.Update(s,Environment::Exterior,0,12);
        const auto first=p.Update(s,Environment::Interior,0.5f,12).lut;
        Check(first.file=="Warm.cube" && Near(first.strength,0.6875f),"Different LUTs fade out the old file");
        const auto middle=p.Update(s,Environment::Interior,0.5f,12).lut;
        Check(middle.strength==0 && middle.file=="Identity.cube","Different LUTs meet at neutral");
        const auto last=p.Update(s,Environment::Interior,1,12).lut;
        Check(last.strength==1 && last.file=="Identity.cube","LUT transition reaches the new file");
        s.interior.lut.file="Warm.cube";s.interior.lut.enabled=false;p.Reset();p.Update(s,Environment::Exterior,0,12);
        Check(Near(p.Update(s,Environment::Interior,1,12).lut.strength,0.5f),"Same LUT fades to a disabled profile");
    }
    {
        Settings s;s.lensDirt={true,0.8f,0.2f};s.SetSeparateProfiles(true);s.transitionSeconds=2;
        s.interior.lensDirt.enabled=false;ProfileTransition p;p.Update(s,Environment::Exterior,0,12);
        Check(Near(p.Update(s,Environment::Interior,1,12).lensDirt.strength,0.4f),"Lens Dirt fades toward a disabled profile");
        Check(Near(p.Update(s,Environment::Interior,1,12).lensDirt.strength,0),"Lens Dirt reaches zero at a disabled profile");
        s.exteriorNight.lensDirt.strength=0.2f;s.exteriorNight.lensDirt.threshold=0.8f;s.transitionSeconds=0;p.Reset();
        Check(Near(p.Update(s,Environment::Exterior,0,20).lensDirt.strength,0.5f),"Lens Dirt strength blends between day and night");
        Check(Near(p.Update(s,Environment::Exterior,0,20).lensDirt.threshold,0.5f),"Dirt illumination threshold blends between day and night");
    }
    {
        Settings s;s.lensDistortion={true,-0.8f};s.SetSeparateProfiles(true);s.transitionSeconds=2;
        s.interior.lensDistortion.enabled=false;ProfileTransition p;p.Update(s,Environment::Exterior,0,12);
        Check(Near(p.Update(s,Environment::Interior,1,12).lensDistortion.amount,-0.4f),"Negative distortion fades toward a disabled profile");
        Check(Near(p.Update(s,Environment::Interior,1,12).lensDistortion.amount,0),"Disabled distortion reaches zero");
        s.exteriorNight.lensDistortion.amount=0.8f;s.transitionSeconds=0;p.Reset();
        Check(Near(p.Update(s,Environment::Exterior,0,20).lensDistortion.amount,0),"Opposite lens distortions blend through neutral at dusk");
    }
    {
        Settings s;s.ssao={true,0.8f,80,0.05f,2,8000,2};s.SetSeparateProfiles(true);s.transitionSeconds=2;
        s.interior.ssao.enabled=false;ProfileTransition p;p.Update(s,Environment::Exterior,0,12);
        Check(Near(p.Update(s,Environment::Interior,1,12).ssao.strength,0.4f),"SSAO fades toward disabled profile");
        Check(Near(p.Update(s,Environment::Interior,1,12).ssao.strength,0),"Disabled SSAO transition reaches zero");
        s.exteriorNight.ssao={true,0.2f,40,0.15f,1,4000,0};s.transitionSeconds=0;p.Reset();
        const auto dusk=p.Update(s,Environment::Exterior,0,20).ssao;
        Check(Near(dusk.strength,0.5f) && Near(dusk.radius,60) && Near(dusk.power,1.5f) && dusk.quality==0,"SSAO numeric settings blend and quality stays discrete");
    }
    std::puts("PASS: profile selection, smooth transitions, retargeting, loading and shared mode.");
}
