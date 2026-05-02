#pragma once

class World;

/// <summary>
/// PlayerTagを持つEntityの入力を速度へ変換するゲーム固有System。
/// </summary>
struct PlayerControlSystem {
    /// <summary>
    /// 入力状態に応じてプレイヤーEntityのVelocityを更新する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void Update(World &world, float deltaTime);
};
