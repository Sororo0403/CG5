#pragma once
#include "WorldCommandBuffer.h"

#include <functional>
#include <utility>
#include <vector>

/// <summary>
/// Sceneが実行するSystem順を保持する薄いパイプライン。
/// </summary>
class SystemPipeline {
  public:
    using Step = std::function<void()>;
    using WorldStep = std::function<void(World &, WorldCommandBuffer &)>;

    void AddUpdateStep(Step step) {
        updateSteps_.push_back(
            [step = std::move(step)](World &, WorldCommandBuffer &) {
                step();
            });
    }
    void AddWorldUpdateStep(WorldStep step) {
        updateSteps_.push_back(std::move(step));
    }
    void AddCommandFlushStep() {
        updateSteps_.push_back([](World &world,
                                  WorldCommandBuffer &commandBuffer) {
            commandBuffer.Playback(world);
        });
    }
    void AddDrawStep(Step step) { drawSteps_.push_back(std::move(step)); }

    void Update(World &world, WorldCommandBuffer &commandBuffer) {
        for (auto &step : updateSteps_) {
            step(world, commandBuffer);
        }
        commandBuffer.Playback(world);
    }

    void Draw() {
        for (auto &step : drawSteps_) {
            step();
        }
    }

    void Clear() {
        updateSteps_.clear();
        drawSteps_.clear();
    }

  private:
    std::vector<WorldStep> updateSteps_;
    std::vector<Step> drawSteps_;
};
