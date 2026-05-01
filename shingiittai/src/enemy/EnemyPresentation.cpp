#include "Enemy.h"
#include "ModelManager.h"

#include <algorithm>
#include <cmath>

void Enemy::UpdateParts() {
    const float usedYaw = facingYaw_;

    const float rightX = std::cos(usedYaw);
    const float rightZ = -std::sin(usedYaw);

    bodyTf_ = tf_;
    bodyTf_.position = tf_.position;
    bodyTf_.scale = {1.2f, 1.4f, 0.8f};

    visualTf_ = tf_;
    visualTf_.position = tf_.position;
    visualTf_.scale = {1.0f, 1.0f, 1.0f};

    leftHandTf_ = tf_;
    leftHandTf_.position = tf_.position;
    leftHandTf_.position.x += (-rightX) * 1.2f;
    leftHandTf_.position.y += 0.9f;
    leftHandTf_.position.z += (-rightZ) * 1.2f;
    leftHandTf_.scale = {0.6f, 0.6f, 0.6f};

    rightHandTf_ = tf_;
    rightHandTf_.position = tf_.position;
    rightHandTf_.position.x += rightX * 1.2f;
    rightHandTf_.position.y += 0.9f;
    rightHandTf_.position.z += rightZ * 1.2f;
    rightHandTf_.scale = {0.6f, 0.6f, 0.6f};

    DirectX::XMVECTOR hitboxRot =
        DirectX::XMQuaternionRotationRollPitchYaw(0.0f, usedYaw, 0.0f);
    DirectX::XMStoreFloat4(&bodyTf_.rotation, hitboxRot);
    DirectX::XMStoreFloat4(&leftHandTf_.rotation, hitboxRot);
    DirectX::XMStoreFloat4(&rightHandTf_.rotation, hitboxRot);
}

void Enemy::Draw(ModelManager *modelManager, const Camera &camera) {
    if (runtime_.deathFinished) {
        return;
    }

    float hitFlash = 0.0f;
    if (hitReactionDuration_ > 0.0001f) {
        hitFlash =
            std::clamp(runtime_.hitReactionTimer / hitReactionDuration_, 0.0f, 1.0f);
    }
    const bool isHitFlashing = hitFlash > 0.0f;

    ModelDrawEffect hitEffect{};
    if (isHitFlashing) {
        hitEffect.enabled = true;
        hitEffect.additiveBlend = false;
        hitEffect.color = {1.00f, 0.22f, 0.22f, 0.92f};
        hitEffect.intensity = 0.75f + 0.85f * hitFlash;
        hitEffect.fresnelPower = 2.6f;
        hitEffect.noiseAmount = 0.10f;
        hitEffect.time = runtime_.stateTimer;
    }

    const uint32_t effectModelId =
        (projectileModelId_ != 0) ? projectileModelId_ : modelId_;
    ModelDrawEffect warpEffect{};

    if (action_.kind == ActionKind::Warp) {
        warpEffect.enabled = true;
        warpEffect.additiveBlend = true;
        warpEffect.color = {0.70f, 0.04f, 0.22f, 0.82f};
        warpEffect.intensity = (action_.step == ActionStep::Move) ? 2.05f : 1.70f;
        warpEffect.fresnelPower = 4.6f;
        warpEffect.noiseAmount = 0.88f;
        warpEffect.time = stateTimer_;

        if (isHitFlashing) {
            warpEffect.color = {1.00f, 0.18f, 0.28f, 0.92f};
            warpEffect.intensity += 0.75f * hitFlash;
            warpEffect.noiseAmount += 0.10f * hitFlash;
        }

        modelManager->SetDrawEffect(warpEffect);
    } else if (isHitFlashing) {
        modelManager->SetDrawEffect(hitEffect);
    }

    auto drawEnemyVisual = [&](const Transform &visual) {
        modelManager->Draw(modelId_, visual, camera);
    };

    if (isVisible_) {
        drawEnemyVisual(visualTf_);
    }

    if (action_.kind == ActionKind::Warp || isHitFlashing) {
        modelManager->ClearDrawEffect();
    }

    for (const auto &bullet : bullets_) {
        if (!bullet.isAlive) {
            continue;
        }

        Transform bulletTf = tf_;
        bulletTf.position = bullet.position;
        bulletTf.scale = {0.2f, 0.2f, 0.2f};
        modelManager->Draw(effectModelId, bulletTf, camera);
    }

    for (const auto &wave : waves_) {
        if (!wave.isAlive) {
            continue;
        }

        Transform waveTf = tf_;
        waveTf.position = wave.position;
        waveTf.scale = {0.6f, 0.2f, 1.2f};
        modelManager->Draw(effectModelId, waveTf, camera);
    }
}

void Enemy::UpdatePresentationEvents() {
    ActionStep currentWarpStep = ActionStep::None;
    if (action_.kind == ActionKind::Warp) {
        currentWarpStep = action_.step;
    }

    if (currentWarpStep == ActionStep::Start &&
        prevPresentationWarpStep_ != ActionStep::Start) {
        EnemyElectricRingSpawnRequest req{};
        req.worldPos =
            warp_.hasDeparturePos ? warp_.departurePos : tf_.position;
        req.isWarpEnd = false;
        electricRingSpawnRequests_.push_back(req);
    }

    if (currentWarpStep == ActionStep::End &&
        prevPresentationWarpStep_ != ActionStep::End) {
        EnemyElectricRingSpawnRequest req{};
        req.worldPos = warp_.targetPos;
        req.isWarpEnd = true;
        electricRingSpawnRequests_.push_back(req);
    }

    prevPresentationWarpStep_ = currentWarpStep;
}

std::vector<EnemyElectricRingSpawnRequest>
Enemy::ConsumeElectricRingSpawnRequests() {
    std::vector<EnemyElectricRingSpawnRequest> out = electricRingSpawnRequests_;
    electricRingSpawnRequests_.clear();
    return out;
}
