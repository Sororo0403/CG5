#pragma once
#include "BaseScene.h"
#include "Camera.h"
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

    void OnResize(int width, int height) override;

  private:
    void UpdatePostEffectControls();
    void UpdateLighting();
    void ApplyDissolveMaterial();
    void DrawPostEffectControls();
    void DrawOffscreenScene();

    Camera camera_;
    Transform modelTransform_;
    uint32_t modelId_ = 0;
    uint32_t skyboxModelId_ = 0;
    uint32_t environmentTextureId_ = 0;
    uint32_t dissolveNoiseTextureId_ = 0;
    int renderWidth_ = 0;
    int renderHeight_ = 0;
    float time_ = 0.0f;
    bool randomNoiseAnimate_ = true;
    float randomNoiseTime_ = 0.0f;
    float randomNoiseStrength_ = 0.12f;
    float randomNoiseScale_ = 360.0f;
    bool dissolveEnabled_ = true;
    bool dissolveAutoAnimate_ = true;
    float dissolveThreshold_ = 0.35f;
    float dissolveEdgeWidth_ = 0.06f;
    float dissolveEdgeColor_[4] = {1.0f, 0.32f, 0.0f, 1.0f};
    bool pointLightAnimate_ = true;
};
