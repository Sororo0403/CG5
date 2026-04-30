#pragma once
#include <string>
#include <vector>

enum class DebugUIValueType {
    Bool,
    Int,
    Float,
    Float2,
    Float3,
    Float4,
    Color3,
    Color4,
    Combo,
};

struct DebugUIItem {
    std::string group;
    std::string label;
    DebugUIValueType type = DebugUIValueType::Bool;
    void *value = nullptr;

    float minFloat = 0.0f;
    float maxFloat = 1.0f;
    int minInt = 0;
    int maxInt = 1;

    std::vector<std::string> comboLabels;
};

/// <summary>
/// Tuning Mode中に表示するデバッグ用GUIの登録所。
/// 各SceneやGameObjectはImGuiを直接書かず、ここに変数を登録する。
/// </summary>
class DebugUIRegistry {
  public:
    static DebugUIRegistry &GetInstance();

    /// <summary>
    /// 登録済み項目をすべて削除する。
    /// Scene切り替え時など、登録した変数の寿命が切れる前に呼び出す。
    /// </summary>
    void Clear();

    void RegisterBool(const std::string &group, const std::string &label,
                      bool *value);
    void RegisterInt(const std::string &group, const std::string &label,
                     int *value, int minValue, int maxValue);
    void RegisterFloat(const std::string &group, const std::string &label,
                       float *value, float minValue, float maxValue);
    void RegisterFloat2(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterFloat3(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterFloat4(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterColor3(const std::string &group, const std::string &label,
                        float *value);
    void RegisterColor4(const std::string &group, const std::string &label,
                        float *value);
    void RegisterCombo(const std::string &group, const std::string &label,
                       int *value, const std::vector<std::string> &labels);

    void Draw();

  private:
    DebugUIRegistry() = default;

    void AddItem(DebugUIItem item);

    std::vector<DebugUIItem> items_;
    std::vector<std::string> groups_;
};
