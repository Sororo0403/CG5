#include "gameobject/ColliderComponent.h"

#include "CollisionUtil.h"

#include <utility>

ColliderComponent::ColliderComponent(ColliderLayer layer,
                                     OBBProvider obbProvider)
    : layer_(layer), obbProvider_(std::move(obbProvider)) {}

void ColliderComponent::SetOBBProvider(OBBProvider obbProvider) {
    obbProvider_ = std::move(obbProvider);
}

OBB ColliderComponent::GetOBB() const {
    if (obbProvider_) {
        return obbProvider_();
    }
    return {};
}

bool ColliderComponent::Intersects(const ColliderComponent &other) const {
    if (!enabled_ || !other.enabled_) {
        return false;
    }
    return Intersects(GetOBB(), other.GetOBB());
}

bool ColliderComponent::Intersects(const OBB &a, const OBB &b) {
    return CollisionUtil::CheckOBB(a, b);
}
