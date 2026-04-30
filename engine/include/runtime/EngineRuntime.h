#pragma once

enum class EngineRuntimeMode {
    Play,
    Tuning,
};

struct EngineRuntimeSettings {
    EngineRuntimeMode mode = EngineRuntimeMode::Play;

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
};

/// <summary>
/// エンジン全体の実行モードを管理する。
/// Play Modeでは通常のゲーム実行を行い、Tuning ModeではGUIによる調整やデバッグ表示を行う。
/// </summary>
class EngineRuntime {
  public:
    static EngineRuntime &GetInstance();

    void ToggleMode();
    void SetMode(EngineRuntimeMode mode);
    EngineRuntimeMode GetMode() const;

    bool IsPlayMode() const;
    bool IsTuningMode() const;

    EngineRuntimeSettings &Settings();
    const EngineRuntimeSettings &Settings() const;

  private:
    EngineRuntime() = default;

    EngineRuntimeSettings settings_;
};
