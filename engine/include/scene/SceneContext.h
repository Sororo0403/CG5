#pragma once

class Input;
class WinApp;
class SoundManager;
class ModelManager;
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

#ifdef _DEBUG
class ImguiManager;
#endif // _DEBUG

struct CoreServices {
    Input *input = nullptr;
    WinApp *winApp = nullptr;
    SoundManager *sound = nullptr;
    DirectXCommon *dxCommon = nullptr;
    SrvManager *srv = nullptr;
};

struct AssetServices {
    ModelManager *model = nullptr;
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

    Input *input = nullptr;
    WinApp *winApp = nullptr;
    SoundManager *sound = nullptr;
    ModelManager *model = nullptr;
    SpriteManager *sprite = nullptr;
    TextureManager *texture = nullptr;
    DirectXCommon *dxCommon = nullptr;
    SrvManager *srv = nullptr;
    EffectSystem *effects = nullptr;
    DebugDraw *debugDraw = nullptr;
    float deltaTime = 0.0f;

#ifdef _DEBUG
    ImguiManager *imgui = nullptr;
#endif // _DEBUG
};
