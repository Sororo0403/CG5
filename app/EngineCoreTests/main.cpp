#include "ActionInput.h"
#include "AIController.h"
#include "AISystem.h"
#include "AnimationComponents.h"
#include "AnimationEvents.h"
#include "AnimationEventSystem.h"
#include "AnimationStateMachine.h"
#include "AnimationStateMachineSystem.h"
#include "Camera.h"
#include "CameraFollow.h"
#include "CameraFollowSystem.h"
#include "CharacterController.h"
#include "CharacterControllerSystem.h"
#include "Collider.h"
#include "CollisionBody.h"
#include "CollisionResolutionSystem.h"
#include "CollisionSystem.h"
#include "ContinuousCollisionSystem.h"
#include "DamageSystem.h"
#include "GameplayComponents.h"
#include "GroundingComponents.h"
#include "GroundingSystem.h"
#include "HitboxLifetimeSystem.h"
#include "LockOn.h"
#include "LockOnSystem.h"
#include "MovementSystem.h"
#include "PhysicsBody.h"
#include "PhysicsSystem.h"
#include "PreviousTransformSystem.h"
#include "RootMotion.h"
#include "RootMotionSystem.h"
#include "SystemPipeline.h"
#include "Transform.h"
#include "TransformHierarchy.h"
#include "TransformSystem.h"
#include "Velocity.h"
#include "World.h"
#include "WorldCommandBuffer.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Health {
    int value = 0;
};

struct Mana {
    int value = 0;
};

struct Marker {};

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void RequireNear(float actual, float expected, const char *message) {
    if (!NearlyEqual(actual, expected)) {
        throw std::runtime_error(message);
    }
}

void TestWorldEntityAndComponentLifetime() {
    World world;
    const Entity first = world.CreateEntity();
    const Entity second = world.CreateEntity();

    Require(first.IsValid(), "created entity should be valid");
    Require(second.IsValid(), "second entity should be valid");
    Require(first != second, "created entities should be unique");
    Require(world.IsAlive(first), "created entity should be alive");

    world.Add<Health>(first, Health{10});
    world.Add<Mana>(first, Mana{3});
    world.Add<Health>(second, Health{20});

    int healthSum = 0;
    int healthAndManaCount = 0;
    world.View<Health>([&](Entity, Health &health) {
        healthSum += health.value;
    });
    world.View<Health, Mana>([&](Entity, Health &health, Mana &mana) {
        health.value += mana.value;
        ++healthAndManaCount;
    });

    Require(healthSum == 30, "View<Health> should visit all Health components");
    Require(healthAndManaCount == 1,
            "View<Health, Mana> should only visit entities with both");
    Require(world.Get<Health>(first).value == 13,
            "View should provide mutable component references");

    world.DestroyEntity(first);
    Require(!world.IsAlive(first), "destroyed entity should not be alive");
    Require(!world.Has<Health>(first),
            "destroying an entity should remove its components");
    Require(world.IsAlive(second), "destroying one entity should not kill others");

    const Entity recycled = world.CreateEntity();
    Require(recycled.index == first.index,
            "destroyed entity indices should be recycled");
    Require(recycled.generation != first.generation,
            "recycled entity should have a new generation");
}

void TestWorldCommandBufferDefersStructuralChanges() {
    World world;
    WorldCommandBuffer commandBuffer;
    const Entity entity = world.CreateEntity();
    world.Add<Health>(entity, Health{7});

    world.View<Health>([&](Entity visited, Health &) {
        commandBuffer.Add<Mana>(visited, Mana{5});
    });

    Require(!world.Has<Mana>(entity),
            "queued component add should not run during View");
    commandBuffer.Playback(world);
    Require(world.Has<Mana>(entity),
            "queued component add should run on playback");
    Require(world.Get<Mana>(entity).value == 5,
            "queued component data should be preserved");

    commandBuffer.DestroyEntity(entity);
    commandBuffer.Playback(world);
    Require(!world.IsAlive(entity),
            "queued entity destruction should run on playback");
}

void TestSystemPipelineFlushStep() {
    World world;
    WorldCommandBuffer commandBuffer;
    SystemPipeline pipeline;
    bool sawCreatedEntity = false;

    pipeline.AddWorldUpdateStep([](World &, WorldCommandBuffer &commands) {
        commands.CreateEntity([](World &targetWorld, Entity entity) {
            targetWorld.Add<Marker>(entity);
        });
    });
    pipeline.AddCommandFlushStep();
    pipeline.AddWorldUpdateStep([&](World &targetWorld, WorldCommandBuffer &) {
        targetWorld.View<Marker>([&](Entity, Marker &) {
            sawCreatedEntity = true;
        });
    });

    pipeline.Update(world, commandBuffer);
    Require(sawCreatedEntity,
            "SystemPipeline flush step should expose queued entities to later steps");
}

void TestMovementAndPhysicsSystems() {
    World world;
    const Entity entity = world.CreateEntity();
    world.Add<Transform>(entity, Transform{{1.0f, 2.0f, 3.0f}});
    world.Add<Velocity>(entity, Velocity{{2.0f, -1.0f, 0.5f}});
    world.Add<PhysicsBody>(
        entity, PhysicsBody{{4.0f, 6.0f, -2.0f}, true, 0.5f});

    PhysicsSystem physics;
    physics.SetGravity({0.0f, -10.0f, 0.0f});
    physics.Update(world, 0.5f);

    const Velocity &velocity = world.Get<Velocity>(entity);
    RequireNear(velocity.linear.x, 4.0f,
                "PhysicsSystem should integrate acceleration on x");
    RequireNear(velocity.linear.y, -0.5f,
                "PhysicsSystem should integrate acceleration and gravity on y");
    RequireNear(velocity.linear.z, -0.5f,
                "PhysicsSystem should integrate acceleration on z");

    MovementSystem movement;
    movement.Update(world, 2.0f);

    const Transform &transform = world.Get<Transform>(entity);
    RequireNear(transform.position.x, 9.0f,
                "MovementSystem should move transform by velocity x");
    RequireNear(transform.position.y, 1.0f,
                "MovementSystem should move transform by velocity y");
    RequireNear(transform.position.z, 2.0f,
                "MovementSystem should move transform by velocity z");
}

void TestMovementSystemUsesLocalTransformWhenPresent() {
    World world;
    const Entity entity = world.CreateEntity();
    world.Add<Transform>(entity, Transform{{100.0f, 0.0f, 0.0f}});
    world.Add<LocalTransform>(entity,
                              LocalTransform{Transform{{1.0f, 2.0f, 3.0f}}});
    world.Add<Velocity>(entity, Velocity{{2.0f, 0.0f, 0.0f}});

    MovementSystem movement;
    movement.Update(world, 3.0f);

    RequireNear(world.Get<Transform>(entity).position.x, 100.0f,
                "MovementSystem should leave world Transform unchanged when LocalTransform exists");
    RequireNear(world.Get<LocalTransform>(entity).transform.position.x, 7.0f,
                "MovementSystem should move LocalTransform when present");
}

void TestTransformSystemCreatesWorldMatricesAndResolvesHierarchy() {
    World world;
    const Entity parent = world.CreateEntity();
    const Entity child = world.CreateEntity();

    world.Add<Transform>(parent, Transform{{10.0f, 0.0f, 0.0f}});
    world.Add<Transform>(child, Transform{});
    world.Add<LocalTransform>(child,
                              LocalTransform{Transform{{2.0f, 3.0f, 4.0f}}});
    world.Add<ParentComponent>(child, ParentComponent{parent});

    TransformSystem transformSystem;
    transformSystem.Update(world);

    Require(world.Has<WorldTransformMatrix>(parent),
            "TransformSystem should create WorldTransformMatrix for transform entities");
    Require(world.Has<WorldTransformMatrix>(child),
            "TransformSystem should create WorldTransformMatrix for child entities");

    const Transform &childTransform = world.Get<Transform>(child);
    RequireNear(childTransform.position.x, 12.0f,
                "TransformSystem should apply parent translation on x");
    RequireNear(childTransform.position.y, 3.0f,
                "TransformSystem should preserve local y under parent");
    RequireNear(childTransform.position.z, 4.0f,
                "TransformSystem should preserve local z under parent");

    const WorldTransformMatrix &childMatrix =
        world.Get<WorldTransformMatrix>(child);
    RequireNear(childMatrix.matrix._41, 12.0f,
                "child world matrix should contain resolved x translation");
    RequireNear(childMatrix.matrix._42, 3.0f,
                "child world matrix should contain resolved y translation");
    RequireNear(childMatrix.matrix._43, 4.0f,
                "child world matrix should contain resolved z translation");
}

bool ContainsEvent(const std::vector<CollisionEvent> &events, Entity a,
                   Entity b, CollisionEventType type) {
    for (const CollisionEvent &event : events) {
        const bool sameOrder = event.a == a && event.b == b;
        const bool reverseOrder = event.a == b && event.b == a;
        if ((sameOrder || reverseOrder) && event.type == type) {
            return true;
        }
    }
    return false;
}

void TestCollisionSystemEnterStayExitAndLayerMask() {
    World world;
    const Entity first = world.CreateEntity();
    const Entity second = world.CreateEntity();
    const Entity ignored = world.CreateEntity();

    world.Add<Transform>(first, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(first,
                        Collider{ColliderShape::AABB, {}, {1.0f, 1.0f, 1.0f},
                                 0.5f, 0b0001u, 0b0010u});

    world.Add<Transform>(second, Transform{{1.0f, 0.0f, 0.0f}});
    world.Add<Collider>(second,
                        Collider{ColliderShape::AABB, {}, {1.0f, 1.0f, 1.0f},
                                 0.5f, 0b0010u, 0b0001u});

    world.Add<Transform>(ignored, Transform{{1.0f, 0.0f, 0.0f}});
    world.Add<Collider>(ignored,
                        Collider{ColliderShape::AABB, {}, {1.0f, 1.0f, 1.0f},
                                 0.5f, 0b0100u, 0b0100u});

    CollisionSystem collision;
    collision.SetBroadPhaseCellSize(2.0f);

    collision.Update(world);
    Require(ContainsEvent(collision.Events(), first, second,
                          CollisionEventType::Enter),
            "CollisionSystem should emit Enter for new overlaps");
    Require(!ContainsEvent(collision.Events(), first, ignored,
                           CollisionEventType::Enter),
            "CollisionSystem should respect collider layer masks");

    collision.Update(world);
    Require(ContainsEvent(collision.Events(), first, second,
                          CollisionEventType::Stay),
            "CollisionSystem should emit Stay for persistent overlaps");

    world.Get<Transform>(second).position.x = 10.0f;
    collision.Update(world);
    Require(ContainsEvent(collision.Events(), first, second,
                          CollisionEventType::Exit),
            "CollisionSystem should emit Exit when an overlap ends");
}

void TestCollisionResolutionPushesOutAndGrounds() {
    World world;
    const Entity player = world.CreateEntity();
    const Entity ground = world.CreateEntity();

    world.Add<Transform>(player, Transform{{0.0f, 0.75f, 0.0f}});
    world.Add<Collider>(player,
                        Collider{ColliderShape::AABB, {}, {0.5f, 0.5f, 0.5f}});
    world.Add<CollisionBody>(
        player, CollisionBody{CollisionBodyType::Dynamic, true, true, 0.5f});
    world.Add<GroundedState>(player);
    world.Add<Velocity>(player, Velocity{{0.0f, -5.0f, 0.0f}});

    world.Add<Transform>(ground, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(ground,
                        Collider{ColliderShape::AABB, {}, {3.0f, 0.5f, 3.0f}});
    world.Add<CollisionBody>(
        ground, CollisionBody{CollisionBodyType::Static, true, true, 0.5f});

    CollisionResolutionSystem resolution;
    resolution.Update(world);

    RequireNear(world.Get<Transform>(player).position.y, 1.0f,
                "CollisionResolutionSystem should push dynamic body out of ground");
    Require(world.Get<GroundedState>(player).grounded,
            "CollisionResolutionSystem should mark upward contacts as grounded");
    RequireNear(world.Get<Velocity>(player).linear.y, 0.0f,
                "CollisionResolutionSystem should remove velocity into contact normal");
}

void TestCharacterControllerMoveJumpAndDash() {
    World world;
    const Entity player = world.CreateEntity();

    world.Add<Transform>(player);
    world.Add<Velocity>(player);
    world.Add<ActionInput>(player, ActionInput{{1.0f, 0.0f}});
    world.Add<InputBuffer>(player);
    world.Add<GroundedState>(player, GroundedState{true});
    world.Add<CharacterController>(player);

    CharacterControllerSystem controller;
    controller.Update(world, 0.1f);

    RequireNear(world.Get<Velocity>(player).linear.x, 4.5f,
                "CharacterControllerSystem should accelerate on ground");

    world.Get<ActionInput>(player) = ActionInput{};
    world.Get<ActionInput>(player).jumpPressed = true;
    world.Get<GroundedState>(player).grounded = true;
    controller.Update(world, 0.1f);
    RequireNear(world.Get<Velocity>(player).linear.y, 8.5f,
                "CharacterControllerSystem should jump from grounded state");

    world.Get<ActionInput>(player) = ActionInput{{0.0f, 1.0f}};
    world.Get<ActionInput>(player).dashPressed = true;
    controller.Update(world, 0.1f);
    RequireNear(world.Get<Velocity>(player).linear.z, 14.0f,
                "CharacterControllerSystem should apply dash speed");
}

void TestDamageSystemAppliesHitboxDamage() {
    World world;
    const Entity attacker = world.CreateEntity();
    const Entity victim = world.CreateEntity();

    world.Add<Transform>(attacker, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(attacker,
                        Collider{ColliderShape::AABB, {}, {0.5f, 0.5f, 0.5f}});
    world.Add<Faction>(attacker, Faction{0b0001u, 0b0010u});
    world.Add<Hitbox>(attacker,
                      Hitbox{2, 0.2f, {1.0f, 0.0f, 0.0f}, attacker, true});

    world.Add<Transform>(victim, Transform{{0.25f, 0.0f, 0.0f}});
    world.Add<Collider>(victim,
                        Collider{ColliderShape::AABB, {}, {0.5f, 0.5f, 0.5f}});
    world.Add<Faction>(victim, Faction{0b0010u, 0b0001u});
    world.Add<Hurtbox>(victim, Hurtbox{victim, true});
    world.Add<::Health>(victim, ::Health{5, 5, 0.0f});

    DamageSystem damage;
    damage.Update(world, 0.016f);

    Require(world.Get<::Health>(victim).current == 3,
            "DamageSystem should subtract hitbox damage from victim health");
    Require(damage.Events().size() == 1,
            "DamageSystem should emit one DamageEvent for the hit");
}

void TestAnimationStateMachineUpdatesAnimationComponent() {
    World world;
    const Entity entity = world.CreateEntity();

    world.Add<AnimationStateMachine>(
        entity, AnimationStateMachine{"Idle", "Run", "", 0.0f, 0.15f});
    world.Add<AnimationComponent>(entity, AnimationComponent{"Idle"});

    AnimationStateMachineSystem animationState;
    animationState.Update(world, 0.016f);

    Require(world.Get<AnimationStateMachine>(entity).state == "Run",
            "AnimationStateMachineSystem should apply requested state");
    Require(world.Get<AnimationComponent>(entity).currentAnimation == "Run",
            "AnimationStateMachineSystem should update AnimationComponent");
    Require(world.Get<AnimationComponent>(entity).playing,
            "AnimationStateMachineSystem should start playback");
}

void TestCapsuleCollisionEnterAndResolution() {
    World world;
    const Entity first = world.CreateEntity();
    const Entity second = world.CreateEntity();

    Collider capsule{};
    capsule.shape = ColliderShape::Capsule;
    capsule.radius = 0.5f;
    capsule.capsuleHalfHeight = 1.0f;

    world.Add<Transform>(first, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(first, capsule);
    world.Add<CollisionBody>(
        first, CollisionBody{CollisionBodyType::Dynamic, true, true, 0.5f});
    world.Add<Velocity>(first);

    world.Add<Transform>(second, Transform{{0.75f, 0.0f, 0.0f}});
    world.Add<Collider>(second, capsule);
    world.Add<CollisionBody>(
        second, CollisionBody{CollisionBodyType::Static, true, true, 0.5f});

    CollisionSystem collision;
    collision.Update(world);
    Require(ContainsEvent(collision.Events(), first, second,
                          CollisionEventType::Enter),
            "CollisionSystem should detect capsule overlaps");

    CollisionResolutionSystem resolution;
    resolution.Update(world);
    Require(world.Get<Transform>(first).position.x < 0.0f,
            "CollisionResolutionSystem should push capsule out horizontally");
}

void TestLockOnSystemChoosesNearestTarget() {
    World world;
    const Entity player = world.CreateEntity();
    const Entity nearTarget = world.CreateEntity();
    const Entity farTarget = world.CreateEntity();

    world.Add<Transform>(player, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<ActionInput>(player, ActionInput{});
    world.Get<ActionInput>(player).lockOnPressed = true;
    world.Add<LockOnState>(player);

    world.Add<Transform>(nearTarget, Transform{{3.0f, 0.0f, 0.0f}});
    world.Add<LockOnTarget>(nearTarget);
    world.Add<Transform>(farTarget, Transform{{10.0f, 0.0f, 0.0f}});
    world.Add<LockOnTarget>(farTarget);

    LockOnSystem lockOn;
    lockOn.Update(world);

    Require(world.Get<LockOnState>(player).locked,
            "LockOnSystem should lock when a target is available");
    Require(world.Get<LockOnState>(player).target == nearTarget,
            "LockOnSystem should choose the nearest target");
}

void TestCameraFollowSystemMovesCamera() {
    World world;
    const Entity player = world.CreateEntity();

    world.Add<Transform>(player, Transform{{5.0f, 1.0f, 2.0f}});
    world.Add<CameraFollow>(
        player, CameraFollow{kInvalidEntity, {0.0f, 3.0f, -6.0f},
                             {0.0f, 1.0f, 0.0f}, 100.0f, 100.0f});

    Camera camera;
    camera.Initialize(16.0f / 9.0f);

    CameraFollowSystem cameraFollow;
    cameraFollow.Update(world, camera, 1.0f);

    RequireNear(camera.GetPosition().x, 5.0f,
                "CameraFollowSystem should follow target x");
    RequireNear(camera.GetPosition().y, 4.0f,
                "CameraFollowSystem should apply camera offset y");
    RequireNear(camera.GetPosition().z, -4.0f,
                "CameraFollowSystem should apply camera offset z");
}

void TestAISystemPursuesTarget() {
    World world;
    const Entity enemy = world.CreateEntity();
    const Entity target = world.CreateEntity();

    world.Add<Transform>(enemy, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Velocity>(enemy);
    world.Add<PursuitAI>(enemy, PursuitAI{target, 4.0f, 20.0f, 1.0f, 10.0f});

    world.Add<Transform>(target, Transform{{5.0f, 0.0f, 0.0f}});

    AISystem ai;
    ai.Update(world, 0.1f);

    RequireNear(world.Get<Velocity>(enemy).linear.x, 2.0f,
                "AISystem should accelerate toward target");
    RequireNear(world.Get<Velocity>(enemy).linear.z, 0.0f,
                "AISystem should not move sideways when target is on x axis");
}

void TestHitboxLifetimeDisablesExpiredHitbox() {
    World world;
    WorldCommandBuffer commands;
    const Entity hitboxEntity = world.CreateEntity();

    world.Add<Hitbox>(hitboxEntity);
    world.Add<HitboxLifetime>(hitboxEntity,
                              HitboxLifetime{0.05f, false});

    HitboxLifetimeSystem lifetime;
    lifetime.Update(world, commands, 0.1f);
    commands.Playback(world);

    Require(!world.Get<Hitbox>(hitboxEntity).active,
            "HitboxLifetimeSystem should disable expired hitboxes");
    Require(world.IsAlive(hitboxEntity),
            "HitboxLifetimeSystem should keep entity when destroy flag is false");
}

void TestGroundingSystemSnapsToGround() {
    World world;
    const Entity player = world.CreateEntity();
    const Entity ground = world.CreateEntity();

    world.Add<Transform>(player, Transform{{0.0f, 1.12f, 0.0f}});
    world.Add<Collider>(player,
                        Collider{ColliderShape::AABB, {}, {0.5f, 0.5f, 0.5f}});
    world.Add<CollisionBody>(
        player, CollisionBody{CollisionBodyType::Dynamic, true, true, 0.5f});
    world.Add<GroundedState>(player);
    world.Add<GroundingAssist>(player, GroundingAssist{0.2f, 0.35f, 0.0f});
    world.Add<Velocity>(player, Velocity{{0.0f, -1.0f, 0.0f}});

    world.Add<Transform>(ground, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(ground,
                        Collider{ColliderShape::AABB, {}, {3.0f, 0.5f, 3.0f}});
    world.Add<CollisionBody>(ground);
    world.Add<GroundSurface>(ground);

    GroundingSystem grounding;
    grounding.Update(world);

    Require(world.Get<GroundedState>(player).grounded,
            "GroundingSystem should mark nearby ground as grounded");
    RequireNear(world.Get<Transform>(player).position.y, 1.0f,
                "GroundingSystem should snap body onto the ground top");
    RequireNear(world.Get<Velocity>(player).linear.y, 0.0f,
                "GroundingSystem should clear downward velocity");
}

void TestContinuousCollisionSystemStopsFastFall() {
    World world;
    const Entity player = world.CreateEntity();
    const Entity ground = world.CreateEntity();

    world.Add<Transform>(player, Transform{{0.0f, 0.2f, 0.0f}});
    world.Add<PreviousTransform>(
        player, PreviousTransform{Transform{{0.0f, 3.0f, 0.0f}}});
    world.Add<Collider>(player,
                        Collider{ColliderShape::AABB, {}, {0.5f, 0.5f, 0.5f}});
    world.Add<CollisionBody>(
        player, CollisionBody{CollisionBodyType::Dynamic, true, true, 0.5f});
    world.Add<GroundedState>(player);
    world.Add<Velocity>(player, Velocity{{0.0f, -30.0f, 0.0f}});

    world.Add<Transform>(ground, Transform{{0.0f, 0.0f, 0.0f}});
    world.Add<Collider>(ground,
                        Collider{ColliderShape::AABB, {}, {3.0f, 0.5f, 3.0f}});
    world.Add<CollisionBody>(ground);

    ContinuousCollisionSystem ccd;
    ccd.Update(world);

    RequireNear(world.Get<Transform>(player).position.y, 1.0f,
                "ContinuousCollisionSystem should snap fast fall to ground");
    Require(world.Get<GroundedState>(player).grounded,
            "ContinuousCollisionSystem should ground the falling entity");
}

void TestRootMotionSystemAppliesDelta() {
    World world;
    const Entity entity = world.CreateEntity();

    SkeletonPoseComponent pose{};
    pose.hasRootAnimation = true;
    pose.rootAnimationMatrix._11 = 1.0f;
    pose.rootAnimationMatrix._22 = 1.0f;
    pose.rootAnimationMatrix._33 = 1.0f;
    pose.rootAnimationMatrix._44 = 1.0f;
    pose.rootAnimationMatrix._41 = 1.0f;

    world.Add<Transform>(entity);
    world.Add<SkeletonPoseComponent>(entity, pose);
    world.Add<AnimationComponent>(entity, AnimationComponent{"Run", 0.5f});
    world.Add<RootMotion>(entity);
    world.Add<RootMotionState>(entity);

    RootMotionSystem rootMotion;
    rootMotion.Update(world);

    world.Get<SkeletonPoseComponent>(entity).rootAnimationMatrix._41 = 2.5f;
    world.Get<AnimationComponent>(entity).previousTime = 0.5f;
    world.Get<AnimationComponent>(entity).time = 0.6f;
    rootMotion.Update(world);

    RequireNear(world.Get<Transform>(entity).position.x, 1.5f,
                "RootMotionSystem should apply root translation delta");
}

void TestAnimationEventSystemQueuesCrossedEvents() {
    World world;
    const Entity entity = world.CreateEntity();

    world.Add<AnimationComponent>(
        entity, AnimationComponent{"Attack", 0.55f, false, true, false, 0.10f});
    world.Add<AnimationEventTrack>(
        entity, AnimationEventTrack{{{"hit_on", 0.25f}, {"hit_off", 0.50f}}});

    AnimationEventSystem events;
    events.Update(world);

    const AnimationEventQueue &queue = world.Get<AnimationEventQueue>(entity);
    Require(queue.events.size() == 2,
            "AnimationEventSystem should queue all crossed events");
    Require(queue.events[0] == "hit_on" && queue.events[1] == "hit_off",
            "AnimationEventSystem should preserve event order");
}

using TestFn = void (*)();

struct TestCase {
    const char *name = "";
    TestFn run = nullptr;
};

int RunAllTests() {
    const TestCase tests[]{
        {"World entity/component lifetime", TestWorldEntityAndComponentLifetime},
        {"WorldCommandBuffer defers structure changes",
         TestWorldCommandBufferDefersStructuralChanges},
        {"SystemPipeline command flush", TestSystemPipelineFlushStep},
        {"MovementSystem and PhysicsSystem", TestMovementAndPhysicsSystems},
        {"MovementSystem uses LocalTransform",
         TestMovementSystemUsesLocalTransformWhenPresent},
        {"TransformSystem hierarchy", TestTransformSystemCreatesWorldMatricesAndResolvesHierarchy},
        {"CollisionSystem events and masks",
         TestCollisionSystemEnterStayExitAndLayerMask},
        {"CollisionResolutionSystem push-out and grounding",
         TestCollisionResolutionPushesOutAndGrounds},
        {"CharacterControllerSystem move jump and dash",
         TestCharacterControllerMoveJumpAndDash},
        {"DamageSystem hitbox damage", TestDamageSystemAppliesHitboxDamage},
        {"AnimationStateMachineSystem state request",
         TestAnimationStateMachineUpdatesAnimationComponent},
        {"Capsule collision enter and resolution",
         TestCapsuleCollisionEnterAndResolution},
        {"LockOnSystem nearest target", TestLockOnSystemChoosesNearestTarget},
        {"CameraFollowSystem movement", TestCameraFollowSystemMovesCamera},
        {"AISystem pursuit", TestAISystemPursuesTarget},
        {"HitboxLifetimeSystem expiry",
         TestHitboxLifetimeDisablesExpiredHitbox},
        {"GroundingSystem snap", TestGroundingSystemSnapsToGround},
        {"ContinuousCollisionSystem fast fall",
         TestContinuousCollisionSystemStopsFastFall},
        {"RootMotionSystem delta", TestRootMotionSystemAppliesDelta},
        {"AnimationEventSystem crossed events",
         TestAnimationEventSystemQueuesCrossedEvents},
    };

    int passed = 0;
    for (const TestCase &test : tests) {
        try {
            test.run();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception &e) {
            std::cerr << "[FAIL] " << test.name << ": " << e.what() << '\n';
            return 1;
        }
    }

    std::cout << passed << " engine core tests passed.\n";
    return 0;
}

} // namespace

int main() {
    try {
        return RunAllTests();
    } catch (const std::exception &e) {
        std::cerr << "[FATAL] " << e.what() << '\n';
    } catch (...) {
        std::cerr << "[FATAL] unknown exception\n";
    }
    return 1;
}
