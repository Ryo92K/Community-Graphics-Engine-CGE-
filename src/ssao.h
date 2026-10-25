#pragma once
#include "settings.h"
#include "depth_inputs.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <string>
namespace cge {
class SsaoRenderer {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    struct Surface {
        Ptr<ID3D11Texture2D> texture;
        Ptr<ID3D11ShaderResourceView> view;
        Ptr<ID3D11RenderTargetView> target;
    };
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext1> context_;
    Ptr<ID3DDeviceContextState> isolated_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> geometryPixel_,aoPixel_,aoReducedPixel_,filterPixel_,ambientPixel_,filterAmbientPixel_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Surface geometry_,linearDepth_,guide_,ao_,ping_,ambient_;
    unsigned width_{},height_{},aoWidth_{},aoHeight_{};
    bool ready_{};
    void ReleaseTargets() noexcept;
    bool MakeSurface(Surface&,unsigned,unsigned,DXGI_FORMAT);
public:
    bool Initialize(ID3D11Device*,std::string& error);
    std::string PrepareOcclusion(const DepthInputs&,const SsaoSettings&,bool enabled);
    ID3D11ShaderResourceView* Occlusion() const noexcept {return ambient_.view.Get();}
};
}
