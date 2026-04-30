#pragma once
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

struct Transform;

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
    Text,
    ReadonlyFloat,
    ReadonlyInt,
};

enum class DebugUIControlType {
    Slider,
    Drag,
};

struct DebugUIItem {
    std::string group;
    std::string label;
    DebugUIValueType type = DebugUIValueType::Bool;
    void *value = nullptr;

    float minFloat = 0.0f;
    float maxFloat = 1.0f;
    float speed = 0.01f;
    int minInt = 0;
    int maxInt = 1;
    DebugUIControlType control = DebugUIControlType::Slider;

    std::vector<std::string> comboLabels;
    std::function<std::string()> textGetter;
    std::function<float()> floatGetter;
    std::function<int()> intGetter;
};

/// <summary>
/// Tuning Mode中に表示するデバッグ用GUIの登録所。
/// 各SceneやGameObjectはImGuiを直接書かず、ここに変数を登録する。
///
/// 使用例:
/// DebugUIRegistry::GetInstance().RegisterFloat("Player", "Speed", &speed, 0.0f, 20.0f);
/// DebugUIRegistry::GetInstance().RegisterBool("Renderer", "Use Deferred", &useDeferred);
///
/// 登録した変数の寿命が切れる前にClearすること。
/// 保存したい値は将来的にJSON保存対象にし、setterが必要な値はUpdate側で反映する。
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
    void RegisterSliderInt(const std::string &group, const std::string &label,
                           int *value, int minValue, int maxValue);
    void RegisterFloat(const std::string &group, const std::string &label,
                       float *value, float minValue, float maxValue);
    void RegisterSliderFloat(const std::string &group,
                             const std::string &label, float *value,
                             float minValue, float maxValue);
    void RegisterDragFloat(const std::string &group, const std::string &label,
                           float *value, float speed, float minValue,
                           float maxValue);
    void RegisterFloat2(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterFloat3(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterFloat4(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue);
    void RegisterDragFloat3(const std::string &group, const std::string &label,
                            float *value, float speed);
    void RegisterTransform(const std::string &group, const std::string &label,
                           Transform *transform);
    void RegisterColor3(const std::string &group, const std::string &label,
                        float *value);
    void RegisterColor4(const std::string &group, const std::string &label,
                        float *value);
    void RegisterCombo(const std::string &group, const std::string &label,
                       int *value, const std::vector<std::string> &labels);
    void RegisterCombo(const std::string &group, const std::string &label,
                       int *value,
                       std::initializer_list<const char *> labels);
    void RegisterText(const std::string &group, const std::string &label,
                      std::function<std::string()> getter);
    void RegisterReadonlyFloat(const std::string &group,
                               const std::string &label,
                               std::function<float()> getter);
    void RegisterReadonlyInt(const std::string &group,
                             const std::string &label,
                             std::function<int()> getter);

    void SetVisible(bool visible);
    bool IsVisible() const;

    void Draw();

  private:
    DebugUIRegistry() = default;

    void AddItem(DebugUIItem item);

    std::vector<DebugUIItem> items_;
    std::vector<std::string> groups_;
    bool visible_ = true;
};
