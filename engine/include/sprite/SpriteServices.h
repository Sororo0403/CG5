#pragma once
#include <memory>

class DirectXCommon;
class SpriteAssets;
class SpriteRenderer;
class SrvManager;
class TextureManager;

class SpriteServices {
  public:
    SpriteServices();
    ~SpriteServices();

    SpriteServices(const SpriteServices &) = delete;
    SpriteServices &operator=(const SpriteServices &) = delete;
    SpriteServices(SpriteServices &&) = delete;
    SpriteServices &operator=(SpriteServices &&) = delete;

    void Initialize(DirectXCommon *dxCommon, TextureManager *textureManager,
                    SrvManager *srvManager, int width, int height);

    SpriteAssets &Assets() { return *assets_; }
    const SpriteAssets &Assets() const { return *assets_; }

    SpriteRenderer &Renderer() { return *renderer_; }
    const SpriteRenderer &Renderer() const { return *renderer_; }

    void Resize(int width, int height);

  private:
    std::unique_ptr<SpriteAssets> assets_;
    std::unique_ptr<SpriteRenderer> renderer_;
};
