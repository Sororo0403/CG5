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
#include <algorithm>
#include <cmath>
#endif

class SwordTrailRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *,
                    uint32_t maxPoints) {
        maxPoints_ = (std::max)(maxPoints, 2u);
        points_.clear();
    }
    void SetLifeTime(float lifeTime) { lifeTime_ = lifeTime; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    void BeginFrame(float deltaTime) {
        for (TrailPoint &point : points_) {
            point.age += deltaTime;
        }
        points_.erase(std::remove_if(points_.begin(), points_.end(),
                                     [this](const TrailPoint &point) {
                                         return point.age > lifeTime_;
                                     }),
                      points_.end());
    }
    void AddPoint(const DirectX::XMFLOAT3 &base, const DirectX::XMFLOAT3 &tip,
                  float width) {
        if (!enabled_) {
            return;
        }
        points_.push_back({base, tip, width, 0.0f});
        while (points_.size() > maxPoints_) {
            points_.erase(points_.begin());
        }
    }
    void EndFrame() {
        while (points_.size() > maxPoints_) {
            points_.erase(points_.begin());
        }
    }
    void Draw(const Camera &camera);

  private:
    struct TrailPoint {
        DirectX::XMFLOAT3 base;
        DirectX::XMFLOAT3 tip;
        float width = 1.0f;
        float age = 0.0f;
    };

    bool enabled_ = true;
    float lifeTime_ = 0.24f;
    std::vector<TrailPoint> points_{};
    uint32_t maxPoints_ = 64;
};

inline void SwordTrailRenderer::Draw(const Camera &camera) {
#ifndef IMGUI_DISABLED
    if (!enabled_ || points_.size() < 2) {
        return;
    }
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

    for (size_t i = 1; i < points_.size(); ++i) {
        const TrailPoint &prev = points_[i - 1];
        const TrailPoint &cur = points_[i];
        ImVec2 p0{}, p1{}, p2{}, p3{};
        if (!project(prev.base, p0) || !project(prev.tip, p1) ||
            !project(cur.tip, p2) || !project(cur.base, p3)) {
            continue;
        }
        const float age = (std::min)(prev.age, cur.age);
        const float fade = std::clamp(1.0f - age / (std::max)(0.001f, lifeTime_),
                                      0.0f, 1.0f);
        const ImU32 fill =
            IM_COL32(220, 245, 255, static_cast<int>(fade * 72.0f));
        const ImU32 edge =
            IM_COL32(85, 180, 255, static_cast<int>(fade * 156.0f));
        ImVec2 quad[4] = {p0, p1, p2, p3};
        drawList->AddConvexPolyFilled(quad, 4, fill);
        drawList->AddLine(p1, p2, edge, 2.0f + cur.width * 0.8f);
    }
#else
    (void)camera;
#endif
}

