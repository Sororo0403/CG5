#include "SceneSerializer.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

namespace {

constexpr int kCurrentSceneVersion = 1;

void SetMessage(std::string *message, const std::string &text) {
    if (message) {
        *message = text;
    }
}

bool ReadFloat3(const nlohmann::json &json, DirectX::XMFLOAT3 &value) {
    if (!json.is_array() || json.size() != 3) {
        return false;
    }
    for (const nlohmann::json &element : json) {
        if (!element.is_number()) {
            return false;
        }
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    return true;
}

bool ReadFloat4(const nlohmann::json &json, DirectX::XMFLOAT4 &value) {
    if (!json.is_array() || json.size() != 4) {
        return false;
    }
    for (const nlohmann::json &element : json) {
        if (!element.is_number()) {
            return false;
        }
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    value.w = json[3].get<float>();
    return true;
}

bool LoadObject(const nlohmann::json &objectJson, EditableSceneObject &object,
                std::string *message) {
    if (!objectJson.is_object()) {
        SetMessage(message, "Invalid scene json: object must be object");
        return false;
    }
    if (!objectJson.contains("kind") || !objectJson["kind"].is_string()) {
        SetMessage(message, "Invalid scene json: object kind must be string");
        return false;
    }

    object.id = objectJson.value("id", 0ull);
    object.name = objectJson.value("name", std::string{});
    object.kind = objectJson.value("kind", std::string{});
    object.gridX = objectJson.value("gridX", 0);
    object.gridY = objectJson.value("gridY", 0);
    object.collider = objectJson.value("collider", std::string{});
    object.locked = objectJson.value("locked", false);
    object.visible = objectJson.value("visible", true);

    const std::string mapCode =
        objectJson.value("mapCode", std::string{"0"});
    object.mapCode = mapCode.empty() ? '0' : mapCode[0];

    if (!objectJson.contains("transform") ||
        !objectJson["transform"].is_object()) {
        SetMessage(message, "Invalid object transform");
        return false;
    }

    const nlohmann::json &transformJson = objectJson["transform"];
    if (!transformJson.contains("position") ||
        !ReadFloat3(transformJson["position"], object.transform.position) ||
        !transformJson.contains("rotation") ||
        !ReadFloat4(transformJson["rotation"], object.transform.rotation) ||
        !transformJson.contains("scale") ||
        !ReadFloat3(transformJson["scale"], object.transform.scale)) {
        SetMessage(message, "Invalid object transform");
        return false;
    }

    return true;
}

} // namespace

bool SceneSerializer::Load(const std::filesystem::path &path,
                           EditableSceneDocument &outDocument,
                           std::string *message) {
    std::ifstream file(path);
    if (!file) {
        SetMessage(message, "Failed to open file: " + path.generic_string());
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception &e) {
        SetMessage(message, "Invalid scene json in " + path.generic_string() +
                                ": " + e.what());
        return false;
    }

    if (!root.is_object()) {
        SetMessage(message, "Invalid scene json: root must be object");
        return false;
    }

    EditableSceneDocument document{};
    if (root.contains("version")) {
        if (!root["version"].is_number_integer()) {
            SetMessage(message, "Invalid scene json: version must be integer");
            return false;
        }
        document.hasVersion = true;
        document.version = root["version"].get<int>();
        if (document.version > kCurrentSceneVersion) {
            SetMessage(message, "Unsupported scene version " +
                                    std::to_string(document.version) +
                                    " in " + path.generic_string() +
                                    " (current: " +
                                    std::to_string(kCurrentSceneVersion) + ")");
            return false;
        }
    }

    document.name = root.value("name", std::string{"game_stage"});
    document.tileSize = root.value("tileSize", 1.0f);

    if (!root.contains("rows") || !root["rows"].is_array()) {
        SetMessage(message, "Invalid scene json: rows must be array");
        return false;
    }
    for (const nlohmann::json &row : root["rows"]) {
        if (!row.is_string()) {
            SetMessage(message, "Invalid scene json: rows must be strings");
            return false;
        }
        document.rows.push_back(row.get<std::string>());
    }
    if (document.rows.empty()) {
        SetMessage(message, "Invalid scene json: rows must not be empty");
        return false;
    }

    if (root.contains("objects")) {
        if (!root["objects"].is_array()) {
            SetMessage(message, "Invalid scene json: objects must be array");
            return false;
        }
        document.hasObjects = true;
        for (const nlohmann::json &objectJson : root["objects"]) {
            EditableSceneObject object{};
            if (!LoadObject(objectJson, object, message)) {
                return false;
            }
            document.objects.push_back(object);
        }
    }

    outDocument = std::move(document);
    if (outDocument.hasVersion) {
        SetMessage(message, "Loaded scene: " + path.generic_string());
    } else {
        SetMessage(message,
                   "Loaded legacy scene format: " + path.generic_string());
    }
    return true;
}

bool SceneSerializer::Save(const std::filesystem::path &path,
                           const EditableSceneDocument &document,
                           std::string *message) {
    if (path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            SetMessage(message, "Failed to create scene directory: " +
                                    path.parent_path().generic_string() +
                                    " (" + error.message() + ")");
            return false;
        }
    }

    nlohmann::json root;
    root["version"] = kCurrentSceneVersion;
    root["name"] = document.name;
    root["tileSize"] = document.tileSize;
    root["legend"] = {
        {"0", "empty"},
        {"1", "floor"},
        {"2", "wall"},
        {"3", "player_start"},
        {"4", "goal_marker"},
    };
    root["rows"] = document.rows;
    root["objects"] = nlohmann::json::array();

    for (const EditableSceneObject &object : document.objects) {
        nlohmann::json objectJson;
        objectJson["id"] = object.id;
        objectJson["name"] = object.name;
        objectJson["kind"] = object.kind;
        objectJson["gridX"] = object.gridX;
        objectJson["gridY"] = object.gridY;
        objectJson["mapCode"] = std::string(1, object.mapCode);
        objectJson["collider"] = object.collider;
        objectJson["locked"] = object.locked;
        objectJson["visible"] = object.visible;
        objectJson["transform"] = {
            {"position",
             {object.transform.position.x, object.transform.position.y,
              object.transform.position.z}},
            {"rotation",
             {object.transform.rotation.x, object.transform.rotation.y,
              object.transform.rotation.z, object.transform.rotation.w}},
            {"scale",
             {object.transform.scale.x, object.transform.scale.y,
              object.transform.scale.z}},
        };
        root["objects"].push_back(objectJson);
    }

    std::ofstream file(path);
    if (!file) {
        SetMessage(message, "Failed to open file: " + path.generic_string());
        return false;
    }

    file << root.dump(2);
    if (!file) {
        SetMessage(message, "Failed to write file: " + path.generic_string());
        return false;
    }
    SetMessage(message, "Saved scene: " + path.generic_string());
    return true;
}
