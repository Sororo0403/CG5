#include "DamageSystem.h"

#include "Collider.h"
#include "Transform.h"
#include "World.h"

#include <algorithm>
#include <vector>

namespace {

struct DamageShape {
    Entity entity = kInvalidEntity;
    Entity owner = kInvalidEntity;
    Transform *transform = nullptr;
    Collider *collider = nullptr;
    Faction *faction = nullptr;
};

struct Bounds {
    DirectX::XMFLOAT3 min{};
    DirectX::XMFLOAT3 max{};
};

Bounds MakeBounds(const Transform &transform, const Collider &collider) {
    const DirectX::XMFLOAT3 center{
        transform.position.x + collider.center.x,
        transform.position.y + collider.center.y,
        transform.position.z + collider.center.z};

    DirectX::XMFLOAT3 halfSize = collider.halfSize;
    if (collider.shape == ColliderShape::Sphere) {
        halfSize = {collider.radius, collider.radius, collider.radius};
    } else if (collider.shape == ColliderShape::Capsule) {
        halfSize = {collider.radius, collider.radius + collider.capsuleHalfHeight,
                    collider.radius};
    }

    return {{center.x - halfSize.x, center.y - halfSize.y,
             center.z - halfSize.z},
            {center.x + halfSize.x, center.y + halfSize.y,
             center.z + halfSize.z}};
}

bool Overlaps(const Bounds &a, const Bounds &b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

bool CanDamage(const Faction *hitFaction, const Faction *hurtFaction) {
    if (!hitFaction || !hurtFaction) {
        return true;
    }
    return (hitFaction->mask & hurtFaction->layer) != 0;
}

} // namespace

void DamageSystem::Update(World &world, float deltaTime) {
    events_.clear();

    world.View<Health>([deltaTime](Entity, Health &health) {
        health.invulnerabilityTime =
            (std::max)(0.0f, health.invulnerabilityTime - deltaTime);
    });

    std::vector<DamageShape> hitboxes;
    std::vector<DamageShape> hurtboxes;

    world.View<Transform, Collider, Hitbox>(
        [&world, &hitboxes](Entity entity, Transform &transform,
                            Collider &collider, Hitbox &hitbox) {
            if (!hitbox.active) {
                return;
            }
            hitboxes.push_back(
                {entity, hitbox.owner, &transform, &collider,
                 world.TryGet<Faction>(entity)});
        });

    world.View<Transform, Collider, Hurtbox>(
        [&world, &hurtboxes](Entity entity, Transform &transform,
                             Collider &collider, Hurtbox &hurtbox) {
            if (!hurtbox.active) {
                return;
            }
            hurtboxes.push_back(
                {entity, hurtbox.owner, &transform, &collider,
                 world.TryGet<Faction>(entity)});
        });

    for (const DamageShape &hitShape : hitboxes) {
        const Hitbox &hitbox = world.Get<Hitbox>(hitShape.entity);
        const Bounds hitBounds =
            MakeBounds(*hitShape.transform, *hitShape.collider);

        for (const DamageShape &hurtShape : hurtboxes) {
            if (hitShape.entity == hurtShape.entity ||
                hitbox.owner == hurtShape.entity ||
                (hitbox.owner.IsValid() && hitbox.owner == hurtShape.owner)) {
                continue;
            }
            if (!CanDamage(hitShape.faction, hurtShape.faction)) {
                continue;
            }

            const Bounds hurtBounds =
                MakeBounds(*hurtShape.transform, *hurtShape.collider);
            if (!Overlaps(hitBounds, hurtBounds)) {
                continue;
            }

            Entity victim = hurtShape.owner.IsValid() ? hurtShape.owner
                                                      : hurtShape.entity;
            Health *health = world.TryGet<Health>(victim);
            if (!health || health->invulnerabilityTime > 0.0f) {
                continue;
            }

            health->current = (std::max)(0, health->current - hitbox.damage);
            health->invulnerabilityTime = hitbox.victimInvulnerabilityTime;
            events_.push_back(
                {hitbox.owner.IsValid() ? hitbox.owner : hitShape.entity,
                 victim, hitbox.damage, hitbox.knockback});
        }
    }
}
