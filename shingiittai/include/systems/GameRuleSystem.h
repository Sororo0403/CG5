#pragma once
#include "CollisionSystem.h"

#include <vector>

class World;

/// <summary>
/// 衝突イベントをゲーム固有ルールへ変換するSystem。
/// </summary>
struct GameRuleSystem {
    /// <summary>
    /// 衝突イベントを読み取り、ゲーム固有の勝敗やダメージ処理を更新する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="collisions">EngineのCollisionSystemが検出した衝突イベント。</param>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void Update(World &world, const std::vector<CollisionEvent> &collisions,
                float deltaTime);

    /// <summary>
    /// 直近でプレイヤーと敵が衝突したかを判定する。
    /// </summary>
    /// <returns>プレイヤーと敵の衝突が記録されている場合はtrue。</returns>
    bool HasPlayerEnemyCollision() const;

    /// <summary>
    /// 直近のプレイヤー対敵の衝突イベントを取得する。
    /// </summary>
    /// <returns>直近のプレイヤー対敵の衝突イベント。</returns>
    CollisionEvent LastPlayerEnemyCollision() const;

    /// <summary>
    /// 記録済みのゲームルールイベントをクリアする。
    /// </summary>
    void ClearFrameState();

  private:
    CollisionEvent lastPlayerEnemyCollision_{};
};
