#include "Enemy.h"
#include "Model.h"

#include <algorithm>
#include <string>

namespace {

const std::string kBossAnimWarp = "テレポート";
const std::string kBossAnimShot = "弾発射";
const std::string kBossAnimSweepRight = "横薙ぎ(右から)";
const std::string kBossAnimSweep = "横薙ぎ払い";
const std::string kBossAnimWave = "波状攻撃";
const std::string kBossAnimPhaseTransition = "第二形態移行";
const std::string kBossAnimSmash = "縦振り下ろし";

float Remaining(float duration, float elapsed) {
    return (std::max)(0.0f, duration - elapsed);
}

float ClipDuration(const Model *model, const std::string &animationName) {
    if (model == nullptr) {
        return 0.0f;
    }

    auto it = model->animations.find(animationName);
    if (it == model->animations.end()) {
        return 0.0f;
    }

    return (std::max)(0.0f, it->second.duration);
}

void FitMeleeTimingToClip(EnemyMeleeAttackProfile &profile,
                          float clipDuration) {
    if (clipDuration <= 0.001f) {
        return;
    }

    const float currentCharge = (std::max)(0.05f, profile.base.chargeTime);
    const float currentTotal = (std::max)(0.05f, profile.base.timing.totalTime);
    const float currentRecovery =
        (std::max)(0.0f, currentTotal - profile.base.timing.recoveryStartTime);
    const float currentWhole =
        (std::max)(0.001f, currentCharge + currentTotal + currentRecovery);
    const float scale = clipDuration / currentWhole;

    profile.base.chargeTime = (std::max)(0.05f, currentCharge * scale);

    AttackTimingParam &timing = profile.base.timing;
    timing.totalTime = (std::max)(0.05f, currentTotal * scale);
    timing.trackingEndTime =
        (std::max)(0.0f, timing.trackingEndTime * scale);
    timing.activeStartTime =
        (std::max)(0.0f, timing.activeStartTime * scale);
    timing.activeEndTime =
        (std::max)(timing.activeStartTime, timing.activeEndTime * scale);

    const float fittedRecovery =
        (std::max)(0.0f, clipDuration - profile.base.chargeTime -
                             timing.totalTime);
    timing.recoveryStartTime =
        timing.totalTime - (std::min)(timing.totalTime, fittedRecovery);
    timing.recoveryStartTime =
        (std::max)(timing.activeEndTime, timing.recoveryStartTime);
}

} // namespace

float Enemy::GetCurrentActionTime() const { return stateTimer_; }

float Enemy::GetCurrentAnimationRemainingTime() const {
    if (phaseTransitionActive_) {
        return Remaining(phaseTransitionDuration_, phaseTransitionTimer_);
    }

    switch (action_.kind) {
    case ActionKind::Smash:
    case ActionKind::Sweep: {
        const AttackTimingParam *timing = GetCurrentAttackTiming();
        if (timing == nullptr) {
            return 0.0f;
        }

        const float chargeTime =
            (action_.kind == ActionKind::Smash) ? GetCurrentSmashChargeTime()
                                                : GetCurrentSweepChargeTime();
        const float recoveryTime =
            (std::max)(0.0f, timing->totalTime - timing->recoveryStartTime);

        switch (action_.step) {
        case ActionStep::Charge:
            return Remaining(chargeTime, stateTimer_) + timing->totalTime +
                   recoveryTime;
        case ActionStep::Hold:
            return Remaining(currentHoldDuration_, stateTimer_) +
                   timing->totalTime + recoveryTime;
        case ActionStep::Active:
            return Remaining(timing->totalTime, stateTimer_) + recoveryTime;
        case ActionStep::Recovery:
            return Remaining(recoveryTime, stateTimer_);
        default:
            return 0.0f;
        }
    }

    case ActionKind::Shot:
        switch (action_.step) {
        case ActionStep::Charge:
            return Remaining(config_.attacks.shot.chargeTime, stateTimer_) +
                   static_cast<float>(config_.attacks.shot.maxCount) *
                       config_.attacks.shot.interval +
                   config_.attacks.shot.recoveryTime;
        case ActionStep::Active:
            return static_cast<float>((std::max)(0, shotsRemaining_)) *
                       config_.attacks.shot.interval +
                   config_.attacks.shot.recoveryTime;
        case ActionStep::Recovery:
            return Remaining(config_.attacks.shot.recoveryTime, stateTimer_);
        default:
            return 0.0f;
        }

    case ActionKind::Wave:
        switch (action_.step) {
        case ActionStep::Charge:
            return Remaining(config_.attacks.wave.chargeTime, stateTimer_) +
                   config_.attacks.wave.recoveryTime;
        case ActionStep::Active:
            return config_.attacks.wave.recoveryTime;
        case ActionStep::Recovery:
            return Remaining(config_.attacks.wave.recoveryTime, stateTimer_);
        default:
            return 0.0f;
        }

    case ActionKind::Warp:
        switch (action_.step) {
        case ActionStep::Start:
            return Remaining(config_.warp.startTime, stateTimer_) +
                   config_.warp.moveTime + config_.warp.endTime;
        case ActionStep::Move:
            return Remaining(config_.warp.moveTime, stateTimer_) +
                   config_.warp.endTime;
        case ActionStep::End:
            return Remaining(config_.warp.endTime, stateTimer_);
        default:
            return 0.0f;
        }

    case ActionKind::Stalk:
        return Remaining(stalkDurationMax_, stateTimer_);

    default:
        return 0.0f;
    }
}

float Enemy::GetCurrentSmashChargeTime() const {
    float result = config_.attacks.smash.melee.base.chargeTime;
    if (action_.id == ActionId::DelaySmash) {
        result += config_.attacks.smash.delayExtraChargeTime;
    }

    result += GetAdaptiveChargeOffset(ActionKind::Smash);
    if (result < 0.05f) {
        result = 0.05f;
    }
    return result;
}

float Enemy::GetCurrentSweepChargeTime() const {
    float result = config_.attacks.sweep.melee.base.chargeTime;
    if (action_.id == ActionId::DoubleSweep && isDoubleSweepSecondStage_) {
        result *= config_.attacks.sweep.secondChargeScale;
    }

    result += GetAdaptiveChargeOffset(ActionKind::Sweep);
    if (result < 0.05f) {
        result = 0.05f;
    }
    return result;
}

bool Enemy::HasReachedTrackingEnd() const {
    const AttackTimingParam *timing = GetCurrentAttackTiming();
    return timing ? GetCurrentActionTime() >= timing->trackingEndTime : false;
}

bool Enemy::HasReachedHitStart() const {
    const AttackTimingParam *timing = GetCurrentAttackTiming();
    return timing ? GetCurrentActionTime() >= timing->activeStartTime : false;
}

bool Enemy::HasReachedHitEnd() const {
    const AttackTimingParam *timing = GetCurrentAttackTiming();
    return timing ? GetCurrentActionTime() > timing->activeEndTime : false;
}

bool Enemy::HasReachedRecoveryStart() const {
    const AttackTimingParam *timing = GetCurrentAttackTiming();
    return timing ? GetCurrentActionTime() >= timing->recoveryStartTime : false;
}

ActionId Enemy::MakeDefaultActionId(ActionKind kind) const {
    switch (kind) {
    case ActionKind::Smash:
        return ActionId::Smash;
    case ActionKind::Sweep:
        return ActionId::Sweep;
    case ActionKind::Shot:
        return ActionId::Shot;
    case ActionKind::Wave:
        return ActionId::Wave;
    case ActionKind::Warp:
        return (warp_.type == WarpType::Escape) ? ActionId::WarpEscape
                                                : ActionId::WarpApproach;
    default:
        return ActionId::None;
    }
}

void Enemy::ValidateTiming(AttackTimingParam &timing, float chargeTime) {
    if (timing.totalTime < 0.0f) {
        timing.totalTime = 0.0f;
    }
    if (timing.trackingEndTime < 0.0f) {
        timing.trackingEndTime = 0.0f;
    }
    if (timing.trackingEndTime > chargeTime) {
        timing.trackingEndTime = chargeTime;
    }
    if (timing.activeStartTime < 0.0f) {
        timing.activeStartTime = 0.0f;
    }
    if (timing.activeEndTime < timing.activeStartTime) {
        timing.activeEndTime = timing.activeStartTime;
    }
    if (timing.recoveryStartTime < timing.activeEndTime) {
        timing.recoveryStartTime = timing.activeEndTime;
    }
    if (timing.totalTime < timing.recoveryStartTime) {
        timing.totalTime = timing.recoveryStartTime;
    }
}

void Enemy::ValidateAllTimings() {
    ValidateTiming(config_.attacks.smash.melee.base.timing,
                   config_.attacks.smash.melee.base.chargeTime);
    ValidateTiming(config_.attacks.sweep.melee.base.timing,
                   config_.attacks.sweep.melee.base.chargeTime);
}

void Enemy::ApplyAnimationTimingsFromModel(const Model *model) {
    const float warpDuration = ClipDuration(model, kBossAnimWarp);
    const float shotDuration = ClipDuration(model, kBossAnimShot);
    const float sweepRightDuration = ClipDuration(model, kBossAnimSweepRight);
    const float smashDuration = ClipDuration(model, kBossAnimSmash);
    const float sweepDuration = ClipDuration(model, kBossAnimSweep);
    const float waveDuration = ClipDuration(model, kBossAnimWave);
    const float phaseTransitionDuration =
        ClipDuration(model, kBossAnimPhaseTransition);

    FitMeleeTimingToClip(config_.attacks.smash.melee, smashDuration);
    FitMeleeTimingToClip(
        config_.attacks.sweep.melee,
        (sweepDuration > 0.001f) ? sweepDuration : sweepRightDuration);

    if (warpDuration > 0.001f) {
        config_.warp.startTime = (std::max)(0.05f, warpDuration * 0.30f);
        config_.warp.moveTime = (std::max)(0.05f, warpDuration * 0.22f);
        config_.warp.endTime = (std::max)(
            0.05f, warpDuration - config_.warp.startTime -
                       config_.warp.moveTime);
    }

    if (shotDuration > 0.001f) {
        const float shotFireTime =
            static_cast<float>((std::max)(1, config_.attacks.shot.maxCount)) *
            config_.attacks.shot.interval;
        config_.attacks.shot.chargeTime =
            (std::max)(0.10f, shotDuration * 0.32f);
        config_.attacks.shot.recoveryTime =
            (std::max)(0.10f, shotDuration -
                                   config_.attacks.shot.chargeTime -
                                   shotFireTime);
    }

    if (waveDuration > 0.001f) {
        const float waveChargeRatio = 0.36f;
        config_.attacks.wave.chargeTime =
            (std::max)(0.10f, waveDuration * waveChargeRatio);
        config_.attacks.wave.recoveryTime =
            (std::max)(0.10f, waveDuration - config_.attacks.wave.chargeTime);
    }

    if (phaseTransitionDuration > 0.001f) {
        phaseTransitionDuration_ = phaseTransitionDuration;
    }

    ValidateAllTimings();
}

EnemyTuningPreset Enemy::CreateTuningPreset() const {
    EnemyTuningPreset p{};

    p.enemyMaxHp = config_.core.maxHp;
    p.phase2HealthRatioThreshold = config_.core.phase2HealthRatioThreshold;
    p.nearAttackDistance = config_.core.nearAttackDistance;
    p.farAttackDistance = config_.core.farAttackDistance;

    p.smash.damage = config_.attacks.smash.melee.base.attack.damage;
    p.smash.knockback = config_.attacks.smash.melee.base.attack.knockback;
    p.smash.hitBoxSize = config_.attacks.smash.melee.base.attack.hitBoxSize;
    p.smashChargeTime = config_.attacks.smash.melee.base.chargeTime;
    p.smashAttackForwardOffset = config_.attacks.smash.attackForwardOffset;
    p.smashAttackHeightOffset = config_.attacks.smash.attackHeightOffset;
    p.smashTiming.totalTime = config_.attacks.smash.melee.base.timing.totalTime;
    p.smashTiming.trackingEndTime =
        config_.attacks.smash.melee.base.timing.trackingEndTime;
    p.smashTiming.activeStartTime =
        config_.attacks.smash.melee.base.timing.activeStartTime;
    p.smashTiming.activeEndTime =
        config_.attacks.smash.melee.base.timing.activeEndTime;
    p.smashTiming.recoveryStartTime =
        config_.attacks.smash.melee.base.timing.recoveryStartTime;

    p.sweep.damage = config_.attacks.sweep.melee.base.attack.damage;
    p.sweep.knockback = config_.attacks.sweep.melee.base.attack.knockback;
    p.sweep.hitBoxSize = config_.attacks.sweep.melee.base.attack.hitBoxSize;
    p.sweepChargeTime = config_.attacks.sweep.melee.base.chargeTime;
    p.sweepAttackSideOffset = config_.attacks.sweep.attackSideOffset;
    p.sweepAttackHeightOffset = config_.attacks.sweep.attackHeightOffset;
    p.sweepTiming.totalTime = config_.attacks.sweep.melee.base.timing.totalTime;
    p.sweepTiming.trackingEndTime =
        config_.attacks.sweep.melee.base.timing.trackingEndTime;
    p.sweepTiming.activeStartTime =
        config_.attacks.sweep.melee.base.timing.activeStartTime;
    p.sweepTiming.activeEndTime =
        config_.attacks.sweep.melee.base.timing.activeEndTime;
    p.sweepTiming.recoveryStartTime =
        config_.attacks.sweep.melee.base.timing.recoveryStartTime;

    p.bullet.damage = config_.attacks.shot.attack.damage;
    p.bullet.knockback = config_.attacks.shot.attack.knockback;
    p.bullet.hitBoxSize = config_.attacks.shot.attack.hitBoxSize;
    p.shotChargeTime = config_.attacks.shot.chargeTime;
    p.shotRecoveryTime = config_.attacks.shot.recoveryTime;
    p.shotInterval = config_.attacks.shot.interval;
    p.shotMinCount = config_.attacks.shot.minCount;
    p.shotMaxCount = config_.attacks.shot.maxCount;
    p.bulletSpeed = config_.attacks.shot.bulletSpeed;
    p.bulletLifeTime = config_.attacks.shot.bulletLifeTime;
    p.bulletSpawnHeightOffset = config_.attacks.shot.spawnHeightOffset;

    p.wave.damage = config_.attacks.wave.attack.damage;
    p.wave.knockback = config_.attacks.wave.attack.knockback;
    p.wave.hitBoxSize = config_.attacks.wave.attack.hitBoxSize;
    p.waveChargeTime = config_.attacks.wave.chargeTime;
    p.waveRecoveryTime = config_.attacks.wave.recoveryTime;
    p.waveSpeed = config_.attacks.wave.speed;
    p.waveMaxDistance = config_.attacks.wave.maxDistance;
    p.waveSpawnForwardOffset = config_.attacks.wave.spawnForwardOffset;
    p.waveSpawnHeightOffset = config_.attacks.wave.spawnHeightOffset;

    p.warpStartTime = config_.warp.startTime;
    p.warpMoveTime = config_.warp.moveTime;
    p.warpEndTime = config_.warp.endTime;
    p.warpApproachChainMaxSteps = config_.chain.warpApproachMaxSteps;
    p.warpEscapeChainMaxSteps = config_.chain.warpEscapeMaxSteps;
    p.approachChainContinueDistance = config_.chain.approachContinueDistance;
    p.escapeChainContinueDistance = config_.chain.escapeContinueDistance;
    p.sweepWarpSmashMaxDistance = config_.chain.sweepWarpSmashMaxDistance;
    p.sweepWarpSmashChance = config_.chain.sweepWarpSmashChance;
    p.waveWarpSmashMinDistance = config_.chain.waveWarpSmashMinDistance;
    p.waveWarpSmashChance = config_.chain.waveWarpSmashChance;

    p.movement.hitReactionMoveSpeed = hitReactionMoveSpeed_;
    p.movement.chargeTurnSpeed = chargeTurnSpeed_;
    p.movement.recoveryTurnSpeed = recoveryTurnSpeed_;
    p.movement.idleTurnSpeed = idleTurnSpeed_;
    p.movement.stagnantDistanceThreshold = stagnantDistanceThreshold_;
    p.movement.stagnantTimeThreshold = stagnantTimeThreshold_;
    p.movement.stagnantWarpBonus = stagnantWarpBonus_;
    p.movement.closePressureDistance = closePressureDistance_;
    p.movement.closePressureTimeThreshold = closePressureTimeThreshold_;
    p.movement.farDistanceWarpTimeThreshold = farDistanceWarpTimeThreshold_;
    p.movement.farDistanceWarpBonus = farDistanceWarpBonus_;
    p.movement.warpNearRadiusMin = warpNearRadiusMin_;
    p.movement.warpNearRadiusMax = warpNearRadiusMax_;
    p.movement.warpApproachForwardDistance = warpApproachForwardDistance_;
    p.movement.warpApproachSideDistance = warpApproachSideDistance_;
    p.movement.warpApproachLongFrontDistance = warpApproachLongFrontDistance_;
    p.movement.stalkDurationMin = stalkDurationMin_;
    p.movement.stalkDurationMax = stalkDurationMax_;
    p.movement.stalkMoveSpeed = stalkMoveSpeed_;
    p.movement.stalkStrafeRadiusWeight = stalkStrafeRadiusWeight_;
    p.movement.stalkForwardAdjustWeight = stalkForwardAdjustWeight_;
    p.movement.stalkNearEnterChance = stalkNearEnterChance_;
    p.movement.stalkMidEnterChance = stalkMidEnterChance_;
    p.movement.stalkRepeatLimit = stalkRepeatLimit_;

    return p;
}

void Enemy::ApplyTuningPreset(const EnemyTuningPreset &p) {
    config_.core.maxHp = (p.enemyMaxHp < 1.0f) ? 1.0f : p.enemyMaxHp;
    hp_ = config_.core.maxHp;

    config_.core.phase2HealthRatioThreshold = p.phase2HealthRatioThreshold;
    if (config_.core.phase2HealthRatioThreshold < 0.05f) {
        config_.core.phase2HealthRatioThreshold = 0.05f;
    }
    if (config_.core.phase2HealthRatioThreshold > 0.95f) {
        config_.core.phase2HealthRatioThreshold = 0.95f;
    }

    config_.core.nearAttackDistance = p.nearAttackDistance;
    config_.core.farAttackDistance = p.farAttackDistance;

    config_.attacks.smash.melee.base.attack.damage = p.smash.damage;
    config_.attacks.smash.melee.base.attack.knockback = p.smash.knockback;
    config_.attacks.smash.melee.base.attack.hitBoxSize = p.smash.hitBoxSize;
    config_.attacks.smash.melee.base.chargeTime = p.smashChargeTime;
    config_.attacks.smash.attackForwardOffset = p.smashAttackForwardOffset;
    config_.attacks.smash.attackHeightOffset = p.smashAttackHeightOffset;
    config_.attacks.smash.melee.base.timing.totalTime = p.smashTiming.totalTime;
    config_.attacks.smash.melee.base.timing.activeStartTime =
        p.smashTiming.activeStartTime;
    config_.attacks.smash.melee.base.timing.activeEndTime =
        p.smashTiming.activeEndTime;
    config_.attacks.smash.melee.base.timing.recoveryStartTime =
        p.smashTiming.recoveryStartTime;
    config_.attacks.smash.melee.base.timing.trackingEndTime =
        p.smashTiming.trackingEndTime;

    config_.attacks.sweep.melee.base.attack.damage = p.sweep.damage;
    config_.attacks.sweep.melee.base.attack.knockback = p.sweep.knockback;
    config_.attacks.sweep.melee.base.attack.hitBoxSize = p.sweep.hitBoxSize;
    config_.attacks.sweep.melee.base.chargeTime = p.sweepChargeTime;
    config_.attacks.sweep.attackSideOffset = p.sweepAttackSideOffset;
    config_.attacks.sweep.attackHeightOffset = p.sweepAttackHeightOffset;
    config_.attacks.sweep.melee.base.timing.totalTime = p.sweepTiming.totalTime;
    config_.attacks.sweep.melee.base.timing.activeStartTime =
        p.sweepTiming.activeStartTime;
    config_.attacks.sweep.melee.base.timing.activeEndTime =
        p.sweepTiming.activeEndTime;
    config_.attacks.sweep.melee.base.timing.recoveryStartTime =
        p.sweepTiming.recoveryStartTime;
    config_.attacks.sweep.melee.base.timing.trackingEndTime =
        p.sweepTiming.trackingEndTime;

    config_.attacks.shot.attack.damage = p.bullet.damage;
    config_.attacks.shot.attack.knockback = p.bullet.knockback;
    config_.attacks.shot.attack.hitBoxSize = p.bullet.hitBoxSize;
    config_.attacks.shot.chargeTime = p.shotChargeTime;
    config_.attacks.shot.recoveryTime = p.shotRecoveryTime;
    config_.attacks.shot.interval = p.shotInterval;
    config_.attacks.shot.minCount = p.shotMinCount;
    config_.attacks.shot.maxCount = p.shotMaxCount;
    config_.attacks.shot.bulletSpeed = p.bulletSpeed;
    config_.attacks.shot.bulletLifeTime = p.bulletLifeTime;
    config_.attacks.shot.spawnHeightOffset = p.bulletSpawnHeightOffset;

    config_.attacks.wave.attack.damage = p.wave.damage;
    config_.attacks.wave.attack.knockback = p.wave.knockback;
    config_.attacks.wave.attack.hitBoxSize = p.wave.hitBoxSize;
    config_.attacks.wave.chargeTime = p.waveChargeTime;
    config_.attacks.wave.recoveryTime = p.waveRecoveryTime;
    config_.attacks.wave.speed = p.waveSpeed;
    config_.attacks.wave.maxDistance = p.waveMaxDistance;
    config_.attacks.wave.spawnForwardOffset = p.waveSpawnForwardOffset;
    config_.attacks.wave.spawnHeightOffset = p.waveSpawnHeightOffset;

    config_.warp.startTime = (p.warpStartTime < 0.0f) ? 0.0f : p.warpStartTime;
    config_.warp.moveTime = (p.warpMoveTime < 0.0f) ? 0.0f : p.warpMoveTime;
    config_.warp.endTime = (p.warpEndTime < 0.0f) ? 0.0f : p.warpEndTime;

    config_.chain.warpApproachMaxSteps = p.warpApproachChainMaxSteps;
    config_.chain.warpEscapeMaxSteps = p.warpEscapeChainMaxSteps;
    config_.chain.approachContinueDistance = p.approachChainContinueDistance;
    config_.chain.escapeContinueDistance = p.escapeChainContinueDistance;
    config_.chain.sweepWarpSmashMaxDistance = p.sweepWarpSmashMaxDistance;
    config_.chain.sweepWarpSmashChance = p.sweepWarpSmashChance;
    config_.chain.waveWarpSmashMinDistance = p.waveWarpSmashMinDistance;
    config_.chain.waveWarpSmashChance = p.waveWarpSmashChance;

    hitReactionMoveSpeed_ =
        (p.movement.hitReactionMoveSpeed < 0.0f) ? 0.0f
                                                 : p.movement.hitReactionMoveSpeed;
    chargeTurnSpeed_ =
        (p.movement.chargeTurnSpeed < 0.0f) ? 0.0f : p.movement.chargeTurnSpeed;
    recoveryTurnSpeed_ = (p.movement.recoveryTurnSpeed < 0.0f)
                             ? 0.0f
                             : p.movement.recoveryTurnSpeed;
    idleTurnSpeed_ =
        (p.movement.idleTurnSpeed < 0.0f) ? 0.0f : p.movement.idleTurnSpeed;

    stagnantDistanceThreshold_ = (p.movement.stagnantDistanceThreshold < 0.0f)
                                     ? 0.0f
                                     : p.movement.stagnantDistanceThreshold;
    stagnantTimeThreshold_ = (p.movement.stagnantTimeThreshold < 0.0f)
                                 ? 0.0f
                                 : p.movement.stagnantTimeThreshold;
    stagnantWarpBonus_ =
        (p.movement.stagnantWarpBonus < 0) ? 0 : p.movement.stagnantWarpBonus;

    closePressureDistance_ = (p.movement.closePressureDistance < 0.0f)
                                 ? 0.0f
                                 : p.movement.closePressureDistance;
    closePressureTimeThreshold_ = (p.movement.closePressureTimeThreshold < 0.0f)
                                      ? 0.0f
                                      : p.movement.closePressureTimeThreshold;

    farDistanceWarpTimeThreshold_ =
        (p.movement.farDistanceWarpTimeThreshold < 0.0f)
            ? 0.0f
            : p.movement.farDistanceWarpTimeThreshold;
    farDistanceWarpBonus_ = (p.movement.farDistanceWarpBonus < 0)
                                ? 0
                                : p.movement.farDistanceWarpBonus;

    warpNearRadiusMin_ = (p.movement.warpNearRadiusMin < 0.0f)
                             ? 0.0f
                             : p.movement.warpNearRadiusMin;
    warpNearRadiusMax_ = (p.movement.warpNearRadiusMax < warpNearRadiusMin_)
                             ? warpNearRadiusMin_
                             : p.movement.warpNearRadiusMax;
    warpApproachForwardDistance_ =
        (p.movement.warpApproachForwardDistance < 0.0f)
            ? 0.0f
            : p.movement.warpApproachForwardDistance;
    warpApproachSideDistance_ = (p.movement.warpApproachSideDistance < 0.0f)
                                    ? 0.0f
                                    : p.movement.warpApproachSideDistance;
    warpApproachLongFrontDistance_ =
        (p.movement.warpApproachLongFrontDistance < 0.0f)
            ? 0.0f
            : p.movement.warpApproachLongFrontDistance;

    stalkDurationMin_ = (p.movement.stalkDurationMin < 0.0f)
                            ? 0.0f
                            : p.movement.stalkDurationMin;
    stalkDurationMax_ = (p.movement.stalkDurationMax < stalkDurationMin_)
                            ? stalkDurationMin_
                            : p.movement.stalkDurationMax;
    stalkMoveSpeed_ =
        (p.movement.stalkMoveSpeed < 0.0f) ? 0.0f : p.movement.stalkMoveSpeed;
    stalkStrafeRadiusWeight_ = p.movement.stalkStrafeRadiusWeight;
    stalkForwardAdjustWeight_ = p.movement.stalkForwardAdjustWeight;
    stalkNearEnterChance_ = p.movement.stalkNearEnterChance;
    stalkMidEnterChance_ = p.movement.stalkMidEnterChance;
    stalkRepeatLimit_ =
        (p.movement.stalkRepeatLimit < 0) ? 0 : p.movement.stalkRepeatLimit;

    if (config_.core.nearAttackDistance > config_.core.farAttackDistance) {
        config_.core.farAttackDistance = config_.core.nearAttackDistance;
    }

    ValidateAllTimings();
}

void Enemy::ResetTuningPreset() { ApplyTuningPreset(EnemyTuningPreset{}); }
