#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
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
    std::string kind;
    int gridX = 0;
    int gridY = 0;
    char mapCode = '0';
    std::string collider;
    bool locked = false;
    bool visible = true;
    EditableSceneTransform transform;
};

struct EditableSceneDocument {
    int version = 1;
    bool hasVersion = false;
    bool hasObjects = false;
    std::string name = "game_stage";
    float tileSize = 1.0f;
    std::vector<std::string> rows;
    std::vector<EditableSceneObject> objects;
};

class SceneSerializer {
  public:
    static bool Load(const std::filesystem::path &path,
                     EditableSceneDocument &outDocument,
                     std::string *message);
    static bool Save(const std::filesystem::path &path,
                     const EditableSceneDocument &document,
                     std::string *message);
};
