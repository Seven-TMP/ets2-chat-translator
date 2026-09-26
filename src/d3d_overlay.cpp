#include "d3d_overlay.h"

#include <d3dcompiler.h>

#include <cstring>
#include <string>

namespace
{
const char* kShaderSource = R"(
cbuffer OverlayParams : register(b0)
{
    float2 gViewport;
    float2 gOrigin;
    float2 gSize;
    float2 gPadding;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VsInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VsOutput VSMain(VsInput input)
{
    float2 pixel = gOrigin + input.position * gSize;
    float2 ndc = float2(pixel.x / gViewport.x * 2.0f - 1.0f, 1.0f - pixel.y / gViewport.y * 2.0f);
    VsOutput output;
    output.position = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 PSMain(VsOutput input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv);
}
)";

struct OverlayVertex
{
    float x;
    float y;
    float u;
    float v;
};

struct OverlayConstants
{
    float viewport[2];
    float origin[2];
    float size[2];
    float padding[2];
};

std::wstring ErrorText(const wchar_t* stage, HRESULT hr)
{
    wchar_t buffer[160] = {};
    swprintf_s(buffer, L"%s failed (0x%08lX)", stage, (unsigned long)hr);
    return buffer;
}
} // namespace

D3dOverlayRenderer::~D3dOverlayRenderer()
{
    Reset();
}

void D3dOverlayRenderer::ReleaseTexture()
{
    if (textureView_) {
        textureView_->Release();
        textureView_ = nullptr;
    }
    if (texture_) {
        texture_->Release();
        texture_ = nullptr;
    }
    textureFormat_ = DXGI_FORMAT_UNKNOWN;
    textureWidth_ = 0;
    textureHeight_ = 0;
    uploadedRevision_ = 0;
}

void D3dOverlayRenderer::ReleasePipeline()
{
    auto release = [](auto*& object) {
        if (object) {
            object->Release();
            object = nullptr;
        }
    };
    release(vertexShader_);
    release(pixelShader_);
    release(inputLayout_);
    release(vertexBuffer_);
    release(constants_);
    release(pointSampler_);
    release(linearSampler_);
    release(blendState_);
    release(depthState_);
    release(rasterState_);
}

void D3dOverlayRenderer::Reset()
{
    ReleaseTexture();
    ReleasePipeline();
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    ready_ = false;
}

bool D3dOverlayRenderer::IsSrgb(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

DXGI_FORMAT D3dOverlayRenderer::TextureFormatFor(DXGI_FORMAT targetFormat)
{
    return IsSrgb(targetFormat) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
}

bool D3dOverlayRenderer::Initialize(ID3D11Device* device)
{
    if (!device) return false;
    if (ready_ && device_ == device) return true;

    Reset();

    device_ = device;
    device_->AddRef();

    device_->GetImmediateContext(&context_);
    if (!context_) {
        lastError_ = L"GetImmediateContext failed";
        Reset();
        return false;
    }

    if (!CreatePipeline()) {
        Reset();
        return false;
    }

    lastError_.clear();
    ready_ = true;
    return true;
}

bool D3dOverlayRenderer::CreatePipeline()
{
    const UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS;
    ID3DBlob* vertexBlob = nullptr;
    ID3DBlob* pixelBlob = nullptr;
    ID3DBlob* errors = nullptr;

    const size_t sourceLength = strlen(kShaderSource);
    HRESULT hr = D3DCompile(kShaderSource, sourceLength, "ets2_overlay.hlsl", nullptr, nullptr,
        "VSMain", "vs_4_0", flags, 0, &vertexBlob, &errors);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"D3DCompile(VSMain)", hr);
        if (errors) errors->Release();
        return false;
    }
    if (errors) {
        errors->Release();
        errors = nullptr;
    }

    hr = D3DCompile(kShaderSource, sourceLength, "ets2_overlay.hlsl", nullptr, nullptr,
        "PSMain", "ps_4_0", flags, 0, &pixelBlob, &errors);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"D3DCompile(PSMain)", hr);
        if (errors) errors->Release();
        vertexBlob->Release();
        return false;
    }
    if (errors) {
        errors->Release();
        errors = nullptr;
    }

    hr = device_->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, &vertexShader_);
    if (SUCCEEDED(hr)) {
        const D3D11_INPUT_ELEMENT_DESC elements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = device_->CreateInputLayout(elements, 2, vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &inputLayout_);
    }
    if (SUCCEEDED(hr)) {
        hr = device_->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, &pixelShader_);
    }
    pixelBlob->Release();
    vertexBlob->Release();
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"shader creation", hr);
        return false;
    }

    const OverlayVertex vertices[6] = {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
    };
    D3D11_BUFFER_DESC vertexDesc{};
    vertexDesc.ByteWidth = sizeof(vertices);
    vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = vertices;
    hr = device_->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer_);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateBuffer(vertex)", hr);
        return false;
    }

    D3D11_BUFFER_DESC constantDesc{};
    constantDesc.ByteWidth = sizeof(OverlayConstants);
    constantDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device_->CreateBuffer(&constantDesc, nullptr, &constants_);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateBuffer(constants)", hr);
        return false;
    }

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sampler, &pointSampler_);
    if (SUCCEEDED(hr)) {
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        hr = device_->CreateSamplerState(&sampler, &linearSampler_);
    }
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateSamplerState", hr);
        return false;
    }

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device_->CreateBlendState(&blend, &blendState_);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateBlendState", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth.StencilEnable = FALSE;
    hr = device_->CreateDepthStencilState(&depth, &depthState_);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateDepthStencilState", hr);
        return false;
    }

    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.FrontCounterClockwise = FALSE;
    raster.DepthBias = 0;
    raster.DepthClipEnable = TRUE;
    raster.ScissorEnable = FALSE;
    raster.MultisampleEnable = FALSE;
    raster.AntialiasedLineEnable = FALSE;
    hr = device_->CreateRasterizerState(&raster, &rasterState_);
    if (FAILED(hr)) {
        lastError_ = ErrorText(L"CreateRasterizerState", hr);
        return false;
    }

    return true;
}

bool D3dOverlayRenderer::UploadTexture(const OverlayFrame& frame, DXGI_FORMAT targetFormat)
{
    if (!frame.pixels) return false;
    const size_t needed = (size_t)frame.width * (size_t)frame.height;
    if (frame.pixels->size() < needed) return false;

    const DXGI_FORMAT format = TextureFormatFor(targetFormat);
    if (!texture_ || textureFormat_ != format || textureWidth_ != frame.width || textureHeight_ != frame.height) {
        ReleaseTexture();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = (UINT)frame.width;
        desc.Height = (UINT)frame.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &texture_);
        if (FAILED(hr)) {
            lastError_ = ErrorText(L"CreateTexture2D", hr);
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = format;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;
        hr = device_->CreateShaderResourceView(texture_, &view, &textureView_);
        if (FAILED(hr)) {
            lastError_ = ErrorText(L"CreateShaderResourceView", hr);
            ReleaseTexture();
            return false;
        }

        textureFormat_ = format;
        textureWidth_ = frame.width;
        textureHeight_ = frame.height;
        uploadedRevision_ = 0;
    }

    if (uploadedRevision_ == frame.revision) return true;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        lastError_ = L"Map(texture) failed";
        return false;
    }

    const std::uint8_t* source = reinterpret_cast<const std::uint8_t*>(frame.pixels->data());
    const size_t rowBytes = (size_t)frame.width * sizeof(std::uint32_t);
    for (int y = 0; y < frame.height; ++y) {
        memcpy(static_cast<std::uint8_t*>(mapped.pData) + (size_t)y * mapped.RowPitch,
            source + (size_t)y * rowBytes, rowBytes);
    }
    context_->Unmap(texture_, 0);

    uploadedRevision_ = frame.revision;
    return true;
}

void D3dOverlayRenderer::SaveState(SavedState& state)
{
    context_->OMGetRenderTargets(1, &state.renderTarget, &state.depthTarget);

    state.viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    context_->RSGetViewports(&state.viewportCount, state.viewports);

    state.scissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    context_->RSGetScissorRects(&state.scissorCount, state.scissors);

    context_->IAGetInputLayout(&state.inputLayout);
    context_->IAGetPrimitiveTopology(&state.topology);
    context_->IAGetVertexBuffers(0, 1, &state.vertexBuffer, &state.vertexStride, &state.vertexOffset);

    context_->VSGetShader(&state.vertexShader, nullptr, nullptr);
    context_->VSGetConstantBuffers(0, 1, &state.vertexConstants);

    context_->PSGetShader(&state.pixelShader, nullptr, nullptr);
    context_->PSGetShaderResources(0, 1, &state.pixelResource);
    context_->PSGetSamplers(0, 1, &state.pixelSampler);
    context_->PSGetConstantBuffers(0, 1, &state.pixelConstants);

    context_->OMGetBlendState(&state.blendState, state.blendFactor, &state.sampleMask);
    context_->OMGetDepthStencilState(&state.depthState, &state.stencilRef);
    context_->RSGetState(&state.rasterState);
}

void D3dOverlayRenderer::ReleaseState(SavedState& state)
{
    auto release = [](auto*& object) {
        if (object) {
            object->Release();
            object = nullptr;
        }
    };
    release(state.renderTarget);
    release(state.depthTarget);
    release(state.inputLayout);
    release(state.vertexBuffer);
    release(state.vertexShader);
    release(state.vertexConstants);
    release(state.pixelShader);
    release(state.pixelResource);
    release(state.pixelSampler);
    release(state.pixelConstants);
    release(state.blendState);
    release(state.depthState);
    release(state.rasterState);
}

void D3dOverlayRenderer::RestoreState(SavedState& state)
{
    ID3D11RenderTargetView* targets[1] = { state.renderTarget };
    context_->OMSetRenderTargets(state.renderTarget ? 1 : 0, targets, state.depthTarget);
    if (state.viewportCount > 0) {
        context_->RSSetViewports(state.viewportCount, state.viewports);
    }
    context_->RSSetScissorRects(state.scissorCount, state.scissors);
    context_->RSSetState(state.rasterState);

    context_->IASetInputLayout(state.inputLayout);
    context_->IASetPrimitiveTopology(state.topology);
    context_->IASetVertexBuffers(0, 1, &state.vertexBuffer, &state.vertexStride, &state.vertexOffset);

    context_->VSSetShader(state.vertexShader, nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, &state.vertexConstants);

    context_->PSSetShader(state.pixelShader, nullptr, 0);
    ID3D11ShaderResourceView* resources[1] = { state.pixelResource };
    context_->PSSetShaderResources(0, 1, resources);
    context_->PSSetSamplers(0, 1, &state.pixelSampler);
    context_->PSSetConstantBuffers(0, 1, &state.pixelConstants);

    context_->OMSetBlendState(state.blendState, state.blendFactor, state.sampleMask);
    context_->OMSetDepthStencilState(state.depthState, state.stencilRef);

    ReleaseState(state);
}

bool D3dOverlayRenderer::Draw(const OverlayFrame& frame, int clientWidth, int clientHeight)
{
    if (!ready_ || !context_ || !device_) return false;
    if (!frame.pixels || frame.width <= 0 || frame.height <= 0) return false;

    SavedState saved;
    SaveState(saved);
    if (!saved.renderTarget) {
        ReleaseState(saved);
        return false;
    }

    DXGI_FORMAT targetFormat = DXGI_FORMAT_UNKNOWN;
    int targetWidth = 0;
    int targetHeight = 0;
    ID3D11Resource* targetResource = nullptr;
    saved.renderTarget->GetResource(&targetResource);
    if (targetResource) {
        ID3D11Texture2D* targetTexture = nullptr;
        if (SUCCEEDED(targetResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&targetTexture))) && targetTexture) {
            D3D11_TEXTURE2D_DESC desc{};
            targetTexture->GetDesc(&desc);
            targetFormat = desc.Format;
            targetWidth = (int)desc.Width;
            targetHeight = (int)desc.Height;
            targetTexture->Release();
        }
        targetResource->Release();
    }
    if (targetWidth <= 0 || targetHeight <= 0) {
        ReleaseState(saved);
        return false;
    }

    if (!UploadTexture(frame, targetFormat)) {
        ReleaseState(saved);
        return false;
    }

    const float scaleX = clientWidth > 0 ? (float)targetWidth / (float)clientWidth : 1.0f;
    const float scaleY = clientHeight > 0 ? (float)targetHeight / (float)clientHeight : 1.0f;
    const float quadWidth = (float)frame.width * scaleX;
    const float quadHeight = (float)frame.height * scaleY;
    if (quadWidth < 1.0f || quadHeight < 1.0f) {
        ReleaseState(saved);
        return false;
    }

    OverlayConstants constants{};
    constants.viewport[0] = (float)targetWidth;
    constants.viewport[1] = (float)targetHeight;
    constants.origin[0] = (float)frame.x * scaleX;
    constants.origin[1] = (float)frame.y * scaleY;
    constants.size[0] = quadWidth;
    constants.size[1] = quadHeight;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(constants_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        lastError_ = L"Map(constants) failed";
        ReleaseState(saved);
        return false;
    }
    memcpy(mapped.pData, &constants, sizeof(constants));
    context_->Unmap(constants_, 0);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)targetWidth;
    viewport.Height = (float)targetHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
    context_->RSSetScissorRects(0, nullptr);
    context_->RSSetState(rasterState_);

    const UINT stride = sizeof(OverlayVertex);
    const UINT offset = 0;
    context_->IASetInputLayout(inputLayout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);

    context_->VSSetShader(vertexShader_, nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, &constants_);

    context_->PSSetShader(pixelShader_, nullptr, 0);
    ID3D11ShaderResourceView* resources[1] = { textureView_ };
    context_->PSSetShaderResources(0, 1, resources);
    ID3D11SamplerState* samplers[1] = { (scaleX == 1.0f && scaleY == 1.0f) ? pointSampler_ : linearSampler_ };
    context_->PSSetSamplers(0, 1, samplers);
    context_->PSSetConstantBuffers(0, 1, &constants_);

    const FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    context_->OMSetBlendState(blendState_, blendFactor, 0xFFFFFFFFu);
    context_->OMSetDepthStencilState(depthState_, 0);

    context_->Draw(6, 0);

    RestoreState(saved);
    return true;
}

