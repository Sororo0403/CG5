#include "PlacementMap.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

PlacementMap::PlacementMap()
    : rows_{
          "0000000000",
          "0111111100",
          "0120002100",
          "0100300100",
          "0100400100",
          "0111111100",
          "0000000000",
      } {}

int PlacementMap::GetWidth() const {
    size_t width = 0;
    for (const std::string &row : rows_) {
        width = (std::max)(width, row.size());
    }
    return static_cast<int>(width);
}

int PlacementMap::GetHeight() const {
    return static_cast<int>(rows_.size());
}

char PlacementMap::GetTile(int x, int y) const {
    if (y < 0 || y >= GetHeight()) {
        return '0';
    }

    const std::string &row = rows_[static_cast<size_t>(y)];
    if (x < 0 || x >= static_cast<int>(row.size())) {
        return '0';
    }

    return row[static_cast<size_t>(x)];
}

void PlacementMap::SetTile(int x, int y, char tile) {
    if (y < 0 || y >= GetHeight() || x < 0) {
        return;
    }

    std::string &row = rows_[static_cast<size_t>(y)];
    if (x >= static_cast<int>(row.size())) {
        return;
    }

    row[static_cast<size_t>(x)] = tile;
}

DirectX::XMFLOAT3 PlacementMap::GetCellCenter(int x, int y, float height) const {
    const float originX = -static_cast<float>(GetWidth() - 1) * tileSize_ * 0.5f;
    const float originZ = -static_cast<float>(GetHeight() - 1) * tileSize_ * 0.5f;
    return {originX + static_cast<float>(x) * tileSize_, height,
            originZ + static_cast<float>(y) * tileSize_};
}

bool PlacementMap::LoadFromJson(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    nlohmann::json root;
    file >> root;
    if (!root.is_object() || !root.contains("rows") || !root["rows"].is_array()) {
        return false;
    }

    std::vector<std::string> loadedRows;
    for (const nlohmann::json &row : root["rows"]) {
        if (!row.is_string()) {
            return false;
        }
        loadedRows.push_back(row.get<std::string>());
    }

    if (loadedRows.empty()) {
        return false;
    }

    rows_ = std::move(loadedRows);
    tileSize_ = root.value("tileSize", tileSize_);
    return true;
}

bool PlacementMap::SaveToJson(const std::filesystem::path &path) const {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    nlohmann::json root;
    root["name"] = "game_stage";
    root["tileSize"] = tileSize_;
    root["legend"] = {
        {"0", "empty"},
        {"1", "floor"},
        {"2", "wall"},
        {"3", "player_start"},
        {"4", "goal_marker"},
    };
    root["rows"] = rows_;

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << root.dump(2);
    return true;
}
