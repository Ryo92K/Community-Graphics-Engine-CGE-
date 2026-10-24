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
namespace cge {
Settings Settings::Load(const std::filesystem::path& path) {
    Settings result;std::ifstream file(path);std::string line,section;
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
        float number{};auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(number))continue;
        if(section=="Color" && key=="ExposureStops")result.exposure=std::clamp(number,-3.0f,3.0f);
        if(section=="Color" && key=="Contrast")result.contrast=std::clamp(number,0.0f,2.0f);
        if(section=="Color" && key=="Saturation")result.saturation=std::clamp(number,0.0f,2.0f);
        if(section=="Detail" && key=="Sharpen")result.sharpen=std::clamp(number,0.0f,1.0f);
    }return result;
}
bool Settings::Save(const std::filesystem::path& path) const noexcept {
    try {
        if(!std::isfinite(exposure)||!std::isfinite(contrast)||!std::isfinite(saturation)||!std::isfinite(sharpen))return false;
        std::ostringstream text;text.imbue(std::locale::classic());text<<std::setprecision(9);
        text<<"[CGE]\nEnabled="<<(enabled?"true":"false")<<"\n\n[Color]\nExposureStops="<<std::clamp(exposure,-3.0f,3.0f)
            <<"\nContrast="<<std::clamp(contrast,0.0f,2.0f)<<"\nSaturation="<<std::clamp(saturation,0.0f,2.0f)
            <<"\n\n[Detail]\nSharpen="<<std::clamp(sharpen,0.0f,1.0f)<<"\n";
        auto temporary=path.wstring()+L".tmp";
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<text.str();file.flush();if(!file)return false;}
        return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    }catch(...){return false;}
}
}
