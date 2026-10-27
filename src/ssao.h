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
        Ptr<ID3D11UnorderedAccessView> unordered;
    };
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext1> context_;
    Ptr<ID3DDeviceContextState> isolated_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> geometryPixel_,aoPixel_,aoReducedPixel_,filterPixel_,ambientPixel_,filterAmbientPixel_,temporalPixel_,temporalCachedPixel_;
    Ptr<ID3D11ComputeShader> temporalCompute_;
    Ptr<ID3D11Buffer> constants_,temporalConstants_;
    Ptr<ID3D11RasterizerState> raster_;
    Surface geometry_,linearDepth_,guide_,ao_,ping_,ambient_,previousGeometry_,history_,temporal_;
    CameraPose previousCamera_;
    SsaoSettings previousSettings_;
    std::array<float,6> previousProjection_{};
    unsigned temporalFrame_{};
    bool historyValid_{},temporalOutput_{};
    unsigned width_{},height_{},aoWidth_{},aoHeight_{};
    bool ready_{},computeAvailable_{};
    void ReleaseTargets() noexcept;
    bool MakeSurface(Surface&,unsigned,unsigned,DXGI_FORMAT,bool unordered=false);
public:
    bool Initialize(ID3D11Device*,std::string& error);
    std::string PrepareOcclusion(const DepthInputs&,const SsaoSettings&,bool enabled);
    void ResetHistory(bool restartSampling=true) noexcept {historyValid_=false;if(restartSampling)temporalFrame_=0;}
    bool TemporalActive() const noexcept {return temporalOutput_;}
    bool SharedTemporalAvailable() const noexcept {return computeAvailable_;}
    ID3D11ShaderResourceView* Occlusion() const noexcept {return temporalOutput_?history_.view.Get():ambient_.view.Get();}
};
}
