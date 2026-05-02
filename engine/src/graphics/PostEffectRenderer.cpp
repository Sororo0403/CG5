#include "PostEffectRenderer.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "ShaderCompiler.h"
#include "SpriteRenderer.h"
#include "SrvManager.h"

using namespace DxUtils;

void PostEffectRenderer::Initialize(DirectXCommon *dxCommon,
                                    SrvManager *srvManager,
                                    SpriteRenderer *spriteRenderer, int width,
                                    int height) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteRenderer_ = spriteRenderer;

    CreateRootSignature();
    CreatePipelineState();
    CreateConstantBuffer();

    colorGradingPass_.Initialize(dxCommon, srvManager, width, height);
    filterPass_.Initialize(dxCommon, srvManager, width, height);
    edgePass_.Initialize(dxCommon, srvManager, width, height);
    noisePass_.Initialize(dxCommon, srvManager, width, height);
    radialBlurPass_.Initialize(dxCommon, srvManager, width, height);
    vignettePass_.Initialize(dxCommon, srvManager, width, height);
    distortionPass_.Initialize(dxCommon, srvManager, width, height);
    overlayPass_.Initialize(dxCommon, srvManager, width, height);

    Resize(width, height);
}

void PostEffectRenderer::Resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = width_;
    scissorRect_.bottom = height_;

    colorGradingPass_.Resize(width_, height_);
    filterPass_.Resize(width_, height_);
    edgePass_.Resize(width_, height_);
    noisePass_.Resize(width_, height_);
    radialBlurPass_.Resize(width_, height_);
    vignettePass_.Resize(width_, height_);
    distortionPass_.Resize(width_, height_);
    overlayPass_.Resize(width_, height_);
}

void PostEffectRenderer::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
                              D3D12_GPU_DESCRIPTOR_HANDLE depthHandle) {
    PostEffectConstants constants = BuildConstants(textureHandle, depthHandle);
    UpdateConstantBuffer(constants);

    auto commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetGraphicsRootDescriptorTable(0, textureHandle);
    commandList->SetGraphicsRootDescriptorTable(1, depthHandle);
    commandList->SetGraphicsRootConstantBufferView(
        2, constBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void PostEffectRenderer::SetColorMode(ColorMode mode) {
    colorGradingPass_.SetMode(mode);
}

void PostEffectRenderer::SetFilterMode(FilterMode mode) {
    filterPass_.SetMode(mode);
}

void PostEffectRenderer::SetEdgeMode(EdgeMode mode) { edgePass_.SetMode(mode); }

void PostEffectRenderer::SetLuminanceEdgeThreshold(float threshold) {
    edgePass_.SetLuminanceThreshold(threshold);
}

void PostEffectRenderer::SetDepthEdgeThreshold(float threshold) {
    edgePass_.SetDepthThreshold(threshold);
}

void PostEffectRenderer::SetDepthParameters(float nearZ, float farZ) {
    edgePass_.SetDepthParameters(nearZ, farZ);
}

void PostEffectRenderer::SetVignettingEnabled(bool enabled) {
    vignettePass_.SetEnabled(enabled);
}

void PostEffectRenderer::SetRadialBlurCenter(float x, float y) {
    radialBlurPass_.SetCenter(x, y);
}

void PostEffectRenderer::SetRadialBlurStrength(float strength) {
    radialBlurPass_.SetStrength(strength);
}

void PostEffectRenderer::SetRadialBlurSampleCount(int32_t sampleCount) {
    radialBlurPass_.SetSampleCount(sampleCount);
}

void PostEffectRenderer::SetRandomMode(RandomMode mode) {
    noisePass_.SetMode(mode);
}

void PostEffectRenderer::SetRandomStrength(float strength) {
    noisePass_.SetStrength(strength);
}

void PostEffectRenderer::SetRandomScale(float scale) {
    noisePass_.SetScale(scale);
}

void PostEffectRenderer::SetRandomTime(float time) { noisePass_.SetTime(time); }

void PostEffectRenderer::Request(PostEffectType type) {
    if (type == PostEffectType::CounterVignette) {
        overlayPass_.Request(OverlayEffectType::CounterVignette);
    }
}

void PostEffectRenderer::Request(PostEffectType type,
                                 const DirectX::XMFLOAT3 &worldPosition) {
    if (type == PostEffectType::WarpRingStart) {
        overlayPass_.Request(OverlayEffectType::WarpRingStart, worldPosition);
        return;
    }
    if (type == PostEffectType::WarpRingEnd) {
        overlayPass_.Request(OverlayEffectType::WarpRingEnd, worldPosition);
        return;
    }

    Request(type);
}

void PostEffectRenderer::Request(PostEffectType type,
                                 const DistortionEffectParams &params) {
    if (type == PostEffectType::Distortion) {
        distortionPass_.Request(params);
        return;
    }

    Request(type);
}

void PostEffectRenderer::SetCounterVignetteActive(bool active) {
    overlayPass_.SetCounterVignetteActive(active);
}

void PostEffectRenderer::UpdateScreenEffects(float deltaTime,
                                             const Camera &camera, int width,
                                             int height) {
    colorGradingPass_.Update(deltaTime);
    filterPass_.Update(deltaTime);
    edgePass_.Update(deltaTime);
    noisePass_.Update(deltaTime);
    radialBlurPass_.Update(deltaTime);
    vignettePass_.Update(deltaTime);
    distortionPass_.Update(deltaTime);
    overlayPass_.Update(deltaTime, camera, width, height);
}

void PostEffectRenderer::DrawScreenOverlays() const {
    if (spriteRenderer_ == nullptr) {
        return;
    }
    spriteRenderer_->PreDraw();
    distortionPass_.Render({}, {}, spriteRenderer_);
    overlayPass_.Render(spriteRenderer_);
    spriteRenderer_->PostDraw();
}

const ElectricRingParamGPU &PostEffectRenderer::GetElectricRingParam() const {
    return overlayPass_.GetElectricRingParam();
}

void PostEffectRenderer::CreateRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE textureRange{};
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE depthRange{};
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_ROOT_PARAMETER params[3]{};
    params[0].InitAsDescriptorTable(1, &textureRange,
                                    D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &depthRange,
                                    D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler{};
    sampler.Init(0);
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    CD3DX12_ROOT_SIGNATURE_DESC desc{};
    desc.Init(_countof(params), params, 1, &sampler,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "Serialize PostEffect root signature failed");

    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&rootSignature_)),
                  "Create PostEffect root signature failed");
}

void PostEffectRenderer::CreatePipelineState() {
    auto vs = ShaderCompiler::Compile(
        L"engine/resources/shaders/posteffect/PostEffectVS.hlsl", "main",
        "vs_5_0");
    auto ps = ShaderCompiler::Compile(
        L"engine/resources/shaders/posteffect/PostEffectPS.hlsl", "main",
        "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    D3D12_DEPTH_STENCIL_DESC depth =
        CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState = depth;
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateGraphicsPipelineState(
                      &desc, IID_PPV_ARGS(&pipelineState_)),
                  "Create PostEffect pipeline state failed");
}

void PostEffectRenderer::CreateConstantBuffer() {
    const UINT size = Align256(sizeof(PostEffectConstants));

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heap, D3D12_HEAP_FLAG_NONE, &desc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&constBuffer_)),
                  "Create PostEffect constant buffer failed");

    ThrowIfFailed(constBuffer_->Map(
                      0, nullptr,
                      reinterpret_cast<void **>(&mappedConstBuffer_)),
                  "Map PostEffect constant buffer failed");

    PostEffectConstants constants{};
    UpdateConstantBuffer(constants);
}

void PostEffectRenderer::UpdateConstantBuffer(
    const PostEffectConstants &constants) {
    if (!mappedConstBuffer_) {
        return;
    }

    *mappedConstBuffer_ = constants;
}

PostEffectConstants PostEffectRenderer::BuildConstants(
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE depthHandle) {
    PostEffectConstants constants{};
    constants.texelSize[0] = 1.0f / static_cast<float>(width_);
    constants.texelSize[1] = 1.0f / static_cast<float>(height_);

    PostEffectPassContext context{constants, textureHandle, depthHandle, width_,
                                  height_};

    // Keep the shader's existing order to avoid changing the current look.
    filterPass_.Render(context);
    radialBlurPass_.Render(context);
    colorGradingPass_.Render(context);
    vignettePass_.Render(context);
    edgePass_.Render(context);
    noisePass_.Render(context);

    return constants;
}
