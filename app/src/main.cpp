#include "DirectXCommon.h"
#include "GameScene.h"
#include "Input.h"
#include "LightManager.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "PostEffectRenderer.h"
#include "RenderTexture.h"
#include "SceneContext.h"
#include "SceneManager.h"
#include "ShaderCompiler.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "SpriteRenderer.h"
#include "SrvManager.h"
#include "SkyboxRenderer.h"
#include "TextureManager.h"
#include "WinApp.h"
#ifdef _DEBUG
#include "ImguiManager.h"
#endif // _DEBUG
#include <memory>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // WinApp初期化
    WinApp winApp;
    winApp.Initialize(hInstance, nCmdShow, 1280, 720, L"CG5");

    // クライアント領域の幅と高さ
    int width = winApp.GetWidth();
    int height = winApp.GetHeight();

    // DirectX
    DirectXCommon dxCommon;
    dxCommon.Initialize(winApp.GetHwnd(), width, height);
    ShaderCompiler::ValidateFiles({
        L"engine/resources/shaders/model/ModelVS.hlsl",
        L"engine/resources/shaders/model/ModelPS.hlsl",
        L"engine/resources/shaders/model/SkinningCS.hlsl",
        L"engine/resources/shaders/sprite/SpriteVS.hlsl",
        L"engine/resources/shaders/sprite/SpritePS.hlsl",
        L"engine/resources/shaders/skybox/SkyboxVS.hlsl",
        L"engine/resources/shaders/skybox/SkyboxPS.hlsl",
        L"engine/resources/shaders/posteffect/PostEffectVS.hlsl",
        L"engine/resources/shaders/posteffect/PostEffectPS.hlsl",
        L"engine/resources/shaders/particle/GPUParticleVS.hlsl",
        L"engine/resources/shaders/particle/GPUParticlePS.hlsl",
        L"engine/resources/shaders/particle/GPUParticleUpdateCS.hlsl",
        L"engine/resources/shaders/debug/SkeletonDebugVS.hlsl",
        L"engine/resources/shaders/debug/SkeletonDebugPS.hlsl",
    });

    // SrvManager
    SrvManager srvManager;
    srvManager.Initialize(&dxCommon, 512);
    dxCommon.CreateDepthStencilSrv(&srvManager);

    // Input
    Input input;
    input.Initialize(hInstance, winApp.GetHwnd());

    // SoundManager
    SoundManager soundManager;
    soundManager.Initialize();

    auto startupUpload = dxCommon.BeginUploadContext();

    // TextureManager
    TextureManager textureManager;
    textureManager.Initialize(&dxCommon, &srvManager, startupUpload);

    // ModelManager
    ModelManager modelManager;
    modelManager.Initialize(&dxCommon, &srvManager, &textureManager);

    // SpriteManager
    SpriteManager spriteManager;
    spriteManager.Initialize(&dxCommon, &textureManager, &srvManager, width,
                             height);

    LightManager lightManager;

    startupUpload.Finish();

    textureManager.ReleaseUploadBuffers();

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

    RenderTexture renderTexture;
    PostEffectRenderer postEffectRenderer;
    SkyboxRenderer skyboxRenderer;
    sceneCtx.renderer.renderTexture = &renderTexture;
    sceneCtx.renderer.postEffectRenderer = &postEffectRenderer;
    sceneCtx.renderer.skyboxRenderer = &skyboxRenderer;

    sceneCtx.frame.deltaTime = 0.0f;
    sceneCtx.frame.width = width;
    sceneCtx.frame.height = height;

#ifdef _DEBUG
    ImguiManager imguiManager;
    imguiManager.Initialize(&winApp, &dxCommon, &srvManager);
    sceneCtx.imgui = &imguiManager;
#endif // _DEBUG

    // SceneManager
    SceneManager sceneManager;
    sceneManager.Initialize(sceneCtx);
    sceneManager.ChangeScene(std::make_unique<GameScene>());

    // 高精細タイマの周波数を取得
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER prevTime;
    QueryPerformanceCounter(&prevTime);

    // メインループ
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

        // deltaTime計算
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        float deltaTime =
            static_cast<float>(currentTime.QuadPart - prevTime.QuadPart) /
            static_cast<float>(freq.QuadPart);

        prevTime = currentTime;

        sceneCtx.frame.deltaTime = deltaTime;

        // 入力更新
        input.Update(deltaTime);

        // Scene 更新
        sceneManager.Update();

        // 描画
        dxCommon.BeginFrame();

#ifdef _DEBUG
        imguiManager.Begin(dxCommon.GetCommandList());
#endif // _DEBUG
        sceneManager.Draw();
#ifdef _DEBUG
        imguiManager.End(dxCommon.GetCommandList());
#endif // _DEBUG

        dxCommon.EndFrame();
    }

    dxCommon.ReleaseDepthStencilSrv();

    return 0;
}
