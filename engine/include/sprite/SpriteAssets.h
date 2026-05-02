#pragma once
#include "DirectXCommon.h"
#include "Sprite.h"
#include <cstdint>
#include <string>
#include <vector>

class TextureManager;

class SpriteAssets {
  public:
    void Initialize(TextureManager *textureManager);

    uint32_t Create(const DirectXCommon::UploadContext &uploadContext,
                    const std::wstring &filePath);

    Sprite &GetSprite(uint32_t id);
    const Sprite &GetSprite(uint32_t id) const;

    size_t GetCount() const { return sprites_.size(); }
    void Clear() { sprites_.clear(); }

  private:
    TextureManager *textureManager_ = nullptr;
    std::vector<Sprite> sprites_;
};
