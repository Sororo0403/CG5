#include "SpriteServices.h"
#include "SpriteAssets.h"
#include "SpriteRenderer.h"

SpriteServices::SpriteServices()
    : assets_(std::make_unique<SpriteAssets>()),
      renderer_(std::make_unique<SpriteRenderer>()) {}

SpriteServices::~SpriteServices() = default;

void SpriteServices::Initialize(DirectXCommon *dxCommon,
                                TextureManager *textureManager,
                                SrvManager *srvManager, int width,
                                int height) {
    assets_->Initialize(textureManager);
    renderer_->Initialize(dxCommon, textureManager, srvManager, width, height);
}

void SpriteServices::Resize(int width, int height) {
    renderer_->UpdateProjection(width, height);
}
