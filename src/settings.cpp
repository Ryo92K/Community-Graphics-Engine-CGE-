#include "settings.h"
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>
namespace cge {
Settings Settings::Load(const std::filesystem::path& path) {
    Settings result;std::ifstream file(path);std::string line,section;
    struct Entry {std::string section,key,value;};std::vector<Entry> profileEntries;
    auto trim=[](std::string s) {
        auto begin=s.find_first_not_of(" \t\r");
        return begin==std::string::npos?std::string{}:s.substr(begin,s.find_last_not_of(" \t\r")-begin+1);
    };
    while(std::getline(file,line)) {
        line=trim(line);if(line.empty() || line[0]==';' || line[0]=='#')continue;
        if(line.front()=='[' && line.back()==']'){section=line.substr(1,line.size()-2);continue;}
        auto separator=line.find('=');if(separator==std::string::npos)continue;
        auto key=trim(line.substr(0,separator)),value=trim(line.substr(separator+1));
        if(section=="CGE" && key=="Enabled") {
            if(value=="true" || value=="1")result.enabled=true;
            else if(value=="false" || value=="0")result.enabled=false;
        }
        if(section=="Profiles" && key=="Separate") {
            if(value=="true" || value=="1")result.separateProfiles=true;
            else if(value=="false" || value=="0")result.separateProfiles=false;
        }
        if(section=="Interior" || section=="Exterior" || section=="ExteriorDay" || section=="ExteriorNight")profileEntries.push_back({section,key,value});
        if(section=="Clarity" && key=="Enabled") {
            if(value=="true" || value=="1")result.clarity.enabled=true;
            else if(value=="false" || value=="0")result.clarity.enabled=false;
        }
        if(section=="DepthOfField" && key=="Enabled") {
            if(value=="true" || value=="1")result.dof.enabled=true;
            else if(value=="false" || value=="0")result.dof.enabled=false;
        }
        if(section=="Fog" && key=="Enabled") {
            if(value=="true" || value=="1")result.fog.enabled=true;
            else if(value=="false" || value=="0")result.fog.enabled=false;
        }
        if(section=="Bloom" && key=="Enabled") {
            if(value=="true" || value=="1")result.bloom.enabled=true;
            else if(value=="false" || value=="0")result.bloom.enabled=false;
        }
        if(section=="MotionBlur" && key=="Enabled") {
            if(value=="true" || value=="1")result.motion.enabled=true;
            else if(value=="false" || value=="0")result.motion.enabled=false;
        }
        if(section=="MotionBlur" && key=="Samples") {
            int count{};auto read=std::from_chars(value.data(),value.data()+value.size(),count);
            if(read.ec==std::errc{} && read.ptr==value.data()+value.size() && MotionSettings::ValidSamples(count))result.motion.samples=count;
        }
        float number{};auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(number))continue;
        if(section=="Clarity" && key=="Strength")result.clarity.strength=std::clamp(number,0.0f,1.0f);
        if(section=="DepthOfField" && key=="FocusDistance")result.dof.focusDistance=std::clamp(number,1.0f,50000.0f);
        if(section=="DepthOfField" && key=="Strength")result.dof.strength=std::clamp(number,0.0f,1.0f);
        if(section=="Fog" && key=="Density")result.fog.density=std::clamp(number,0.0f,1.0f);
        if(section=="Fog" && key=="StartDistance")result.fog.startDistance=std::clamp(number,0.0f,50000.0f);
        if(section=="Bloom" && key=="Intensity")result.bloom.intensity=std::clamp(number,0.0f,2.0f);
        if(section=="Bloom" && key=="Threshold")result.bloom.threshold=std::clamp(number,0.0f,1.0f);
        if(section=="Bloom" && key=="Radius")result.bloom.radius=std::clamp(number,1.0f,64.0f);
        if(section=="MotionBlur" && key=="Strength")result.motion.strength=std::clamp(number,0.0f,2.0f);
        if(section=="MotionBlur" && key=="MaxRadius")result.motion.radius=std::clamp(number,1.0f,64.0f);
        if(section=="Profiles" && key=="DayHour")result.dayHour=std::clamp(number,0.0f,11.0f);
        if(section=="Profiles" && key=="NightHour")result.nightHour=std::clamp(number,13.0f,23.0f);
        if(section=="Profiles" && key=="TransitionSeconds")result.transitionSeconds=std::clamp(number,0.0f,5.0f);
        if(section=="Color" && key=="ExposureStops")result.exposure=std::clamp(number,-3.0f,3.0f);
        if(section=="Color" && key=="Contrast")result.contrast=std::clamp(number,0.0f,2.0f);
        if(section=="Color" && key=="Saturation")result.saturation=std::clamp(number,0.0f,2.0f);
        if(section=="Color" && key=="Gamma")result.gamma=std::clamp(number,0.5f,2.5f);
        if(section=="Color" && key=="Temperature")result.temperature=std::clamp(number,-100.0f,100.0f);
        if(section=="Detail" && key=="Sharpen")result.sharpen=std::clamp(number,0.0f,1.0f);
    }
    result.interior=result.exterior=static_cast<const ColorSettings&>(result);
    result.profilesConfigured=!profileEntries.empty();
    auto apply=[&](const char* name,ColorSettings& profile) {
        for(const auto& entry:profileEntries) {
            if(entry.section!=name)continue;
            if(entry.key=="ClarityEnabled") {
                if(entry.value=="true" || entry.value=="1")profile.clarity.enabled=true;
                else if(entry.value=="false" || entry.value=="0")profile.clarity.enabled=false;
            }
            if(entry.key=="DofEnabled") {
                if(entry.value=="true" || entry.value=="1")profile.dof.enabled=true;
                else if(entry.value=="false" || entry.value=="0")profile.dof.enabled=false;
            }
            if(entry.key=="FogEnabled") {
                if(entry.value=="true" || entry.value=="1")profile.fog.enabled=true;
                else if(entry.value=="false" || entry.value=="0")profile.fog.enabled=false;
            }
            if(entry.key=="BloomEnabled") {
                if(entry.value=="true" || entry.value=="1")profile.bloom.enabled=true;
                else if(entry.value=="false" || entry.value=="0")profile.bloom.enabled=false;
            }
            float number{};const auto& value=entry.value;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(number))continue;
            if(entry.key=="ClarityStrength")profile.clarity.strength=std::clamp(number,0.0f,1.0f);
            if(entry.key=="DofFocusDistance")profile.dof.focusDistance=std::clamp(number,1.0f,50000.0f);
            if(entry.key=="DofStrength")profile.dof.strength=std::clamp(number,0.0f,1.0f);
            if(entry.key=="FogDensity")profile.fog.density=std::clamp(number,0.0f,1.0f);
            if(entry.key=="FogStartDistance")profile.fog.startDistance=std::clamp(number,0.0f,50000.0f);
            if(entry.key=="BloomIntensity")profile.bloom.intensity=std::clamp(number,0.0f,2.0f);
            if(entry.key=="BloomThreshold")profile.bloom.threshold=std::clamp(number,0.0f,1.0f);
            if(entry.key=="BloomRadius")profile.bloom.radius=std::clamp(number,1.0f,64.0f);
            if(entry.key=="ExposureStops")profile.exposure=std::clamp(number,-3.0f,3.0f);
            if(entry.key=="Contrast")profile.contrast=std::clamp(number,0.0f,2.0f);
            if(entry.key=="Saturation")profile.saturation=std::clamp(number,0.0f,2.0f);
            if(entry.key=="Sharpen")profile.sharpen=std::clamp(number,0.0f,1.0f);
            if(entry.key=="Gamma")profile.gamma=std::clamp(number,0.5f,2.5f);
            if(entry.key=="Temperature")profile.temperature=std::clamp(number,-100.0f,100.0f);
        }
    };
    apply("Interior",result.interior);
    apply("Exterior",result.exterior);                       
    apply("ExteriorDay",result.exterior);
    result.exteriorNight=result.exterior;
    apply("ExteriorNight",result.exteriorNight);
    return result;
}
bool Settings::Save(const std::filesystem::path& path) const noexcept {
    try {
        auto finite=[](const ColorSettings& p){return std::isfinite(p.exposure)&&std::isfinite(p.contrast)&&
            std::isfinite(p.saturation)&&std::isfinite(p.sharpen)&&std::isfinite(p.gamma)&&std::isfinite(p.temperature)&&std::isfinite(p.bloom.intensity)&&
            std::isfinite(p.bloom.threshold)&&std::isfinite(p.bloom.radius)&&std::isfinite(p.fog.density)&&std::isfinite(p.fog.startDistance)&&std::isfinite(p.dof.focusDistance)&&std::isfinite(p.dof.strength)&&std::isfinite(p.clarity.strength);};
        if(!finite(*this)||!finite(interior)||!finite(exterior)||!finite(exteriorNight)||!std::isfinite(dayHour)||!std::isfinite(nightHour)||!std::isfinite(transitionSeconds))return false;
        if(!std::isfinite(motion.strength)||!std::isfinite(motion.radius)||!MotionSettings::ValidSamples(motion.samples))return false;
        std::ostringstream text;text.imbue(std::locale::classic());text<<std::setprecision(9);
        text<<"[CGE]\nEnabled="<<(enabled?"true":"false")<<"\n\n[Color]\nExposureStops="<<std::clamp(exposure,-3.0f,3.0f)
            <<"\nContrast="<<std::clamp(contrast,0.0f,2.0f)<<"\nSaturation="<<std::clamp(saturation,0.0f,2.0f)
            <<"\nGamma="<<std::clamp(gamma,0.5f,2.5f)<<"\nTemperature="<<std::clamp(temperature,-100.0f,100.0f)
            <<"\n\n[Detail]\nSharpen="<<std::clamp(sharpen,0.0f,1.0f)<<"\n";
        text<<"\n[Profiles]\nSeparate="<<(separateProfiles?"true":"false")
            <<"\nTransitionSeconds="<<std::clamp(transitionSeconds,0.0f,5.0f)
            <<"\nDayHour="<<std::clamp(dayHour,0.0f,11.0f)<<"\nNightHour="<<std::clamp(nightHour,13.0f,23.0f)<<"\n";
        auto writeProfile=[&](const char* name,const ColorSettings& p) {
            text<<"\n["<<name<<"]\nExposureStops="<<std::clamp(p.exposure,-3.0f,3.0f)
                <<"\nContrast="<<std::clamp(p.contrast,0.0f,2.0f)<<"\nSaturation="<<std::clamp(p.saturation,0.0f,2.0f)
                <<"\nSharpen="<<std::clamp(p.sharpen,0.0f,1.0f)<<"\nGamma="<<std::clamp(p.gamma,0.5f,2.5f)
                <<"\nTemperature="<<std::clamp(p.temperature,-100.0f,100.0f)
                <<"\nBloomEnabled="<<(p.bloom.enabled?"true":"false")
                <<"\nBloomIntensity="<<std::clamp(p.bloom.intensity,0.0f,2.0f)
                <<"\nBloomThreshold="<<std::clamp(p.bloom.threshold,0.0f,1.0f)
                <<"\nBloomRadius="<<std::clamp(p.bloom.radius,1.0f,64.0f)
                <<"\nFogEnabled="<<(p.fog.enabled?"true":"false")
                <<"\nFogDensity="<<std::clamp(p.fog.density,0.0f,1.0f)
                <<"\nFogStartDistance="<<std::clamp(p.fog.startDistance,0.0f,50000.0f)
                <<"\nDofEnabled="<<(p.dof.enabled?"true":"false")
                <<"\nDofFocusDistance="<<std::clamp(p.dof.focusDistance,1.0f,50000.0f)
                <<"\nDofStrength="<<std::clamp(p.dof.strength,0.0f,1.0f)
                <<"\nClarityEnabled="<<(p.clarity.enabled?"true":"false")
                <<"\nClarityStrength="<<std::clamp(p.clarity.strength,0.0f,1.0f)<<"\n";
        };
        if(profilesConfigured || separateProfiles){writeProfile("Interior",interior);writeProfile("ExteriorDay",exterior);writeProfile("ExteriorNight",exteriorNight);}
        text<<"\n[Bloom]\nEnabled="<<(bloom.enabled?"true":"false")
            <<"\nIntensity="<<std::clamp(bloom.intensity,0.0f,2.0f)
            <<"\nThreshold="<<std::clamp(bloom.threshold,0.0f,1.0f)
            <<"\nRadius="<<std::clamp(bloom.radius,1.0f,64.0f)<<"\n";
        text<<"\n[Fog]\nEnabled="<<(fog.enabled?"true":"false")
            <<"\nDensity="<<std::clamp(fog.density,0.0f,1.0f)
            <<"\nStartDistance="<<std::clamp(fog.startDistance,0.0f,50000.0f)<<"\n";
        text<<"\n[DepthOfField]\nEnabled="<<(dof.enabled?"true":"false")
            <<"\nFocusDistance="<<std::clamp(dof.focusDistance,1.0f,50000.0f)
            <<"\nStrength="<<std::clamp(dof.strength,0.0f,1.0f)<<"\n";
        text<<"\n[Clarity]\nEnabled="<<(clarity.enabled?"true":"false")
            <<"\nStrength="<<std::clamp(clarity.strength,0.0f,1.0f)<<"\n";
        text<<"\n[MotionBlur]\nEnabled="<<(motion.enabled?"true":"false")
            <<"\nStrength="<<std::clamp(motion.strength,0.0f,2.0f)
            <<"\nMaxRadius="<<std::clamp(motion.radius,1.0f,64.0f)<<"\nSamples="<<motion.samples<<"\n";
        auto temporary=path.wstring()+L".tmp";
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<text.str();file.flush();if(!file)return false;}
        const bool moved=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
        if(!moved)DeleteFileW(temporary.c_str());
        return moved;
    }catch(...){return false;}
}
}
