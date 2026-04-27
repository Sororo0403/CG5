#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "PostEffectRenderer.h"
#include "RenderTexture.h"
#include "SkyboxRenderer.h"
#include "Sprite.h"
#include "Transform.h"
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

  private:
    void ResizeOffscreenIfNeeded();
    void DrawOffscreenScene();

    RenderTexture renderTexture_;
    PostEffectRenderer postEffectRenderer_;
    Camera camera_;
    SkyboxRenderer skyboxRenderer_;
    Transform modelTransform_;
    uint32_t modelId_ = 0;
    uint32_t skyboxModelId_ = 0;
    uint32_t environmentTextureId_ = 0;
    int renderWidth_ = 0;
    int renderHeight_ = 0;
    float time_ = 0.0f;
};
