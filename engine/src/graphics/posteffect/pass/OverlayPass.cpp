#include "posteffect/pass/OverlayPass.h"
#include "Camera.h"
#include "SpriteRenderer.h"
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

void OverlayPass::Render(SpriteRenderer *spriteRenderer) const {
    if (spriteRenderer == nullptr) {
        return;
    }

    auto drawVignette = [](const DirectX::XMFLOAT3 &color, float intensity,
                           float innerRadius, float power, int width,
                           int height, SpriteRenderer *renderer) {
        if (intensity <= 0.001f) {
            return;
        }

        const DirectX::XMFLOAT2 center{static_cast<float>(width) * 0.5f,
                                       static_cast<float>(height) * 0.5f};
        const float maxRadius =
            std::sqrt(static_cast<float>(width * width + height * height)) *
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
            renderer->DrawFilledCircle(
                center, maxRadius * t,
                {std::clamp(color.x, 0.0f, 1.0f),
                 std::clamp(color.y, 0.0f, 1.0f),
                 std::clamp(color.z, 0.0f, 1.0f),
                 std::clamp(alpha, 0.0f, 1.0f)},
                96);
        }
    };

    if (counterVignetteAlpha_ > 0.001f) {
        drawVignette({0.06f, 0.0f, 0.0f}, counterVignetteAlpha_ * 0.78f,
                     0.34f, 1.9f, width_, height_, spriteRenderer);
    }

    if (electricRingParam_.enabled > 0.5f) {
        const DirectX::XMFLOAT2 center{
            electricRingParam_.center.x * static_cast<float>(width_),
            electricRingParam_.center.y * static_cast<float>(height_)};
        const float radius =
            electricRingParam_.radius *
            static_cast<float>((std::min)(width_, height_));
        const float alpha =
            std::clamp(electricRingParam_.brightness * 72.0f, 0.0f, 180.0f) /
            255.0f;
        spriteRenderer->DrawCircle(
            center, radius, {70.0f / 255.0f, 190.0f / 255.0f, 1.0f, alpha},
            (std::max)(2.0f, electricRingParam_.ringWidth * 420.0f), 96);
        spriteRenderer->DrawCircle(center, radius * 1.12f,
                                   {1.0f, 80.0f / 255.0f,
                                    180.0f / 255.0f, alpha * 0.5f},
                                   2.0f, 96);
        spriteRenderer->DrawFilledCircle(center, radius * 0.12f,
                                         {190.0f / 255.0f,
                                          240.0f / 255.0f, 1.0f,
                                          alpha / 3.0f},
                                         48);
    }
}
