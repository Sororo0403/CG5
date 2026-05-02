#pragma once
#include "ComponentStorage.h"
#include "Entity.h"

#include <any>
#include <cassert>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/// <summary>
/// EntityとComponentStorageを管理するECSの中心。
/// </summary>
class World {
  public:
    /// <summary>
    /// 新しいEntity IDを生成する。
    /// </summary>
    /// <returns>生成されたEntity ID。</returns>
    Entity CreateEntity();

    /// <summary>
    /// Entityと、そのEntityが持つ全Componentを削除する。
    /// </summary>
    /// <param name="entity">削除するEntity ID。</param>
    void DestroyEntity(Entity entity);

    /// <summary>
    /// Entityが生存しているかを判定する。
    /// </summary>
    /// <param name="entity">判定対象のEntity ID。</param>
    /// <returns>生存している場合はtrue。</returns>
    bool IsAlive(Entity entity) const;

    /// <summary>
    /// EntityにComponentを追加する。
    /// </summary>
    /// <param name="entity">Componentを追加するEntity ID。</param>
    /// <param name="component">追加するComponentデータ。</param>
    /// <returns>追加されたComponentへの参照。</returns>
    template <class T>
    T &Add(Entity entity, T component = {}) {
        AssertCanChangeStructure();
        assert(IsAlive(entity));
        if (!IsAlive(entity)) {
            throw std::logic_error("Cannot add a component to a dead Entity.");
        }
        return Storage<T>().Add(entity, std::move(component));
    }

    /// <summary>
    /// EntityからComponentを削除する。
    /// </summary>
    /// <param name="entity">Componentを削除するEntity ID。</param>
    template <class T>
    void Remove(Entity entity) {
        AssertCanChangeStructure();
        Storage<T>().Remove(entity);
    }

    /// <summary>
    /// Entityが指定Componentを持っているかを判定する。
    /// </summary>
    /// <param name="entity">判定対象のEntity ID。</param>
    /// <returns>Componentを持っている場合はtrue。</returns>
    template <class T>
    bool Has(Entity entity) const {
        const ComponentStorage<T> *storage = TryStorage<T>();
        return storage != nullptr && storage->Has(entity);
    }

    /// <summary>
    /// EntityのComponentを取得する。存在しない場合はnullptrを返す。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへのポインタ。存在しない場合はnullptr。</returns>
    template <class T>
    T *TryGet(Entity entity) {
        ComponentStorage<T> *storage = TryStorage<T>();
        return storage == nullptr ? nullptr : storage->TryGet(entity);
    }

    /// <summary>
    /// EntityのComponentを読み取り専用で取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへのポインタ。存在しない場合はnullptr。</returns>
    template <class T>
    const T *TryGet(Entity entity) const {
        const ComponentStorage<T> *storage = TryStorage<T>();
        return storage == nullptr ? nullptr : storage->TryGet(entity);
    }

    /// <summary>
    /// EntityのComponentを取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへの参照。</returns>
    template <class T>
    T &Get(Entity entity) {
        T *component = TryGet<T>(entity);
        assert(component != nullptr);
        if (component == nullptr) {
            throw std::logic_error("Entity does not have the requested component.");
        }
        return *component;
    }

    /// <summary>
    /// EntityのComponentを読み取り専用で取得する。
    /// </summary>
    /// <param name="entity">取得対象のEntity ID。</param>
    /// <returns>Componentへの参照。</returns>
    template <class T>
    const T &Get(Entity entity) const {
        const T *component = TryGet<T>(entity);
        assert(component != nullptr);
        if (component == nullptr) {
            throw std::logic_error("Entity does not have the requested component.");
        }
        return *component;
    }

    /// <summary>
    /// 指定Componentをすべて持つEntityを列挙する。
    /// </summary>
    /// <param name="fn">EntityとComponent参照を受け取る関数。</param>
    template <class... Components, class Fn>
    void View(Fn &&fn) {
        static_assert(sizeof...(Components) > 0,
                      "World::View requires at least one component type.");

        using First =
            std::tuple_element_t<0, std::tuple<Components...>>;

        ComponentStorage<First> *firstStorage = TryStorage<First>();
        if (firstStorage == nullptr) {
            return;
        }

        ViewScope viewScope(*this);
        const std::vector<Entity> &entities = firstStorage->Entities();
        for (Entity entity : entities) {
            if (IsAlive(entity) && (Has<Components>(entity) && ...)) {
                fn(entity, Get<Components>(entity)...);
            }
        }
    }

    /// <summary>
    /// 生存中のEntity配列を取得する。
    /// </summary>
    /// <returns>生存中Entity IDの配列。</returns>
    const std::vector<Entity> &AliveEntities() const;

  private:
    void AssertCanChangeStructure() const {
        assert(viewDepth_ == 0 &&
               "Do not change World structure while iterating a View. Use "
               "WorldCommandBuffer and play it back after systems run.");
        if (viewDepth_ != 0) {
            throw std::logic_error(
                "Do not change World structure while iterating a View. Use "
                "WorldCommandBuffer and play it back after systems run.");
        }
    }

    struct ViewScope {
        explicit ViewScope(World &world) : world(world) { ++world.viewDepth_; }
        ~ViewScope() { --world.viewDepth_; }

        World &world;
    };

    struct StorageEntry {
        std::any storage;
        void (*remove)(std::any &, Entity) = nullptr;
    };

    template <class T>
    ComponentStorage<T> &Storage() {
        const std::type_index key(typeid(T));
        auto it = storages_.find(key);
        if (it == storages_.end()) {
            StorageEntry entry;
            entry.storage = ComponentStorage<T>{};
            entry.remove = [](std::any &storage, Entity entity) {
                std::any_cast<ComponentStorage<T> &>(storage).Remove(entity);
            };

            it = storages_.emplace(key, std::move(entry)).first;
        }

        return std::any_cast<ComponentStorage<T> &>(it->second.storage);
    }

    template <class T>
    ComponentStorage<T> *TryStorage() {
        const std::type_index key(typeid(T));
        auto it = storages_.find(key);
        if (it == storages_.end()) {
            return nullptr;
        }
        return &std::any_cast<ComponentStorage<T> &>(it->second.storage);
    }

    template <class T>
    const ComponentStorage<T> *TryStorage() const {
        const std::type_index key(typeid(T));
        auto it = storages_.find(key);
        if (it == storages_.end()) {
            return nullptr;
        }
        return &std::any_cast<const ComponentStorage<T> &>(it->second.storage);
    }

  private:
    std::vector<uint32_t> generations_{0};
    std::vector<Entity> aliveEntities_;
    std::unordered_map<Entity, uint32_t> aliveEntityToIndex_;
    std::vector<uint32_t> freeIndices_;
    std::unordered_set<Entity> aliveSet_;
    std::unordered_map<std::type_index, StorageEntry> storages_;
    uint32_t viewDepth_ = 0;
};
