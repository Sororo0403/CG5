#pragma once
#include "BaseScene.h"
#include "SystemPipeline.h"
#include "World.h"
#include "WorldCommandBuffer.h"

/// <summary>
/// WorldとSystemPipelineを持つECS向けScene基底。
/// </summary>
class EcsScene : public BaseScene {
  public:
    void Initialize(const SceneContext &ctx) override {
        BaseScene::Initialize(ctx);
        OnInitialize();
    }

    void Update() override { pipeline_.Update(world_, commandBuffer_); }
    void Draw() override { pipeline_.Draw(); }

  protected:
    virtual void OnInitialize() {}

    World world_;
    WorldCommandBuffer commandBuffer_;
    SystemPipeline pipeline_;
};
