#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>

class PlacementMap {
  public:
    PlacementMap();

    int GetWidth() const;
    int GetHeight() const;
    float GetTileSize() const { return tileSize_; }
    void SetTileSize(float tileSize) { tileSize_ = tileSize; }

    char GetTile(int x, int y) const;
    void SetTile(int x, int y, char tile);
    const std::vector<std::string> &GetRows() const { return rows_; }
    void SetRows(const std::vector<std::string> &rows);
    DirectX::XMFLOAT3 GetCellCenter(int x, int y, float height = 0.0f) const;

  private:
    std::vector<std::string> rows_;
    float tileSize_ = 1.0f;
};
