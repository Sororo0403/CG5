#include "Collider.h"
#include "DirectXCommon.h"
#include "EcsScene.h"
#include "Input.h"
#include "InputComponent.h"
#include "MovementSystem.h"
#include "SceneManager.h"
#include "SpriteRenderer.h"
#include "SpriteServices.h"
#include "SrvManager.h"
#include "Tags.h"
#include "TextureManager.h"
#include "Transform.h"
#include "Velocity.h"
#include "WinApp.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <vector>

namespace {

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;
constexpr float kWorldWidth = 2480.0f;

struct RectSize {
    DirectX::XMFLOAT2 value{0.0f, 0.0f};
};

struct PreviousPosition {
    DirectX::XMFLOAT3 value{0.0f, 0.0f, 0.0f};
};

struct PlayerState {
    bool onGround = false;
    bool facingRight = true;
    bool won = false;
};

struct JumpInput {
    bool triggered = false;
};

struct PlatformTag {};
struct CoinTag {};
struct GoalTag {};

struct EnemyPatrol {
    float leftLimit = 0.0f;
    float rightLimit = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

float CalculateDeltaTime(std::chrono::steady_clock::time_point &previous) {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<float> delta = now - previous;
    previous = now;
    return delta.count();
}

void ShowErrorMessage(const char *message) {
    MessageBoxA(nullptr, message, "EngineTestApp Error", MB_OK | MB_ICONERROR);
}

Rect MakeRect(const Transform &transform, const RectSize &size) {
    return {transform.position.x, transform.position.y, size.value.x,
            size.value.y};
}

bool Intersects(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

void DrawScreenRect(SpriteRenderer &renderer, const Rect &rect,
                    const DirectX::XMFLOAT4 &color) {
    Sprite sprite{};
    sprite.position = {rect.x, rect.y};
    sprite.size = {rect.w, rect.h};
    sprite.color = color;
    renderer.Draw(sprite);
}

void DrawWorldRect(SpriteRenderer &renderer, const Rect &rect,
                   const DirectX::XMFLOAT4 &color, float cameraX) {
    DrawScreenRect(renderer, {rect.x - cameraX, rect.y, rect.w, rect.h}, color);
}

class MiniMarioScene : public EcsScene {
  public:
    void OnResize(int width, int height) override {
        if (ctx_ && ctx_->assets.spriteServices) {
            ctx_->assets.spriteServices->Resize(width, height);
        }
    }

  private:
    void OnInitialize() override {
        ResetWorld();

        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                UpdateInput(world);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                UpdateEnemyPatrol(world);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                ApplyGravity(world);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                StorePreviousPositions(world);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                movementSystem_.Update(world, DeltaTime());
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                ResolvePlatformContacts(world);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &commandBuffer) {
                CollectCoins(world, commandBuffer);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &commandBuffer) {
                ResolveEnemyContacts(world, commandBuffer);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &commandBuffer) {
                CheckGoalAndFall(world, commandBuffer);
            });
        pipeline_.AddWorldUpdateStep(
            [this](World &world, WorldCommandBuffer &) {
                UpdateCamera(world);
            });
        pipeline_.AddDrawStep([this]() { DrawScene(); });
    }

    float DeltaTime() const {
        return (std::min)(ctx_ ? ctx_->frame.deltaTime : 0.0f, 1.0f / 30.0f);
    }

    void ResetWorld() {
        const std::vector<Entity> alive = world_.AliveEntities();
        for (Entity entity : alive) {
            world_.DestroyEntity(entity);
        }

        cameraX_ = 0.0f;
        resetRequested_ = false;
        CreateLevel();
    }

    Entity CreateRectEntity(float x, float y, float w, float h) {
        Entity entity = world_.CreateEntity();
        world_.Add<Transform>(entity, Transform{{x, y, 0.0f}});
        world_.Add<RectSize>(entity, RectSize{{w, h}});
        world_.Add<PreviousPosition>(entity, PreviousPosition{{x, y, 0.0f}});
        world_.Add<Collider>(entity,
                             Collider{ColliderShape::AABB,
                                      {w * 0.5f, h * 0.5f, 0.0f},
                                      {w * 0.5f, h * 0.5f, 0.5f}});
        return entity;
    }

    void CreateLevel() {
        playerEntity_ = CreateRectEntity(96.0f, 484.0f, 36.0f, 52.0f);
        world_.Add<PlayerTag>(playerEntity_);
        world_.Add<PlayerState>(playerEntity_);
        world_.Add<InputComponent>(playerEntity_);
        world_.Add<JumpInput>(playerEntity_);
        world_.Add<Velocity>(playerEntity_);

        const std::array<Rect, 13> platforms{
            Rect{0.0f, 536.0f, 620.0f, 96.0f},
            Rect{690.0f, 536.0f, 300.0f, 96.0f},
            Rect{1080.0f, 536.0f, 500.0f, 96.0f},
            Rect{1680.0f, 536.0f, 800.0f, 96.0f},
            Rect{360.0f, 430.0f, 120.0f, 28.0f},
            Rect{560.0f, 365.0f, 132.0f, 28.0f},
            Rect{875.0f, 420.0f, 120.0f, 28.0f},
            Rect{1020.0f, 330.0f, 150.0f, 28.0f},
            Rect{1240.0f, 455.0f, 122.0f, 28.0f},
            Rect{1450.0f, 382.0f, 138.0f, 28.0f},
            Rect{1625.0f, 322.0f, 132.0f, 28.0f},
            Rect{1870.0f, 366.0f, 130.0f, 28.0f},
            Rect{2090.0f, 296.0f, 120.0f, 28.0f}};
        for (const Rect &platform : platforms) {
            Entity entity =
                CreateRectEntity(platform.x, platform.y, platform.w, platform.h);
            world_.Add<PlatformTag>(entity);
        }

        const std::array<DirectX::XMFLOAT2, 7> coins{
            DirectX::XMFLOAT2{300.0f, 395.0f},
            DirectX::XMFLOAT2{460.0f, 315.0f},
            DirectX::XMFLOAT2{720.0f, 340.0f},
            DirectX::XMFLOAT2{1035.0f, 275.0f},
            DirectX::XMFLOAT2{1260.0f, 390.0f},
            DirectX::XMFLOAT2{1580.0f, 325.0f},
            DirectX::XMFLOAT2{1900.0f, 250.0f}};
        for (const DirectX::XMFLOAT2 &coin : coins) {
            Entity entity =
                CreateRectEntity(coin.x - 11.0f, coin.y - 11.0f, 22.0f, 22.0f);
            world_.Add<CoinTag>(entity);
        }

        CreateEnemy(760.0f, 500.0f, -80.0f, 690.0f, 910.0f);
        CreateEnemy(1390.0f, 500.0f, 90.0f, 1320.0f, 1530.0f);
        CreateEnemy(2050.0f, 500.0f, -95.0f, 1960.0f, 2200.0f);

        Entity goal = CreateRectEntity(2320.0f, 462.0f, 76.0f, 74.0f);
        world_.Add<GoalTag>(goal);
    }

    void CreateEnemy(float x, float y, float velocityX, float leftLimit,
                     float rightLimit) {
        Entity entity = CreateRectEntity(x, y, 34.0f, 36.0f);
        world_.Add<EnemyTag>(entity);
        world_.Add<Velocity>(entity, Velocity{{velocityX, 0.0f, 0.0f}});
        world_.Add<EnemyPatrol>(entity, EnemyPatrol{leftLimit, rightLimit});
    }

    void UpdateInput(World &world) {
        if (!ctx_ || !ctx_->core.input) {
            return;
        }

        const Input &input = *ctx_->core.input;
        if (input.IsKeyTrigger(DIK_R)) {
            resetRequested_ = true;
            return;
        }

        world.View<InputComponent, JumpInput, PlayerState, Velocity>(
            [&input](Entity, InputComponent &inputComponent, JumpInput &jump,
                     PlayerState &state, Velocity &velocity) {
                if (state.won) {
                    velocity.linear.x = 0.0f;
                    jump.triggered = false;
                    return;
                }

                inputComponent.move = {0.0f, 0.0f};
                if (input.IsKeyPress(DIK_A) || input.IsKeyPress(DIK_LEFT)) {
                    inputComponent.move.x -= 1.0f;
                    state.facingRight = false;
                }
                if (input.IsKeyPress(DIK_D) || input.IsKeyPress(DIK_RIGHT)) {
                    inputComponent.move.x += 1.0f;
                    state.facingRight = true;
                }

                jump.triggered = input.IsKeyTrigger(DIK_SPACE) ||
                                 input.IsKeyTrigger(DIK_W) ||
                                 input.IsKeyTrigger(DIK_UP) ||
                                 input.IsKeyTrigger(DIK_Z);

                constexpr float kMoveSpeed = 245.0f;
                constexpr float kJumpSpeed = -590.0f;
                velocity.linear.x = inputComponent.move.x * kMoveSpeed;
                if (jump.triggered && state.onGround) {
                    velocity.linear.y = kJumpSpeed;
                    state.onGround = false;
                }
            });
    }

    void UpdateEnemyPatrol(World &world) {
        world.View<Transform, RectSize, Velocity, EnemyPatrol>(
            [](Entity, Transform &transform, RectSize &size, Velocity &velocity,
               EnemyPatrol &patrol) {
                if (transform.position.x < patrol.leftLimit) {
                    transform.position.x = patrol.leftLimit;
                    velocity.linear.x = std::abs(velocity.linear.x);
                } else if (transform.position.x + size.value.x >
                           patrol.rightLimit) {
                    transform.position.x = patrol.rightLimit - size.value.x;
                    velocity.linear.x = -std::abs(velocity.linear.x);
                }
            });
    }

    void ApplyGravity(World &world) {
        const float deltaTime = DeltaTime();
        world.View<Velocity, PlayerState>(
            [deltaTime](Entity, Velocity &velocity, PlayerState &state) {
                if (state.won) {
                    return;
                }
                constexpr float kGravity = 1580.0f;
                velocity.linear.y =
                    (std::min)(velocity.linear.y + kGravity * deltaTime,
                               920.0f);
            });
    }

    void StorePreviousPositions(World &world) {
        world.View<Transform, PreviousPosition>(
            [](Entity, Transform &transform, PreviousPosition &previous) {
                previous.value = transform.position;
            });
    }

    void ResolvePlatformContacts(World &world) {
        world.View<Transform, RectSize, Velocity, PlayerState, PreviousPosition>(
            [&world](Entity, Transform &transform, RectSize &size,
                     Velocity &velocity, PlayerState &state,
                     PreviousPosition &previous) {
                state.onGround = false;
                Rect player = MakeRect(transform, size);
                const Rect previousPlayer{previous.value.x, previous.value.y,
                                          size.value.x, size.value.y};

                world.View<Transform, RectSize, PlatformTag>(
                    [&](Entity, Transform &platformTransform,
                        RectSize &platformSize, PlatformTag &) {
                        const Rect platform =
                            MakeRect(platformTransform, platformSize);
                        if (!Intersects(player, platform)) {
                            return;
                        }

                        const float previousBottom =
                            previousPlayer.y + previousPlayer.h;
                        const float previousTop = previousPlayer.y;
                        const float previousRight =
                            previousPlayer.x + previousPlayer.w;
                        const float previousLeft = previousPlayer.x;

                        if (previousBottom <= platform.y &&
                            velocity.linear.y >= 0.0f) {
                            transform.position.y = platform.y - size.value.y;
                            velocity.linear.y = 0.0f;
                            state.onGround = true;
                        } else if (previousTop >= platform.y + platform.h &&
                                   velocity.linear.y < 0.0f) {
                            transform.position.y = platform.y + platform.h;
                            velocity.linear.y = 0.0f;
                        } else if (previousRight <= platform.x &&
                                   velocity.linear.x > 0.0f) {
                            transform.position.x = platform.x - size.value.x;
                            velocity.linear.x = 0.0f;
                        } else if (previousLeft >= platform.x + platform.w &&
                                   velocity.linear.x < 0.0f) {
                            transform.position.x = platform.x + platform.w;
                            velocity.linear.x = 0.0f;
                        }

                        player = MakeRect(transform, size);
                    });

                transform.position.x = (std::max)(transform.position.x, 0.0f);
            });
    }

    void CollectCoins(World &world, WorldCommandBuffer &commandBuffer) {
        const Rect player = GetPlayerRect(world, 8.0f);
        if (player.w <= 0.0f) {
            return;
        }

        world.View<Transform, RectSize, CoinTag>(
            [&commandBuffer, &player](Entity entity, Transform &transform,
                                      RectSize &size, CoinTag &) {
                if (Intersects(player, MakeRect(transform, size))) {
                    commandBuffer.DestroyEntity(entity);
                }
            });
    }

    void ResolveEnemyContacts(World &world, WorldCommandBuffer &commandBuffer) {
        Transform *playerTransform = world.TryGet<Transform>(playerEntity_);
        RectSize *playerSize = world.TryGet<RectSize>(playerEntity_);
        Velocity *playerVelocity = world.TryGet<Velocity>(playerEntity_);
        PlayerState *playerState = world.TryGet<PlayerState>(playerEntity_);
        if (!playerTransform || !playerSize || !playerVelocity || !playerState) {
            return;
        }

        const Rect player = MakeRect(*playerTransform, *playerSize);
        world.View<Transform, RectSize, EnemyTag>(
            [&](Entity enemy, Transform &enemyTransform, RectSize &enemySize,
                EnemyTag &) {
                const Rect enemyRect = MakeRect(enemyTransform, enemySize);
                if (!Intersects(player, enemyRect)) {
                    return;
                }

                const float playerBottom = player.y + player.h;
                const bool stomped = playerVelocity->linear.y > 90.0f &&
                                     playerBottom - enemyRect.y < 24.0f;
                if (stomped) {
                    commandBuffer.DestroyEntity(enemy);
                    playerVelocity->linear.y = -360.0f;
                    playerState->onGround = false;
                } else {
                    resetRequested_ = true;
                }
            });
    }

    void CheckGoalAndFall(World &world, WorldCommandBuffer &commandBuffer) {
        if (resetRequested_) {
            commandBuffer.Clear();
            ResetWorld();
            return;
        }

        Transform *playerTransform = world.TryGet<Transform>(playerEntity_);
        RectSize *playerSize = world.TryGet<RectSize>(playerEntity_);
        PlayerState *playerState = world.TryGet<PlayerState>(playerEntity_);
        Velocity *playerVelocity = world.TryGet<Velocity>(playerEntity_);
        if (!playerTransform || !playerSize || !playerState || !playerVelocity) {
            return;
        }

        if (playerTransform->position.y > 760.0f) {
            commandBuffer.Clear();
            ResetWorld();
            return;
        }

        const Rect player = MakeRect(*playerTransform, *playerSize);
        world.View<Transform, RectSize, GoalTag>(
            [&player, playerState, playerVelocity](Entity, Transform &goal,
                                                   RectSize &goalSize,
                                                   GoalTag &) {
                if (Intersects(player, MakeRect(goal, goalSize))) {
                    playerState->won = true;
                    playerVelocity->linear.x = 0.0f;
                    playerVelocity->linear.y = 0.0f;
                }
            });
    }

    Rect GetPlayerRect(World &world, float padding = 0.0f) const {
        const Transform *transform = world.TryGet<Transform>(playerEntity_);
        const RectSize *size = world.TryGet<RectSize>(playerEntity_);
        if (!transform || !size) {
            return {};
        }
        return {transform->position.x - padding, transform->position.y - padding,
                size->value.x + padding * 2.0f,
                size->value.y + padding * 2.0f};
    }

    void UpdateCamera(World &world) {
        const Transform *transform = world.TryGet<Transform>(playerEntity_);
        if (!transform || !ctx_) {
            return;
        }
        const float target =
            transform->position.x - static_cast<float>(ctx_->frame.width) * 0.42f;
        const float maxCamera =
            (std::max)(0.0f, kWorldWidth - static_cast<float>(ctx_->frame.width));
        cameraX_ = std::clamp(target, 0.0f, maxCamera);
    }

    void DrawScene() {
        if (!ctx_ || !ctx_->renderer.spriteRenderer) {
            return;
        }

        SpriteRenderer &renderer = *ctx_->renderer.spriteRenderer;
        renderer.PreDraw();

        DrawScreenRect(renderer,
                       {0.0f, 0.0f, static_cast<float>(ctx_->frame.width),
                        static_cast<float>(ctx_->frame.height)},
                       {0.43f, 0.77f, 1.0f, 1.0f});
        DrawCloud(renderer, 160.0f, 110.0f, 0.0f);
        DrawCloud(renderer, 720.0f, 78.0f, 0.35f);
        DrawCloud(renderer, 1220.0f, 145.0f, 0.15f);
        DrawHills(renderer);
        DrawLevelEntities(renderer);
        DrawPlayer(renderer);

        renderer.PostDraw();
    }

    void DrawLevelEntities(SpriteRenderer &renderer) {
        const float viewLeft = cameraX_ - 80.0f;
        const float viewRight =
            cameraX_ + static_cast<float>(ctx_->frame.width) + 80.0f;

        world_.View<Transform, RectSize>(
            [&](Entity entity, Transform &transform, RectSize &size) {
                if (entity == playerEntity_) {
                    return;
                }

                const Rect rect = MakeRect(transform, size);
                if (rect.x + rect.w < viewLeft || rect.x > viewRight) {
                    return;
                }

                if (world_.Has<PlatformTag>(entity)) {
                    const bool isGround = rect.h >= 80.0f;
                    DrawWorldRect(
                        renderer, rect,
                        isGround
                            ? DirectX::XMFLOAT4{0.35f, 0.73f, 0.24f, 1.0f}
                            : DirectX::XMFLOAT4{0.75f, 0.38f, 0.16f, 1.0f},
                        cameraX_);
                    DrawWorldRect(
                        renderer,
                        {rect.x, rect.y, rect.w, isGround ? 12.0f : 7.0f},
                        isGround
                            ? DirectX::XMFLOAT4{0.52f, 0.91f, 0.32f, 1.0f}
                            : DirectX::XMFLOAT4{0.96f, 0.67f, 0.24f, 1.0f},
                        cameraX_);
                } else if (world_.Has<CoinTag>(entity)) {
                    renderer.DrawFilledCircle(
                        {rect.x + rect.w * 0.5f - cameraX_,
                         rect.y + rect.h * 0.5f},
                        13.0f, {1.0f, 0.82f, 0.18f, 1.0f}, 24);
                    renderer.DrawCircle({rect.x + rect.w * 0.5f - cameraX_,
                                         rect.y + rect.h * 0.5f},
                                        13.0f,
                                        {1.0f, 0.97f, 0.42f, 1.0f}, 3.0f,
                                        24);
                } else if (world_.Has<EnemyTag>(entity)) {
                    DrawEnemy(renderer, rect);
                } else if (world_.Has<GoalTag>(entity)) {
                    DrawGoal(renderer, rect);
                }
            });
    }

    void DrawPlayer(SpriteRenderer &renderer) {
        const Transform *transform = world_.TryGet<Transform>(playerEntity_);
        const PlayerState *state = world_.TryGet<PlayerState>(playerEntity_);
        if (!transform || !state) {
            return;
        }

        const float x = transform->position.x - cameraX_;
        const float y = transform->position.y;
        DrawScreenRect(renderer, {x + 7.0f, y + 14.0f, 23.0f, 17.0f},
                       {0.98f, 0.72f, 0.52f, 1.0f});
        DrawScreenRect(renderer, {x + 4.0f, y + 3.0f, 30.0f, 14.0f},
                       {0.9f, 0.08f, 0.08f, 1.0f});
        DrawScreenRect(renderer,
                       {state->facingRight ? x + 25.0f : x - 1.0f,
                        y + 11.0f, 10.0f, 6.0f},
                       {0.9f, 0.08f, 0.08f, 1.0f});
        DrawScreenRect(renderer, {x + 6.0f, y + 31.0f, 25.0f, 19.0f},
                       {0.1f, 0.22f, 0.85f, 1.0f});
        DrawScreenRect(renderer, {x + 2.0f, y + 48.0f, 12.0f, 6.0f},
                       {0.28f, 0.12f, 0.04f, 1.0f});
        DrawScreenRect(renderer, {x + 23.0f, y + 48.0f, 12.0f, 6.0f},
                       {0.28f, 0.12f, 0.04f, 1.0f});
        DrawScreenRect(renderer,
                       {state->facingRight ? x + 25.0f : x + 9.0f,
                        y + 19.0f, 4.0f, 4.0f},
                       {0.05f, 0.04f, 0.03f, 1.0f});
    }

    void DrawEnemy(SpriteRenderer &renderer, const Rect &rect) const {
        const float x = rect.x - cameraX_;
        const float y = rect.y;
        DrawScreenRect(renderer, {x, y + 8.0f, rect.w, 24.0f},
                       {0.54f, 0.27f, 0.08f, 1.0f});
        DrawScreenRect(renderer, {x + 5.0f, y, rect.w - 10.0f, 16.0f},
                       {0.69f, 0.35f, 0.13f, 1.0f});
        DrawScreenRect(renderer, {x + 8.0f, y + 12.0f, 5.0f, 5.0f},
                       {0.03f, 0.02f, 0.01f, 1.0f});
        DrawScreenRect(renderer, {x + 21.0f, y + 12.0f, 5.0f, 5.0f},
                       {0.03f, 0.02f, 0.01f, 1.0f});
        DrawScreenRect(renderer, {x + 2.0f, y + 30.0f, 10.0f, 6.0f},
                       {0.19f, 0.09f, 0.03f, 1.0f});
        DrawScreenRect(renderer, {x + 22.0f, y + 30.0f, 10.0f, 6.0f},
                       {0.19f, 0.09f, 0.03f, 1.0f});
    }

    void DrawGoal(SpriteRenderer &renderer, const Rect &rect) const {
        const PlayerState *state = world_.TryGet<PlayerState>(playerEntity_);
        const bool won = state && state->won;
        DrawWorldRect(renderer, {rect.x + 18.0f, rect.y - 126.0f, 7.0f, 150.0f},
                      {0.93f, 0.93f, 0.88f, 1.0f}, cameraX_);
        DrawWorldRect(renderer, {rect.x + 25.0f, rect.y - 118.0f, 52.0f, 34.0f},
                      won ? DirectX::XMFLOAT4{1.0f, 0.86f, 0.18f, 1.0f}
                          : DirectX::XMFLOAT4{0.92f, 0.12f, 0.12f, 1.0f},
                      cameraX_);
    }

    void DrawCloud(SpriteRenderer &renderer, float worldX, float y,
                   float parallax) const {
        const float x = worldX - cameraX_ * parallax;
        renderer.DrawFilledCircle({x, y + 15.0f}, 24.0f,
                                  {0.96f, 0.99f, 1.0f, 1.0f}, 24);
        renderer.DrawFilledCircle({x + 29.0f, y}, 28.0f,
                                  {0.96f, 0.99f, 1.0f, 1.0f}, 24);
        renderer.DrawFilledCircle({x + 61.0f, y + 18.0f}, 22.0f,
                                  {0.96f, 0.99f, 1.0f, 1.0f}, 24);
        DrawScreenRect(renderer, {x - 1.0f, y + 17.0f, 72.0f, 23.0f},
                       {0.96f, 0.99f, 1.0f, 1.0f});
    }

    void DrawHills(SpriteRenderer &renderer) const {
        const float base = static_cast<float>(ctx_->frame.height) - 94.0f;
        const float offset = -std::fmod(cameraX_ * 0.18f, 520.0f);
        for (int i = -1; i < 4; ++i) {
            const float x = offset + static_cast<float>(i) * 520.0f;
            renderer.DrawFilledCircle({x + 130.0f, base + 60.0f}, 110.0f,
                                      {0.41f, 0.82f, 0.43f, 1.0f}, 36);
            renderer.DrawFilledCircle({x + 335.0f, base + 70.0f}, 86.0f,
                                      {0.30f, 0.72f, 0.36f, 1.0f}, 36);
        }
    }

    MovementSystem movementSystem_;
    Entity playerEntity_ = kInvalidEntity;
    float cameraX_ = 0.0f;
    bool resetRequested_ = false;
};

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    try {
        WinApp winApp;
        winApp.Initialize(hInstance, nCmdShow, kClientWidth, kClientHeight,
                          L"Engine ECS Mini Mario");

        DirectXCommon dxCommon;
        dxCommon.Initialize(winApp.GetHwnd(), winApp.GetWidth(),
                            winApp.GetHeight());

        SrvManager srvManager;
        srvManager.Initialize(&dxCommon);

        TextureManager textureManager;
        {
            auto uploadContext = dxCommon.BeginUploadContext();
            textureManager.Initialize(&dxCommon, &srvManager, uploadContext);
        }
        textureManager.ReleaseUploadBuffers();

        SpriteServices spriteServices;
        spriteServices.Initialize(&dxCommon, &textureManager, &srvManager,
                                  winApp.GetWidth(), winApp.GetHeight());

        Input input;
        input.Initialize(hInstance, winApp.GetHwnd());

        SceneContext sceneContext{};
        sceneContext.core.input = &input;
        sceneContext.core.winApp = &winApp;
        sceneContext.core.dxCommon = &dxCommon;
        sceneContext.core.srv = &srvManager;
        sceneContext.assets.spriteServices = &spriteServices;
        sceneContext.assets.spriteAssets = &spriteServices.Assets();
        sceneContext.assets.textureManager = &textureManager;
        sceneContext.renderer.spriteRenderer = &spriteServices.Renderer();
        sceneContext.frame.width = winApp.GetWidth();
        sceneContext.frame.height = winApp.GetHeight();

        SceneManager sceneManager;
        sceneManager.Initialize(sceneContext);
        sceneManager.ChangeScene(std::make_unique<MiniMarioScene>());

        int currentWidth = winApp.GetWidth();
        int currentHeight = winApp.GetHeight();
        auto previousTime = std::chrono::steady_clock::now();

        while (winApp.ProcessMessage()) {
            sceneContext.frame.deltaTime = CalculateDeltaTime(previousTime);
            input.Update(sceneContext.frame.deltaTime);

            if (input.IsKeyTrigger(DIK_ESCAPE)) {
                PostQuitMessage(0);
                continue;
            }

            const int width = winApp.GetWidth();
            const int height = winApp.GetHeight();
            if (width != currentWidth || height != currentHeight) {
                currentWidth = width;
                currentHeight = height;
                sceneContext.frame.width = width;
                sceneContext.frame.height = height;
                dxCommon.Resize(width, height);
                sceneManager.Resize(width, height);
            }

            sceneManager.Update();

            dxCommon.BeginFrame();
            sceneManager.Draw();
            dxCommon.EndFrame();
        }

        return 0;
    } catch (const std::exception &e) {
        ShowErrorMessage(e.what());
    } catch (...) {
        ShowErrorMessage("Unknown error");
    }

    return 1;
}
