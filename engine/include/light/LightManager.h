#pragma once
#include <DirectXMath.h>
#include <array>
#include <cstdint>

inline constexpr uint32_t kMaxForwardPointLights = 8;

/// <summary>
/// 点光源1灯分のパラメータ
/// </summary>
struct PointLight {
    DirectX::XMFLOAT4 positionRange = {0.0f, 2.0f, -1.0f, 8.0f};
    DirectX::XMFLOAT4 colorIntensity = {1.0f, 0.55f, 0.35f, 1.1f};
};

/// <summary>
/// シーン全体で共有するライティング定数
/// </summary>
struct SceneLighting {
    DirectX::XMFLOAT3 keyLightDirection = {-0.35f, -1.0f, 0.25f};
    float padding0 = 0.0f;
    DirectX::XMFLOAT4 keyLightColor = {1.20f, 1.08f, 0.96f, 1.0f};
    DirectX::XMFLOAT3 fillLightDirection = {0.55f, -0.35f, -0.75f};
    float padding1 = 0.0f;
    DirectX::XMFLOAT4 fillLightColor = {0.22f, 0.32f, 0.48f, 0.38f};
    DirectX::XMFLOAT4 ambientColor = {0.28f, 0.30f, 0.34f, 1.0f};
    std::array<PointLight, kMaxForwardPointLights> pointLights = {{
        {{0.0f, 2.0f, -1.0f, 8.0f}, {1.0f, 0.55f, 0.35f, 1.1f}},
        {{0.0f, 1.5f, 2.5f, 7.0f}, {0.25f, 0.45f, 1.0f, 0.75f}},
    }};
    uint32_t pointLightCount = 2;
    DirectX::XMFLOAT4 lightingParams = {48.0f, 0.30f, 2.8f, 0.22f};
};

class LightManager {
  public:
    const SceneLighting &GetSceneLighting() const { return lighting_; }
    SceneLighting &GetMutableSceneLighting() { return lighting_; }

    void SetSceneLighting(const SceneLighting &lighting);
    void SetPointLightCount(uint32_t count);
    void SetPointLight(uint32_t index, const PointLight &light);

    PointLight *GetMutablePointLight(uint32_t index);
    const PointLight *GetPointLight(uint32_t index) const;

  private:
    SceneLighting lighting_{};
};
