#pragma once
#include "World.h"

#include <functional>
#include <utility>
#include <vector>

/// <summary>
/// World走査中に発生したEntity/Componentの構造変更を後段で適用する。
/// </summary>
class WorldCommandBuffer {
  public:
    using Command = std::function<void(World &)>;

    /// <summary>
    /// 任意のWorld操作を予約する。
    /// </summary>
    void Enqueue(Command command) { commands_.push_back(std::move(command)); }

    /// <summary>
    /// Entity生成を予約し、生成直後に初期化処理を実行する。
    /// </summary>
    void CreateEntity(std::function<void(World &, Entity)> setup = {}) {
        commands_.push_back([setup = std::move(setup)](World &world) {
            Entity entity = world.CreateEntity();
            if (setup) {
                setup(world, entity);
            }
        });
    }

    /// <summary>
    /// Entity破棄を予約する。
    /// </summary>
    void DestroyEntity(Entity entity) {
        commands_.push_back([entity](World &world) {
            world.DestroyEntity(entity);
        });
    }

    /// <summary>
    /// Component追加を予約する。
    /// </summary>
    template <class T>
    void Add(Entity entity, T component = {}) {
        commands_.push_back(
            [entity, component = std::move(component)](World &world) mutable {
                if (world.IsAlive(entity) && !world.Has<T>(entity)) {
                    world.Add<T>(entity, std::move(component));
                }
            });
    }

    /// <summary>
    /// Component追加または上書きを予約する。
    /// </summary>
    template <class T>
    void AddOrReplace(Entity entity, T component = {}) {
        commands_.push_back(
            [entity, component = std::move(component)](World &world) mutable {
                if (!world.IsAlive(entity)) {
                    return;
                }
                if (world.Has<T>(entity)) {
                    world.Get<T>(entity) = std::move(component);
                } else {
                    world.Add<T>(entity, std::move(component));
                }
            });
    }

    /// <summary>
    /// Component削除を予約する。
    /// </summary>
    template <class T>
    void Remove(Entity entity) {
        commands_.push_back([entity](World &world) {
            if (world.IsAlive(entity) && world.Has<T>(entity)) {
                world.Remove<T>(entity);
            }
        });
    }

    /// <summary>
    /// 予約済み操作を追加順に適用する。
    /// </summary>
    void Playback(World &world) {
        for (auto &command : commands_) {
            command(world);
        }
        commands_.clear();
    }

    bool Empty() const { return commands_.empty(); }
    void Clear() { commands_.clear(); }

  private:
    std::vector<Command> commands_;
};
