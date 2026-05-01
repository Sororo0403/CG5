#pragma once

class Enemy;
class Player;
struct SceneContext;

class CombatSystem {
  public:
    struct ResolveContext {
        float deltaTime = 0.0f;
        bool freezeEnemyMotion = false;
        bool counterCinematicActive = false;
        float guardDamageMultiplier = 0.25f;
        float reflectDamageMultiplier = 2.0f;
    };

    struct ResolveResult {
        bool startCounterCinematicThisFrame = false;
        bool stopCounterCinematicThisFrame = false;
        bool forceSyncEnemyAnimationThisFrame = false;
        float reflectedDamage = 0.0f;
    };

    ResolveResult Resolve(Player &player, Enemy &enemy, const SceneContext *ctx,
                          const ResolveContext &context);
    void Reset();

  private:
    void UpdateCooldowns(float deltaTime);
    void ResolvePlayerAttack(Player &player, Enemy &enemy,
                             const SceneContext *ctx,
                             const ResolveContext &context,
                             ResolveResult &result);
    void ResolveMelee(Player &player, Enemy &enemy, const SceneContext *ctx,
                      const ResolveContext &context, ResolveResult &result);
    void ResolveBullets(Player &player, Enemy &enemy, const SceneContext *ctx,
                        const ResolveContext &context, ResolveResult &result);
    void ResolveWaves(Player &player, Enemy &enemy, const SceneContext *ctx,
                      const ResolveContext &context, ResolveResult &result);
    void TriggerSuccessfulCounter(Player &player, Enemy &enemy,
                                  const SceneContext *ctx, float enemyDamage,
                                  float hitCooldown, ResolveResult &result);

    float playerHitCooldown_ = 0.0f;
    float enemyHitCooldown_ = 0.0f;
    float reflectedDamage_ = 0.0f;
};
