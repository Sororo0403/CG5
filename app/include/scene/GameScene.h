#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "GridPlacementTest.h"
#include <cstdint>

class GameScene : public BaseScene {
  public:
    /// <summary>
    /// ゲームシーンを初期化する
    /// </summary>
    /// <param name="ctx">シーンが利用する共有コンテキスト</param>
    void Initialize(const SceneContext &ctx) override;

    /// <summary>
    /// ゲームシーンを更新する
    /// </summary>
    void Update() override;

    /// <summary>
    /// ゲームシーンを描画する
    /// </summary>
    void Draw() override;

    void OnResize(int width, int height) override;

  private:
    void DrawGBufferScene();
    void DrawOffscreenScene();

    Camera camera_;
    GridPlacementTest gridPlacementTest_;
    uint32_t skyboxModelId_ = 0;
    uint32_t environmentTextureId_ = 0;
    int renderWidth_ = 0;
    int renderHeight_ = 0;
    bool useDeferredRendering_ = true;
};
