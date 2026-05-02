#pragma once
#include "Collider.h"
#include "Entity.h"
#include "Transform.h"

#include <unordered_set>
#include <vector>

class World;

/// <summary>
/// 衝突イベントの種類。
/// </summary>
enum class CollisionEventType {
    Enter,
    Stay,
    Exit,
};

/// <summary>
/// 衝突したEntityペアを表すイベント。
/// </summary>
struct CollisionEvent {
    Entity a = kInvalidEntity;
    Entity b = kInvalidEntity;
    CollisionEventType type = CollisionEventType::Enter;
};

/// <summary>
/// TransformとColliderを持つEntity同士の衝突を検出するSystem。
/// </summary>
struct CollisionSystem {
    /// <summary>
    /// 衝突判定を更新し、衝突イベントを再構築する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    void Update(World &world);

    /// <summary>
    /// BroadPhaseで使う空間グリッドのセルサイズを設定する。
    /// </summary>
    void SetBroadPhaseCellSize(float cellSize);

    /// <summary>
    /// 直近のUpdateで検出された衝突イベント配列を取得する。
    /// </summary>
    /// <returns>衝突イベント配列。</returns>
    const std::vector<CollisionEvent> &Events() const;

  private:
    struct CollisionPair {
        Entity a = kInvalidEntity;
        Entity b = kInvalidEntity;

        bool operator==(const CollisionPair &other) const {
            return a == other.a && b == other.b;
        }
    };

    struct CollisionPairHash {
        size_t operator()(const CollisionPair &pair) const {
            return std::hash<Entity>{}(pair.a) ^
                   (std::hash<Entity>{}(pair.b) << 1);
        }
    };

    CollisionPair MakePair(Entity a, Entity b) const;
    bool IsPairTrackable(const World &world, const CollisionPair &pair) const;

    /// <summary>
    /// レイヤー設定上、2つのColliderが衝突判定対象かを判定する。
    /// </summary>
    /// <param name="a">1つ目のCollider。</param>
    /// <param name="b">2つ目のCollider。</param>
    /// <returns>判定対象の場合はtrue。</returns>
    bool CanCollide(const Collider &a, const Collider &b) const;

  private:
    std::vector<CollisionEvent> events_;
    std::unordered_set<CollisionPair, CollisionPairHash> previousPairs_;
    float broadPhaseCellSize_ = 8.0f;
};
