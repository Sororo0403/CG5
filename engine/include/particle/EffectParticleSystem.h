#pragma once
#include "BillboardRenderer.h"
#include "Camera.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

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
    void SetBillboardRenderer(BillboardRenderer *renderer) {
        billboardRenderer_ = renderer;
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
    BillboardRenderer *billboardRenderer_ = nullptr;
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
    if (billboardRenderer_ == nullptr) {
        return;
    }

    using namespace DirectX;
    XMMATRIX billboard = camera.GetView();
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    billboard = XMMatrixInverse(nullptr, billboard);
    XMFLOAT3 cameraRight{};
    XMFLOAT3 cameraUp{};
    XMFLOAT3 cameraForward{};
    XMStoreFloat3(&cameraRight, billboard.r[0]);
    XMStoreFloat3(&cameraUp, billboard.r[1]);
    XMStoreFloat3(&cameraForward, billboard.r[2]);

    auto offset = [](const XMFLOAT3 &origin, const XMFLOAT3 &axis,
                     float amount) {
        return XMFLOAT3{origin.x + axis.x * amount, origin.y + axis.y * amount,
                        origin.z + axis.z * amount};
    };

    auto add = [](const XMFLOAT3 &a, const XMFLOAT3 &b) {
        return XMFLOAT3{a.x + b.x, a.y + b.y, a.z + b.z};
    };

    billboardRenderer_->PreDraw(camera);
    for (const PointBurst &burst : pointBursts_) {
        const float normalizedAge =
            burst.lifeTime > 0.0001f ? burst.age / burst.lifeTime : 1.0f;
        const float fade = std::clamp(1.0f - normalizedAge, 0.0f, 1.0f);
        const bool isExplosion = burst.type == ParticleType::Explosion;
        const bool isCharge = burst.type == ParticleType::Charge;
        const int sparks = isExplosion ? 18 : (isCharge ? 10 : 12);
        const float radius =
            (isExplosion ? 0.62f : (isCharge ? 0.34f : 0.42f)) *
            (0.35f + normalizedAge);
        const DirectX::XMFLOAT4 coreColor =
            isCharge ? DirectX::XMFLOAT4{120.0f / 255.0f, 190.0f / 255.0f,
                                         1.0f, fade * 190.0f / 255.0f}
                     : DirectX::XMFLOAT4{1.0f, 92.0f / 255.0f,
                                         72.0f / 255.0f,
                                         fade * 220.0f / 255.0f};
        const DirectX::XMFLOAT4 glowColor =
            isCharge ? DirectX::XMFLOAT4{80.0f / 255.0f, 160.0f / 255.0f,
                                         1.0f, fade * 70.0f / 255.0f}
                     : DirectX::XMFLOAT4{1.0f, 190.0f / 255.0f,
                                         80.0f / 255.0f,
                                         fade * 90.0f / 255.0f};

        billboardRenderer_->DrawDisc(burst.position, 0.10f + radius * 0.12f,
                                     glowColor, burst.age * 2.2f);
        billboardRenderer_->DrawDisc(burst.position, 0.04f + fade * 0.05f,
                                     coreColor, -burst.age * 3.1f);

        for (int i = 0; i < sparks; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(sparks);
            const float angle = ratio * DirectX::XM_2PI + burst.age * 13.0f;
            const float inner = radius * 0.22f;
            const float outer =
                radius * (0.68f + 0.18f * std::sinf(angle * 1.7f));
            XMFLOAT3 a = add(offset(burst.position, cameraRight,
                                    std::cosf(angle) * inner),
                             offset({0.0f, 0.0f, 0.0f}, cameraUp,
                                    std::sinf(angle) * inner));
            XMFLOAT3 b = add(offset(burst.position, cameraRight,
                                    std::cosf(angle) * outer),
                             offset({0.0f, 0.0f, 0.0f}, cameraUp,
                                    std::sinf(angle) * outer));
            billboardRenderer_->DrawBeam(a, b, 0.018f + fade * 0.032f,
                                         coreColor);
        }
    }

    for (const Burst &burst : bursts_) {
        const float fade = std::clamp(1.0f - burst.age / 0.34f, 0.0f, 1.0f);
        XMVECTOR startV = XMLoadFloat3(&burst.start);
        XMVECTOR endV = XMLoadFloat3(&burst.end);
        XMVECTOR segment = endV - startV;
        const float len = XMVectorGetX(XMVector3Length(segment));
        if (len <= 0.001f) {
            continue;
        }
        XMVECTOR dirV = XMVector3Normalize(segment);
        XMVECTOR sideV = XMVector3Cross(XMLoadFloat3(&cameraForward), dirV);
        if (XMVectorGetX(XMVector3LengthSq(sideV)) <= 0.000001f) {
            sideV = XMLoadFloat3(&cameraRight);
        } else {
            sideV = XMVector3Normalize(sideV);
        }
        XMFLOAT3 dir{};
        XMFLOAT3 sideAxis{};
        XMStoreFloat3(&dir, dirV);
        XMStoreFloat3(&sideAxis, sideV);
        const int sparks = static_cast<int>(std::min<uint32_t>(burst.count, 32));
        for (int i = 0; i < sparks; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sparks);
            const float phase = burst.age * 18.0f + t * 23.0f;
            const float along = len * (0.15f + 0.75f * t);
            const float sideAmount = std::sin(phase) *
                                     (burst.isSweep ? 0.52f : 0.32f) *
                                     burst.scale * fade;
            XMFLOAT3 a{burst.start.x + dir.x * along + sideAxis.x * sideAmount,
                       burst.start.y + dir.y * along + sideAxis.y * sideAmount,
                       burst.start.z + dir.z * along + sideAxis.z * sideAmount};
            XMFLOAT3 b{
                a.x + dir.x * (0.18f + 0.18f * fade) +
                    sideAxis.x * std::cos(phase) * 0.12f,
                a.y + dir.y * (0.18f + 0.18f * fade) +
                    sideAxis.y * std::cos(phase) * 0.12f,
                a.z + dir.z * (0.18f + 0.18f * fade) +
                    sideAxis.z * std::cos(phase) * 0.12f};
            billboardRenderer_->DrawBeam(
                a, b,
                0.012f + fade * 0.024f,
                {1.0f, 80.0f / 255.0f, 120.0f / 255.0f,
                 fade * 210.0f / 255.0f});
        }
    }
    billboardRenderer_->PostDraw();
}
