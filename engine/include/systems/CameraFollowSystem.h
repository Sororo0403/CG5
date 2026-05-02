#pragma once

class Camera;
class World;

/// <summary>
/// CameraFollow ComponentからCameraを更新するSystem。
/// </summary>
struct CameraFollowSystem {
    void Update(World &world, Camera &camera, float deltaTime);
};
