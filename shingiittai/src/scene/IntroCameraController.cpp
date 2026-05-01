#include "IntroCameraController.h"
#include <cmath>

using namespace DirectX;

void IntroCameraController::Apply(Camera &camera, float &cameraYaw,
                                  const Enemy &enemy) const {
    const XMFLOAT3 &enemyPos = enemy.GetTransform().position;
    const float enemyIntroRatio = enemy.GetIntroRatio();
    const float enemyYaw = enemy.GetFacingYaw();
    const float frontX = std::sinf(enemyYaw);
    const float frontZ = std::cosf(enemyYaw);
    const float rightX = std::cosf(enemyYaw);
    const float rightZ = -std::sinf(enemyYaw);
    const float introSide =
        (enemy.GetIntroPhase() == IntroPhase::SpinSlash) ? 0.42f : 0.16f;
    const float introDistance = 10.2f - 0.35f * enemyIntroRatio;

    const XMFLOAT3 cameraPos = {
        enemyPos.x + frontX * introDistance + rightX * introSide,
        enemyPos.y + 2.12f + 0.10f * enemyIntroRatio,
        enemyPos.z + frontZ * introDistance + rightZ * introSide};
    const XMFLOAT3 lookAt = {enemyPos.x, enemyPos.y + lookAtHeight_ + 0.18f,
                             enemyPos.z};

    camera.SetPosition(cameraPos);
    camera.LookAt(lookAt);
    cameraYaw = std::atan2f(enemyPos.x - cameraPos.x,
                            enemyPos.z - cameraPos.z);
}
