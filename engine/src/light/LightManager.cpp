#include "LightManager.h"
#include <algorithm>

ForwardLightingData LightManager::CreateForwardLightingData() const {
    return CreateForwardLightingData(lighting_);
}

ForwardLightingData LightManager::CreateForwardLightingData(
    const SceneLighting &lighting) {
    ForwardLightingData data{};
    data.keyLightDirection = {
        lighting.keyLightDirection.x,
        lighting.keyLightDirection.y,
        lighting.keyLightDirection.z,
        0.0f,
    };
    data.keyLightColor = lighting.keyLightColor;
    data.fillLightDirection = {
        lighting.fillLightDirection.x,
        lighting.fillLightDirection.y,
        lighting.fillLightDirection.z,
        0.0f,
    };
    data.fillLightColor = lighting.fillLightColor;
    data.ambientColor = lighting.ambientColor;

    const uint32_t pointLightCount = std::min<uint32_t>(
        lighting.pointLightCount,
        static_cast<uint32_t>(lighting.pointLights.size()));
    for (uint32_t lightIndex = 0; lightIndex < lighting.pointLights.size();
         ++lightIndex) {
        data.pointLights[lightIndex].positionRange =
            lighting.pointLights[lightIndex].positionRange;
        data.pointLights[lightIndex].colorIntensity =
            lighting.pointLights[lightIndex].colorIntensity;
    }
    data.pointLightParams = {
        static_cast<float>(pointLightCount), 0.0f, 0.0f, 0.0f};
    data.lightingParams = lighting.lightingParams;
    return data;
}

void LightManager::SetSceneLighting(const SceneLighting &lighting) {
    lighting_ = lighting;
    SetPointLightCount(lighting_.pointLightCount);
}

void LightManager::SetPointLightCount(uint32_t count) {
    lighting_.pointLightCount = std::min<uint32_t>(count, kMaxForwardPointLights);
}

void LightManager::SetPointLight(uint32_t index, const PointLight &light) {
    if (index >= lighting_.pointLights.size()) {
        return;
    }

    lighting_.pointLights[index] = light;
    if (index >= lighting_.pointLightCount) {
        SetPointLightCount(index + 1);
    }
}

PointLight *LightManager::GetMutablePointLight(uint32_t index) {
    if (index >= lighting_.pointLights.size()) {
        return nullptr;
    }

    return &lighting_.pointLights[index];
}

const PointLight *LightManager::GetPointLight(uint32_t index) const {
    if (index >= lighting_.pointLights.size()) {
        return nullptr;
    }

    return &lighting_.pointLights[index];
}
