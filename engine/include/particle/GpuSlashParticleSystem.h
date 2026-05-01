#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class [[deprecated("Use EffectParticleSystem/EffectSystem particle presets instead.")]]
GpuSlashParticleSystem {
  public:
    enum class EffectType {
        Hit,
        Explosion,
        Charge,
    };

    enum class EffectPreset {
        HitSpark,
        Explosion,
        ChargeSpark,
    };

    void Initialize(DirectXCommon *, SrvManager *, TextureManager *,
                    uint32_t maxBursts) {
        maxBursts_ = (std::max)(maxBursts, 1u);
        bursts_.clear();
        pointBursts_.clear();
    }
    void PlayParticle(EffectPreset preset, const DirectX::XMFLOAT3 &position);
    void RequestEffect(EffectType type, const DirectX::XMFLOAT3 &position);
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
    struct PointBurst {
        DirectX::XMFLOAT3 position;
        EffectType type = EffectType::Hit;
        float age = 0.0f;
        float lifeTime = 0.26f;
    };

    std::vector<Burst> bursts_{};
    std::vector<PointBurst> pointBursts_{};
    uint32_t maxBursts_ = 8;
};

inline void GpuSlashParticleSystem::RequestEffect(
    EffectType type, const DirectX::XMFLOAT3 &position) {
    PointBurst burst{};
    burst.position = position;
    burst.type = type;

    switch (type) {
    case EffectType::Hit:
        burst.lifeTime = 0.26f;
        break;
    case EffectType::Explosion:
        burst.lifeTime = 0.42f;
        break;
    case EffectType::Charge:
        burst.lifeTime = 0.34f;
        break;
    }

    pointBursts_.push_back(burst);
    while (pointBursts_.size() > maxBursts_ * 4u) {
        pointBursts_.erase(pointBursts_.begin());
    }
}

inline void GpuSlashParticleSystem::PlayParticle(
    EffectPreset preset, const DirectX::XMFLOAT3 &position) {
    switch (preset) {
    case EffectPreset::HitSpark:
        RequestEffect(EffectType::Hit, position);
        break;
    case EffectPreset::Explosion:
        RequestEffect(EffectType::Explosion, position);
        break;
    case EffectPreset::ChargeSpark:
        RequestEffect(EffectType::Charge, position);
        break;
    }
}

inline void GpuSlashParticleSystem::Render(const Camera &camera,
                                           float deltaTime) {
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
    (void)camera;
}

