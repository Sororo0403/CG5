#pragma once
#include "Camera.h"
#include "SpriteRenderer.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class TrailRenderer {
  public:
    enum class TrailPreset {
        SwordSlash,
    };

    void Initialize(DirectXCommon *, SrvManager *, TextureManager *,
                    uint32_t maxPoints) {
        maxPoints_ = (std::max)(maxPoints, 2u);
        points_.clear();
    }
    void SetLifeTime(float lifeTime) { lifeTime_ = lifeTime; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    void SetScreenRenderer(SpriteRenderer *renderer, int width, int height) {
        spriteRenderer_ = renderer;
        Resize(width, height);
    }
    void Resize(int width, int height) {
        width_ = width > 0 ? width : 1;
        height_ = height > 0 ? height : 1;
    }
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
    void EmitTrail(TrailPreset preset, const DirectX::XMFLOAT3 &base,
                   const DirectX::XMFLOAT3 &tip, float width) {
        if (!enabled_) {
            return;
        }
        points_.push_back({preset, base, tip, width, 0.0f});
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
        TrailPreset preset = TrailPreset::SwordSlash;
        DirectX::XMFLOAT3 base;
        DirectX::XMFLOAT3 tip;
        float width = 1.0f;
        float age = 0.0f;
    };

    bool enabled_ = true;
    float lifeTime_ = 0.24f;
    std::vector<TrailPoint> points_{};
    uint32_t maxPoints_ = 64;
    SpriteRenderer *spriteRenderer_ = nullptr;
    int width_ = 1;
    int height_ = 1;
};

inline void TrailRenderer::Draw(const Camera &camera) {
    if (!enabled_ || points_.size() < 2 || spriteRenderer_ == nullptr) {
        return;
    }

    auto project = [&](const DirectX::XMFLOAT3 &world,
                       DirectX::XMFLOAT2 &out) {
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
        out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width_);
        out.y = (-ndcY * 0.5f + 0.5f) * static_cast<float>(height_);
        return true;
    };

    spriteRenderer_->PreDraw();
    for (size_t i = 1; i < points_.size(); ++i) {
        const TrailPoint &prev = points_[i - 1];
        const TrailPoint &cur = points_[i];
        DirectX::XMFLOAT2 p0{}, p1{}, p2{}, p3{};
        if (!project(prev.base, p0) || !project(prev.tip, p1) ||
            !project(cur.tip, p2) || !project(cur.base, p3)) {
            continue;
        }
        const float age = (std::min)(prev.age, cur.age);
        const float fade = std::clamp(
            1.0f - age / (std::max)(0.001f, lifeTime_), 0.0f, 1.0f);
        DirectX::XMFLOAT4 fill{220.0f / 255.0f, 245.0f / 255.0f, 1.0f,
                               fade * 72.0f / 255.0f};
        DirectX::XMFLOAT4 edge{85.0f / 255.0f, 180.0f / 255.0f, 1.0f,
                               fade * 156.0f / 255.0f};
        switch (cur.preset) {
        case TrailPreset::SwordSlash:
            break;
        }
        DirectX::XMFLOAT2 quad[4] = {p0, p1, p2, p3};
        spriteRenderer_->DrawConvexPolygon(quad, 4, fill);
        spriteRenderer_->DrawLine(p1, p2, edge, 2.0f + cur.width * 0.8f);
    }
    spriteRenderer_->PostDraw();
}

