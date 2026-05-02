#pragma once
#include "World.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

/// <summary>
/// World走査中に発生したEntity/Componentの構造変更を後段で適用する。
/// </summary>
class WorldCommandBuffer {
  public:
    /// <summary>
    /// 任意のWorld操作を予約する。
    /// </summary>
    template <class Fn>
    void Enqueue(Fn &&command) {
        using Command = std::decay_t<Fn>;
        commands_.push_back(
            std::make_unique<CommandModel<Command>>(std::forward<Fn>(command)));
    }

    /// <summary>
    /// Entity生成を予約し、生成直後に初期化処理を実行する。
    /// </summary>
    void CreateEntity() {
        Enqueue([](World &world) {
            world.CreateEntity();
        });
    }

    /// <summary>
    /// Entity生成を予約し、生成直後に初期化処理を実行する。
    /// </summary>
    template <class Setup>
    void CreateEntity(Setup &&setup) {
        Enqueue([setup = std::forward<Setup>(setup)](World &world) mutable {
            Entity entity = world.CreateEntity();
            setup(world, entity);
        });
    }

    /// <summary>
    /// Entity破棄を予約する。
    /// </summary>
    void DestroyEntity(Entity entity) {
        Enqueue([entity](World &world) {
            world.DestroyEntity(entity);
        });
    }

    /// <summary>
    /// Component追加を予約する。
    /// </summary>
    template <class T>
    void Add(Entity entity, T component = {}) {
        Enqueue([entity, component = std::move(component)](World &world) mutable {
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
        Enqueue([entity, component = std::move(component)](World &world) mutable {
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
        Enqueue([entity](World &world) {
            if (world.IsAlive(entity) && world.Has<T>(entity)) {
                world.Remove<T>(entity);
            }
        });
    }

    /// <summary>
    /// 予約済み操作を追加順に適用する。
    /// </summary>
    void Playback(World &world) {
        constexpr uint32_t kMaxPlaybackBatches = 1024;
        uint32_t batchCount = 0;

        while (!commands_.empty()) {
            if (++batchCount > kMaxPlaybackBatches) {
                throw std::logic_error(
                    "WorldCommandBuffer playback exceeded the batch limit. "
                    "A command may be re-enqueuing itself indefinitely.");
            }

            std::vector<std::unique_ptr<ICommand>> pending;
            pending.swap(commands_);

            for (auto &command : pending) {
                command->Execute(world);
            }
        }
    }

    bool Empty() const { return commands_.empty(); }
    void Clear() { commands_.clear(); }

  private:
    struct ICommand {
        virtual ~ICommand() = default;
        virtual void Execute(World &world) = 0;
    };

    template <class Fn>
    struct CommandModel final : ICommand {
        explicit CommandModel(Fn command) : command(std::move(command)) {}

        void Execute(World &world) override { command(world); }

        Fn command;
    };

    std::vector<std::unique_ptr<ICommand>> commands_;
};
