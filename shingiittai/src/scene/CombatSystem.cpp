#include "CombatSystem.h"
#include "EffectSystem.h"
#include "Enemy.h"
#include "Player.h"
#include "SceneContext.h"
#include "gameobject/ColliderComponent.h"

#include <cmath>

namespace {

void RequestHitSpark(const SceneContext *ctx,
                     const DirectX::XMFLOAT3 &position) {
    if (ctx == nullptr || ctx->effects == nullptr) {
        return;
    }

    ctx->effects->EmitParticles(EffectParticlePreset::HitSpark, position);
}

void NormalizeHorizontal(float &x, float &z) {
    float len = std::sqrt(x * x + z * z);
    if (len < 0.0001f) {
        len = 1.0f;
    }

    x /= len;
    z /= len;
}

bool IsEnemySmashCounterWindow(ActionKind kind, ActionStep step) {
    return kind == ActionKind::Smash && step == ActionStep::Active;
}

bool IsEnemySweepCounterWindow(ActionKind kind, ActionStep step) {
    return kind == ActionKind::Sweep && step == ActionStep::Active;
}

bool IsEnemySmashMeleeWindow(ActionKind kind, ActionStep step) {
    return kind == ActionKind::Smash &&
           (step == ActionStep::Active || step == ActionStep::Recovery);
}

bool IsEnemySweepMeleeWindow(ActionKind kind, ActionStep step) {
    return kind == ActionKind::Sweep &&
           (step == ActionStep::Active || step == ActionStep::Recovery);
}

} // namespace

CombatSystem::ResolveResult
CombatSystem::Resolve(Player &player, Enemy &enemy, const SceneContext *ctx,
                      const ResolveContext &context) {
    ResolveResult result{};

    UpdateCooldowns(context.deltaTime);
    ResolvePlayerAttack(player, enemy, ctx, context, result);
    ResolveMelee(player, enemy, ctx, context, result);

    if (!context.freezeEnemyMotion) {
        ResolveBullets(player, enemy, ctx, context, result);
        ResolveWaves(player, enemy, ctx, context, result);
    }

    result.reflectedDamage = reflectedDamage_;
    return result;
}

void CombatSystem::Reset() {
    playerHitCooldown_ = 0.0f;
    enemyHitCooldown_ = 0.0f;
    reflectedDamage_ = 0.0f;
}

void CombatSystem::UpdateCooldowns(float deltaTime) {
    if (enemyHitCooldown_ > 0.0f) {
        enemyHitCooldown_ -= deltaTime;
        if (enemyHitCooldown_ < 0.0f) {
            enemyHitCooldown_ = 0.0f;
        }
    }

    if (playerHitCooldown_ > 0.0f) {
        playerHitCooldown_ -= deltaTime;
        if (playerHitCooldown_ < 0.0f) {
            playerHitCooldown_ = 0.0f;
        }
    }
}

void CombatSystem::ResolvePlayerAttack(Player &player, Enemy &enemy,
                                       const SceneContext *ctx,
                                       const ResolveContext &context,
                                       ResolveResult &result) {
    const auto swords = player.GetSwords();
    const auto swordSlashStates = player.GetSwordSlashStates();

    for (size_t i = 0; i < swords.size(); ++i) {
        const Sword *sword = swords[i];
        if (sword == nullptr || !swordSlashStates[i]) {
            continue;
        }

        auto swordHitBox = sword->GetOBB();
        auto bodyBox = enemy.GetBodyOBB();
        const bool hitBody = ColliderComponent::Intersects(swordHitBox, bodyBox);

        if (enemyHitCooldown_ <= 0.0f && hitBody) {
            enemy.TakeDamage(10.0f);
            RequestHitSpark(ctx, bodyBox.center);
            enemyHitCooldown_ = 0.2f;
            if (context.counterCinematicActive) {
                result.stopCounterCinematicThisFrame = true;
            }
        }

        if (enemyHitCooldown_ > 0.0f) {
            break;
        }
    }
}

void CombatSystem::ResolveMelee(Player &player, Enemy &enemy,
                                const SceneContext *ctx,
                                const ResolveContext &context,
                                ResolveResult &result) {
    if (context.freezeEnemyMotion) {
        return;
    }

    const ActionKind enemyActionKind = enemy.GetActionKind();
    const ActionStep enemyActionStep = enemy.GetActionStep();
    const bool isEnemyMeleeActive =
        IsEnemySmashMeleeWindow(enemyActionKind, enemyActionStep) ||
        IsEnemySweepMeleeWindow(enemyActionKind, enemyActionStep);
    if (!isEnemyMeleeActive) {
        return;
    }

    const auto enemyAttackBox = enemy.GetAttackOBB();
    const auto playerBox = player.GetOBB();
    if (!ColliderComponent::Intersects(enemyAttackBox, playerBox) ||
        playerHitCooldown_ > 0.0f) {
        return;
    }

    float dx = player.GetTransform().position.x - enemy.GetTransform().position.x;
    float dz = player.GetTransform().position.z - enemy.GetTransform().position.z;
    NormalizeHorizontal(dx, dz);

    const float enemyAttackDamage = enemy.GetCurrentAttackDamage();
    const float enemyAttackKnockback = enemy.GetCurrentAttackKnockback();
    const bool isCounterAxisMatch =
        (IsEnemySmashCounterWindow(enemyActionKind, enemyActionStep) &&
         player.GetCounterAxis() == SwordCounterAxis::Vertical) ||
        (IsEnemySweepCounterWindow(enemyActionKind, enemyActionStep) &&
         player.GetCounterAxis() == SwordCounterAxis::Horizontal);
    const bool canCounterThisHit = player.IsCounterStance() && isCounterAxisMatch;

    if (canCounterThisHit) {
        TriggerSuccessfulCounter(player, enemy, ctx, enemyAttackDamage, 0.2f,
                                 result);
    } else if (player.IsGuarding()) {
        enemy.NotifyAttackGuarded();
        player.TakeDamage(enemyAttackDamage * context.guardDamageMultiplier);
        RequestHitSpark(ctx, player.GetTransform().position);
        player.AddKnockback({dx * (enemyAttackKnockback * 0.5f), 0.0f,
                             dz * (enemyAttackKnockback * 0.5f)});
        playerHitCooldown_ = 0.2f;
    } else {
        enemy.NotifyAttackConnected();
        player.TakeDamage(enemyAttackDamage);
        RequestHitSpark(ctx, player.GetTransform().position);
        player.AddKnockback(
            {dx * enemyAttackKnockback, 0.0f, dz * enemyAttackKnockback});
        playerHitCooldown_ = 0.4f;
    }
}

void CombatSystem::ResolveBullets(Player &player, Enemy &enemy,
                                  const SceneContext *ctx,
                                  const ResolveContext &context,
                                  ResolveResult &result) {
    const auto counterBox = player.GetSword().GetCounterOBB();
    const auto playerBox = player.GetOBB();
    const bool isPlayerCountering = player.GetSword().IsSlashMode();
    const bool isPlayerGuarding = player.IsGuarding();
    const auto enemyBodyBox = enemy.GetBodyOBB();
    const auto &bullets = enemy.GetBullets();

    for (size_t i = 0; i < bullets.size(); ++i) {
        const auto &bullet = bullets[i];
        if (!bullet.isAlive) {
            continue;
        }

        OBB bulletBox{};
        bulletBox.center = bullet.position;
        bulletBox.size = enemy.GetBulletHitBoxSize();
        bulletBox.rotation = player.GetTransform().rotation;

        if (!bullet.isReflected && isPlayerCountering &&
            ColliderComponent::Intersects(bulletBox, counterBox)) {
            enemy.ReflectBullet(i, enemy.GetTransform().position);
            continue;
        }

        if (bullet.isReflected) {
            if (ColliderComponent::Intersects(bulletBox, enemyBodyBox) &&
                enemyHitCooldown_ <= 0.0f) {
                reflectedDamage_ =
                    enemy.GetBulletDamage() * context.reflectDamageMultiplier;
                enemy.TakeDamage(reflectedDamage_);
                RequestHitSpark(ctx, bullet.position);
                enemy.DestroyBullet(i);
                enemyHitCooldown_ = 0.2f;
            }
            continue;
        }

        if (ColliderComponent::Intersects(bulletBox, playerBox)) {
            if (playerHitCooldown_ <= 0.0f) {
                float vx = bullet.velocity.x;
                float vz = bullet.velocity.z;
                NormalizeHorizontal(vx, vz);

                if (player.IsCounterStance()) {
                    TriggerSuccessfulCounter(
                        player, enemy, ctx, enemy.GetBulletDamage() * 2.0f,
                        0.12f, result);
                } else if (isPlayerGuarding) {
                    player.TakeDamage(enemy.GetBulletDamage() *
                                      context.guardDamageMultiplier);
                    RequestHitSpark(ctx, bullet.position);
                    player.AddKnockback(
                        {vx * (enemy.GetBulletKnockback() * 0.5f), 0.0f,
                         vz * (enemy.GetBulletKnockback() * 0.5f)});
                    enemy.DestroyBullet(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player.TakeDamage(enemy.GetBulletDamage());
                    RequestHitSpark(ctx, bullet.position);
                    player.AddKnockback(
                        {vx * enemy.GetBulletKnockback(), 0.0f,
                         vz * enemy.GetBulletKnockback()});
                    enemy.DestroyBullet(i);
                    playerHitCooldown_ = 0.3f;
                }
            }

            enemy.ConsumeBullet(i);
            break;
        }
    }
}

void CombatSystem::ResolveWaves(Player &player, Enemy &enemy,
                                const SceneContext *ctx,
                                const ResolveContext &context,
                                ResolveResult &result) {
    const auto counterBox = player.GetSword().GetCounterOBB();
    const auto playerBox = player.GetOBB();
    const bool isPlayerCountering = player.GetSword().IsSlashMode();
    const bool isPlayerGuarding = player.IsGuarding();
    const auto enemyBodyBox = enemy.GetBodyOBB();
    const auto &waves = enemy.GetWaves();

    for (size_t i = 0; i < waves.size(); ++i) {
        const auto &wave = waves[i];
        if (!wave.isAlive) {
            continue;
        }

        OBB waveBox{};
        waveBox.center = wave.position;
        waveBox.size = enemy.GetWaveHitBoxSize();
        waveBox.rotation = player.GetTransform().rotation;

        if (!wave.isReflected && isPlayerCountering &&
            ColliderComponent::Intersects(waveBox, counterBox)) {
            enemy.ReflectWave(i, enemy.GetTransform().position);
            continue;
        }

        if (wave.isReflected) {
            if (ColliderComponent::Intersects(waveBox, enemyBodyBox) &&
                enemyHitCooldown_ <= 0.0f) {
                reflectedDamage_ =
                    enemy.GetWaveDamage() * context.reflectDamageMultiplier;
                enemy.TakeDamage(reflectedDamage_);
                RequestHitSpark(ctx, wave.position);
                enemy.DestroyWave(i);
                enemyHitCooldown_ = 0.2f;
            }
            continue;
        }

        if (ColliderComponent::Intersects(waveBox, playerBox)) {
            if (playerHitCooldown_ <= 0.0f) {
                float vx = wave.direction.x;
                float vz = wave.direction.z;
                NormalizeHorizontal(vx, vz);

                if (player.IsCounterStance()) {
                    TriggerSuccessfulCounter(player, enemy, ctx,
                                             enemy.GetWaveDamage() * 2.0f,
                                             0.12f, result);
                } else if (isPlayerGuarding) {
                    player.TakeDamage(enemy.GetWaveDamage() *
                                      context.guardDamageMultiplier);
                    RequestHitSpark(ctx, wave.position);
                    player.AddKnockback(
                        {vx * (enemy.GetWaveKnockback() * 0.5f), 0.0f,
                         vz * (enemy.GetWaveKnockback() * 0.5f)});
                    enemy.DestroyWave(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player.TakeDamage(enemy.GetWaveDamage());
                    RequestHitSpark(ctx, wave.position);
                    player.AddKnockback({vx * enemy.GetWaveKnockback(), 0.0f,
                                         vz * enemy.GetWaveKnockback()});
                    enemy.DestroyWave(i);
                    playerHitCooldown_ = 0.35f;
                }
            }

            enemy.ConsumeWave(i);
            break;
        }
    }
}

void CombatSystem::TriggerSuccessfulCounter(Player &player, Enemy &enemy,
                                            const SceneContext *ctx,
                                            float enemyDamage,
                                            float hitCooldown,
                                            ResolveResult &result) {
    player.NotifyCounterSuccess();
    if (enemy.NotifyCountered()) {
        result.forceSyncEnemyAnimationThisFrame = true;
    }
    enemy.TakeDamage(enemyDamage);
    RequestHitSpark(ctx, enemy.GetTransform().position);
    playerHitCooldown_ = hitCooldown;
    result.startCounterCinematicThisFrame = true;
}
