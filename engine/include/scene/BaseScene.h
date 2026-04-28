#pragma once
#include "SceneContext.h"

class SceneManager;

/// <summary>
/// すべてのシーン実装が継承する基底クラス
/// </summary>
class BaseScene {
  public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    virtual ~BaseScene() = default;

    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="ctx">シーンコンテキスト</param>
    virtual void Initialize(const SceneContext &ctx) { ctx_ = &ctx; }

    /// <summary>
    /// 更新処理
    /// </summary>
    virtual void Update() = 0;

    /// <summary>
    /// 描画処理
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// 描画領域のサイズ変更通知
    /// </summary>
    virtual void OnResize(int width, int height) {
        (void)width;
        (void)height;
    }

    /// <summary>
    /// シーンマネージャーを設定する
    /// </summary>
    /// <param name="sceneManager">関連付けるシーンマネージャー</param>
    void SetSceneManager(SceneManager *sceneManager) {
        sceneManager_ = sceneManager;
    }

  protected:
    SceneManager *sceneManager_ = nullptr;
    const SceneContext *ctx_ = nullptr;
};
