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

class GpuSlashParticleSystem {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *,
                    uint32_t maxBursts) {
        maxBursts_ = (std::max)(maxBursts, 1u);
        bursts_.clear();
    }
    void EmitSlashBurst(const DirectX::XMFLOAT3 &start,
                        const DirectX::XMFLOAT3 &end, uint32_t count,
        float scale, bool isSweep) {
        bursts_.push_back({start, end, count, scale, isSweep, 0.0f});
        while (bursts_.size() > maxBursts_) {
            bursts_.erase(bursts_.begin());
        }
    }
    void Render(const Camera &camera, float deltaTime);

  private:
    struct Burst {
        DirectX::XMFLOAT3 start;
        DirectX::XMFLOAT3 end;
        uint32_t count = 0;
        float scale = 1.0f;
        bool isSweep = false;
        float age = 0.0f;
    };

    std::vector<Burst> bursts_{};
    uint32_t maxBursts_ = 8;
};

inline void GpuSlashParticleSystem::Render(const Camera &camera,
                                           float deltaTime) {
#ifndef IMGUI_DISABLED
    for (Burst &burst : bursts_) {
        burst.age += deltaTime;
    }
    bursts_.erase(std::remove_if(bursts_.begin(), bursts_.end(),
                                 [](const Burst &burst) {
                                     return burst.age > 0.34f;
                                 }),
                  bursts_.end());

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
            const float side = std::sin(phase) * (burst.isSweep ? 52.0f : 32.0f) *
                               burst.scale * fade;
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
    (void)deltaTime;
#endif
}

