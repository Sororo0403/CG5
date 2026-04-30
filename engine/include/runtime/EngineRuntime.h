#pragma once

enum class EngineRuntimeMode {
    Gameplay,
    Editor,
    Play = Gameplay,
    Tuning = Editor,
};

struct EngineRuntimeSettings {
    EngineRuntimeMode mode = EngineRuntimeMode::Gameplay;

    /// <summary>
    /// 調整用GUIを表示するか。
    /// </summary>
    bool showDebugUI = false;

    /// <summary>
    /// 調整モード中にゲーム進行を停止するか。
    /// </summary>
    bool pauseGameInTuningMode = true;

    /// <summary>
    /// GUI操作中に入力をGUI側で扱うか。
    /// </summary>
    bool guiCapturesInput = true;

    /// <summary>
    /// ゲーム進行に掛ける時間倍率。
    /// </summary>
    float timeScale = 1.0f;

    bool viewportInputEnabled = true;
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float viewportMouseX = 0.0f;
    float viewportMouseY = 0.0f;
    bool viewportClicked = false;
};

/// <summary>
/// エンジン全体の実行モードを管理する。
/// Gameplay Modeでは通常のゲーム実行を行い、Editor ModeではGUIによる編集やデバッグ表示を行う。
/// </summary>
class EngineRuntime {
  public:
    static EngineRuntime &GetInstance();

    void ToggleMode();
    void SetMode(EngineRuntimeMode mode);
    EngineRuntimeMode GetMode() const;

    bool IsPlayMode() const;
    bool IsTuningMode() const;
    bool IsGameplayMode() const;
    bool IsEditorMode() const;

    EngineRuntimeSettings &Settings();
    const EngineRuntimeSettings &Settings() const;

  private:
    EngineRuntime() = default;

    EngineRuntimeSettings settings_;
};
