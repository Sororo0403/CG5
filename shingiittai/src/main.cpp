#include "DebugDraw.h"
#include "DirectXCommon.h"
#include "ElectricRingEffectRenderer.h"
#include "EditorLayer.h"
#include "EngineRuntime.h"
#include "GameScene.h"
#include "GpuSlashParticleSystem.h"
#include "Input.h"
#include "LightManager.h"
#include "MagnetismicRenderer.h"
#include "ModelManager.h"
#include "RenderTexture.h"
#include "SceneContext.h"
#include "SceneManager.h"
#include "ShaderCompiler.h"
#include "SlashEffectRenderer.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "SrvManager.h"
#include "SwordTrailRenderer.h"
#include "TextureManager.h"
#include "WarpPostEffectRenderer.h"
#include "WinApp.h"
#include <memory>

#ifdef _DEBUG
#include "ImguiManager.h"
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WinApp winApp;
    winApp.Initialize(hInstance, nCmdShow, 1280, 720, L"3145_身技一体");

    int width = winApp.GetWidth();
    int height = winApp.GetHeight();

    DirectXCommon dxCommon;
    dxCommon.Initialize(winApp.GetHwnd(), width, height);
    ShaderCompiler::ValidateFiles({
        L"engine/resources/shaders/model/ModelVS.hlsl",
        L"engine/resources/shaders/model/ModelPS.hlsl",
        L"engine/resources/shaders/model/ModelGBufferPS.hlsl",
        L"engine/resources/shaders/model/SkinningCS.hlsl",
        L"engine/resources/shaders/sprite/SpriteVS.hlsl",
        L"engine/resources/shaders/sprite/SpritePS.hlsl",
        L"engine/resources/shaders/debug/SkeletonDebugVS.hlsl",
        L"engine/resources/shaders/debug/SkeletonDebugPS.hlsl",
    });

    SrvManager srvManager;
    srvManager.Initialize(&dxCommon, 512);
    dxCommon.CreateDepthStencilSrv(&srvManager);

    Input input;
    input.Initialize(hInstance, winApp.GetHwnd());

    SoundManager soundManager;
    soundManager.Initialize();

    auto startupUpload = dxCommon.BeginUploadContext();

    TextureManager textureManager;
    textureManager.Initialize(&dxCommon, &srvManager, startupUpload);

    ModelManager modelManager;
    modelManager.Initialize(&dxCommon, &srvManager, &textureManager);

    SpriteManager spriteManager;
    spriteManager.Initialize(&dxCommon, &textureManager, &srvManager, width,
                             height);

    uint32_t boxModelId =
        modelManager.Load(startupUpload, L"shingiittai/resources/model/debug/box.glb");

    startupUpload.Finish();
    textureManager.ReleaseUploadBuffers();

    WarpPostEffectParamGPU warpPostParam{};
    ElectricRingParamGPU electricRingParam{};
    SlashEffectRenderer slashEffectRenderer;
    GpuSlashParticleSystem gpuSlashParticleSystem;
    SwordTrailRenderer swordTrailRenderer;
    MagnetismicRenderer magnetismicRenderer;
    DebugDraw debugDraw;
    debugDraw.Initialize(boxModelId);

    LightManager lightManager;

    SceneContext sceneCtx{};
    sceneCtx.core.input = &input;
    sceneCtx.core.winApp = &winApp;
    sceneCtx.core.sound = &soundManager;
    sceneCtx.core.dxCommon = &dxCommon;
    sceneCtx.core.srv = &srvManager;
    sceneCtx.assets.model = &modelManager;
    sceneCtx.assets.sprite = &spriteManager;
    sceneCtx.assets.texture = &textureManager;
    sceneCtx.renderer.model = modelManager.GetRenderer();
    sceneCtx.renderer.sprite = spriteManager.GetRenderer();
    sceneCtx.renderer.light = &lightManager;

    sceneCtx.input = &input;
    sceneCtx.winApp = &winApp;
    sceneCtx.sound = &soundManager;
    sceneCtx.model = &modelManager;
    sceneCtx.sprite = &spriteManager;
    sceneCtx.texture = &textureManager;
    sceneCtx.dxCommon = &dxCommon;
    sceneCtx.srv = &srvManager;
    sceneCtx.warpPostEffectParam = &warpPostParam;
    sceneCtx.electricRingParam = &electricRingParam;
    sceneCtx.slashEffectRenderer = &slashEffectRenderer;
    sceneCtx.gpuSlashParticleSystem = &gpuSlashParticleSystem;
    sceneCtx.swordTrailRenderer = &swordTrailRenderer;
    sceneCtx.magnetismicRenderer = &magnetismicRenderer;
    sceneCtx.debugDraw = &debugDraw;
    sceneCtx.frame.width = width;
    sceneCtx.frame.height = height;

#ifdef _DEBUG
    ImguiManager imguiManager;
    imguiManager.Initialize(&winApp, &dxCommon, &srvManager);
    sceneCtx.imgui = &imguiManager;

    RenderTexture editorViewportTexture;
    editorViewportTexture.Initialize(&dxCommon, &srvManager, width, height);
    EditorLayer editorLayer;
    EngineRuntime::GetInstance().SetMode(EngineRuntimeMode::Editor);
#endif

    SceneManager sceneManager;
    sceneManager.Initialize(sceneCtx);
    auto gameScene = std::make_unique<GameScene>();
    GameScene *gameScenePtr = gameScene.get();
    sceneManager.ChangeScene(std::move(gameScene));

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER prevTime;
    QueryPerformanceCounter(&prevTime);

    while (winApp.ProcessMessage()) {
        const int currentWidth = winApp.GetWidth();
        const int currentHeight = winApp.GetHeight();
        if (currentWidth != width || currentHeight != height) {
            width = currentWidth;
            height = currentHeight;
            if (width > 0 && height > 0) {
                dxCommon.Resize(width, height);
                spriteManager.Resize(width, height);
                sceneCtx.frame.width = width;
                sceneCtx.frame.height = height;
                sceneManager.Resize(width, height);
            }
        }

        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        float deltaTime =
            static_cast<float>(currentTime.QuadPart - prevTime.QuadPart) /
            static_cast<float>(freq.QuadPart);
        prevTime = currentTime;

        sceneCtx.frame.deltaTime = deltaTime;
        sceneCtx.deltaTime = deltaTime;

        input.Update(deltaTime);
#ifdef _DEBUG
        if (input.IsKeyTrigger(DIK_F1)) {
            EngineRuntime::GetInstance().ToggleMode();
        }
#endif
        sceneManager.Update();

        dxCommon.BeginFrame();

#ifdef _DEBUG
        imguiManager.Begin(dxCommon.GetCommandList());
        editorViewportTexture.Resize(width, height);
        editorViewportTexture.BeginRender({0.04f, 0.05f, 0.07f, 1.0f});
        sceneManager.Draw();
        editorViewportTexture.EndRender();

        dxCommon.SetBackBufferRenderTarget(true);
        editorLayer.Draw(gameScenePtr, &editorViewportTexture,
                         gameScenePtr != nullptr
                             ? gameScenePtr->GetCurrentCamera()
                             : nullptr);
#else
        dxCommon.SetBackBufferRenderTarget(true);
        sceneManager.Draw();
#endif
#ifdef _DEBUG
        imguiManager.End(dxCommon.GetCommandList());
#endif

        dxCommon.EndFrame();
    }

    dxCommon.ReleaseDepthStencilSrv();
    return 0;
}
