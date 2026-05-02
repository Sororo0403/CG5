#pragma once
#include "Entity.h"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

/// <summary>
/// Componentを型ごとの連続配列として保持する。
/// </summary>
template <class T>
class ComponentStorage {
  public:
    /// <summary>
    /// EntityがこのComponentを持っているかを判定する。
    /// </summary>
    /// <param name="entity">判定対象のEntity ID。</param>
    /// <returns>Componentを持っている場合はtrue。</returns>
    bool Has(Entity entity) const {
        return entityToIndex_.find(entity) != entityToIndex_.end();
    }

    /// <summary>
    /// EntityにComponentを追加する。
    /// </summary>
    /// <param name="entity">Componentを追加するEntity ID。</param>
    /// <param name="component">追加するComponentデータ。</param>
    /// <returns>追加されたComponentへの参照。</returns>
    T &Add(Entity entity, T component = {}) {
        assert(!Has(entity));

        const uint32_t index = static_cast<uint32_t>(components_.size());
        entities_.push_back(entity);
        components_.push_back(std::move(component));
        entityToIndex_[entity] = index;

        return components_.back();
    }

    /// <summary>
    /// EntityからComponentを削除する。
    /// </summary>
    /// <param name="entity">Componentを削除するEntity ID。</param>
    void Remove(Entity entity) {
        const auto it = entityToIndex_.find(entity);
        if (it == entityToIndex_.end()) {
            return;
        }

        const uint32_t index = it->second;
        const uint32_t lastIndex =
            static_cast<uint32_t>(components_.size() - 1);

        if (index != lastIndex) {
            components_[index] = std::move(components_[lastIndex]);
            entities_[index] = entities_[lastIndex];
            entityToIndex_[entities_[index]] = index;
        }

        components_.pop_back();
        entities_.pop_back();
        entityToIndex_.erase(it);
    }

    /// <summary>
    /// EntityのComponentを取得する。存在しない場合はnullptrを返す。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへのポインタ。存在しない場合はnullptr。</returns>
    T *TryGet(Entity entity) {
        const auto it = entityToIndex_.find(entity);
        if (it == entityToIndex_.end()) {
            return nullptr;
        }
        return &components_[it->second];
    }

    /// <summary>
    /// EntityのComponentを読み取り専用で取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへのポインタ。存在しない場合はnullptr。</returns>
    const T *TryGet(Entity entity) const {
        const auto it = entityToIndex_.find(entity);
        if (it == entityToIndex_.end()) {
            return nullptr;
        }
        return &components_[it->second];
    }

    /// <summary>
    /// EntityのComponentを取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへの参照。</returns>
    T &Get(Entity entity) {
        T *component = TryGet(entity);
        assert(component != nullptr);
        return *component;
    }

    /// <summary>
    /// EntityのComponentを読み取り専用で取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへの参照。</returns>
    const T &Get(Entity entity) const {
        const T *component = TryGet(entity);
        assert(component != nullptr);
        return *component;
    }

    /// <summary>
    /// Componentを持つEntity配列を取得する。
    /// </summary>
    /// <returns>Entity IDの連続配列。</returns>
    const std::vector<Entity> &Entities() const { return entities_; }

    /// <summary>
    /// Componentの連続配列を取得する。
    /// </summary>
    /// <returns>Componentの連続配列。</returns>
    std::vector<T> &Components() { return components_; }

    /// <summary>
    /// Componentの連続配列を読み取り専用で取得する。
    /// </summary>
    /// <returns>Componentの連続配列。</returns>
    const std::vector<T> &Components() const { return components_; }

  private:
    std::vector<Entity> entities_;
    std::vector<T> components_;
    std::unordered_map<Entity, uint32_t> entityToIndex_;
};
