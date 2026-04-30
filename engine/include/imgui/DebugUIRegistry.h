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
    bool serializable = true;

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

    bool initialBool = false;
    int initialInt = 0;
    std::vector<float> initialFloats;
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
/// Scene固有の値はapp側で登録し、保存処理本体はengine側で行う。
/// 保存したい値はJSON保存対象にし、setterが必要な値はUpdate側で反映する。
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
                      bool *value, bool serializable = true);
    void RegisterInt(const std::string &group, const std::string &label,
                     int *value, int minValue, int maxValue,
                     bool serializable = true);
    void RegisterSliderInt(const std::string &group, const std::string &label,
                           int *value, int minValue, int maxValue,
                           bool serializable = true);
    void RegisterFloat(const std::string &group, const std::string &label,
                       float *value, float minValue, float maxValue,
                       bool serializable = true);
    void RegisterSliderFloat(const std::string &group,
                             const std::string &label, float *value,
                             float minValue, float maxValue,
                             bool serializable = true);
    void RegisterDragFloat(const std::string &group, const std::string &label,
                           float *value, float speed, float minValue,
                           float maxValue, bool serializable = true);
    void RegisterFloat2(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue,
                        bool serializable = true);
    void RegisterFloat3(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue,
                        bool serializable = true);
    void RegisterFloat4(const std::string &group, const std::string &label,
                        float *value, float minValue, float maxValue,
                        bool serializable = true);
    void RegisterDragFloat3(const std::string &group, const std::string &label,
                            float *value, float speed,
                            bool serializable = true);
    void RegisterTransform(const std::string &group, const std::string &label,
                           Transform *transform, bool serializable = true);
    void RegisterColor3(const std::string &group, const std::string &label,
                        float *value, bool serializable = true);
    void RegisterColor4(const std::string &group, const std::string &label,
                        float *value, bool serializable = true);
    void RegisterCombo(const std::string &group, const std::string &label,
                       int *value, const std::vector<std::string> &labels,
                       bool serializable = true);
    void RegisterCombo(const std::string &group, const std::string &label,
                       int *value,
                       std::initializer_list<const char *> labels,
                       bool serializable = true);
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

    /// <summary>
    /// DebugUIRegistryに登録した調整値をJSONへ保存する。
    /// 読み取り専用項目は保存対象にしない。
    /// </summary>
    bool SaveToJson(const std::string &path) const;
    bool LoadFromJson(const std::string &path);
    void ResetSerializableValues();

    void Draw();

  private:
    DebugUIRegistry() = default;

    void AddItem(DebugUIItem item);
    void CaptureInitialValue(DebugUIItem &item) const;
    static int GetFloatComponentCount(DebugUIValueType type);

    std::vector<DebugUIItem> items_;
    std::vector<std::string> groups_;
    bool visible_ = true;
    mutable std::string statusMessage_;
};
