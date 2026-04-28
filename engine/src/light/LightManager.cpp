#include "LightManager.h"
#include <algorithm>

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
