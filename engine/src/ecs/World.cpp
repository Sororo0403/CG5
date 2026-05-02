#include "World.h"

#include <algorithm>

Entity World::CreateEntity() {
    Entity entity = kInvalidEntity;
    if (!freeEntities_.empty()) {
        entity = freeEntities_.back();
        freeEntities_.pop_back();
    } else {
        entity = nextEntity_++;
    }

    aliveEntities_.push_back(entity);
    aliveSet_.insert(entity);
    return entity;
}

void World::DestroyEntity(Entity entity) {
    if (!IsAlive(entity)) {
        return;
    }

    for (auto &[_, entry] : storages_) {
        entry.remove(entry.storage, entity);
    }

    aliveSet_.erase(entity);
    aliveEntities_.erase(
        std::remove(aliveEntities_.begin(), aliveEntities_.end(), entity),
        aliveEntities_.end());
    freeEntities_.push_back(entity);
}

bool World::IsAlive(Entity entity) const {
    return aliveSet_.find(entity) != aliveSet_.end();
}

const std::vector<Entity> &World::AliveEntities() const {
    return aliveEntities_;
}
