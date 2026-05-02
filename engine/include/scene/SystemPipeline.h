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

    void AddUpdateStep(Step step) { updateSteps_.push_back(std::move(step)); }
    void AddDrawStep(Step step) { drawSteps_.push_back(std::move(step)); }

    void Update() {
        for (auto &step : updateSteps_) {
            step();
        }
    }

    void Update(World &world, WorldCommandBuffer &commandBuffer) {
        Update();
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
    std::vector<Step> updateSteps_;
    std::vector<Step> drawSteps_;
};
