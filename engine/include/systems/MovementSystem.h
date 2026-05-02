#pragma once

class World;

/// <summary>
/// TransformとVelocityを持つEntityの位置を更新するSystem。
/// </summary>
struct MovementSystem {
    /// <summary>
    /// VelocityをTransformへ積分する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void Update(World &world, float deltaTime);
};
