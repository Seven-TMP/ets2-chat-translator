#pragma once

#include "overlay_host.h"

#include <d3d11.h>

#include <cstdint>
#include <string>

class D3dOverlayRenderer
{
public:
    D3dOverlayRenderer() = default;
    ~D3dOverlayRenderer();

    D3dOverlayRenderer(const D3dOverlayRenderer&) = delete;
    D3dOverlayRenderer& operator=(const D3dOverlayRenderer&) = delete;

    bool Initialize(ID3D11Device* device);

    void Reset();

    bool Ready() const { return ready_; }
    const std::wstring& LastError() const { return lastError_; }

    bool Draw(const OverlayFrame& frame, int clientWidth, int clientHeight);

private:
    struct SavedState
    {
        ID3D11RenderTargetView* renderTarget = nullptr;
        ID3D11DepthStencilView* depthTarget = nullptr;
        UINT viewportCount = 0;
        D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
        UINT scissorCount = 0;
        D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
        ID3D11InputLayout* inputLayout = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ID3D11Buffer* vertexBuffer = nullptr;
        UINT vertexStride = 0;
        UINT vertexOffset = 0;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11Buffer* vertexConstants = nullptr;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D11ShaderResourceView* pixelResource = nullptr;
        ID3D11SamplerState* pixelSampler = nullptr;
        ID3D11Buffer* pixelConstants = nullptr;
        ID3D11BlendState* blendState = nullptr;
        FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        UINT sampleMask = 0xFFFFFFFFu;
        ID3D11DepthStencilState* depthState = nullptr;
        UINT stencilRef = 0;
        ID3D11RasterizerState* rasterState = nullptr;
    };

    bool CreatePipeline();
    bool UploadTexture(const OverlayFrame& frame, DXGI_FORMAT targetFormat);
    void ReleaseTexture();
    void ReleasePipeline();

    void SaveState(SavedState& state);
    void RestoreState(SavedState& state);
    void ReleaseState(SavedState& state);

    static bool IsSrgb(DXGI_FORMAT format);
    static DXGI_FORMAT TextureFormatFor(DXGI_FORMAT targetFormat);

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;

    ID3D11VertexShader* vertexShader_ = nullptr;
    ID3D11PixelShader* pixelShader_ = nullptr;
    ID3D11InputLayout* inputLayout_ = nullptr;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* constants_ = nullptr;
    ID3D11SamplerState* pointSampler_ = nullptr;
    ID3D11SamplerState* linearSampler_ = nullptr;
    ID3D11BlendState* blendState_ = nullptr;
    ID3D11DepthStencilState* depthState_ = nullptr;
    ID3D11RasterizerState* rasterState_ = nullptr;

    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* textureView_ = nullptr;
    DXGI_FORMAT textureFormat_ = DXGI_FORMAT_UNKNOWN;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    std::uint64_t uploadedRevision_ = 0;

    bool ready_ = false;
    std::wstring lastError_;
};
