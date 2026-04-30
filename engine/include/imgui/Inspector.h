#pragma once
#include <functional>
#include <string>
#include <vector>

class Camera;
struct Transform;

struct InspectorObject {
    std::string category;
    std::string name;
    void *object = nullptr;
    std::function<void()> drawFunc;
};

/// <summary>
/// Tuning Mode中に選択オブジェクトの情報を表示・編集する軽量Inspector。
/// 完全なEditorではなく、ランタイム調整用の補助UIとして使う。
///
/// 登録対象の寿命が切れる前にClearすること。
/// Inspectorはまずランタイム編集用とし、保存が必要な値はDebugUIRegistryや将来のScene保存へ登録する。
/// </summary>
class Inspector {
  public:
    static Inspector &GetInstance();

    /// <summary>
    /// 登録済みオブジェクトをすべて削除する。
    /// Scene切り替え時など、登録対象の寿命が切れる前に呼び出す。
    /// </summary>
    void Clear();

    void RegisterObject(const std::string &name, void *object,
                        std::function<void()> drawFunc);
    void RegisterObject(const std::string &category, const std::string &name,
                        void *object, std::function<void()> drawFunc);
    void RegisterTransform(const std::string &name, Transform *transform);
    void RegisterCameraInfo(const std::string &name, const Camera *camera);

    void SetVisible(bool visible);
    bool IsVisible() const;

    void Draw();

    static void DrawTransform(const std::string &label, Transform &transform);

  private:
    Inspector() = default;

    std::vector<InspectorObject> objects_;
    int selectedIndex_ = -1;
    bool visible_ = true;
};
