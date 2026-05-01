#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

#ifndef IMGUI_DISABLED
#include "Camera.h"
#include "imgui.h"
#include <cmath>
#endif

enum class EffectParticlePreset {
    HitSpark,
    Explosion,
    ChargeSpark,
};

class EffectParticleSystem {
  public:
    enum class ParticleType {
        Hit,
        Explosion,
        Charge,
    };

    void Initialize(DirectXCommon *, SrvManager *, TextureManager *,
                    uint32_t maxBursts) {
        maxBursts_ = (std::max)(maxBursts, 1u);
        bursts_.clear();
        pointBursts_.clear();
    }

    void Emit(EffectParticlePreset preset, const DirectX::XMFLOAT3 &position);
    void Emit(ParticleType type, const DirectX::XMFLOAT3 &position);
    void EmitArcSparks(const DirectX::XMFLOAT3 &start,
                       const DirectX::XMFLOAT3 &end, uint32_t count,
                       float scale, bool isSweep);
    void Update(float deltaTime);
    void Draw(const Camera &camera) const;

  private:
    struct Burst {
        DirectX::XMFLOAT3 start;
        DirectX::XMFLOAT3 end;
        uint32_t count = 0;
        float scale = 1.0f;
        bool isSweep = false;
        float age = 0.0f;
    };

    struct PointBurst {
        DirectX::XMFLOAT3 position;
        ParticleType type = ParticleType::Hit;
        float age = 0.0f;
        float lifeTime = 0.26f;
    };

    std::vector<Burst> bursts_{};
    std::vector<PointBurst> pointBursts_{};
    uint32_t maxBursts_ = 8;
};

inline void EffectParticleSystem::Emit(ParticleType type,
                                       const DirectX::XMFLOAT3 &position) {
    PointBurst burst{};
    burst.position = position;
    burst.type = type;

    switch (type) {
    case ParticleType::Hit:
        burst.lifeTime = 0.26f;
        break;
    case ParticleType::Explosion:
        burst.lifeTime = 0.42f;
        break;
    case ParticleType::Charge:
        burst.lifeTime = 0.34f;
        break;
    }

    pointBursts_.push_back(burst);
    while (pointBursts_.size() > maxBursts_ * 4u) {
        pointBursts_.erase(pointBursts_.begin());
    }
}

inline void EffectParticleSystem::Emit(EffectParticlePreset preset,
                                       const DirectX::XMFLOAT3 &position) {
    switch (preset) {
    case EffectParticlePreset::HitSpark:
        Emit(ParticleType::Hit, position);
        break;
    case EffectParticlePreset::Explosion:
        Emit(ParticleType::Explosion, position);
        break;
    case EffectParticlePreset::ChargeSpark:
        Emit(ParticleType::Charge, position);
        break;
    }
}

inline void EffectParticleSystem::EmitArcSparks(
    const DirectX::XMFLOAT3 &start, const DirectX::XMFLOAT3 &end,
    uint32_t count, float scale, bool isSweep) {
    bursts_.push_back({start, end, count, scale, isSweep, 0.0f});
    while (bursts_.size() > maxBursts_) {
        bursts_.erase(bursts_.begin());
    }
}

inline void EffectParticleSystem::Update(float deltaTime) {
    for (Burst &burst : bursts_) {
        burst.age += deltaTime;
    }
    bursts_.erase(std::remove_if(bursts_.begin(), bursts_.end(),
                                 [](const Burst &burst) {
                                     return burst.age > 0.34f;
                                 }),
                  bursts_.end());

    for (PointBurst &burst : pointBursts_) {
        burst.age += deltaTime;
    }
    pointBursts_.erase(std::remove_if(pointBursts_.begin(), pointBursts_.end(),
                                      [](const PointBurst &burst) {
                                          return burst.age > burst.lifeTime;
                                      }),
                       pointBursts_.end());
}

inline void EffectParticleSystem::Draw(const Camera &camera) const {
#ifndef IMGUI_DISABLED
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    if (viewport == nullptr || drawList == nullptr) {
        return;
    }

    auto project = [&](const DirectX::XMFLOAT3 &world, ImVec2 &out) {
        using namespace DirectX;
        XMVECTOR pos = XMVectorSet(world.x, world.y, world.z, 1.0f);
        XMVECTOR clip =
            XMVector4Transform(pos, camera.GetView() * camera.GetProj());
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
    };

    for (const PointBurst &burst : pointBursts_) {
        ImVec2 center{};
        if (!project(burst.position, center)) {
            continue;
        }

        const float normalizedAge =
            burst.lifeTime > 0.0001f ? burst.age / burst.lifeTime : 1.0f;
        const float fade = std::clamp(1.0f - normalizedAge, 0.0f, 1.0f);
        const bool isExplosion = burst.type == ParticleType::Explosion;
        const bool isCharge = burst.type == ParticleType::Charge;
        const int sparks = isExplosion ? 18 : (isCharge ? 10 : 12);
        const float radius =
            (isExplosion ? 62.0f : (isCharge ? 34.0f : 42.0f)) *
            (0.35f + normalizedAge);
        const ImU32 coreColor =
            isCharge ? IM_COL32(120, 190, 255, static_cast<int>(fade * 190.0f))
                     : IM_COL32(255, 92, 72, static_cast<int>(fade * 220.0f));
        const ImU32 glowColor =
            isCharge ? IM_COL32(80, 160, 255, static_cast<int>(fade * 70.0f))
                     : IM_COL32(255, 190, 80, static_cast<int>(fade * 90.0f));

        drawList->AddCircleFilled(center, 10.0f + radius * 0.12f, glowColor);
        drawList->AddCircleFilled(center, 4.0f + fade * 5.0f, coreColor);

        for (int i = 0; i < sparks; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(sparks);
            const float angle = ratio * DirectX::XM_2PI + burst.age * 13.0f;
            const float inner = radius * 0.22f;
            const float outer =
                radius * (0.68f + 0.18f * std::sinf(angle * 1.7f));
            ImVec2 a(center.x + std::cosf(angle) * inner,
                     center.y + std::sinf(angle) * inner);
            ImVec2 b(center.x + std::cosf(angle) * outer,
                     center.y + std::sinf(angle) * outer);
            drawList->AddLine(a, b, coreColor, 1.0f + fade * 2.2f);
        }
    }

    for (const Burst &burst : bursts_) {
        ImVec2 start{}, end{};
        if (!project(burst.start, start) || !project(burst.end, end)) {
            continue;
        }
        const float fade = std::clamp(1.0f - burst.age / 0.34f, 0.0f, 1.0f);
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.001f) {
            continue;
        }
        const float invLen = 1.0f / len;
        const float dirX = dx * invLen;
        const float dirY = dy * invLen;
        const float perpX = -dirY;
        const float perpY = dirX;
        const int sparks = static_cast<int>(std::min<uint32_t>(burst.count, 32));
        for (int i = 0; i < sparks; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sparks);
            const float phase = burst.age * 18.0f + t * 23.0f;
            const float along = len * (0.15f + 0.75f * t);
            const float side = std::sin(phase) *
                               (burst.isSweep ? 52.0f : 32.0f) * burst.scale *
                               fade;
            ImVec2 a(start.x + dirX * along + perpX * side,
                     start.y + dirY * along + perpY * side);
            ImVec2 b(a.x + dirX * (18.0f + 18.0f * fade) +
                         perpX * std::cos(phase) * 12.0f,
                     a.y + dirY * (18.0f + 18.0f * fade) +
                         perpY * std::cos(phase) * 12.0f);
            drawList->AddLine(
                a, b, IM_COL32(255, 80, 120, static_cast<int>(fade * 210.0f)),
                1.0f + fade * 2.0f);
        }
    }
#else
    (void)camera;
#endif
}
