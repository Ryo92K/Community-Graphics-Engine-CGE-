#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace cge {
using CameraVector=std::array<float,3>;
inline float CameraDot(const CameraVector& a,const CameraVector& b) noexcept {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct CameraPose {
    CameraVector right{},up{},forward{},position{};
    bool valid{};
    bool Valid() const noexcept {
        if(!valid)return false;
        for(const auto& v:{right,up,forward,position})for(float f:v)if(!std::isfinite(f) || std::abs(f)>1e8f)return false;
        return std::abs(CameraDot(right,right)-1)<0.01f && std::abs(CameraDot(up,up)-1)<0.01f &&
            std::abs(CameraDot(forward,forward)-1)<0.01f && std::abs(CameraDot(right,up))<0.01f &&
            std::abs(CameraDot(right,forward))<0.01f && std::abs(CameraDot(up,forward))<0.01f;
    }
};
inline CameraPose CameraFromProjection(const std::array<float,16>& matrix,const CameraVector& position) noexcept {
    CameraPose pose;pose.position=position;
    for(float f:matrix)if(!std::isfinite(f))return {};
    pose.forward={matrix[12],matrix[13],matrix[14]};
    const float length=std::sqrt(CameraDot(pose.forward,pose.forward));
    if(!std::isfinite(length) || std::abs(length-1)>0.05f)return {};
    if(std::abs(CameraDot(pose.forward,position)+matrix[15])>std::max(0.05f,
        1e-5f*std::max({std::abs(position[0]),std::abs(position[1]),std::abs(position[2])})))return {};
    for(float& f:pose.forward)f/=length;
    auto axis=[&](unsigned row) {
        CameraVector v{matrix[row],matrix[row+1],matrix[row+2]};const float along=CameraDot(v,pose.forward);
        for(unsigned i=0;i<3;++i)v[i]-=along*pose.forward[i];
        const float squared=CameraDot(v,v);
        if(!std::isfinite(squared) || squared<1e-12f)return CameraVector{};
        for(float& f:v)f/=std::sqrt(squared);return v;
    };
    pose.right=axis(0);pose.up=axis(4);pose.valid=true;
    return pose.Valid()?pose:CameraPose{};
}
inline bool CompatibleCameraMotion(const CameraPose& previous,const CameraPose& current,float radius) noexcept {
    if(!previous.Valid() || !current.Valid() || CameraDot(previous.forward,current.forward)<0.94f ||
        CameraDot(previous.right,current.right)<0.94f)return false;
    double distance=0;
    for(unsigned i=0;i<3;++i){const double d=double(current.position[i])-previous.position[i];distance+=d*d;}
    const float limit=std::max(100.0f,radius*2);
    return distance<double(limit)*limit;
}
inline std::array<float,12> PreviousCameraTransform(const CameraPose& previous,const CameraPose& current) noexcept {
    std::array<float,12> rows{};const CameraVector axes[]={previous.right,previous.up,previous.forward};
    for(unsigned row=0;row<3;++row) {
        rows[row*4]=CameraDot(axes[row],current.right);rows[row*4+1]=CameraDot(axes[row],current.up);
        rows[row*4+2]=CameraDot(axes[row],current.forward);double translation=0;
        for(unsigned i=0;i<3;++i)translation+=(double(current.position[i])-previous.position[i])*axes[row][i];
        rows[row*4+3]=static_cast<float>(translation);
    }
    return rows;
}
}
