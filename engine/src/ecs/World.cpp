#include "World.h"

#include <algorithm>

Entity World::CreateEntity() {
    AssertCanChangeStructure();

    uint32_t index = 0;
    if (!freeIndices_.empty()) {
        index = freeIndices_.back();
        freeIndices_.pop_back();
    } else {
        index = static_cast<uint32_t>(generations_.size());
        generations_.push_back(1);
    }

    Entity entity{index, generations_[index]};
    aliveEntityToIndex_[entity] = static_cast<uint32_t>(aliveEntities_.size());
    aliveEntities_.push_back(entity);
    aliveSet_.insert(entity);
    return entity;
}

void World::DestroyEntity(Entity entity) {
    AssertCanChangeStructure();

    if (!IsAlive(entity)) {
        return;
    }

    for (auto &[_, entry] : storages_) {
        entry.remove(entry.storage, entity);
    }

    aliveSet_.erase(entity);
    const auto aliveIt = aliveEntityToIndex_.find(entity);
    if (aliveIt != aliveEntityToIndex_.end()) {
        const uint32_t index = aliveIt->second;
        const uint32_t lastIndex =
            static_cast<uint32_t>(aliveEntities_.size() - 1);
        if (index != lastIndex) {
            const Entity lastEntity = aliveEntities_[lastIndex];
            aliveEntities_[index] = lastEntity;
            aliveEntityToIndex_[lastEntity] = index;
        }
        aliveEntities_.pop_back();
        aliveEntityToIndex_.erase(aliveIt);
    }

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
