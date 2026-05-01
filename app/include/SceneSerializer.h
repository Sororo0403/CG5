#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct EditableSceneTransform {
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
};

struct EditableSceneObject {
    uint64_t id = 0;
    std::string name;
    std::string type;
    bool enabled = true;
    bool locked = false;
    bool hasTransform = false;
    EditableSceneTransform transform;

    struct GridPlacementComponent {
        bool enabled = true;
        int gridX = 0;
        int gridY = 0;
        char mapCode = '0';
    } gridPlacement;

    struct ColliderComponent {
        bool enabled = false;
        std::string mode;
    } collider;
};

struct EditableSceneDocument {
    struct Settings {
        float tileSize = 1.0f;
    };

    struct Grid {
        std::vector<std::string> rows;
        std::map<std::string, std::string> legend;
    };

    int version = 2;
    bool hasVersion = false;
    bool hasObjects = false;
    std::string name = "game_stage";
    Settings settings;
    Grid grid;
    std::vector<EditableSceneObject> objects;
};

class SceneSerializer {
  public:
    static bool Load(const std::filesystem::path &path,
                     EditableSceneDocument &outDocument,
                     std::string *message);
    static bool LoadFromString(const std::string &text,
                               EditableSceneDocument &outDocument,
                               std::string *message);
    static bool Save(const std::filesystem::path &path,
                     const EditableSceneDocument &document,
                     std::string *message);
    static bool SaveToString(const EditableSceneDocument &document,
                             std::string &outText,
                             std::string *message);
};
