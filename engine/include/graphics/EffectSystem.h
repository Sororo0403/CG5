#pragma once
#include "EffectParticleSystem.h"
#include "TrailRenderer.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

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
                    TextureManager *textureManager, uint32_t maxTrailPoints,
                    uint32_t maxParticleBursts) {
        trailRenderer_.Initialize(dxCommon, srvManager, textureManager,
                                  maxTrailPoints);
        particles_.Initialize(dxCommon, srvManager, textureManager,
                              maxParticleBursts);
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

inline void EffectSystem::Draw(const Camera &camera) {
    trailRenderer_.Draw(camera);
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
    particles_.Draw(camera);
}

#ifndef IMGUI_DISABLED
inline bool ProjectEffectPoint(const Camera &camera,
                               const DirectX::XMFLOAT3 &world, ImVec2 &out) {
    using namespace DirectX;
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return false;
    }

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

    out.x = viewport->Pos.x + (ndcX * 0.5f + 0.5f) * viewport->Size.x;
    out.y = viewport->Pos.y + (-ndcY * 0.5f + 0.5f) * viewport->Size.y;
    return true;
}

inline ImU32 EffectColor(float r, float g, float b, float a) {
    return IM_COL32(static_cast<int>(std::clamp(r, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(g, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(b, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255.0f));
}
#endif

inline void EffectSystem::DrawSlashArc(const Camera &camera,
                                       const ActiveEffect &effect) const {
#ifndef IMGUI_DISABLED
    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    ImVec2 start{};
    ImVec2 end{};
    if (!ProjectEffectPoint(camera, effect.request.start, start) ||
        !ProjectEffectPoint(camera, effect.request.end, end)) {
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
    const ImVec2 control((start.x + end.x) * 0.5f + perpX * curveOffset,
                         (start.y + end.y) * 0.5f + perpY * curveOffset);

    constexpr int kSegments = 18;
    ImVec2 path[kSegments + 1];
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

    const ImU32 shadow = EffectColor(0.03f, 0.01f, 0.02f, phaseAlpha * 0.96f);
    const ImU32 darkRim = EffectColor(0.17f, 0.01f, 0.04f, phaseAlpha * 0.82f);
    const ImU32 hot = EffectColor(0.96f, 0.12f, 0.30f, phaseAlpha * 0.68f);
    const ImU32 core = EffectColor(1.00f, 0.95f, 0.97f, phaseAlpha * 0.94f);
    const ImU32 bloom = EffectColor(0.74f, 0.58f, 1.00f, phaseAlpha * 0.35f);

    ImVec2 ribbon[(kSegments + 1) * 2];
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
        ribbon[i] =
            ImVec2(path[i].x + normalX * width, path[i].y + normalY * width);
        ribbon[(kSegments + 1) * 2 - 1 - i] =
            ImVec2(path[i].x - normalX * width * 0.65f,
                   path[i].y - normalY * width * 0.65f);
    }

    drawList->AddConvexPolyFilled(ribbon, (kSegments + 1) * 2, shadow);
    drawList->AddPolyline(path, kSegments + 1, bloom, false,
                          outerThickness * 1.55f);
    drawList->AddPolyline(path, kSegments + 1, shadow, false, outerThickness);
    drawList->AddPolyline(path, kSegments + 1, darkRim, false,
                          outerThickness * 0.72f);
    drawList->AddPolyline(path, kSegments + 1, hot, false,
                          outerThickness * 0.34f);
    drawList->AddPolyline(path, kSegments + 1, core, false, coreThickness);

    const ImVec2 impact = path[kSegments];
    const float branchLen = length * (effect.request.sweep ? 0.82f : 0.68f) * 0.22f;
    const float branchForward = length * 0.08f;
    drawList->AddLine(
        ImVec2(impact.x - perpX * branchLen, impact.y - perpY * branchLen),
        ImVec2(impact.x + perpX * branchLen + dirX * branchForward,
               impact.y + perpY * branchLen + dirY * branchForward),
        core, coreThickness * 0.46f);

    constexpr int kSparkCount = 12;
    const float pulse = 0.84f + 0.16f * std::sinf(actionTime * 36.0f);
    for (int i = 0; i < kSparkCount; ++i) {
        const float ratio = static_cast<float>(i) / static_cast<float>(kSparkCount);
        const float angle = actionTime * 16.0f + ratio * DirectX::XM_2PI * 1.17f;
        const float sparkLen = 70.0f * (0.28f + 0.72f * (1.0f - ratio)) *
                               (0.84f + 0.18f * pulse);
        const float offset = 70.0f * 0.20f * ratio;
        ImVec2 sparkStart(impact.x + dirX * offset + std::cosf(angle) * 8.0f,
                          impact.y + dirY * offset +
                              std::sinf(angle * 1.2f) * 8.0f);
        ImVec2 sparkEnd(sparkStart.x + std::cosf(angle) * sparkLen +
                            perpX * sparkLen * 0.12f,
                        sparkStart.y + std::sinf(angle) * sparkLen +
                            perpY * sparkLen * 0.12f);
        drawList->AddLine(sparkStart, sparkEnd, hot,
                          1.2f + (1.0f - ratio) * 1.8f);
    }

    drawList->AddCircleFilled(impact, 11.0f + phaseAlpha * 8.0f, core);
    drawList->AddCircleFilled(impact, 21.0f + phaseAlpha * 12.0f,
                              EffectColor(0.96f, 0.12f, 0.30f,
                                          phaseAlpha * 0.18f));
#else
    (void)camera;
    (void)effect;
#endif
}

inline void EffectSystem::DrawMagneticField(const Camera &camera,
                                            const ActiveEffect &effect) const {
#ifndef IMGUI_DISABLED
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    if (viewport == nullptr || drawList == nullptr) {
        return;
    }

    ImVec2 center{};
    if (!ProjectEffectPoint(camera, effect.request.position, center)) {
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
        const ImU32 color = IM_COL32(6, 92, 192,
                                     static_cast<int>(alpha *
                                                      (38.0f + 24.0f * pulse) *
                                                      (1.0f - t * 0.08f)));
        drawList->AddCircle(center, radius * t, color, 72, 2.0f + i * 0.8f);
    }

    drawList->AddCircle(center, radius * 1.12f,
                        IM_COL32(255, 80, 180,
                                 static_cast<int>(alpha * 52.0f)),
                        96, 2.0f);
    drawList->AddCircleFilled(
        center, radius * 0.18f,
        IM_COL32(255, 95, 190, static_cast<int>(alpha * 120.0f)), 36);
#else
    (void)camera;
    (void)effect;
#endif
}
