#include "SceneSerializer.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

namespace {

constexpr int kCurrentSceneVersion = 2;

void SetMessage(std::string *message, const std::string &text) {
    if (message) {
        *message = text;
    }
}

std::map<std::string, std::string> MakeDefaultGridLegend() {
    return {
        {"0", "empty"},
        {"1", "floor"},
        {"2", "wall"},
        {"3", "player_start"},
        {"4", "goal_marker"},
    };
}

bool ReadFloat3(const nlohmann::json &json, DirectX::XMFLOAT3 &value,
                std::string *message, const std::string &fieldName) {
    if (!json.is_array() || json.size() != 3) {
        SetMessage(message, "Invalid transform " + fieldName +
                                ": expected array of 3 numbers");
        return false;
    }
    for (const nlohmann::json &element : json) {
        if (!element.is_number()) {
            SetMessage(message, "Invalid transform " + fieldName +
                                    ": expected array of 3 numbers");
            return false;
        }
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    return true;
}

bool ReadFloat4(const nlohmann::json &json, DirectX::XMFLOAT4 &value,
                std::string *message, const std::string &fieldName) {
    if (!json.is_array() || json.size() != 4) {
        SetMessage(message, "Invalid transform " + fieldName +
                                ": expected array of 4 numbers");
        return false;
    }
    for (const nlohmann::json &element : json) {
        if (!element.is_number()) {
            SetMessage(message, "Invalid transform " + fieldName +
                                    ": expected array of 4 numbers");
            return false;
        }
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    value.w = json[3].get<float>();
    return true;
}

bool ReadTransform(const nlohmann::json &objectJson,
                   EditableSceneObject &object, std::string *message) {
    if (!objectJson.contains("transform")) {
        object.hasTransform = false;
        return true;
    }
    if (!objectJson["transform"].is_object()) {
        SetMessage(message, "Invalid object transform: transform must be object");
        return false;
    }

    const nlohmann::json &transformJson = objectJson["transform"];
    if (!transformJson.contains("position") ||
        !ReadFloat3(transformJson["position"], object.transform.position,
                    message, "position") ||
        !transformJson.contains("rotation") ||
        !ReadFloat4(transformJson["rotation"], object.transform.rotation,
                    message, "rotation") ||
        !transformJson.contains("scale") ||
        !ReadFloat3(transformJson["scale"], object.transform.scale, message,
                    "scale")) {
        return false;
    }
    object.hasTransform = true;
    return true;
}

bool ReadRows(const nlohmann::json &rowsJson, const std::string &sourceName,
              std::vector<std::string> &rows, std::string *message) {
    if (!rowsJson.is_array()) {
        SetMessage(message, "Invalid scene json in " + sourceName +
                                ": grid rows must be array");
        return false;
    }

    size_t expectedWidth = 0;
    for (const nlohmann::json &row : rowsJson) {
        if (!row.is_string()) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": grid rows must be strings");
            return false;
        }
        std::string rowText = row.get<std::string>();
        if (rowText.empty()) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": grid rows must not be empty strings");
            return false;
        }
        if (expectedWidth == 0) {
            expectedWidth = rowText.size();
        } else if (rowText.size() != expectedWidth) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": grid rows must have equal length");
            return false;
        }
        rows.push_back(std::move(rowText));
    }
    if (rows.empty()) {
        SetMessage(message, "Invalid scene json in " + sourceName +
                                ": grid rows must not be empty");
        return false;
    }
    return true;
}

char ReadMapCode(const nlohmann::json &json, const char fallback) {
    if (!json.is_string()) {
        return fallback;
    }
    const std::string text = json.get<std::string>();
    return text.empty() ? fallback : text[0];
}

bool LoadObjectV1(const nlohmann::json &objectJson,
                  EditableSceneObject &object, std::string *message) {
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
    object.type = objectJson.value("kind", std::string{});
    object.enabled = objectJson.value("visible", true);
    object.locked = objectJson.value("locked", false);
    object.gridPlacement.gridX = objectJson.value("gridX", 0);
    object.gridPlacement.gridY = objectJson.value("gridY", 0);
    object.gridPlacement.mapCode =
        ReadMapCode(objectJson.value("mapCode", nlohmann::json{"0"}), '0');
    object.collider.mode = objectJson.value("collider", std::string{});
    object.collider.enabled = !object.collider.mode.empty();
    return ReadTransform(objectJson, object, message);
}

bool LoadObjectV2(const nlohmann::json &objectJson,
                  EditableSceneObject &object, std::string *message) {
    if (!objectJson.is_object()) {
        SetMessage(message, "Invalid scene json: object must be object");
        return false;
    }

    if (objectJson.contains("type")) {
        if (!objectJson["type"].is_string()) {
            SetMessage(message, "Invalid scene json: object type must be string");
            return false;
        }
        object.type = objectJson["type"].get<std::string>();
    } else if (objectJson.contains("kind") && objectJson["kind"].is_string()) {
        object.type = objectJson["kind"].get<std::string>();
    } else {
        SetMessage(message, "Invalid scene json: object type must be string");
        return false;
    }

    object.id = objectJson.value("id", 0ull);
    object.name = objectJson.value("name", std::string{});
    object.enabled =
        objectJson.contains("enabled")
            ? objectJson.value("enabled", true)
            : objectJson.value("visible", true);
    object.locked = objectJson.value("locked", false);

    if (objectJson.contains("components")) {
        if (!objectJson["components"].is_object()) {
            SetMessage(message,
                       "Invalid scene json: object components must be object");
            return false;
        }
        const nlohmann::json &components = objectJson["components"];
        if (components.contains("GridPlacement")) {
            const nlohmann::json &grid = components["GridPlacement"];
            if (!grid.is_object()) {
                SetMessage(message,
                           "Invalid scene json: GridPlacement must be object");
                return false;
            }
            object.gridPlacement.enabled = true;
            object.gridPlacement.gridX = grid.value("gridX", 0);
            object.gridPlacement.gridY = grid.value("gridY", 0);
            object.gridPlacement.mapCode =
                ReadMapCode(grid.value("mapCode", nlohmann::json{"0"}), '0');
        }
        if (components.contains("Collider")) {
            const nlohmann::json &collider = components["Collider"];
            if (!collider.is_object()) {
                SetMessage(message,
                           "Invalid scene json: Collider must be object");
                return false;
            }
            object.collider.enabled = true;
            object.collider.mode = collider.value("mode", std::string{});
        }
    } else {
        object.gridPlacement.gridX = objectJson.value("gridX", 0);
        object.gridPlacement.gridY = objectJson.value("gridY", 0);
        object.gridPlacement.mapCode =
            ReadMapCode(objectJson.value("mapCode", nlohmann::json{"0"}),
                        '0');
        object.collider.mode = objectJson.value("collider", std::string{});
        object.collider.enabled = !object.collider.mode.empty();
    }

    return ReadTransform(objectJson, object, message);
}

bool ReadLegend(const nlohmann::json &gridJson,
                EditableSceneDocument::Grid &grid, std::string *message) {
    grid.legend = MakeDefaultGridLegend();
    if (!gridJson.contains("legend")) {
        return true;
    }
    if (!gridJson["legend"].is_object()) {
        SetMessage(message, "Invalid scene json: grid.legend must be object");
        return false;
    }
    for (auto it = gridJson["legend"].begin(); it != gridJson["legend"].end();
         ++it) {
        if (!it.value().is_string()) {
            SetMessage(message,
                       "Invalid scene json: grid.legend values must be strings");
            return false;
        }
        grid.legend[it.key()] = it.value().get<std::string>();
    }
    return true;
}

bool LoadDocumentFromJson(const nlohmann::json &root,
                          const std::string &sourceName,
                          EditableSceneDocument &outDocument,
                          std::string *message) {
    if (!root.is_object()) {
        SetMessage(message, "Invalid scene json in " + sourceName +
                                ": root must be object");
        return false;
    }

    EditableSceneDocument document{};
    document.version = kCurrentSceneVersion;
    document.grid.legend = MakeDefaultGridLegend();

    int sourceVersion = 1;
    if (root.contains("version")) {
        if (!root["version"].is_number_integer()) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": version must be integer");
            return false;
        }
        document.hasVersion = true;
        sourceVersion = root["version"].get<int>();
        if (sourceVersion > kCurrentSceneVersion) {
            SetMessage(message, "Unsupported scene version " +
                                    std::to_string(sourceVersion) + " in " +
                                    sourceName + " (current: " +
                                    std::to_string(kCurrentSceneVersion) + ")");
            return false;
        }
        if (sourceVersion < 1) {
            SetMessage(message, "Invalid scene version " +
                                    std::to_string(sourceVersion) + " in " +
                                    sourceName);
            return false;
        }
    }

    if (root.contains("name") && !root["name"].is_string()) {
        SetMessage(message, "Invalid scene json in " + sourceName +
                                ": name must be string");
        return false;
    }
    document.name = root.value("name", std::string{"game_stage"});

    if (sourceVersion >= 2 && root.contains("settings")) {
        if (!root["settings"].is_object()) {
            SetMessage(message,
                       "Invalid scene json: settings must be object");
            return false;
        }
        const nlohmann::json &settings = root["settings"];
        if (settings.contains("tileSize") &&
            !settings["tileSize"].is_number()) {
            SetMessage(message,
                       "Invalid scene json: settings.tileSize must be number");
            return false;
        }
        document.settings.tileSize = settings.value("tileSize", 1.0f);
    } else {
        if (root.contains("tileSize") && !root["tileSize"].is_number()) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": tileSize must be number");
            return false;
        }
        document.settings.tileSize = root.value("tileSize", 1.0f);
    }

    if (sourceVersion >= 2 && root.contains("grid")) {
        if (!root["grid"].is_object()) {
            SetMessage(message, "Invalid scene json: grid must be object");
            return false;
        }
        const nlohmann::json &gridJson = root["grid"];
        if (!gridJson.contains("rows") ||
            !ReadRows(gridJson["rows"], sourceName, document.grid.rows,
                      message) ||
            !ReadLegend(gridJson, document.grid, message)) {
            return false;
        }
    } else {
        if (!root.contains("rows") ||
            !ReadRows(root["rows"], sourceName, document.grid.rows, message)) {
            return false;
        }
        if (root.contains("legend") && root["legend"].is_object()) {
            for (auto it = root["legend"].begin(); it != root["legend"].end();
                 ++it) {
                if (it.value().is_string()) {
                    document.grid.legend[it.key()] =
                        it.value().get<std::string>();
                }
            }
        }
    }

    if (root.contains("objects")) {
        if (!root["objects"].is_array()) {
            SetMessage(message, "Invalid scene json in " + sourceName +
                                    ": objects must be array");
            return false;
        }
        document.hasObjects = true;
        for (const nlohmann::json &objectJson : root["objects"]) {
            EditableSceneObject object{};
            const bool loaded = sourceVersion >= 2
                                    ? LoadObjectV2(objectJson, object, message)
                                    : LoadObjectV1(objectJson, object, message);
            if (!loaded) {
                return false;
            }
            document.objects.push_back(object);
        }
    }

    outDocument = std::move(document);
    if (sourceVersion < kCurrentSceneVersion) {
        SetMessage(message, document.hasVersion
                                ? "Loaded older scene version " +
                                      std::to_string(sourceVersion) +
                                      " from " + sourceName
                                : "Loaded legacy scene format: " + sourceName);
    } else {
        SetMessage(message, "Loaded scene: " + sourceName);
    }
    return true;
}

nlohmann::json BuildSceneJson(const EditableSceneDocument &document) {
    nlohmann::json root;
    root["version"] = kCurrentSceneVersion;
    root["name"] = document.name;
    root["settings"] = {{"tileSize", document.settings.tileSize}};
    root["grid"] = {
        {"rows", document.grid.rows},
        {"legend",
         document.grid.legend.empty() ? MakeDefaultGridLegend()
                                      : document.grid.legend},
    };
    root["objects"] = nlohmann::json::array();

    for (const EditableSceneObject &object : document.objects) {
        nlohmann::json objectJson;
        objectJson["id"] = object.id;
        objectJson["name"] = object.name;
        objectJson["type"] = object.type;
        objectJson["enabled"] = object.enabled;
        objectJson["locked"] = object.locked;
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
        objectJson["components"] = {
            {"GridPlacement",
             {{"gridX", object.gridPlacement.gridX},
              {"gridY", object.gridPlacement.gridY},
              {"mapCode", std::string(1, object.gridPlacement.mapCode)}}},
            {"Collider", {{"mode", object.collider.mode}}},
        };
        root["objects"].push_back(objectJson);
    }
    return root;
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

    return LoadDocumentFromJson(root, path.generic_string(), outDocument,
                                message);
}

bool SceneSerializer::LoadFromString(const std::string &text,
                                     EditableSceneDocument &outDocument,
                                     std::string *message) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception &e) {
        SetMessage(message, std::string("Invalid scene state json: ") +
                                e.what());
        return false;
    }
    return LoadDocumentFromJson(root, "memory", outDocument, message);
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

    if (std::filesystem::exists(path)) {
        std::error_code error;
        std::filesystem::copy_file(
            path, path.generic_string() + ".bak",
            std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            SetMessage(message, "Failed to create backup: " +
                                    path.generic_string() + ".bak (" +
                                    error.message() + ")");
            return false;
        }
    }

    std::ofstream file(path);
    if (!file) {
        SetMessage(message, "Failed to open file: " + path.generic_string());
        return false;
    }

    std::string text;
    if (!SaveToString(document, text, message)) {
        return false;
    }
    file << text;
    if (!file) {
        SetMessage(message, "Failed to write file: " + path.generic_string());
        return false;
    }
    SetMessage(message, "Saved scene: " + path.generic_string());
    return true;
}

bool SceneSerializer::SaveToString(const EditableSceneDocument &document,
                                   std::string &outText,
                                   std::string *message) {
    if (document.grid.rows.empty()) {
        SetMessage(message, "Cannot save scene: grid rows must not be empty");
        return false;
    }

    const size_t width = document.grid.rows.front().size();
    if (width == 0) {
        SetMessage(message, "Cannot save scene: grid rows must not be empty");
        return false;
    }
    for (const std::string &row : document.grid.rows) {
        if (row.size() != width) {
            SetMessage(message,
                       "Cannot save scene: grid rows must have equal length");
            return false;
        }
    }

    const nlohmann::json root = BuildSceneJson(document);
    outText = root.dump(2);
    return true;
}
