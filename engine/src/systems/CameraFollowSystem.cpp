#include "CameraFollowSystem.h"

#include "Camera.h"
#include "CameraFollow.h"
#include "LockOn.h"
#include "Transform.h"
#include "World.h"

#include <algorithm>
#include <cmath>

namespace {

DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3 &a,
                      const DirectX::XMFLOAT3 &b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3 &from,
                       const DirectX::XMFLOAT3 &to, float t) {
    return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t};
}

float SmoothFactor(float sharpness, float deltaTime) {
    return 1.0f - std::exp(-(std::max)(0.0f, sharpness) * deltaTime);
}

} // namespace

void CameraFollowSystem::Update(World &world, Camera &camera,
                                float deltaTime) {
    world.View<CameraFollow>([&world, &camera, deltaTime](
                                 Entity entity, CameraFollow &follow) {
        const Entity targetEntity =
            follow.target.IsValid() ? follow.target : entity;
        const Transform *targetTransform =
            world.TryGet<Transform>(targetEntity);
        if (!targetTransform) {
            return;
        }

        DirectX::XMFLOAT3 lookTarget =
            Add(targetTransform->position, follow.lookOffset);

        if (const LockOnState *lockOn = world.TryGet<LockOnState>(targetEntity)) {
            if (lockOn->locked) {
                if (const Transform *lockedTransform =
                        world.TryGet<Transform>(lockOn->target)) {
                    lookTarget = Lerp(lookTarget, lockedTransform->position, 0.45f);
                }
            }
        }

        const DirectX::XMFLOAT3 desiredPosition =
            Add(targetTransform->position, follow.offset);
        const DirectX::XMFLOAT3 position = Lerp(
            camera.GetPosition(), desiredPosition,
            SmoothFactor(follow.positionSharpness, deltaTime));
        const DirectX::XMFLOAT3 target =
            Lerp(camera.GetTarget(), lookTarget,
                 SmoothFactor(follow.lookSharpness, deltaTime));

        camera.SetMode(CameraMode::LookAt);
        camera.SetPosition(position);
        camera.LookAt(target);
        camera.UpdateMatrices();
    });
}
