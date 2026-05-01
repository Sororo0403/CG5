#pragma once
#include <array>
#include <string>

struct EditorContext;

class EnemyTuningPanel {
  public:
    void Draw(EditorContext &context);

    struct AttackEdit {
        float damage = 10.0f;
        float knockback = 4.0f;
        float hitBoxSize[3] = {1.0f, 1.0f, 1.0f};
        float chargeTime = 0.6f;
        float startup = 0.1f;
        float active = 0.1f;
        float recovery = 0.7f;
        float trackingEndTime = 0.0f;
        float forwardOffset = 0.0f;
        float sideOffset = 0.0f;
        float heightOffset = 0.0f;
        float interval = 0.2f;
        int minCount = 1;
        int maxCount = 1;
        float speed = 0.0f;
        float lifeTime = 0.0f;
        float maxDistance = 0.0f;
    };

    struct MovementEdit {
        float hitReactionMoveSpeed = 3.5f;
        float chargeTurnSpeed = 6.0f;
        float recoveryTurnSpeed = 2.0f;
        float idleTurnSpeed = 8.0f;
        float stagnantDistanceThreshold = 0.15f;
        float stagnantTimeThreshold = 1.2f;
        int stagnantWarpBonus = 5;
        float closePressureDistance = 3.0f;
        float closePressureTimeThreshold = 1.0f;
        float farDistanceWarpTimeThreshold = 2.0f;
        int farDistanceWarpBonus = 50;
        float warpNearRadiusMin = 1.8f;
        float warpNearRadiusMax = 3.0f;
        float warpApproachForwardDistance = 2.3f;
        float warpApproachSideDistance = 2.1f;
        float warpApproachLongFrontDistance = 4.0f;
        float stalkDurationMin = 0.45f;
        float stalkDurationMax = 1.1f;
        float stalkMoveSpeed = 2.2f;
        float stalkStrafeRadiusWeight = 0.75f;
        float stalkForwardAdjustWeight = 0.35f;
        float stalkNearEnterChance = 0.28f;
        float stalkMidEnterChance = 0.18f;
        int stalkRepeatLimit = 2;
    };

  private:
    void EnsurePathInitialized();
    void LoadFromFile(EditorContext &context);
    void SaveToFile(EditorContext &context);
    void DrawCore();
    void DrawMovement();
    void DrawAttack(const char *label, AttackEdit &attack, bool melee,
                    bool projectile, bool wave);
    void MarkDirty(bool changed);

    std::array<char, 260> path_{};
    bool pathInitialized_ = false;
    bool loaded_ = false;
    bool dirty_ = false;
    std::string status_;

    float maxHp_ = 1000.0f;
    float phase2HealthRatioThreshold_ = 0.6f;
    float nearAttackDistance_ = 4.0f;
    float farAttackDistance_ = 6.5f;

    AttackEdit smash_{};
    AttackEdit sweep_{};
    AttackEdit shot_{};
    AttackEdit wave_{};
    MovementEdit movement_{};
};
