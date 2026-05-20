#include "core/EngineRuntime.h"

#include "debug/DebugLog.h"
#include "scene/BaseScene.h"

#include <algorithm>

EngineRuntime::~EngineRuntime() {
    winApp_.SetCursorVisible(true);
    dxCommon_.WaitForGpu();
}

int EngineRuntime::Run(HINSTANCE instance, int showCommand,
                       std::unique_ptr<BaseScene> initialScene,
                       const EngineRuntimeConfig &config) {
    Initialize(instance, showCommand, config);
    sceneManager_.ChangeScene(std::move(initialScene));

    while (winApp_.ProcessMessage()) {
        frameTimer_.Tick();
        ResizeIfNeeded();
        UpdateFrameContext();
        input_.Update(sceneContext_.frame.deltaTime);
        sceneManager_.Update();
        RenderFrame();
    }

    dxCommon_.WaitForGpu();
    return 0;
}

int EngineRuntime::Run(HINSTANCE instance, int showCommand,
                       const std::string &initialSceneName,
                       AbstractSceneFactory *sceneFactory,
                       const EngineRuntimeConfig &config) {
    Initialize(instance, showCommand, config);
    sceneManager_.SetSceneFactory(sceneFactory);
    sceneManager_.ChangeScene(initialSceneName);

    while (winApp_.ProcessMessage()) {
        frameTimer_.Tick();
        ResizeIfNeeded();
        UpdateFrameContext();
        input_.Update(sceneContext_.frame.deltaTime);
        sceneManager_.Update();
        RenderFrame();
    }

    dxCommon_.WaitForGpu();
    return 0;
}

void EngineRuntime::Initialize(HINSTANCE instance, int showCommand,
                               const EngineRuntimeConfig &config) {
    winApp_.Initialize(instance, showCommand, config.width, config.height,
                       config.title, config.fullscreen);
    winApp_.SetCursorVisible(config.cursorVisible);
    currentWidth_ = winApp_.GetWidth();
    currentHeight_ = winApp_.GetHeight();

#ifdef _DEBUG
    DebugLog::Get().OpenDefault();
    DebugLog::Get().Write(
        "Engine", "EngineRuntime", "initialize", "start",
        {{"width", std::to_string(currentWidth_)},
         {"height", std::to_string(currentHeight_)}});
#endif

    dxCommon_.Initialize(winApp_.GetHwnd(), currentWidth_, currentHeight_);
    srvManager_.Initialize(&dxCommon_);
    dxCommon_.CreateDepthStencilSrv(&srvManager_);
    dxCommon_.RegisterSceneColorSRV(&srvManager_);

    dxCommon_.BeginUpload();
    textureManager_.Initialize(&dxCommon_, &srvManager_);
    dxCommon_.EndUpload();
    textureManager_.ReleaseUploadBuffers();

    meshManager_.Initialize(&dxCommon_);
    meshRenderer_.Initialize(&dxCommon_, &srvManager_, &textureManager_);
    postEffectRenderer_.Initialize(&dxCommon_, &srvManager_, currentWidth_,
                                   currentHeight_);
    shadowMapRenderer_.Initialize(&dxCommon_, &srvManager_);
    renderPassController_.Initialize(&dxCommon_, &srvManager_);
    input_.Initialize(instance, winApp_.GetHwnd());

#ifdef _DEBUG
    imguiManager_.Initialize(&winApp_, &dxCommon_, &srvManager_);
#endif

    sceneContext_.systems.input = &input_;
    sceneContext_.systems.winApp = &winApp_;
    sceneContext_.systems.texture = &textureManager_;
    sceneContext_.systems.cameraManager = &cameraManager_;
    sceneContext_.rendering.mesh = &meshManager_;
    sceneContext_.rendering.meshRenderer = &meshRenderer_;
    sceneContext_.rendering.texture = &textureManager_;
    sceneContext_.rendering.dxCommon = &dxCommon_;
    sceneContext_.rendering.srv = &srvManager_;
    sceneContext_.rendering.postEffectRenderer = &postEffectRenderer_;
    sceneContext_.rendering.shadowMapRenderer = &shadowMapRenderer_;
    sceneContext_.rendering.transparentQueue = &transparentQueue_;
    sceneContext_.render = renderPassController_.GetContextPtr();
#ifdef _DEBUG
    sceneContext_.systems.imgui = &imguiManager_;
#endif

    frameTimer_.Reset();
    sceneManager_.Initialize(sceneContext_);
}

void EngineRuntime::UpdateFrameContext() {
    const FrameTime &frameTime = frameTimer_.GetFrameTime();
    sceneContext_.frame.frameTime = frameTime;
    sceneContext_.frame.deltaTime =
        static_cast<float>((std::min)(frameTime.deltaTime, 1.0 / 15.0));
#ifdef _DEBUG
    DebugLog::Get().SetFrame(frameTime.frameCount,
                             sceneContext_.frame.deltaTime);
#endif
}

void EngineRuntime::ResizeIfNeeded() {
    const int width = winApp_.GetWidth();
    const int height = winApp_.GetHeight();
    if (width <= 0 || height <= 0 ||
        (width == currentWidth_ && height == currentHeight_)) {
        return;
    }

    currentWidth_ = width;
    currentHeight_ = height;
    dxCommon_.Resize(currentWidth_, currentHeight_);
    postEffectRenderer_.Resize(currentWidth_, currentHeight_);
}

void EngineRuntime::RenderFrame() {
    meshRenderer_.BeginFrame();
    transparentQueue_.Clear();

    dxCommon_.BeginFrame();
    renderPassController_.BeginFrame(
        sceneContext_.frame.frameTime, sceneContext_.frame.deltaTime,
        static_cast<uint32_t>(currentWidth_),
        static_cast<uint32_t>(currentHeight_));

#ifdef _DEBUG
    imguiManager_.Begin(dxCommon_.GetCommandList());
#endif

    renderPassController_.BeginPass(RenderPass::Shadow);
    shadowMapRenderer_.Begin();
    meshRenderer_.PreDrawShadow();
    sceneManager_.DrawShadow();
    shadowMapRenderer_.End();
    renderPassController_.EndPass();

    dxCommon_.BeginScenePass();
    renderPassController_.BeginPass(RenderPass::SceneColor);
    meshRenderer_.PreDraw();
    sceneManager_.Draw();
    renderPassController_.EndPass();

    renderPassController_.BeginPass(RenderPass::Transparent);
    sceneManager_.DrawTransparent();
    transparentQueue_.Flush();
    renderPassController_.EndPass();
    meshRenderer_.PostDraw();
    dxCommon_.EndScenePass();

    dxCommon_.BeginBackBufferPass(false);
    dxCommon_.TransitionDepthToShaderResource();
    renderPassController_.BeginPass(RenderPass::PostEffect);
    postEffectRenderer_.Draw(dxCommon_.GetSceneSrvGpuHandle(&srvManager_),
                             dxCommon_.GetDepthStencilGpuHandle());
    renderPassController_.EndPass();
    dxCommon_.TransitionDepthToWrite();

#ifdef _DEBUG
    renderPassController_.BeginPass(RenderPass::UI);
    imguiManager_.End(dxCommon_.GetCommandList());
    renderPassController_.EndPass();
#endif

    dxCommon_.EndFrame();
}
