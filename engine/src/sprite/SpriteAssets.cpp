#include "SpriteAssets.h"
#include "TextureManager.h"

void SpriteAssets::Initialize(TextureManager *textureManager) {
    textureManager_ = textureManager;
    sprites_.clear();
}

uint32_t SpriteAssets::Create(
    const DirectXCommon::UploadContext &uploadContext,
    const std::wstring &filePath) {
    uint32_t texId = textureManager_->Load(uploadContext, filePath);

    Sprite sprite{};
    sprite.textureId = texId;
    sprite.position = {0.0f, 0.0f};
    sprite.size = {static_cast<float>(textureManager_->GetWidth(texId)),
                   static_cast<float>(textureManager_->GetHeight(texId))};
    sprite.color = {1.0f, 1.0f, 1.0f, 1.0f};

    sprites_.push_back(sprite);
    return static_cast<uint32_t>(sprites_.size() - 1);
}

Sprite &SpriteAssets::GetSprite(uint32_t id) { return sprites_.at(id); }

const Sprite &SpriteAssets::GetSprite(uint32_t id) const {
    return sprites_.at(id);
}
