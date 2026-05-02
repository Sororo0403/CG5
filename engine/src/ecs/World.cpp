#include "World.h"

#include <algorithm>

Entity World::CreateEntity() {
    uint32_t index = 0;
    if (!freeIndices_.empty()) {
        index = freeIndices_.back();
        freeIndices_.pop_back();
    } else {
        index = static_cast<uint32_t>(generations_.size());
        generations_.push_back(1);
    }

    Entity entity{index, generations_[index]};
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

    ++generations_[entity.index];
    if (generations_[entity.index] == 0) {
        generations_[entity.index] = 1;
    }
    freeIndices_.push_back(entity.index);
}

bool World::IsAlive(Entity entity) const {
    return entity.IsValid() && entity.index < generations_.size() &&
           generations_[entity.index] == entity.generation &&
           aliveSet_.find(entity) != aliveSet_.end();
}

const std::vector<Entity> &World::AliveEntities() const {
    return aliveEntities_;
}
