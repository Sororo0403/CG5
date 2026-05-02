#pragma once
#include <DirectXMath.h>

class World;

/// <summary>
/// PhysicsBodyとVelocityを持つEntityの速度を更新するSystem。
/// </summary>
struct PhysicsSystem {
    /// <summary>
    /// 加速度と重力をVelocityへ積分する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void Update(World &world, float deltaTime);

    /// <summary>
    /// World全体で使用する重力加速度を設定する。
    /// </summary>
    /// <param name="gravity">重力加速度。</param>
    void SetGravity(const DirectX::XMFLOAT3 &gravity);

  private:
    DirectX::XMFLOAT3 gravity_{0.0f, -9.8f, 0.0f};
};
