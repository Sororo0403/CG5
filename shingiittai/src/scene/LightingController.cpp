#include "LightingController.h"
#include "Enemy.h"
#include "LightManager.h"
#include "ModelManager.h"
#include "Player.h"
#include "SceneContext.h"
#include <cmath>

using namespace DirectX;

void LightingController::Reset() { sceneLightTime_ = 0.0f; }

void LightingController::Update(const SceneContext &ctx, const Player &player,
                                const Enemy &enemy, float deltaTime) {
    sceneLightTime_ += deltaTime;

    const XMFLOAT3 &playerPos = player.GetTransform().position;
    const XMFLOAT3 &enemyPos = enemy.GetTransform().position;
    XMFLOAT3 accentAnchor = enemy.GetTransform().position;
    if (enemy.GetActionKind() == ActionKind::Warp) {
        accentAnchor = enemy.GetWarpTargetPos();
    }

    const float pulse = 0.82f + 0.18f * std::sinf(sceneLightTime_ * 2.4f);
    const float actionBoost =
        enemy.GetActionKind() == ActionKind::Warp   ? 1.35f
        : enemy.GetActionKind() == ActionKind::Wave ? 1.20f
        : enemy.GetActionKind() == ActionKind::Shot ? 1.10f
                                                    : 1.0f;

    XMFLOAT3 duelCenter = {
        (playerPos.x + enemyPos.x) * 0.5f,
        (playerPos.y + enemyPos.y) * 0.5f,
        (playerPos.z + enemyPos.z) * 0.5f,
    };

    SceneLighting lighting{};
    lighting.keyLightDirection = {-0.45f, -1.0f, 0.30f};
    lighting.keyLightColor = {1.25f, 1.14f, 1.02f, 1.0f};
    lighting.fillLightDirection = {0.70f, -0.28f, -0.65f};
    lighting.fillLightColor = {0.20f, 0.34f, 0.58f, 0.42f};
    lighting.ambientColor = {0.22f, 0.24f, 0.28f, 1.0f};
    lighting.lightingParams = {56.0f, 0.40f, 3.2f, 0.18f};

    lighting.pointLights[0].positionRange = {
        duelCenter.x,
        duelCenter.y + 2.4f,
        duelCenter.z - 0.7f,
        8.5f,
    };
    lighting.pointLights[0].colorIntensity = {
        1.00f,
        0.48f,
        0.26f,
        1.15f * pulse,
    };

    lighting.pointLights[1].positionRange = {
        accentAnchor.x,
        enemyPos.y + 1.6f,
        accentAnchor.z + 0.35f,
        6.8f,
    };
    lighting.pointLights[1].colorIntensity = {
        0.24f,
        0.52f,
        1.00f,
        0.70f * actionBoost,
    };

    lighting.pointLightCount = 2;

    if (ctx.renderer.light != nullptr) {
        ctx.renderer.light->SetSceneLighting(lighting);
    }
    if (ctx.model != nullptr) {
        ctx.model->SetSceneLighting(lighting);
    }
}
