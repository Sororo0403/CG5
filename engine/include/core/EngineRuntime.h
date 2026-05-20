#pragma once

#include "core/FrameTimer.h"
#include "core/WinApp.h"
#include "camera/CameraManager.h"
#include "graphics/DirectXCommon.h"
#include "graphics/PostEffectRenderer.h"
#include "graphics/RenderPassController.h"
#include "graphics/ShadowMapRenderer.h"
#include "graphics/SrvManager.h"
#include "graphics/TransparentRenderQueue.h"
#include "input/Input.h"
#include "model/MeshManager.h"
#include "model/MeshRenderer.h"
#include "scene/AbstractSceneFactory.h"
#include "scene/SceneContext.h"
#include "scene/SceneManager.h"
#include "texture/TextureManager.h"

#include <memory>
#include <string>

#ifdef _DEBUG
#include "imgui/ImguiManager.h"
#endif

class BaseScene;

struct EngineRuntimeConfig {
    int width = 1280;
    int height = 720;
    std::wstring title = L"App";
    bool cursorVisible = true;
    bool fullscreen = false;
};

class EngineRuntime {
  public:
    EngineRuntime() = default;
    ~EngineRuntime();

    EngineRuntime(const EngineRuntime &) = delete;
    EngineRuntime &operator=(const EngineRuntime &) = delete;
    EngineRuntime(EngineRuntime &&) = delete;
    EngineRuntime &operator=(EngineRuntime &&) = delete;

    int Run(HINSTANCE instance, int showCommand,
            std::unique_ptr<BaseScene> initialScene,
            const EngineRuntimeConfig &config = {});
    int Run(HINSTANCE instance, int showCommand,
            const std::string &initialSceneName,
            AbstractSceneFactory *sceneFactory,
            const EngineRuntimeConfig &config = {});

  private:
    void Initialize(HINSTANCE instance, int showCommand,
                    const EngineRuntimeConfig &config);
    void UpdateFrameContext();
    void ResizeIfNeeded();
    void RenderFrame();

    WinApp winApp_;
    DirectXCommon dxCommon_;
    SrvManager srvManager_;
    TextureManager textureManager_;
    MeshManager meshManager_;
    MeshRenderer meshRenderer_;
    PostEffectRenderer postEffectRenderer_;
    ShadowMapRenderer shadowMapRenderer_;
    TransparentRenderQueue transparentQueue_;
    RenderPassController renderPassController_;
    SceneManager sceneManager_;
    CameraManager cameraManager_;
    Input input_;
    FrameTimer frameTimer_;
    SceneContext sceneContext_{};

#ifdef _DEBUG
    ImguiManager imguiManager_;
#endif

    int currentWidth_ = 0;
    int currentHeight_ = 0;
};
