#include "PlacementMap.h"
#include <algorithm>

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

DirectX::XMFLOAT3 PlacementMap::GetCellCenter(int x, int y, float height) const {
    const float originX = -static_cast<float>(GetWidth() - 1) * tileSize_ * 0.5f;
    const float originZ = -static_cast<float>(GetHeight() - 1) * tileSize_ * 0.5f;
    return {originX + static_cast<float>(x) * tileSize_, height,
            originZ + static_cast<float>(y) * tileSize_};
}
