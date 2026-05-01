#include "BattleCameraController.h"
#include "Input.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

void BattleCameraController::ResetForBattle(const XMFLOAT3 &playerPos,
                                            const XMFLOAT3 &enemyPos) {
    cameraYaw_ = 0.0f;
    cameraPitch_ = 0.0f;
    isLockOn_ = true;
    currentFovDeg_ = normalFovDeg_;
    targetFovDeg_ = normalFovDeg_;

    lockOnOrbitCameraPos_ = {playerPos.x, playerPos.y + lockOnOrbitHeight_,
                             playerPos.z - lockOnOrbitRadius_};
    lockOnLookAt_ = {
        playerPos.x * lockOnLookPlayerWeight_ +
            enemyPos.x * lockOnLookEnemyWeight_,
        (playerPos.y + cameraLookHeight_) * 0.52f +
            (enemyPos.y + 1.30f) * 0.48f,
        playerPos.z * lockOnLookPlayerWeight_ +
            enemyPos.z * lockOnLookEnemyWeight_};
}

void BattleCameraController::UpdateInput(Input *input) {
    if (input != nullptr && input->IsKeyTrigger(DIK_Q)) {
        isLockOn_ = !isLockOn_;
    }
}

float BattleCameraController::BlendFov(float targetFovDeg, float lerpSpeed,
                                       float deltaTime) {
    targetFovDeg_ = targetFovDeg;
    float fovAlpha = lerpSpeed * deltaTime;
    if (fovAlpha > 1.0f) {
        fovAlpha = 1.0f;
    }

    currentFovDeg_ += (targetFovDeg_ - currentFovDeg_) * fovAlpha;
    return currentFovDeg_;
}

void BattleCameraController::Apply(Camera &camera, float deltaTime,
                                   const Player &player, const Enemy &enemy) {
    const auto &playerTf = player.GetTransform();
    const auto &enemyTf = enemy.GetTransform();

    const XMFLOAT3 &playerPos = playerTf.position;
    const XMFLOAT3 &enemyPos = enemyTf.position;

    const ActionKind enemyActionKind = enemy.GetActionKind();
    const ActionStep enemyActionStep = enemy.GetActionStep();

    const bool isEnemyWarpStart = (enemyActionKind == ActionKind::Warp &&
                                   enemyActionStep == ActionStep::Start);
    const bool isEnemyWarpMove = (enemyActionKind == ActionKind::Warp &&
                                  enemyActionStep == ActionStep::Move);
    const bool isEnemyWarpEnd = (enemyActionKind == ActionKind::Warp &&
                                 enemyActionStep == ActionStep::End);
    const bool isEnemyPhaseTransition = enemy.IsPhaseTransitionActive();
    const float enemyPhaseTransitionRatio = enemy.GetPhaseTransitionRatio();

    targetFovDeg_ = normalFovDeg_;

    if (isLockOn_) {
        targetFovDeg_ = lockOnFovDeg_;
    }
    if (isEnemyWarpStart || isEnemyWarpMove || isEnemyWarpEnd) {
        targetFovDeg_ = warpFovDeg_;
    }
    if (isEnemyPhaseTransition) {
        targetFovDeg_ = phaseTransitionFovDeg_;
    }

    float usedFovLerpSpeed = fovLerpSpeed_;
    if (isEnemyPhaseTransition) {
        usedFovLerpSpeed = phaseTransitionFovLerpSpeed_;
    }
    const float currentFovDeg =
        BlendFov(targetFovDeg_, usedFovLerpSpeed, deltaTime);

    if (isLockOn_) {
        XMFLOAT3 assistTarget = enemyPos;

        float assistStrength = lockOnAssistStrength_;
        float assistMaxStep = lockOnAssistMaxStep_;

        if (isEnemyWarpStart) {
            assistStrength = warpStartAssistStrength_;
            assistMaxStep = warpStartAssistMaxStep_;
        } else if (isEnemyWarpMove) {
            assistStrength = 0.0f;
            assistMaxStep = 0.0f;
        } else if (isEnemyWarpEnd) {
            assistStrength = warpEndAssistStrength_;
            assistMaxStep = warpEndAssistMaxStep_;
        } else if (isEnemyPhaseTransition) {
            assistStrength = lockOnAssistStrength_ * 1.35f;
            assistMaxStep = lockOnAssistMaxStep_ * 1.35f;
        }

        float dx = assistTarget.x - playerPos.x;
        float dz = assistTarget.z - playerPos.z;

        float targetYaw = std::atan2f(dx, dz);
        float diff = targetYaw - cameraYaw_;

        while (diff > 3.14159265f) {
            diff -= 6.28318530f;
        }
        while (diff < -3.14159265f) {
            diff += 6.28318530f;
        }

        float maxStep = assistMaxStep * deltaTime;
        float applied = diff * assistStrength * deltaTime;

        if (applied > maxStep) {
            applied = maxStep;
        } else if (applied < -maxStep) {
            applied = -maxStep;
        }

        cameraYaw_ += applied;
    }

    float cosPitch = std::cosf(cameraPitch_);
    XMFLOAT3 forward = {std::sinf(cameraYaw_) * cosPitch,
                        std::sinf(cameraPitch_),
                        std::cosf(cameraYaw_) * cosPitch};
    XMFLOAT3 right = {std::cosf(cameraYaw_), 0.0f, -std::sinf(cameraYaw_)};

    XMFLOAT3 cameraTargetBase = {
        playerPos.x, playerPos.y + cameraLookHeight_, playerPos.z};

    XMFLOAT3 cameraPos{};

    if (isLockOn_) {
        float toEnemyX = enemyPos.x - playerPos.x;
        float toEnemyZ = enemyPos.z - playerPos.z;
        float distXZ = std::sqrt(toEnemyX * toEnemyX + toEnemyZ * toEnemyZ);

        if (distXZ < 0.0001f) {
            distXZ = 1.0f;
        }

        float invLen = 1.0f / distXZ;
        float lineX = toEnemyX * invLen;
        float lineZ = toEnemyZ * invLen;
        float orbitRightX = lineZ;
        float orbitRightZ = -lineX;

        float pullT = 0.0f;
        float range = lockOnDistanceMax_ - lockOnDistanceMin_;
        if (range > 0.0001f) {
            pullT = (distXZ - lockOnDistanceMin_) / range;
        }
        pullT = std::clamp(pullT, 0.0f, 1.0f);

        float usedRadius = lockOnOrbitRadius_ + lockOnOrbitPullBackMax_ * pullT;
        if (isEnemyPhaseTransition) {
            usedRadius -= phaseTransitionPushIn_ * enemyPhaseTransitionRatio;
        }

        float lineYaw = std::atan2f(lineX, lineZ);
        float yawDiff = cameraYaw_ - lineYaw;

        while (yawDiff > 3.14159265f) {
            yawDiff -= 6.28318530f;
        }
        while (yawDiff < -3.14159265f) {
            yawDiff += 6.28318530f;
        }

        const float maxOrbitAngle = 0.65f;
        yawDiff = std::clamp(yawDiff, -maxOrbitAngle, maxOrbitAngle);

        float sinA = std::sinf(yawDiff);
        float cosA = std::cosf(yawDiff);

        XMFLOAT3 pairCenter = {
            playerPos.x * lockOnLookPlayerWeight_ +
                enemyPos.x * lockOnLookEnemyWeight_,
            (playerPos.y + cameraLookHeight_) * 0.55f +
                (enemyPos.y + 1.35f) * 0.45f,
            playerPos.z * lockOnLookPlayerWeight_ +
                enemyPos.z * lockOnLookEnemyWeight_};

        XMFLOAT3 desiredCameraPos = {
            pairCenter.x - lineX * usedRadius * cosA +
                orbitRightX * usedRadius * sinA +
                orbitRightX * lockOnOrbitSideBias_,
            pairCenter.y + lockOnOrbitHeight_ + 0.20f * pullT,
            pairCenter.z - lineZ * usedRadius * cosA +
                orbitRightZ * usedRadius * sinA +
                orbitRightZ * lockOnOrbitSideBias_};

        float posAlpha = lockOnOrbitLerpSpeed_ * deltaTime;
        if (posAlpha > 1.0f) {
            posAlpha = 1.0f;
        }

        lockOnOrbitCameraPos_.x +=
            (desiredCameraPos.x - lockOnOrbitCameraPos_.x) * posAlpha;
        lockOnOrbitCameraPos_.y +=
            (desiredCameraPos.y - lockOnOrbitCameraPos_.y) * posAlpha;
        lockOnOrbitCameraPos_.z +=
            (desiredCameraPos.z - lockOnOrbitCameraPos_.z) * posAlpha;

        cameraPos = lockOnOrbitCameraPos_;
    } else {
        float dx = enemyPos.x - playerPos.x;
        float dz = enemyPos.z - playerPos.z;
        float enemyDistanceXZ = std::sqrt(dx * dx + dz * dz);
        float dynamicDistance = cameraDistance_;
        if (enemyDistanceXZ > 5.0f) {
            dynamicDistance +=
                (std::min)(1.1f, (enemyDistanceXZ - 5.0f) * 0.18f);
        }

        cameraPos = {cameraTargetBase.x - forward.x * dynamicDistance +
                         right.x * cameraSideOffset_,
                     cameraTargetBase.y + cameraHeight_ -
                         forward.y * dynamicDistance,
                     cameraTargetBase.z - forward.z * dynamicDistance +
                         right.z * cameraSideOffset_};

        if (isEnemyPhaseTransition) {
            cameraPos.x += forward.x * phaseTransitionPushIn_ *
                           enemyPhaseTransitionRatio;
            cameraPos.y += 0.12f * enemyPhaseTransitionRatio;
            cameraPos.z += forward.z * phaseTransitionPushIn_ *
                           enemyPhaseTransitionRatio;
        }

        lockOnOrbitCameraPos_ = cameraPos;
    }

    XMFLOAT3 lookAt{};

    if (isLockOn_) {
        XMFLOAT3 desiredLookAt = {
            playerPos.x * lockOnLookPlayerWeight_ +
                enemyPos.x * lockOnLookEnemyWeight_,
            (playerPos.y + cameraLookHeight_) * 0.52f +
                (enemyPos.y + 1.30f) * 0.48f,
            playerPos.z * lockOnLookPlayerWeight_ +
                enemyPos.z * lockOnLookEnemyWeight_};

        float lookAlpha = lockOnLookAtLerpSpeed_ * deltaTime;
        if (lookAlpha > 1.0f) {
            lookAlpha = 1.0f;
        }

        lockOnLookAt_.x += (desiredLookAt.x - lockOnLookAt_.x) * lookAlpha;
        lockOnLookAt_.y += (desiredLookAt.y - lockOnLookAt_.y) * lookAlpha;
        lockOnLookAt_.z += (desiredLookAt.z - lockOnLookAt_.z) * lookAlpha;

        lookAt = lockOnLookAt_;
    } else {
        lookAt = {cameraTargetBase.x + forward.x * cameraLookAhead_,
                  cameraTargetBase.y + forward.y * cameraLookAhead_,
                  cameraTargetBase.z + forward.z * cameraLookAhead_};

        lockOnLookAt_ = lookAt;
    }

    if (isEnemyPhaseTransition) {
        XMFLOAT3 transitionLookAt = {
            playerPos.x * (1.0f - phaseTransitionLookAtEnemyWeight_) +
                enemyPos.x * phaseTransitionLookAtEnemyWeight_,
            (playerPos.y + cameraLookHeight_) *
                    (1.0f - phaseTransitionLookAtEnemyWeight_) +
                (enemyPos.y + phaseTransitionLookAtHeight_) *
                    phaseTransitionLookAtEnemyWeight_,
            playerPos.z * (1.0f - phaseTransitionLookAtEnemyWeight_) +
                enemyPos.z * phaseTransitionLookAtEnemyWeight_};

        float blend = enemyPhaseTransitionRatio;
        lookAt.x += (transitionLookAt.x - lookAt.x) * blend;
        lookAt.y += (transitionLookAt.y - lookAt.y) * blend;
        lookAt.z += (transitionLookAt.z - lookAt.z) * blend;
    }

    camera.SetPerspectiveFovDeg(currentFovDeg);
    camera.SetPosition(cameraPos);
    camera.LookAt(lookAt);
    camera.UpdateMatrices();
}
