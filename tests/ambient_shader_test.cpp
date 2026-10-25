#include "ambient_shader.h"
#include "ambient_native.h"
#include "ssao.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP");
    constexpr char source[]=R"(
cbuffer Material:register(b1) {float4 albedo,directLight,emission;};
cbuffer Geometry:register(b2) {float4 padding[11];float4 ambientR,ambientG,ambientB;};
struct Input {float4 position:SV_Position;float4 normal:TEXCOORD0;};
Input Vertex(uint id:SV_VertexID) {
    Input output;float2 uv=float2((id<<1)&2,id&2);
    output.position=float4(uv*float2(2,-2)+float2(-1,1),0,1);output.normal=float4(0,0,1,1);return output;
}
struct Output {float4 color:SV_Target0;float4 motion:SV_Target1;};
Output Pixel(Input input) {
    float3 ambient=max(0,float3(dot(ambientR,input.normal),dot(ambientG,input.normal),dot(ambientB,input.normal)));
    float pointLight=dot(padding[(uint)input.position.x%11],input.normal);
    Output output;output.color=float4((ambient+directLight.rgb+pointLight)*albedo.rgb+emission.rgb,albedo.a);
    output.motion=float4(0.2,0.4,0.6,0.8);return output;
}
float4 Sky(Input input):SV_Target {return float4(0.2,0.4,0.6,1);}
)";
    auto compile=[&](const char* entry,const char* profile) {
        ComPtr<ID3DBlob> code,error;
        Check(SUCCEEDED(D3DCompile(source,sizeof(source)-1,"ambient-fixture",nullptr,nullptr,entry,profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error)),"Fixture compilation");return code;
    };
    auto pixel=compile("Pixel","ps_5_0"),vertex=compile("Vertex","vs_5_0"),sky=compile("Sky","ps_5_0");
    const auto original=std::span(static_cast<const std::uint8_t*>(pixel->GetBufferPointer()),pixel->GetBufferSize());
    auto patched=cge::AddAmbientOcclusion(original);
    Check(!patched.bytecode.empty(),patched.status.c_str());
    ComPtr<ID3D11PixelShader> validation;
    Check(SUCCEEDED(device->CreatePixelShader(patched.bytecode.data(),patched.bytecode.size(),nullptr,&validation)),"Patched shader accepted by driver");
    for(unsigned count=1;count<129;++count) {
        std::vector<unsigned char> padding(count,0x51);ComPtr<ID3DBlob> extra;
        Check(SUCCEEDED(D3DSetBlobPart(original.data(),original.size(),D3D_BLOB_PRIVATE_DATA,0,padding.data(),padding.size(),&extra)),"Checksum fixture");
        auto changed=cge::AddAmbientOcclusion({static_cast<const std::uint8_t*>(extra->GetBufferPointer()),extra->GetBufferSize()});
        Check(!changed.bytecode.empty(),changed.status.c_str());ComPtr<ID3D11PixelShader> accepted;
        Check(SUCCEEDED(device->CreatePixelShader(changed.bytecode.data(),changed.bytecode.size(),nullptr,&accepted)),"Both DXBC checksum padding branches accepted");
    }
    auto corrupt=std::vector<std::uint8_t>(original.begin(),original.end());corrupt.back()^=1;
    Check(cge::AddAmbientOcclusion(corrupt).bytecode.empty(),"Corrupted bytecode bypassed");
    for(std::size_t size=0;size<original.size();++size)Check(cge::AddAmbientOcclusion(original.first(size)).bytecode.empty(),"Truncated bytecode bypassed");
    Check(cge::AddAmbientOcclusion({static_cast<const std::uint8_t*>(sky->GetBufferPointer()),sky->GetBufferSize()}).bytecode.empty(),"Sky shader untouched");
    auto* deviceMethod=(*reinterpret_cast<void***>(device.Get()))[15];auto* contextMethod=(*reinterpret_cast<void***>(context.Get()))[9];
    std::string error;Check(cge::ambient::Attach(device.Get(),error),error.c_str());
    ComPtr<ID3D11PixelShader> native,skyPixel;ComPtr<ID3D11VertexShader> vs;
    Check(SUCCEEDED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&native)),"Native material creation");
    Check(SUCCEEDED(device->CreatePixelShader(sky->GetBufferPointer(),sky->GetBufferSize(),nullptr,&skyPixel)),"Sky creation");
    Check(SUCCEEDED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs)),"Vertex creation");
    constexpr unsigned width=64,height=64;
    std::array<ComPtr<ID3D11Texture2D>,2> targets,staging;
    std::array<ComPtr<ID3D11RenderTargetView>,2> rtvs;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    for(unsigned i=0;i<2;++i) {
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&targets[i])),"Target");
        Check(SUCCEEDED(device->CreateRenderTargetView(targets[i].Get(),nullptr,&rtvs[i])),"RTV");
        auto read=desc;read.BindFlags=0;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&read,nullptr,&staging[i])),"Staging");
    }
    ComPtr<ID3D11Texture2D> visibilityTexture;ComPtr<ID3D11ShaderResourceView> visibility;
    desc.Format=DXGI_FORMAT_R32_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&visibilityTexture)),"Visibility");
    Check(SUCCEEDED(device->CreateShaderResourceView(visibilityTexture.Get(),nullptr,&visibility)),"Visibility view");
    auto buffer=[&](const void* data,unsigned size) {
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=size;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.Usage=D3D11_USAGE_IMMUTABLE;
        D3D11_SUBRESOURCE_DATA init{data,0,0};ComPtr<ID3D11Buffer> result;
        Check(SUCCEEDED(device->CreateBuffer(&bd,&init,&result)),"Constant buffer");return result;
    };
    const float material[]={0.5f,0.6f,0.7f,0.35f,0.3f,0.4f,0.5f,0,0.1f,0.2f,0.3f,0};
    std::array<float,56> geometry{};geometry[47]=0.8f;geometry[51]=0.6f;geometry[55]=0.4f;
    auto mb=buffer(material,sizeof(material)),gb=buffer(geometry.data(),sizeof(geometry));
    auto bind=[&](ID3D11PixelShader* shader) {
        ID3D11RenderTargetView* output[]={rtvs[0].Get(),rtvs[1].Get()};context->OMSetRenderTargets(2,output,nullptr);
        D3D11_VIEWPORT vp{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&vp);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs.Get(),nullptr,0);
        ID3D11Buffer* buffers[]={mb.Get(),gb.Get()};context->PSSetConstantBuffers(1,2,buffers);context->PSSetShader(shader,nullptr,0);
    };
    auto draw=[&](ID3D11PixelShader* shader) {bind(shader);context->Draw(3,0);};
    auto read=[&](unsigned target,unsigned x,unsigned y) {
        context->CopyResource(staging[target].Get(),targets[target].Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(context->Map(staging[target].Get(),0,D3D11_MAP_READ,0,&mapped)),"Readback map");
        std::array<float,4> value{};std::memcpy(value.data(),static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*16,16);
        context->Unmap(staging[target].Get(),0);return value;
    };
    const float ambient[]={0.8f,0.6f,0.4f};
    for(float factor:{1.0f,0.25f,0.0f}) {
        std::vector<float> data(width*height,1-factor);context->UpdateSubresource(visibilityTexture.Get(),0,nullptr,data.data(),width*4,0);
        cge::ambient::Begin(visibility.Get());draw(native.Get());const auto output=read(0,32,32),motion=read(1,32,32);
        for(unsigned c=0;c<3;++c) {
            const float expected=(ambient[c]*factor+material[4+c])*material[c]+material[8+c];
            Check(std::abs(output[c]-expected)<0.00001f,"Only native directional ambient is occluded; albedo, direct and emission preserved");
            Check(std::abs(motion[c]-0.2f*float(c+1))<0.00001f,"Other native render targets preserved");
        }
        Check(output[3]==0.35f,"Material alpha preserved");cge::ambient::End();
        ComPtr<ID3D11PixelShader> restored;context->PSGetShader(&restored,nullptr,nullptr);Check(restored.Get()==native.Get(),"Original material restored at opaque end");
        draw(native.Get());Check(std::abs(read(0,32,32)[0]-0.65f)<0.00001f,"Later transparent or non-opaque pass stays native");
    }
    cge::ambient::Begin(visibility.Get());draw(skyPixel.Get());Check(std::abs(read(0,32,32)[0]-0.2f)<0.00001f,"Sky stays unchanged");cge::ambient::End();
    context->PSSetShaderResources(126,1,visibility.GetAddressOf());cge::ambient::Begin(nullptr);draw(native.Get());
    Check(std::abs(read(0,32,32)[0]-0.65f)<0.00001f,"Absent visibility bypasses shader substitution");
    cge::ambient::End();ID3D11ShaderResourceView* empty{};context->PSSetShaderResources(126,1,&empty);
    cge::SsaoRenderer ssao;Check(ssao.Initialize(device.Get(),error),error.c_str());
    ComPtr<ID3D11Texture2D> depth;Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&depth)),"Depth texture");
    cge::DepthInputs inputs;Check(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth)),"Depth SRV");
    inputs.state=cge::DepthState::Ready;inputs.nearPlane=1;inputs.farPlane=10000;
    inputs.left=-1;inputs.right=1;inputs.top=1;inputs.bottom=-1;
    std::vector<float> depths(width*height);
    auto encode=[](float z){return 10000*(z-1)/(z*9999);};
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)depths[y*width+x]=encode(x<32?275.0f:300.0f);
    context->UpdateSubresource(depth.Get(),0,nullptr,depths.data(),width*4,0);
    cge::SsaoSettings settings;settings.enabled=true;settings.strength=1;settings.radius=80;
    for(int quality=0;quality<3;++quality) {
        settings.quality=quality;
        Check(ssao.PrepareOcclusion(inputs,settings,true)=="Native ambient occlusion ready","Native pre-lighting visibility prepared");
        cge::ambient::Begin(ssao.Occlusion());draw(native.Get());
        const auto contact=read(0,35,32),background=read(0,60,32),foreground=read(0,15,32);
        std::printf("quality %d contact %.8f background %.8f foreground %.8f\n",quality,contact[0],background[0],foreground[0]);
        Check(contact[0]<0.64f && contact[0]>=0.25f,"Geometry contact occludes ambient without reducing direct light or emission");
        Check(std::abs(background[0]-0.65f)<0.0001f && std::abs(foreground[0]-0.65f)<0.0001f,"No full-screen dimming or foreground halo");
        cge::ambient::End();
    }
    std::puts(cge::ambient::Diagnostics().c_str());cge::ambient::Detach();
    draw(native.Get());Check(std::abs(read(0,32,32)[0]-0.65f)<0.00001f,"Live material remains native after backend detach");
    Check((*reinterpret_cast<void***>(device.Get()))[15]==deviceMethod && (*reinterpret_cast<void***>(context.Get()))[9]==contextMethod,"D3D hooks restored");
    std::puts("PASS: native ambient lighting, direct/emissive/alpha/sky/other MRT preservation, opaque scope, visibility quality and DXBC checksums.");
}
