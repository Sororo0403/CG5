#include "GameScene.h"
#include "DeferredRenderer.h"
#include "DirectXCommon.h"
#include "DebugUIRegistry.h"
#include "EngineRuntime.h"
#include "GBuffer.h"
#include "Inspector.h"
#include "Input.h"
#include "LightManager.h"
#include "ModelRenderer.h"
#include "PostEffectRenderer.h"
#include "RenderTexture.h"
#include "SkyboxRenderer.h"
#include "TextureManager.h"
#include <dinput.h>

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);
#ifdef _DEBUG
    DebugUIRegistry::GetInstance().Clear();
    Inspector::GetInstance().Clear();
#endif // _DEBUG

    if (!ctx_->renderer.renderTexture || !ctx_->renderer.gBuffer ||
        !ctx_->renderer.deferredRenderer || !ctx_->renderer.postEffectRenderer ||
        !ctx_->renderer.skyboxRenderer) {
        return;
    }

    renderWidth_ = ctx_->frame.width;
    renderHeight_ = ctx_->frame.height;

    ctx_->renderer.renderTexture->Initialize(ctx_->core.dxCommon, ctx_->core.srv,
                                             renderWidth_, renderHeight_);
    ctx_->renderer.gBuffer->Initialize(ctx_->core.dxCommon, ctx_->core.srv,
                                       renderWidth_, renderHeight_);
    ctx_->renderer.deferredRenderer->Initialize(
        ctx_->core.dxCommon, ctx_->core.srv, renderWidth_, renderHeight_);
    ctx_->renderer.postEffectRenderer->Initialize(
        ctx_->core.dxCommon, ctx_->core.srv, renderWidth_, renderHeight_);
    ctx_->renderer.postEffectRenderer->SetColorMode(
        PostEffectRenderer::ColorMode::None);
    ctx_->renderer.postEffectRenderer->SetFilterMode(
        PostEffectRenderer::FilterMode::None);
    ctx_->renderer.postEffectRenderer->SetEdgeMode(
        PostEffectRenderer::EdgeMode::None);
    ctx_->renderer.postEffectRenderer->SetVignettingEnabled(false);
    ctx_->renderer.postEffectRenderer->SetRadialBlurStrength(0.0f);
    ctx_->renderer.postEffectRenderer->SetRandomMode(
        PostEffectRenderer::RandomMode::None);
    ctx_->renderer.postEffectRenderer->SetRandomStrength(0.0f);

    camera_.Initialize(static_cast<float>(renderWidth_) /
                       static_cast<float>(renderHeight_));
    camera_.SetPosition({0.0f, 7.0f, -9.5f});
    camera_.LookAt({0.0f, 0.0f, 0.0f});
    camera_.SetPerspectiveFovDeg(45.0f);
    camera_.UpdateMatrices();
    ctx_->renderer.postEffectRenderer->SetDepthParameters(camera_.GetNearZ(),
                                                          camera_.GetFarZ());

    if (ctx_->renderer.light) {
        ctx_->renderer.light->SetPointLightCount(2);
        ctx_->renderer.light->SetPointLight(
            0, {{-2.0f, 3.5f, -2.5f, 9.0f}, {1.0f, 0.82f, 0.62f, 1.8f}});
        ctx_->renderer.light->SetPointLight(
            1, {{3.0f, 2.4f, 2.0f, 7.0f}, {0.42f, 0.65f, 1.0f, 1.2f}});
        SceneLighting &lighting = ctx_->renderer.light->GetMutableSceneLighting();
        lighting.ambientColor = {0.20f, 0.22f, 0.24f, 1.0f};
        lighting.lightingParams.y = 0.35f;
        lighting.lightingParams.w = 0.25f;
    }

    auto uploadContext = ctx_->core.dxCommon->BeginUploadContext();
    skyboxModelId_ = ctx_->assets.texture->Load(
        uploadContext, L"resources/rostock_laage_airport_4k.dds");
    uploadContext.Finish();
    ctx_->assets.texture->ReleaseUploadBuffers();

    environmentTextureId_ = skyboxModelId_;
    ctx_->renderer.skyboxRenderer->Initialize(
        ctx_->core.dxCommon, ctx_->core.srv, ctx_->assets.texture);
    ctx_->renderer.model->SetEnvironmentTexture(environmentTextureId_);

    gridPlacementTest_.Initialize(*ctx_);
    RegisterDebugUI();
#ifdef _DEBUG
    DebugUIRegistry::GetInstance().LoadFromJson(
        "resources/settings/debug_tuning.json");
#endif // _DEBUG
    ApplyTuning();
}

void GameScene::Update() {
#ifdef _DEBUG
    if (ctx_->core.input && ctx_->core.input->IsKeyTrigger(DIK_F1)) {
        EngineRuntime::GetInstance().ToggleMode();
    }
#endif // _DEBUG

    gridPlacementTest_.Update(*ctx_, camera_);
    ApplyTuning();
    if (ctx_->renderer.postEffectRenderer) {
        ctx_->renderer.postEffectRenderer->SetDepthParameters(camera_.GetNearZ(),
                                                              camera_.GetFarZ());
    }
}

void GameScene::Draw() {
    if (!ctx_->renderer.renderTexture || !ctx_->renderer.postEffectRenderer) {
        return;
    }

    DrawGBufferScene();

    if (useDeferredRendering_ && ctx_->renderer.deferredRenderer &&
        ctx_->renderer.light) {
        ctx_->renderer.renderTexture->BeginRender({0.02f, 0.04f, 0.08f, 1.0f},
                                                  false, false);
        if (ctx_->renderer.skyboxRenderer) {
            ctx_->renderer.skyboxRenderer->DrawNoDepth(skyboxModelId_, camera_);
        }
        ctx_->core.dxCommon->TransitionDepthToShaderResource();
        ctx_->renderer.deferredRenderer->Draw(
            *ctx_->renderer.gBuffer,
            ctx_->core.dxCommon->GetDepthStencilGpuHandle(), camera_,
            ctx_->renderer.light->GetSceneLighting());
        ctx_->renderer.renderTexture->EndRender();
    } else {
        ctx_->renderer.renderTexture->BeginRender({0.02f, 0.04f, 0.08f, 1.0f});
        DrawOffscreenScene();
        ctx_->renderer.renderTexture->EndRender();
        ctx_->core.dxCommon->TransitionDepthToShaderResource();
    }

    ctx_->core.dxCommon->SetBackBufferRenderTarget(false, false);
    ctx_->renderer.postEffectRenderer->Draw(
        ctx_->renderer.renderTexture->GetGpuHandle(),
        ctx_->core.dxCommon->GetDepthStencilGpuHandle());
    ctx_->core.dxCommon->TransitionDepthToWrite();

#ifdef _DEBUG
    DebugUIRegistry::GetInstance().Draw();
    Inspector::GetInstance().Draw();
#endif // _DEBUG
}

void GameScene::RegisterDebugUI() {
#ifdef _DEBUG
    DebugUIRegistry &registry = DebugUIRegistry::GetInstance();
    registry.RegisterBool("Renderer", "Use Deferred Rendering",
                          &useDeferredRendering_, true);
    registry.RegisterFloat("Post Effect", "Radial Blur Strength",
                           &radialBlurStrength_, 0.0f, 0.15f, true);
    registry.RegisterFloat("Post Effect", "Random Strength", &randomStrength_,
                           0.0f, 0.35f, true);
    registry.RegisterFloat("Lighting", "Point Light 0 Intensity",
                           &pointLight0Intensity_, 0.0f, 5.0f, true);
    registry.RegisterFloat("Lighting", "Point Light 1 Intensity",
                           &pointLight1Intensity_, 0.0f, 5.0f, true);
    registry.RegisterFloat("Lighting", "Ambient Strength", &ambientStrength_,
                           0.0f, 1.0f, true);
#endif // _DEBUG
}

void GameScene::ApplyTuning() {
    if (ctx_->renderer.postEffectRenderer) {
        ctx_->renderer.postEffectRenderer->SetRadialBlurStrength(
            radialBlurStrength_);
        ctx_->renderer.postEffectRenderer->SetRandomStrength(randomStrength_);
        ctx_->renderer.postEffectRenderer->SetRandomMode(
            randomStrength_ > 0.0f
                ? PostEffectRenderer::RandomMode::OverlayNoise
                                   : PostEffectRenderer::RandomMode::None);
    }

    if (ctx_->renderer.light) {
        ctx_->renderer.light->SetPointLight(
            0, {{-2.0f, 3.5f, -2.5f, 9.0f},
                {1.0f, 0.82f, 0.62f, pointLight0Intensity_}});
        ctx_->renderer.light->SetPointLight(
            1, {{3.0f, 2.4f, 2.0f, 7.0f},
                {0.42f, 0.65f, 1.0f, pointLight1Intensity_}});
        SceneLighting &lighting =
            ctx_->renderer.light->GetMutableSceneLighting();
        lighting.ambientColor = {ambientStrength_, ambientStrength_ * 1.1f,
                                 ambientStrength_ * 1.2f, 1.0f};
    }
}

void GameScene::DrawGBufferScene() {
    if (!ctx_->renderer.gBuffer || !ctx_->renderer.model) {
        return;
    }

    ctx_->renderer.gBuffer->BeginGeometryPass();
    gridPlacementTest_.DrawGBuffer(*ctx_, camera_);
    ctx_->renderer.gBuffer->EndGeometryPass();
}

void GameScene::DrawOffscreenScene() {
    if (ctx_->renderer.skyboxRenderer) {
        ctx_->renderer.skyboxRenderer->Draw(skyboxModelId_, camera_);
    }
    gridPlacementTest_.DrawForward(*ctx_, camera_, environmentTextureId_);
}

void GameScene::OnResize(int width, int height) {
    if (!ctx_->renderer.renderTexture || !ctx_->renderer.postEffectRenderer) {
        return;
    }

    if (width <= 0 || height <= 0 ||
        (width == renderWidth_ && height == renderHeight_)) {
        return;
    }

    renderWidth_ = width;
    renderHeight_ = height;
    ctx_->renderer.renderTexture->Resize(renderWidth_, renderHeight_);
    if (ctx_->renderer.gBuffer) {
        ctx_->renderer.gBuffer->Resize(renderWidth_, renderHeight_);
    }
    if (ctx_->renderer.deferredRenderer) {
        ctx_->renderer.deferredRenderer->Resize(renderWidth_, renderHeight_);
    }
    ctx_->renderer.postEffectRenderer->Resize(renderWidth_, renderHeight_);
    camera_.SetAspect(static_cast<float>(renderWidth_) /
                      static_cast<float>(renderHeight_));
    camera_.UpdateMatrices();
}
