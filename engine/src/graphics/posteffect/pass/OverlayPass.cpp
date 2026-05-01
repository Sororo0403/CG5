#include "posteffect/pass/OverlayPass.h"
#include "Camera.h"
#ifndef IMGUI_DISABLED
#include "imgui.h"
#endif
#include <algorithm>
#include <cmath>

void OverlayPass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                             int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    Resize(width, height);
}

void OverlayPass::Resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
}

void OverlayPass::Request(OverlayEffectType type) {
    if (type == OverlayEffectType::CounterVignette) {
        SetCounterVignetteActive(true);
        return;
    }

    if (type == OverlayEffectType::DemoPlayIndicator) {
        SetDemoPlayIndicatorVisible(true);
    }
}

void OverlayPass::Request(OverlayEffectType type,
                          const DirectX::XMFLOAT3 &worldPosition) {
    if (type != OverlayEffectType::WarpRingStart &&
        type != OverlayEffectType::WarpRingEnd &&
        type != OverlayEffectType::ElectricRing) {
        Request(type);
        return;
    }

    activeElectricRing_ = {};
    activeElectricRing_.active = true;
    activeElectricRing_.worldPos = worldPosition;
    activeElectricRing_.worldPos.y += 1.1f;

    if (type == OverlayEffectType::WarpRingStart) {
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

void OverlayPass::SetCounterVignetteActive(bool active) {
    counterVignetteRequested_ = active;
}

void OverlayPass::SetDemoPlayIndicatorVisible(bool visible) {
    demoPlayIndicatorVisible_ = visible;
}

void OverlayPass::Update(float deltaTime, const Camera &camera, int width,
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

void OverlayPass::Render() const {
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
                IM_COL32(
                    static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
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
