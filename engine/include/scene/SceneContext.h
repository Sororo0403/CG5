#pragma once

class Input;
class WinApp;
class SoundManager;
class ModelAssets;
class SpriteManager;
class ModelRenderer;
class SpriteRenderer;
class TextureManager;
class DirectXCommon;
class SrvManager;
class PostEffectRenderer;
class RenderTexture;
class DeferredRenderer;
class GBuffer;
class SkyboxRenderer;
class LightManager;
class DebugDraw;
class EffectSystem;
class BillboardRenderer;

struct CoreServices {
    Input *input = nullptr;
    WinApp *winApp = nullptr;
    SoundManager *sound = nullptr;
    DirectXCommon *dxCommon = nullptr;
    SrvManager *srv = nullptr;
};

struct AssetServices {
    ModelAssets *model = nullptr;
    SpriteManager *sprite = nullptr;
    TextureManager *texture = nullptr;
};

struct RendererServices {
    ModelRenderer *model = nullptr;
    SpriteRenderer *sprite = nullptr;
    RenderTexture *renderTexture = nullptr;
    GBuffer *gBuffer = nullptr;
    DeferredRenderer *deferredRenderer = nullptr;
    PostEffectRenderer *postEffectRenderer = nullptr;
    SkyboxRenderer *skyboxRenderer = nullptr;
    LightManager *light = nullptr;
    BillboardRenderer *billboard = nullptr;
    EffectSystem *effects = nullptr;
    DebugDraw *debugDraw = nullptr;
};

struct FrameState {
    float deltaTime = 0.0f;
    int width = 0;
    int height = 0;
};

/// <summary>
/// シーンが参照する各種システムへのアクセスポイントをまとめる
/// </summary>
struct SceneContext {
    CoreServices core;
    AssetServices assets;
    RendererServices renderer;
    FrameState frame;
};
