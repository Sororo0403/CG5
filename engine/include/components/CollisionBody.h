#pragma once

/// <summary>
/// 衝突解決での剛体種別。
/// </summary>
enum class CollisionBodyType {
    Static,
    Kinematic,
    Dynamic,
};

/// <summary>
/// Colliderを押し戻し解決に参加させるためのComponent。
/// </summary>
struct CollisionBody {
    CollisionBodyType type = CollisionBodyType::Static;
    bool resolve = true;
    bool groundable = true;
    float groundNormalThreshold = 0.55f;
};
