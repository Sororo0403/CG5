#include "PostEffectRenderer.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "ShaderCompiler.h"
#include "SrvManager.h"
#ifndef IMGUI_DISABLED
#include "imgui.h"
#endif
#include <algorithm>
#include <cmath>

using namespace DxUtils;

void PostEffectRenderer::Initialize(DirectXCommon *dxCommon,
                                    SrvManager *srvManager, int width,
                                    int height) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreatePipelineState();
    CreateConstantBuffer();
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

    UpdateConstantBuffer();
}

void PostEffectRenderer::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
                              D3D12_GPU_DESCRIPTOR_HANDLE depthHandle) {
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
    colorMode_ = mode;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetFilterMode(FilterMode mode) {
    filterMode_ = mode;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetEdgeMode(EdgeMode mode) {
    edgeMode_ = mode;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetLuminanceEdgeThreshold(float threshold) {
    luminanceEdgeThreshold_ = threshold;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetDepthEdgeThreshold(float threshold) {
    depthEdgeThreshold_ = threshold;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetDepthParameters(float nearZ, float farZ) {
    nearZ_ = nearZ;
    farZ_ = farZ;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetVignettingEnabled(bool enabled) {
    enableVignetting_ = enabled;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRadialBlurCenter(float x, float y) {
    radialBlurCenter_[0] = std::clamp(x, 0.0f, 1.0f);
    radialBlurCenter_[1] = std::clamp(y, 0.0f, 1.0f);
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRadialBlurStrength(float strength) {
    radialBlurStrength_ = std::clamp(strength, 0.0f, 1.0f);
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRadialBlurSampleCount(int32_t sampleCount) {
    radialBlurSampleCount_ = std::clamp(sampleCount, 2, 32);
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRandomMode(RandomMode mode) {
    randomMode_ = mode;
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRandomStrength(float strength) {
    randomStrength_ = std::clamp(strength, 0.0f, 1.0f);
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRandomScale(float scale) {
    randomScale_ = std::clamp(scale, 1.0f, 4096.0f);
    UpdateConstantBuffer();
}

void PostEffectRenderer::SetRandomTime(float time) {
    randomTime_ = time;
    UpdateConstantBuffer();
}

void PostEffectRenderer::Request(PostEffectType type) {
    if (type == PostEffectType::CounterVignette) {
        SetCounterVignetteActive(true);
        return;
    }
    if (type == PostEffectType::DemoPlayVignette) {
        SetDemoPlayIndicatorVisible(true);
    }
}

void PostEffectRenderer::Request(PostEffectType type,
                                 const DirectX::XMFLOAT3 &worldPosition) {
    if (type != PostEffectType::WarpRingStart &&
        type != PostEffectType::WarpRingEnd) {
        Request(type);
        return;
    }

    activeElectricRing_ = {};
    activeElectricRing_.active = true;
    activeElectricRing_.worldPos = worldPosition;
    activeElectricRing_.worldPos.y += 1.1f;

    if (type == PostEffectType::WarpRingStart) {
        activeElectricRing_.lifeTime = 0.45f;
        activeElectricRing_.startRadius = 0.01f;
        activeElectricRing_.endRadius = 0.18f;
        activeElectricRing_.ringWidth = 0.012f;
        activeElectricRing_.distortionWidth = 0.040f;
        activeElectricRing_.distortionStrength = 0.022f;
        activeElectricRing_.swirlStrength = 0.008f;
        activeElectricRing_.cloudScale = 3.8f;
        activeElectricRing_.cloudIntensity = 1.10f;
        activeElectricRing_.brightness = 1.80f;
        activeElectricRing_.haloIntensity = 0.80f;
    } else {
        activeElectricRing_.lifeTime = 0.65f;
        activeElectricRing_.startRadius = 0.02f;
        activeElectricRing_.endRadius = 0.28f;
        activeElectricRing_.ringWidth = 0.015f;
        activeElectricRing_.distortionWidth = 0.045f;
        activeElectricRing_.distortionStrength = 0.018f;
        activeElectricRing_.swirlStrength = 0.006f;
        activeElectricRing_.cloudScale = 3.5f;
        activeElectricRing_.cloudIntensity = 1.40f;
        activeElectricRing_.brightness = 2.40f;
        activeElectricRing_.haloIntensity = 1.00f;
    }
}

void PostEffectRenderer::SetCounterVignetteActive(bool active) {
    counterVignetteRequested_ = active;
}

void PostEffectRenderer::SetDemoPlayIndicatorVisible(bool visible) {
    demoPlayIndicatorVisible_ = visible;
}

void PostEffectRenderer::UpdateScreenEffects(float deltaTime,
                                             const Camera &camera, int width,
                                             int height) {
    width_ = width > 0 ? width : width_;
    height_ = height > 0 ? height : height_;

    const float targetAlpha = counterVignetteRequested_ ? 1.0f : 0.0f;
    float step = counterVignetteFadeSpeed_ * deltaTime;
    if (step > 1.0f) {
        step = 1.0f;
    }
    counterVignetteAlpha_ += (targetAlpha - counterVignetteAlpha_) * step;
    counterVignetteAlpha_ = std::clamp(counterVignetteAlpha_, 0.0f, 1.0f);
    demoPlayEffectTime_ += deltaTime;

    if (!activeElectricRing_.active) {
        electricRingParam_.enabled = 0.0f;
        return;
    }

    activeElectricRing_.time += deltaTime;
    if (activeElectricRing_.time >= activeElectricRing_.lifeTime) {
        activeElectricRing_.active = false;
        electricRingParam_.enabled = 0.0f;
        return;
    }

    using namespace DirectX;
    XMMATRIX viewProj = camera.GetView() * camera.GetProj();
    XMVECTOR pos = XMVectorSet(activeElectricRing_.worldPos.x,
                               activeElectricRing_.worldPos.y,
                               activeElectricRing_.worldPos.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(pos, viewProj);

    float w = XMVectorGetW(clip);
    if (w <= 0.0001f) {
        electricRingParam_.enabled = 0.0f;
        return;
    }

    float invW = 1.0f / w;
    float ndcX = XMVectorGetX(clip) * invW;
    float ndcY = XMVectorGetY(clip) * invW;
    float ndcZ = XMVectorGetZ(clip) * invW;
    if (ndcZ < 0.0f || ndcZ > 1.0f) {
        electricRingParam_.enabled = 0.0f;
        return;
    }

    float t = activeElectricRing_.time / activeElectricRing_.lifeTime;
    t = std::clamp(t, 0.0f, 1.0f);
    const float ease = 1.0f - std::pow(1.0f - t, 3.0f);
    const float aspect =
        static_cast<float>(width_) / static_cast<float>((std::max)(height_, 1));

    electricRingParam_.center = {ndcX * 0.5f + 0.5f, -ndcY * 0.5f + 0.5f};
    electricRingParam_.radius =
        activeElectricRing_.startRadius +
        (activeElectricRing_.endRadius - activeElectricRing_.startRadius) *
            ease;
    electricRingParam_.time = activeElectricRing_.time;
    electricRingParam_.ringWidth = activeElectricRing_.ringWidth;
    electricRingParam_.distortionWidth = activeElectricRing_.distortionWidth;
    electricRingParam_.distortionStrength =
        activeElectricRing_.distortionStrength * (1.0f - t * 0.75f);
    electricRingParam_.swirlStrength = activeElectricRing_.swirlStrength;
    electricRingParam_.cloudScale = activeElectricRing_.cloudScale;
    electricRingParam_.cloudIntensity = activeElectricRing_.cloudIntensity;
    electricRingParam_.brightness =
        activeElectricRing_.brightness * (1.0f - t * 0.35f);
    electricRingParam_.haloIntensity = activeElectricRing_.haloIntensity;
    electricRingParam_.aspectInvAspect = {aspect, 1.0f / aspect};
    electricRingParam_.innerFade = 0.85f;
    electricRingParam_.outerFade = 1.0f;
    electricRingParam_.enabled = 1.0f;
}

void PostEffectRenderer::DrawScreenOverlays() const {
#ifndef IMGUI_DISABLED
    auto drawVignette = [](const DirectX::XMFLOAT3 &color, float intensity,
                           float innerRadius, float power) {
        if (intensity <= 0.001f) {
            return;
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        if (viewport == nullptr || drawList == nullptr) {
            return;
        }

        const ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f,
                            viewport->Pos.y + viewport->Size.y * 0.5f);
        const float maxRadius =
            std::sqrt(viewport->Size.x * viewport->Size.x +
                      viewport->Size.y * viewport->Size.y) *
            0.5f;
        constexpr int kLayers = 18;

        for (int i = kLayers; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(kLayers);
            const float ring = std::clamp(
                (t - innerRadius) / (std::max)(0.001f, 1.0f - innerRadius),
                0.0f, 1.0f);
            const float alpha = std::pow(ring, power) * intensity * 0.16f;
            if (alpha <= 0.001f) {
                continue;
            }
            drawList->AddCircleFilled(
                center, maxRadius * t,
                IM_COL32(static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
                         static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
                         static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
                         static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)),
                96);
        }
    };

    if (demoPlayIndicatorVisible_) {
        const float pulse = 0.5f + 0.5f * std::sinf(demoPlayEffectTime_ * 2.6f);
        drawVignette({0.02f, 0.05f, 0.10f}, 0.28f + pulse * 0.24f, 0.28f,
                     1.7f);
    }

    if (counterVignetteAlpha_ > 0.001f) {
        drawVignette({0.06f, 0.0f, 0.0f}, counterVignetteAlpha_ * 0.78f,
                     0.34f, 1.9f);
    }

    if (electricRingParam_.enabled > 0.5f) {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        if (viewport != nullptr && drawList != nullptr) {
            const ImVec2 center(
                viewport->Pos.x + electricRingParam_.center.x * viewport->Size.x,
                viewport->Pos.y + electricRingParam_.center.y * viewport->Size.y);
            const float radius =
                electricRingParam_.radius *
                (std::min)(viewport->Size.x, viewport->Size.y);
            const int alpha = static_cast<int>(
                std::clamp(electricRingParam_.brightness * 72.0f, 0.0f, 180.0f));
            drawList->AddCircle(
                center, radius, IM_COL32(70, 190, 255, alpha), 96,
                (std::max)(2.0f, electricRingParam_.ringWidth * 420.0f));
            drawList->AddCircle(center, radius * 1.12f,
                                IM_COL32(255, 80, 180, alpha / 2), 96, 2.0f);
            drawList->AddCircleFilled(center, radius * 0.12f,
                                      IM_COL32(190, 240, 255, alpha / 3), 48);
        }
    }
#endif
}

const ElectricRingParamGPU &PostEffectRenderer::GetElectricRingParam() const {
    return electricRingParam_;
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
    const UINT size = Align256(sizeof(EffectConstBuffer));

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

    UpdateConstantBuffer();
}

void PostEffectRenderer::UpdateConstantBuffer() {
    if (!mappedConstBuffer_) {
        return;
    }

    mappedConstBuffer_->colorMode = static_cast<int32_t>(colorMode_);
    mappedConstBuffer_->enableVignetting = enableVignetting_ ? 1 : 0;
    mappedConstBuffer_->filterMode = static_cast<int32_t>(filterMode_);
    mappedConstBuffer_->texelSize[0] = 1.0f / static_cast<float>(width_);
    mappedConstBuffer_->texelSize[1] = 1.0f / static_cast<float>(height_);
    mappedConstBuffer_->edgeMode = static_cast<int32_t>(edgeMode_);
    mappedConstBuffer_->luminanceEdgeThreshold = luminanceEdgeThreshold_;
    mappedConstBuffer_->depthEdgeThreshold = depthEdgeThreshold_;
    mappedConstBuffer_->nearZ = nearZ_;
    mappedConstBuffer_->farZ = farZ_;
    mappedConstBuffer_->radialBlurCenter[0] = radialBlurCenter_[0];
    mappedConstBuffer_->radialBlurCenter[1] = radialBlurCenter_[1];
    mappedConstBuffer_->radialBlurStrength = radialBlurStrength_;
    mappedConstBuffer_->radialBlurSampleCount = radialBlurSampleCount_;
    mappedConstBuffer_->randomMode = static_cast<int32_t>(randomMode_);
    mappedConstBuffer_->randomStrength = randomStrength_;
    mappedConstBuffer_->randomScale = randomScale_;
    mappedConstBuffer_->randomTime = randomTime_;
}
