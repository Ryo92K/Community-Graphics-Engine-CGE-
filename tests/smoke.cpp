#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <thread>
#include <atomic>
#include <string>
using Microsoft::WRL::ComPtr;

void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void HR(HRESULT hr, const char* message) {
    if (FAILED(hr)) { std::fprintf(stderr, "FAIL: %s: 0x%08lx\n", message,
        static_cast<unsigned long>(hr)); std::exit(1); }
}

void AlphaTest(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = td.Height = 8; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;
    HR(device->CreateTexture2D(&td, nullptr, &target), "Create color target");
    ComPtr<ID3D11RenderTargetView> view;
    HR(device->CreateRenderTargetView(target.Get(), nullptr, &view), "Create RTV");
    const char shader[] =
        "float4 VS(uint id : SV_VertexID) : SV_Position {"
        "float2 p = float2((id << 1) & 2, id & 2);"
        "return float4(p * float2(2,-2) + float2(-1,1), 0, 1); }"
        "float4 PS() : SV_Target { return float4(1,0,0,0.5); }";
    ComPtr<ID3DBlob> vsBlob, psBlob, errors;
    HR(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "VS", "vs_5_0",
        0, 0, &vsBlob, &errors), "Compile VS");
    HR(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "PS", "ps_5_0",
        0, 0, &psBlob, &errors), "Compile PS");
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    HR(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs), "Create VS");
    HR(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps), "Create PS");
    D3D11_BLEND_DESC bd{};
    auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE; rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD; rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D11BlendState> blend;
    HR(device->CreateBlendState(&bd, &blend), "Create blend state");
    ID3D11RenderTargetView* views[] = {view.Get()};
    context->OMSetRenderTargets(1, views, nullptr);
    const float blue[] = {0, 0, 1, 1};
    context->ClearRenderTargetView(view.Get(), blue);
    context->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
    D3D11_VIEWPORT viewport{0, 0, 8, 8, 0, 1};
    context->RSSetViewports(1, &viewport);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(), nullptr, 0);
    context->PSSetShader(ps.Get(), nullptr, 0);
    context->Draw(3, 0);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    HR(device->CreateTexture2D(&td, nullptr, &staging), "Create readback");
    context->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HR(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Read alpha result");
    auto pixel = static_cast<const unsigned char*>(mapped.pData) + 4 * mapped.RowPitch + 4 * 4;
    Check(std::abs(static_cast<int>(pixel[0]) - 128) <= 1 && pixel[1] == 0 &&
        std::abs(static_cast<int>(pixel[2]) - 128) <= 1 && pixel[3] == 255,
        "50% red over opaque blue must preserve alpha and produce purple");
    context->Unmap(staging.Get(), 0);
    context->ClearState();
}

int wmain(int argc, wchar_t** argv) {
    Check(argc == 2, "Expected absolute proxy DLL path");
    const HMODULE proxy = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
        LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(proxy != nullptr, "Load proxy");
    auto create = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(proxy, "D3D11CreateDevice"));
    auto createSwap = reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(
        GetProcAddress(proxy, "D3D11CreateDeviceAndSwapChain"));
    Check(create && createSwap, "Both exports exist");
    std::atomic<int> failures{};
    std::thread threads[4];
    for (auto& thread : threads) thread = std::thread([&] {
        ComPtr<ID3D11Device> device;
        if (FAILED(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &device, nullptr, nullptr))) ++failures;
    });
    for (auto& thread : threads) thread.join();
    Check(failures == 0, "Concurrent first initialization");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HR(create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context), "Create WARP device");
    AlphaTest(device.Get(), context.Get());
    wchar_t systemDirectory[MAX_PATH]{};
    Check(GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) != 0, "System directory");
    const std::wstring systemPath = std::wstring(systemDirectory) + L"\\d3d11.dll";
    const auto system = LoadLibraryExW(systemPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(system && system != proxy, "Real system runtime is distinct from proxy");
    auto realCreate = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(system, "D3D11CreateDevice"));
    Check(realCreate != nullptr, "System export exists");
    const HRESULT expected = realCreate(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        0, nullptr, nullptr, nullptr);
    const HRESULT actual = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        0, nullptr, nullptr, nullptr);
    Check(FAILED(expected) && expected == actual, "Invalid SDK HRESULT preserved");
    // A hidden window avoids interrupting the user while exercising real DXGI presentation.
    HWND window = CreateWindowExW(0, L"STATIC", L"Community Graphics Engine integration test",
        WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "Create hidden window");
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = desc.BufferDesc.Height = 64;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2; desc.OutputWindow = window; desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Device> swapDevice;
    ComPtr<ID3D11DeviceContext> swapContext;
    HR(createSwap(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &desc, &swap, &swapDevice, nullptr, &swapContext), "Create swapchain");
    HR(swap->Present(0, 0), "Present");
    HR(swap->ResizeBuffers(0, 96, 96, DXGI_FORMAT_UNKNOWN, 0), "Resize");
    HR(swap->Present(0, 0), "Present after resize");
    Check(swapDevice->GetDeviceRemovedReason() == S_OK, "Device remains healthy");
    swapContext->ClearState(); swapContext->Flush();
    swapContext.Reset(); swapDevice.Reset(); swap.Reset();
    DestroyWindow(window);
    context->ClearState(); context->Flush(); context.Reset(); device.Reset();
    FreeLibrary(system);
    FreeLibrary(proxy);
    std::puts("PASS: concurrent initialization, WARP device, alpha readback, HRESULT parity, swapchain, present, resize.");
    return 0;
}


