#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "GridPlacementTest.h"
#include "EngineRuntime.h"
#ifdef _DEBUG
#include "EditorLayer.h"
#endif // _DEBUG
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
    void RegisterDebugUI();
    void HandleRuntimeModeChanged();
    void ApplyTuning();
    void DrawGBufferScene();
    void DrawOffscreenScene();

    Camera camera_;
    GridPlacementTest gridPlacementTest_;
#ifdef _DEBUG
    EditorLayer editorLayer_;
#endif // _DEBUG
    uint32_t skyboxModelId_ = 0;
    uint32_t environmentTextureId_ = 0;
    int renderWidth_ = 0;
    int renderHeight_ = 0;
    bool useDeferredRendering_ = true;
    float radialBlurStrength_ = 0.0f;
    float randomStrength_ = 0.0f;
    float pointLight0Intensity_ = 1.8f;
    float pointLight1Intensity_ = 1.2f;
    float ambientStrength_ = 0.20f;
#ifdef _DEBUG
    EngineRuntimeMode previousRuntimeMode_ = EngineRuntimeMode::Gameplay;
#endif // _DEBUG
};
