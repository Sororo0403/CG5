#include "DebugDraw.h"
#include "DirectXCommon.h"
#include "EffectSystem.h"
#include "GameScene.h"
#include "Input.h"
#include "LightManager.h"
#include "ModelManager.h"
#include "PostEffectRenderer.h"
#include "SceneContext.h"
#include "SceneManager.h"
#include "ShaderCompiler.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "SrvManager.h"
#include "TextureManager.h"
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

    EffectSystem effectSystem;
    effectSystem.Initialize(&dxCommon, &srvManager, &textureManager, 96, 12);
    PostEffectRenderer postEffectRenderer;
    postEffectRenderer.Initialize(&dxCommon, &srvManager, width, height);
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
    sceneCtx.renderer.postEffectRenderer = &postEffectRenderer;

    sceneCtx.input = &input;
    sceneCtx.winApp = &winApp;
    sceneCtx.sound = &soundManager;
    sceneCtx.model = &modelManager;
    sceneCtx.sprite = &spriteManager;
    sceneCtx.texture = &textureManager;
    sceneCtx.dxCommon = &dxCommon;
    sceneCtx.srv = &srvManager;
    sceneCtx.effects = &effectSystem;
    sceneCtx.debugDraw = &debugDraw;
    sceneCtx.frame.width = width;
    sceneCtx.frame.height = height;

#ifdef _DEBUG
    ImguiManager imguiManager;
    imguiManager.Initialize(&winApp, &dxCommon, &srvManager);
    sceneCtx.imgui = &imguiManager;
#endif

    SceneManager sceneManager;
    sceneManager.Initialize(sceneCtx);
    auto gameScene = std::make_unique<GameScene>();
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
                postEffectRenderer.Resize(width, height);
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
        sceneManager.Update();

        dxCommon.BeginFrame();

        dxCommon.SetBackBufferRenderTarget(true);
#ifdef _DEBUG
        imguiManager.Begin(dxCommon.GetCommandList());
#endif
        sceneManager.Draw();
#ifdef _DEBUG
        imguiManager.End(dxCommon.GetCommandList());
#endif

        dxCommon.EndFrame();
    }

    dxCommon.ReleaseDepthStencilSrv();
    return 0;
}
