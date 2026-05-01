#include "BattleEffectDirector.h"
#include "BillboardRenderer.h"
#include "Camera.h"
#include "EffectSystem.h"
#include "PostEffectRenderer.h"
#include "SceneContext.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace {
float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float EaseOutCubic(float t) {
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}
} // namespace

void BattleEffectDirector::Reset() { enemySlashActiveLatched_ = false; }

void BattleEffectDirector::BeginFrame(const SceneContext &ctx) {
    if (ctx.effects != nullptr) {
        ctx.effects->BeginFrame(ctx.deltaTime);
    }
}

void BattleEffectDirector::Update(const SceneContext &ctx, Enemy &enemy,
                                  const Camera &camera) {
    UpdateEnemySlashEffects(ctx, enemy);
    UpdateEnemySwordTrail(ctx, enemy);

    enemy.UpdatePresentationEvents();
    auto requests = enemy.ConsumeElectricRingSpawnRequests();
    for (const auto &req : requests) {
        SpawnElectricRing(ctx, req.worldPos, req.isWarpEnd);
    }

    UpdateElectricRing(ctx, camera);
}

void BattleEffectDirector::Draw(const SceneContext &ctx, const Enemy &enemy,
                                const Camera &camera) {
    if (ctx.effects != nullptr) {
        ctx.effects->Draw(camera);
    }

    DrawWarpSmokePass(ctx, enemy, camera);
    DrawWarpDistortionPass(ctx, enemy, camera);
}

bool BattleEffectDirector::ProjectWorldToScreen(
    const SceneContext &ctx, const Camera &camera, const XMFLOAT3 &worldPos,
    XMFLOAT2 &outScreen) const {
    if (ctx.winApp == nullptr) {
        return false;
    }

    XMMATRIX viewProj = camera.GetView() * camera.GetProj();
    XMVECTOR pos = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(pos, viewProj);
    float w = XMVectorGetW(clip);
    if (w <= 0.0001f) {
        return false;
    }

    float invW = 1.0f / w;
    float ndcX = XMVectorGetX(clip) * invW;
    float ndcY = XMVectorGetY(clip) * invW;
    float ndcZ = XMVectorGetZ(clip) * invW;
    if (ndcZ < 0.0f || ndcZ > 1.0f) {
        return false;
    }

    float width = static_cast<float>(ctx.winApp->GetWidth());
    float height = static_cast<float>(ctx.winApp->GetHeight());
    outScreen.x = (ndcX * 0.5f + 0.5f) * width;
    outScreen.y = (-ndcY * 0.5f + 0.5f) * height;
    return true;
}

bool BattleEffectDirector::ComputeEnemySlashWorldEffect(
    const Enemy &enemy, XMFLOAT3 &outStart, XMFLOAT3 &outEnd,
    float &outPhaseAlpha, float &outActionTime,
    ActionKind &outActionKind) const {
    outStart = {};
    outEnd = {};
    outPhaseAlpha = 0.0f;
    outActionTime = 0.0f;
    outActionKind = ActionKind::None;

    const ActionKind actionKind = enemy.GetActionKind();
    const ActionStep actionStep = enemy.GetActionStep();
    const bool isEnemySlashAction =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold ||
         actionStep == ActionStep::Active ||
         actionStep == ActionStep::Recovery);

    if (!isEnemySlashAction) {
        return false;
    }

    XMFLOAT3 slashStartWorld = enemy.GetRightHandTransform().position;
    XMFLOAT3 slashEndWorld = enemy.GetAttackOBB().center;

    const float yaw =
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold)
            ? enemy.GetFacingYaw()
            : enemy.GetLockedAttackYaw();

    const float forwardX = std::sinf(yaw);
    const float forwardZ = std::cosf(yaw);
    const float rightX = std::cosf(yaw);
    const float rightZ = -std::sinf(yaw);

    if (actionKind == ActionKind::Smash) {
        slashStartWorld.x += -forwardX * 0.35f;
        slashStartWorld.y += 0.95f;
        slashStartWorld.z += -forwardZ * 0.35f;

        slashEndWorld.x += forwardX * 0.55f;
        slashEndWorld.y -= 0.55f;
        slashEndWorld.z += forwardZ * 0.55f;
    } else {
        const XMFLOAT3 attackCenter = enemy.GetAttackOBB().center;
        const float sweepHalfWidth = enemy.GetSweepAttackBoxSize().x * 0.55f;

        slashStartWorld = {attackCenter.x - rightX * sweepHalfWidth,
                           attackCenter.y + 0.28f,
                           attackCenter.z - rightZ * sweepHalfWidth};

        slashEndWorld = {attackCenter.x + rightX * sweepHalfWidth,
                         attackCenter.y - 0.18f,
                         attackCenter.z + rightZ * sweepHalfWidth};
    }

    const AttackTimingParam *timing = enemy.GetCurrentAttackTimingPublic();
    const float actionTime = enemy.GetCurrentActionTimePublic();

    float phaseAlpha = 0.0f;
    if (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold) {
        const float previewT =
            (timing != nullptr && timing->activeStartTime > 0.0001f)
                ? actionTime / timing->activeStartTime
                : 1.0f;
        phaseAlpha = enemySlashChargePreviewAlpha_ *
                     (0.35f + 0.65f * EaseOutCubic(previewT));
    } else if (actionStep == ActionStep::Active) {
        float activeT = 0.0f;
        if (timing != nullptr &&
            timing->activeEndTime > timing->activeStartTime) {
            activeT = (actionTime - timing->activeStartTime) /
                      (timing->activeEndTime - timing->activeStartTime);
        }
        phaseAlpha = enemySlashActiveAlpha_ * (1.0f - 0.22f * Clamp01(activeT));
    } else {
        float recoveryT = 0.0f;
        if (timing != nullptr &&
            timing->totalTime > timing->recoveryStartTime) {
            recoveryT = (actionTime - timing->recoveryStartTime) /
                        (timing->totalTime - timing->recoveryStartTime);
        }
        phaseAlpha =
            enemySlashRecoveryAlpha_ * (1.0f - EaseOutCubic(recoveryT));
    }

    if (phaseAlpha <= 0.01f) {
        return false;
    }

    outStart = slashStartWorld;
    outEnd = slashEndWorld;
    outPhaseAlpha = phaseAlpha;
    outActionTime = actionTime;
    outActionKind = actionKind;
    return true;
}

void BattleEffectDirector::UpdateEnemySlashEffects(const SceneContext &ctx,
                                                   const Enemy &enemy) {
    const ActionKind actionKind = enemy.GetActionKind();
    const ActionStep actionStep = enemy.GetActionStep();

    XMFLOAT3 slashStartWorld{};
    XMFLOAT3 slashEndWorld{};
    float phaseAlpha = 0.0f;
    float actionTime = 0.0f;
    ActionKind worldActionKind = ActionKind::None;
    const bool hasSlashArc =
        ComputeEnemySlashWorldEffect(enemy, slashStartWorld, slashEndWorld,
                                     phaseAlpha, actionTime, worldActionKind);
    if (hasSlashArc && ctx.effects != nullptr) {
        EffectRequest request{};
        request.preset = EffectPreset::SlashArc;
        request.start = slashStartWorld;
        request.end = slashEndWorld;
        request.alpha = phaseAlpha;
        request.time = actionTime;
        request.duration = (std::max)(ctx.deltaTime * 2.0f, 0.034f);
        request.sweep = (worldActionKind == ActionKind::Sweep);
        ctx.effects->Play(request);
    }

    const bool isSlashActive =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Active);

    if (isSlashActive && !enemySlashActiveLatched_ && hasSlashArc &&
        ctx.effects != nullptr) {
        XMFLOAT3 effectPosition{(slashStartWorld.x + slashEndWorld.x) * 0.5f,
                                (slashStartWorld.y + slashEndWorld.y) * 0.5f,
                                (slashStartWorld.z + slashEndWorld.z) * 0.5f};
        ctx.effects->EmitParticles(EffectParticlePreset::ChargeSpark,
                                   effectPosition);

        const bool isSweep = (worldActionKind == ActionKind::Sweep);
        const uint32_t particleCount = isSweep ? enemySlashParticleCountSweep_
                                               : enemySlashParticleCountSmash_;
        ctx.effects->EmitArcSparks(slashStartWorld, slashEndWorld,
                                   particleCount,
                                   enemySlashParticleEmitScale_, isSweep);
    }

    enemySlashActiveLatched_ = isSlashActive;
}

void BattleEffectDirector::UpdateEnemySwordTrail(const SceneContext &ctx,
                                                 const Enemy &enemy) {
    if (ctx.effects == nullptr) {
        return;
    }

    ctx.effects->SetTrailEnabled(enemySwordTrailEnabled_);

    const ActionKind actionKind = enemy.GetActionKind();
    const ActionStep actionStep = enemy.GetActionStep();

    const bool isTrailAction =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Active);

    if (!isTrailAction) {
        return;
    }

    XMFLOAT3 baseWorld = enemy.GetRightHandTransform().position;
    XMFLOAT3 tipWorld = enemy.GetAttackOBB().center;

    const float yaw = (actionStep == ActionStep::Charge)
                          ? enemy.GetFacingYaw()
                          : enemy.GetLockedAttackYaw();

    const float forwardX = std::sinf(yaw);
    const float forwardZ = std::cosf(yaw);
    const float rightX = std::cosf(yaw);
    const float rightZ = -std::sinf(yaw);

    if (actionKind == ActionKind::Smash) {
        baseWorld.x += -forwardX * 0.18f;
        baseWorld.y += 0.70f;
        baseWorld.z += -forwardZ * 0.18f;

        tipWorld.x += forwardX * 0.42f;
        tipWorld.y -= 0.38f;
        tipWorld.z += forwardZ * 0.42f;
    } else {
        const XMFLOAT3 attackCenter = enemy.GetAttackOBB().center;
        const float sweepHalfWidth = enemy.GetSweepAttackBoxSize().x * 0.52f;

        baseWorld = {attackCenter.x - rightX * sweepHalfWidth,
                     attackCenter.y + 0.16f,
                     attackCenter.z - rightZ * sweepHalfWidth};

        tipWorld = {attackCenter.x + rightX * sweepHalfWidth,
                    attackCenter.y - 0.10f,
                    attackCenter.z + rightZ * sweepHalfWidth};
    }

    const float width = (actionKind == ActionKind::Sweep)
                            ? enemySwordTrailWidth_ * 1.10f
                            : enemySwordTrailWidth_ * 0.82f;

    ctx.effects->EmitTrail(TrailRenderer::TrailPreset::SwordSlash, baseWorld,
                           tipWorld, width);
}

void BattleEffectDirector::DrawWarpSmokePass(const SceneContext &ctx,
                                             const Enemy &enemy,
                                             const Camera &camera) {
    if (ctx.renderer.billboard == nullptr ||
        enemy.GetActionKind() != ActionKind::Warp) {
        return;
    }

    ActionStep warpStep = enemy.GetActionStep();
    float stepAlpha = 0.0f;
    switch (warpStep) {
    case ActionStep::Start:
        stepAlpha = 0.62f;
        break;
    case ActionStep::Move:
        stepAlpha = 0.78f;
        break;
    case ActionStep::End:
        stepAlpha = 0.92f;
        break;
    default:
        return;
    }

    const XMFLOAT3 targetWorld = enemy.GetWarpTargetPos();
    XMFLOAT3 sourceWorld{};
    const bool hasSource = enemy.HasWarpDeparturePos();
    if (hasSource) {
        sourceWorld = enemy.GetWarpDeparturePos();
    }

    XMMATRIX billboard = camera.GetView();
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    billboard = XMMatrixInverse(nullptr, billboard);
    XMFLOAT3 cameraRight{};
    XMFLOAT3 cameraUp{};
    XMStoreFloat3(&cameraRight, billboard.r[0]);
    XMStoreFloat3(&cameraUp, billboard.r[1]);

    auto addOffset = [](const XMFLOAT3 &origin, const XMFLOAT3 &axis,
                        float amount) {
        return XMFLOAT3{origin.x + axis.x * amount, origin.y + axis.y * amount,
                        origin.z + axis.z * amount};
    };

    ctx.renderer.billboard->PreDraw(camera);

    auto drawSmoke = [&](const XMFLOAT3 &center, float scale, float alpha,
                         bool arrival) {
        const float time = enemy.GetCurrentActionTimePublic();
        const int blobs = arrival ? 9 : 6;
        for (int i = 0; i < blobs; ++i) {
            const float r = static_cast<float>(i) / static_cast<float>(blobs);
            const float angle = time * (2.0f + r) + r * XM_2PI * 1.7f;
            const float drift = (arrival ? 0.56f : 0.34f) * scale * (0.25f + r);
            XMFLOAT3 pos =
                addOffset(center, cameraRight, std::cosf(angle) * drift);
            pos = addOffset(pos, cameraUp,
                            -0.34f * scale +
                                std::sinf(angle * 1.25f) * drift * 0.45f);
            const float radius =
                (arrival ? 0.72f : 0.48f) * scale * (0.65f + 0.55f * r);
            const float a = std::clamp(alpha * (1.0f - r * 0.18f), 0.0f, 1.0f);
            const XMFLOAT4 dark{18.0f / 255.0f, 6.0f / 255.0f, 12.0f / 255.0f,
                                a * (115.0f / 255.0f)};
            const XMFLOAT4 red{180.0f / 255.0f, 18.0f / 255.0f, 38.0f / 255.0f,
                               a * (115.0f / 255.0f) * 0.48f};
            ctx.renderer.billboard->DrawDisc(pos, radius, dark, angle * 0.35f);
            if ((i % 2) == 0) {
                ctx.renderer.billboard->DrawDisc(pos, radius * 0.72f, red,
                                                 angle * -0.25f);
            }
        }
    };

    if (hasSource &&
        (warpStep == ActionStep::Start || warpStep == ActionStep::Move)) {
        XMFLOAT3 sourceCenter = sourceWorld;
        sourceCenter.y += 1.05f;
        drawSmoke(sourceCenter, warpSourceSmokeBloomScale_, stepAlpha, false);
    }
    XMFLOAT3 arrival = targetWorld;
    arrival.y += (warpStep == ActionStep::End) ? 0.85f : 1.25f;
    drawSmoke(arrival, warpArrivalSmokeDenseScale_, stepAlpha, true);

    ctx.renderer.billboard->PostDraw();
}

void BattleEffectDirector::DrawWarpDistortionPass(const SceneContext &ctx,
                                                  const Enemy &enemy,
                                                  const Camera &camera) {
    if (ctx.renderer.postEffectRenderer == nullptr ||
        enemy.GetActionKind() != ActionKind::Warp) {
        return;
    }

    ActionStep warpStep = enemy.GetActionStep();
    float stepIntensity = 0.0f;
    DistortionPhase phase = DistortionPhase::Start;
    switch (warpStep) {
    case ActionStep::Start:
        stepIntensity = 0.72f;
        phase = DistortionPhase::Start;
        break;
    case ActionStep::Move:
        stepIntensity = 1.0f;
        phase = DistortionPhase::Sustain;
        break;
    case ActionStep::End:
        stepIntensity = 0.84f;
        phase = DistortionPhase::End;
        break;
    default:
        return;
    }

    XMFLOAT2 targetScreenF{};
    bool hasTarget =
        ProjectWorldToScreen(ctx, camera, enemy.GetWarpTargetPos(), targetScreenF);

    XMFLOAT2 sourceScreenF{};
    bool hasSource =
        enemy.HasWarpDeparturePos() &&
        ProjectWorldToScreen(ctx, camera, enemy.GetWarpDeparturePos(),
                             sourceScreenF);
    if (!hasSource && !hasTarget) {
        return;
    }

    float time = enemy.GetCurrentActionTimePublic();
    float baseRadius =
        warpDistortionRadiusPx_ * (0.85f + 0.30f * stepIntensity);
    float jitter =
        warpDistortionJitterPx_ * (0.80f + 0.40f * std::sinf(time * 20.0f));
    float alpha =
        warpDistortionAlpha_ + warpDistortionMoveAlphaBonus_ *
                                   (warpStep == ActionStep::Move ? 1.0f : 0.0f);

    DistortionEffectParams params{};
    params.preset = DistortionPreset::WarpPortal;
    params.hasOrigin = hasSource;
    params.hasTarget = hasTarget;
    params.phase = phase;
    params.originScreen = sourceScreenF;
    params.targetScreen = targetScreenF;
    params.time = time;
    params.intensity = stepIntensity;
    params.baseRadius = baseRadius;
    params.jitter = jitter;
    params.alpha = alpha;
    params.thickness = warpDistortionThicknessPx_;
    params.lineLength = warpDistortionLineLengthPx_;
    params.footOffset = warpDistortionFootOffsetPx_;
    params.previewOffset = warpDistortionPreviewOffsetPx_;
    ctx.renderer.postEffectRenderer->Request(
        PostEffectRenderer::PostEffectType::Distortion, params);
}

void BattleEffectDirector::SpawnElectricRing(const SceneContext &ctx,
                                             const XMFLOAT3 &worldPos,
                                             bool isWarpEnd) {
    if (ctx.renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx.renderer.postEffectRenderer->Request(
        isWarpEnd ? PostEffectRenderer::PostEffectType::WarpRingEnd
                  : PostEffectRenderer::PostEffectType::WarpRingStart,
        worldPos);
}

void BattleEffectDirector::UpdateElectricRing(const SceneContext &ctx,
                                              const Camera &camera) {
    if (ctx.renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx.renderer.postEffectRenderer->UpdateScreenEffects(
        ctx.deltaTime, camera, ctx.frame.width, ctx.frame.height);
}
