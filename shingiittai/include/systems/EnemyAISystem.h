#pragma once

class World;

/// <summary>
/// EnemyTagを持つEntityをプレイヤーへ接近させるゲーム固有System。
/// </summary>
struct EnemyAISystem {
    /// <summary>
    /// 敵EntityのVelocityをプレイヤー方向へ更新する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void Update(World &world, float deltaTime);
};
