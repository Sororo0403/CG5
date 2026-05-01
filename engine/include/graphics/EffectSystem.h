#pragma once
#include "EffectParticleSystem.h"
#include "SpriteRenderer.h"
#include "TrailRenderer.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;
class SpriteRenderer;
class BillboardRenderer;

enum class EffectPreset {
    SlashArc,
    MagneticField,
};

struct EffectRequest {
    EffectPreset preset = EffectPreset::SlashArc;
    DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 start = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 end = {0.0f, 0.0f, 0.0f};
    float intensity = 1.0f;
    float duration = 0.08f;
    float radius = 1.0f;
    float alpha = 1.0f;
    float time = 0.0f;
    bool sweep = false;
};

class EffectSystem {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                    TextureManager *textureManager, SpriteRenderer *spriteRenderer,
                    BillboardRenderer *billboardRenderer, int width, int height,
                    uint32_t maxTrailPoints, uint32_t maxParticleBursts) {
        spriteRenderer_ = spriteRenderer;
        billboardRenderer_ = billboardRenderer;
        width_ = width > 0 ? width : 1;
        height_ = height > 0 ? height : 1;
        trailRenderer_.Initialize(dxCommon, srvManager, textureManager,
                                  maxTrailPoints);
        particles_.Initialize(dxCommon, srvManager, textureManager,
                              maxParticleBursts);
        trailRenderer_.SetScreenRenderer(spriteRenderer_, width_, height_);
        particles_.SetBillboardRenderer(billboardRenderer_);
    }

    void BeginFrame(float deltaTime);
    void Play(const EffectRequest &request);
    void EmitTrail(TrailRenderer::TrailPreset preset,
                   const DirectX::XMFLOAT3 &base,
                   const DirectX::XMFLOAT3 &tip, float width);
    void EmitParticles(EffectParticlePreset preset,
                       const DirectX::XMFLOAT3 &position);
    void EmitArcSparks(const DirectX::XMFLOAT3 &start,
                       const DirectX::XMFLOAT3 &end, uint32_t count,
                       float scale, bool isSweep);
    void SetTrailEnabled(bool enabled) { trailRenderer_.SetEnabled(enabled); }
    void SetTrailLifeTime(float lifeTime) { trailRenderer_.SetLifeTime(lifeTime); }
    void Resize(int width, int height);
    void Draw(const Camera &camera);

  private:
    struct ActiveEffect {
        EffectRequest request{};
        float age = 0.0f;
    };

    void DrawSlashArc(const Camera &camera, const ActiveEffect &effect) const;
    void DrawMagneticField(const Camera &camera, const ActiveEffect &effect) const;

    TrailRenderer trailRenderer_{};
    EffectParticleSystem particles_{};
    std::vector<ActiveEffect> activeEffects_{};
    SpriteRenderer *spriteRenderer_ = nullptr;
    BillboardRenderer *billboardRenderer_ = nullptr;
    int width_ = 1;
    int height_ = 1;
    float elapsedTime_ = 0.0f;
};

inline void EffectSystem::BeginFrame(float deltaTime) {
    elapsedTime_ += deltaTime;
    trailRenderer_.BeginFrame(deltaTime);
    particles_.Update(deltaTime);

    for (ActiveEffect &effect : activeEffects_) {
        effect.age += deltaTime;
    }
    activeEffects_.erase(
        std::remove_if(activeEffects_.begin(), activeEffects_.end(),
                       [](const ActiveEffect &effect) {
                           return effect.age >
                                  (std::max)(0.001f, effect.request.duration);
                       }),
        activeEffects_.end());
}

inline void EffectSystem::Play(const EffectRequest &request) {
    ActiveEffect active{};
    active.request = request;
    active.request.duration = (std::max)(0.001f, request.duration);
    activeEffects_.push_back(active);
    constexpr size_t kMaxActiveEffects = 64;
    while (activeEffects_.size() > kMaxActiveEffects) {
        activeEffects_.erase(activeEffects_.begin());
    }
}

inline void EffectSystem::EmitTrail(TrailRenderer::TrailPreset preset,
                                    const DirectX::XMFLOAT3 &base,
                                    const DirectX::XMFLOAT3 &tip, float width) {
    trailRenderer_.EmitTrail(preset, base, tip, width);
}

inline void EffectSystem::EmitParticles(EffectParticlePreset preset,
                                        const DirectX::XMFLOAT3 &position) {
    particles_.Emit(preset, position);
}

inline void EffectSystem::EmitArcSparks(const DirectX::XMFLOAT3 &start,
                                        const DirectX::XMFLOAT3 &end,
                                        uint32_t count, float scale,
                                        bool isSweep) {
    particles_.EmitArcSparks(start, end, count, scale, isSweep);
}

inline void EffectSystem::Resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    trailRenderer_.Resize(width_, height_);
}

inline void EffectSystem::Draw(const Camera &camera) {
    trailRenderer_.Draw(camera);
    if (spriteRenderer_ != nullptr) {
        spriteRenderer_->PreDraw();
    }
    for (const ActiveEffect &effect : activeEffects_) {
        switch (effect.request.preset) {
        case EffectPreset::SlashArc:
            DrawSlashArc(camera, effect);
            break;
        case EffectPreset::MagneticField:
            DrawMagneticField(camera, effect);
            break;
        }
    }
    if (spriteRenderer_ != nullptr) {
        spriteRenderer_->PostDraw();
    }
    particles_.Draw(camera);
}

inline bool ProjectEffectPoint(const Camera &camera,
                               const DirectX::XMFLOAT3 &world,
                               int width, int height,
                               DirectX::XMFLOAT2 &out) {
    using namespace DirectX;
    XMVECTOR pos = XMVectorSet(world.x, world.y, world.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(pos, camera.GetView() * camera.GetProj());
    const float w = XMVectorGetW(clip);
    if (w <= 0.0001f) {
        return false;
    }

    const float ndcX = XMVectorGetX(clip) / w;
    const float ndcY = XMVectorGetY(clip) / w;
    const float ndcZ = XMVectorGetZ(clip) / w;
    if (ndcZ < 0.0f || ndcZ > 1.0f) {
        return false;
    }

    out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
    out.y = (-ndcY * 0.5f + 0.5f) * static_cast<float>(height);
    return true;
}

inline DirectX::XMFLOAT4 EffectColor(float r, float g, float b, float a) {
    return {std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f), std::clamp(a, 0.0f, 1.0f)};
}

inline void EffectSystem::DrawSlashArc(const Camera &camera,
                                       const ActiveEffect &effect) const {
    if (spriteRenderer_ == nullptr) {
        return;
    }

    DirectX::XMFLOAT2 start{};
    DirectX::XMFLOAT2 end{};
    if (!ProjectEffectPoint(camera, effect.request.start, width_, height_,
                            start) ||
        !ProjectEffectPoint(camera, effect.request.end, width_, height_, end)) {
        return;
    }

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrtf(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        return;
    }

    const float invLen = 1.0f / length;
    const float dirX = dx * invLen;
    const float dirY = dy * invLen;
    const float perpX = -dirY;
    const float perpY = dirX;
    const float phaseAlpha = std::clamp(effect.request.alpha, 0.0f, 1.0f);
    const float actionTime = effect.request.time;
    const float sweepScale = effect.request.sweep ? 1.15f : 1.0f;

    const float curveOffset =
        (effect.request.sweep ? 52.0f : 30.0f) *
        (0.84f + 0.16f * std::sinf(actionTime * 36.0f));
    const DirectX::XMFLOAT2 control{
        (start.x + end.x) * 0.5f + perpX * curveOffset,
        (start.y + end.y) * 0.5f + perpY * curveOffset};

    constexpr int kSegments = 18;
    DirectX::XMFLOAT2 path[kSegments + 1];
    const float outerThickness =
        18.0f * sweepScale * (0.86f + phaseAlpha * 0.28f);
    const float coreThickness = 8.0f * (0.82f + phaseAlpha * 0.24f);
    float widths[kSegments + 1];
    for (int i = 0; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const float inv = 1.0f - t;
        path[i].x = inv * inv * start.x + 2.0f * inv * t * control.x +
                    t * t * end.x;
        path[i].y = inv * inv * start.y + 2.0f * inv * t * control.y +
                    t * t * end.y;
        float taper = 1.0f - std::fabs(t - 0.52f) * 1.35f;
        taper = std::clamp(taper, 0.0f, 1.0f);
        widths[i] = outerThickness * (0.25f + 0.75f * taper);
    }

    const DirectX::XMFLOAT4 shadow =
        EffectColor(0.03f, 0.01f, 0.02f, phaseAlpha * 0.96f);
    const DirectX::XMFLOAT4 darkRim =
        EffectColor(0.17f, 0.01f, 0.04f, phaseAlpha * 0.82f);
    const DirectX::XMFLOAT4 hot =
        EffectColor(0.96f, 0.12f, 0.30f, phaseAlpha * 0.68f);
    const DirectX::XMFLOAT4 core =
        EffectColor(1.00f, 0.95f, 0.97f, phaseAlpha * 0.94f);
    const DirectX::XMFLOAT4 bloom =
        EffectColor(0.74f, 0.58f, 1.00f, phaseAlpha * 0.35f);

    DirectX::XMFLOAT2 ribbon[(kSegments + 1) * 2];
    for (int i = 0; i <= kSegments; ++i) {
        const int prevIndex = (i == 0) ? i : i - 1;
        const int nextIndex = (i == kSegments) ? i : i + 1;
        float tangentX = path[nextIndex].x - path[prevIndex].x;
        float tangentY = path[nextIndex].y - path[prevIndex].y;
        float tangentLen = std::sqrtf(tangentX * tangentX + tangentY * tangentY);
        if (tangentLen <= 0.0001f) {
            tangentX = dirX;
            tangentY = dirY;
            tangentLen = 1.0f;
        }
        tangentX /= tangentLen;
        tangentY /= tangentLen;
        const float normalX = -tangentY;
        const float normalY = tangentX;
        const float width = widths[i];
        ribbon[i] = {path[i].x + normalX * width,
                     path[i].y + normalY * width};
        ribbon[(kSegments + 1) * 2 - 1 - i] = {
            path[i].x - normalX * width * 0.65f,
            path[i].y - normalY * width * 0.65f};
    }

    spriteRenderer_->DrawConvexPolygon(ribbon, (kSegments + 1) * 2, shadow);
    spriteRenderer_->DrawPolyline(path, kSegments + 1, bloom, false,
                                  outerThickness * 1.55f);
    spriteRenderer_->DrawPolyline(path, kSegments + 1, shadow, false,
                                  outerThickness);
    spriteRenderer_->DrawPolyline(path, kSegments + 1, darkRim, false,
                                  outerThickness * 0.72f);
    spriteRenderer_->DrawPolyline(path, kSegments + 1, hot, false,
                                  outerThickness * 0.34f);
    spriteRenderer_->DrawPolyline(path, kSegments + 1, core, false,
                                  coreThickness);

    const DirectX::XMFLOAT2 impact = path[kSegments];
    const float branchLen = length * (effect.request.sweep ? 0.82f : 0.68f) * 0.22f;
    const float branchForward = length * 0.08f;
    spriteRenderer_->DrawLine(
        {impact.x - perpX * branchLen, impact.y - perpY * branchLen},
        {impact.x + perpX * branchLen + dirX * branchForward,
         impact.y + perpY * branchLen + dirY * branchForward},
        core, coreThickness * 0.46f);

    constexpr int kSparkCount = 12;
    const float pulse = 0.84f + 0.16f * std::sinf(actionTime * 36.0f);
    for (int i = 0; i < kSparkCount; ++i) {
        const float ratio = static_cast<float>(i) / static_cast<float>(kSparkCount);
        const float angle = actionTime * 16.0f + ratio * DirectX::XM_2PI * 1.17f;
        const float sparkLen = 70.0f * (0.28f + 0.72f * (1.0f - ratio)) *
                               (0.84f + 0.18f * pulse);
        const float offset = 70.0f * 0.20f * ratio;
        DirectX::XMFLOAT2 sparkStart{
            impact.x + dirX * offset + std::cosf(angle) * 8.0f,
            impact.y + dirY * offset + std::sinf(angle * 1.2f) * 8.0f};
        DirectX::XMFLOAT2 sparkEnd{
            sparkStart.x + std::cosf(angle) * sparkLen +
                perpX * sparkLen * 0.12f,
            sparkStart.y + std::sinf(angle) * sparkLen +
                perpY * sparkLen * 0.12f};
        spriteRenderer_->DrawLine(sparkStart, sparkEnd, hot,
                                  1.2f + (1.0f - ratio) * 1.8f);
    }

    spriteRenderer_->DrawFilledCircle(impact, 11.0f + phaseAlpha * 8.0f, core,
                                      32);
    spriteRenderer_->DrawFilledCircle(
        impact, 21.0f + phaseAlpha * 12.0f,
        EffectColor(0.96f, 0.12f, 0.30f, phaseAlpha * 0.18f), 40);
}

inline void EffectSystem::DrawMagneticField(const Camera &camera,
                                            const ActiveEffect &effect) const {
    if (spriteRenderer_ == nullptr) {
        return;
    }

    DirectX::XMFLOAT2 center{};
    if (!ProjectEffectPoint(camera, effect.request.position, width_, height_,
                            center)) {
        return;
    }

    const float time = effect.request.time + elapsedTime_;
    const float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
    const float radius = effect.request.radius * 36.0f * (0.9f + pulse * 0.2f);
    const float alpha =
        std::clamp(effect.request.alpha * effect.request.intensity * 0.55f,
                   0.0f, 1.0f);

    for (int i = 4; i >= 0; --i) {
        const float t = static_cast<float>(i + 1) / 5.0f;
        const float ringAlpha =
            alpha * (38.0f + 24.0f * pulse) * (1.0f - t * 0.08f) / 255.0f;
        spriteRenderer_->DrawCircle(center, radius * t,
                                    EffectColor(6.0f / 255.0f,
                                                92.0f / 255.0f,
                                                192.0f / 255.0f, ringAlpha),
                                    2.0f + i * 0.8f, 72);
    }

    spriteRenderer_->DrawCircle(center, radius * 1.12f,
                                EffectColor(1.0f, 80.0f / 255.0f,
                                            180.0f / 255.0f,
                                            alpha * 52.0f / 255.0f),
                                2.0f, 96);
    spriteRenderer_->DrawFilledCircle(center, radius * 0.18f,
                                      EffectColor(1.0f, 95.0f / 255.0f,
                                                  190.0f / 255.0f,
                                                  alpha * 120.0f / 255.0f),
                                      36);
}
