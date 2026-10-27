#include "ssao.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool v,const char* message){if(!v){std::printf("FAIL: %s\n",message);std::exit(1);}}
CameraPose Pose(float x=0,float yaw=0) {
    CameraPose p;p.right={std::cos(yaw),0,-std::sin(yaw)};p.up={0,1,0};
    p.forward={std::sin(yaw),0,std::cos(yaw)};p.position={x,0,0};p.valid=true;return p;
}
std::array<float,16> Projection(const CameraPose& p) {
    std::array<float,16> m{};const CameraVector rows[]={p.right,p.up,p.forward,p.forward};
    const float scales[]={1,1.3333333f,1.00025f,1};
    for(unsigned r=0;r<4;++r){for(unsigned j=0;j<3;++j)m[r*4+j]=rows[r][j]*scales[r];m[r*4+3]=-CameraDot(rows[r],p.position)*scales[r];}
    m[11]-=5.00125f;return m;
}
int main() {
    auto p=Pose(17,0.15f);const auto reconstructed=CameraFromProjection(Projection(p),p.position);
    Check(reconstructed.Valid() && CameraDot(reconstructed.forward,p.forward)>0.9999f && CameraDot(reconstructed.right,p.right)>0.9999f,"Native projection yields orthonormal camera basis");
    auto invalid=Projection(p);invalid[0]=std::numeric_limits<float>::quiet_NaN();Check(!CameraFromProjection(invalid,p.position).Valid(),"Non-finite camera rejected");
    invalid={};Check(!CameraFromProjection(invalid,p.position).Valid(),"Unavailable projection rejected");
    auto affine=Projection(p);affine[12]=affine[13]=affine[14]=0;affine[15]=1;
    Check(!CameraFromProjection(affine,p.position).Valid(),"Unrecognized camera matrix falls back safely");
    auto identity=Pose();auto moved=Pose(10);const auto transform=PreviousCameraTransform(identity,moved);
    Check(transform[0]==1 && transform[5]==1 && transform[10]==1 && transform[3]==10,"Camera translation reprojects into previous view");
    Check(CompatibleCameraMotion(identity,moved,80) && !CompatibleCameraMotion(identity,Pose(1000),80) &&
        !CompatibleCameraMotion(identity,Pose(0,0.8f),80),"Camera cuts and teleportation reject history");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    SsaoRenderer filtered,raw;std::string error;Check(filtered.Initialize(device.Get(),error) && raw.Initialize(device.Get(),error),error.c_str());
    Check(filtered.SharedTemporalAvailable() && raw.SharedTemporalAvailable(),"Shared-memory temporal resolver compiled and supported");
    unsigned width=96,height=72;ComPtr<ID3D11Texture2D> depth,readback;DepthInputs inputs;
    inputs.nearPlane=5;inputs.farPlane=20000;inputs.left=-1;inputs.right=1;inputs.top=.75f;inputs.bottom=-.75f;
    inputs.state=DepthState::Ready;inputs.camera=Pose();SsaoSettings settings{true,1,80,.05f,1.5f,8000,1};
    auto create=[&](unsigned w,unsigned h){
        width=w;height=h;inputs.depth.Reset();depth.Reset();readback.Reset();D3D11_TEXTURE2D_DESC d{};
        d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&depth)),"Depth texture");Check(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth)),"Depth view");
        d.Format=DXGI_FORMAT_R16_FLOAT;d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"AO readback");
    };create(width,height);
    auto upload=[&](bool occluder){
        std::vector<float> encoded(width*height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            float rx=inputs.left+(float(x)+.5f)/width*(inputs.right-inputs.left);
            float ry=inputs.top+(float(y)+.5f)/height*(inputs.bottom-inputs.top);
            CameraVector direction{};for(unsigned j=0;j<3;++j)direction[j]=inputs.camera.right[j]*rx+inputs.camera.up[j]*ry+inputs.camera.forward[j];
            float z=(300-inputs.camera.position[2])/direction[2];float front=(275-inputs.camera.position[2])/direction[2];
            float wx=inputs.camera.position[0]+front*direction[0],wy=inputs.camera.position[1]+front*direction[1];
            if(occluder && wx>=-140 && wx<0 && wy>=-110 && wy<110)z=front;
            encoded[y*width+x]=inputs.farPlane*(z-inputs.nearPlane)/(z*(inputs.farPlane-inputs.nearPlane));
        }
        context->UpdateSubresource(depth.Get(),0,nullptr,encoded.data(),width*4,0);
    };
    auto render=[&](SsaoRenderer& renderer,bool reset=false){
        if(reset)renderer.ResetHistory(false);Check(renderer.PrepareOcclusion(inputs,settings,true)=="Native ambient occlusion ready","AO ready");
        ComPtr<ID3D11Resource> resource;renderer.Occlusion()->GetResource(&resource);context->CopyResource(readback.Get(),resource.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"AO map");
        std::vector<float> result(width*height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            unsigned bits=*reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+2*x);
            unsigned exponent=(bits>>10)&31;float value=exponent?std::ldexp(float((bits&1023)+1024),int(exponent)-25):std::ldexp(float(bits&1023),-24);
            Check(!(bits&0x8000) && std::isfinite(value) && value>=0 && value<=settings.strength+.001f,"Finite bounded temporal AO");result[y*width+x]=value;
        }
        context->Unmap(readback.Get(),0);return result;
    };
    std::vector<double> sum(width*height),squared(sum.size()),rawSum(sum.size()),rawSquared(sum.size());unsigned samples=0;
    for(unsigned frame=0;frame<48;++frame) {
        upload(true);const auto reference=render(raw,true),image=render(filtered);Check(filtered.TemporalActive(),"Temporal pass available without TAA or motion vectors");
        if(frame==0)Check(reference==image,"First frame seeds history unchanged");
        if(frame>=8){++samples;for(unsigned i=0;i<image.size();++i){sum[i]+=image[i];squared[i]+=image[i]*image[i];rawSum[i]+=reference[i];rawSquared[i]+=reference[i]*reference[i];}}
    }
    double variance=0,rawVariance=0,meanError=0;
    for(unsigned i=0;i<sum.size();++i){variance+=std::max(0.0,squared[i]/samples-std::pow(sum[i]/samples,2));rawVariance+=std::max(0.0,rawSquared[i]/samples-std::pow(rawSum[i]/samples,2));meanError+=std::abs(sum[i]/samples-rawSum[i]/samples);}
    std::printf("Temporal variance %.8f; current-only variance %.8f; mean drift %.8f\n",variance,rawVariance,meanError/sum.size());
    Check(rawVariance>1e-5 && variance<rawVariance*.95,"Temporal accumulation reduces sampling variation");
    Check(meanError/sum.size()<.01,"Temporal filtering retains mean occlusion");
    raw.ResetHistory();filtered.ResetHistory();upload(true);Check(render(raw)==render(filtered),"Explicit load/reset drops history and restarts sampling immediately");
    for(unsigned frame=1;frame<=5;++frame) {
        inputs.camera=Pose(float(frame)*2,.01f*frame);upload(true);const auto reference=render(raw,true),image=render(filtered);
        double difference=0;for(unsigned i=0;i<image.size();++i)difference+=std::abs(image[i]-reference[i]);
        Check(difference/image.size()<.015,"Camera movement and rotation remain aligned with current geometry");
    }
    upload(false);const auto clear=render(filtered);render(raw,true);Check(*std::max_element(clear.begin(),clear.end())<.0031f,"Removed occluder leaves no dark trail");
    inputs.camera.valid=false;upload(true);Check(render(raw,true)==render(filtered) && !filtered.TemporalActive(),"Invalid camera uses identical spatial fallback");
    inputs.camera=Pose();upload(true);Check(render(raw,true)==render(filtered),"Camera recovery seeds fresh history");
    settings.strength=.4f;upload(true);Check(render(raw,true)==render(filtered),"Effect setting changes drop history");
    inputs.left=-.8f;inputs.right=.8f;upload(true);Check(render(raw,true)==render(filtered),"Projection change drops history");
    create(65,49);upload(true);Check(render(raw,true)==render(filtered),"Resize drops history and recreates matching targets");
    for(int quality:{0,1,2}) {
        settings.quality=quality;upload(true);Check(render(raw,true)==render(filtered),"Quality change seeds matching fresh history");
        for(unsigned frame=0;frame<4;++frame){upload(true);render(raw,true);render(filtered);Check(filtered.TemporalActive(),"Temporal accumulation supports every quality");}
    }
    inputs.state=DepthState::Unavailable;Check(filtered.PrepareOcclusion(inputs,settings,true)=="Depth data unavailable","Missing depth bypass");
    inputs.state=DepthState::Ready;upload(true);Check(render(raw,true)==render(filtered),"Missing depth invalidates history");
    Check(filtered.PrepareOcclusion(inputs,settings,false)=="Off" && filtered.Occlusion()==nullptr,"Disabled AO releases history");
    std::puts("PASS: temporal AO, noise, camera reprojection, disocclusion, camera cuts, resets, resize and spatial fallback.");
}
