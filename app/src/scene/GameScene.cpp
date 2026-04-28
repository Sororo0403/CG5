#include "GameScene.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "PostEffectRenderer.h"
#include "RenderTexture.h"
#include "SpriteRenderer.h"
#include "SkyboxRenderer.h"
#include "TextureManager.h"
#include "WinApp.h"
#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG
#include <cmath>

using namespace DirectX;

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);

    if (!ctx_->renderTexture || !ctx_->postEffectRenderer ||
        !ctx_->skyboxRenderer) {
        return;
    }

    renderWidth_ = ctx_->winApp->GetWidth();
    renderHeight_ = ctx_->winApp->GetHeight();

    ctx_->renderTexture->Initialize(ctx_->dxCommon, ctx_->srv, renderWidth_,
                                    renderHeight_);
    ctx_->postEffectRenderer->Initialize(ctx_->dxCommon, ctx_->srv,
                                         renderWidth_, renderHeight_);
    ctx_->postEffectRenderer->SetColorMode(PostEffectRenderer::ColorMode::None);
    ctx_->postEffectRenderer->SetFilterMode(
        PostEffectRenderer::FilterMode::None);
    ctx_->postEffectRenderer->SetEdgeMode(PostEffectRenderer::EdgeMode::Depth);
    ctx_->postEffectRenderer->SetVignettingEnabled(true);

    camera_.Initialize(static_cast<float>(renderWidth_) /
                       static_cast<float>(renderHeight_));
    camera_.SetPosition({0.0f, 1.1f, -4.0f});
    camera_.LookAt({0.0f, 0.85f, 0.0f});
    camera_.SetPerspectiveFovDeg(45.0f);
    camera_.UpdateMatrices();
    ctx_->postEffectRenderer->SetDepthParameters(camera_.GetNearZ(),
                                                 camera_.GetFarZ());

    modelTransform_.position = {0.0f, 0.0f, 0.0f};
    modelTransform_.scale = {1.0f, 1.0f, 1.0f};

    ctx_->dxCommon->BeginUpload();
    modelId_ = ctx_->model->Load(L"resources/model/sneakWalk.gltf");
    skyboxModelId_ =
        ctx_->texture->Load(L"resources/rostock_laage_airport_4k.dds");
    environmentTextureId_ = skyboxModelId_;
    ctx_->dxCommon->EndUpload();
    ctx_->texture->ReleaseUploadBuffers();

    ctx_->skyboxRenderer->Initialize(ctx_->dxCommon, ctx_->srv, ctx_->texture);
    ctx_->modelRenderer->SetEnvironmentTexture(environmentTextureId_);
}

void GameScene::Update() {
    time_ += ctx_->deltaTime;
    ctx_->model->UpdateAnimation(modelId_, ctx_->deltaTime);
    UpdatePostEffectControls();

    const XMVECTOR rotation =
        XMQuaternionRotationRollPitchYaw(0.0f, time_ * 0.35f, 0.0f);
    XMStoreFloat4(&modelTransform_.rotation, rotation);
}

void GameScene::Draw() {
    ResizeOffscreenIfNeeded();
    DrawPostEffectControls();

    if (!ctx_->renderTexture || !ctx_->postEffectRenderer) {
        return;
    }

    ctx_->renderTexture->BeginRender({0.02f, 0.04f, 0.08f, 1.0f});
    DrawOffscreenScene();
    ctx_->renderTexture->EndRender();

    ctx_->dxCommon->TransitionDepthToShaderResource();
    ctx_->dxCommon->SetBackBufferRenderTarget(false, false);
    ctx_->postEffectRenderer->Draw(ctx_->renderTexture->GetGpuHandle(),
                                   ctx_->dxCommon->GetDepthStencilGpuHandle());
    ctx_->dxCommon->TransitionDepthToWrite();
}

void GameScene::UpdatePostEffectControls() {
    if (!ctx_->postEffectRenderer) {
        return;
    }

    if (ctx_->input) {
        if (ctx_->input->IsKeyTrigger(DIK_1)) {
            ctx_->postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::None);
        } else if (ctx_->input->IsKeyTrigger(DIK_2)) {
            ctx_->postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::Grayscale);
        } else if (ctx_->input->IsKeyTrigger(DIK_3)) {
            ctx_->postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::Sepia);
        } else if (ctx_->input->IsKeyTrigger(DIK_4)) {
            ctx_->postEffectRenderer->SetVignettingEnabled(
                !ctx_->postEffectRenderer->IsVignettingEnabled());
        } else if (ctx_->input->IsKeyTrigger(DIK_5)) {
            ctx_->postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Box3x3);
        } else if (ctx_->input->IsKeyTrigger(DIK_6)) {
            ctx_->postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Box5x5);
        } else if (ctx_->input->IsKeyTrigger(DIK_7)) {
            ctx_->postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Gaussian3x3);
        } else if (ctx_->input->IsKeyTrigger(DIK_8)) {
            ctx_->postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::GaussianBlur7x7);
        } else if (ctx_->input->IsKeyTrigger(DIK_9)) {
            ctx_->postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::None);
        } else if (ctx_->input->IsKeyTrigger(DIK_0)) {
            ctx_->postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::None);
        } else if (ctx_->input->IsKeyTrigger(DIK_Q)) {
            ctx_->postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::Luminance);
        } else if (ctx_->input->IsKeyTrigger(DIK_W)) {
            ctx_->postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::Depth);
        }
    }
}

void GameScene::DrawPostEffectControls() {
#ifdef _DEBUG
    if (!ctx_->postEffectRenderer) {
        return;
    }

    int colorMode = static_cast<int>(ctx_->postEffectRenderer->GetColorMode());
    int filterMode =
        static_cast<int>(ctx_->postEffectRenderer->GetFilterMode());
    int edgeMode = static_cast<int>(ctx_->postEffectRenderer->GetEdgeMode());
    bool enableVignetting = ctx_->postEffectRenderer->IsVignettingEnabled();
    float luminanceEdgeThreshold =
        ctx_->postEffectRenderer->GetLuminanceEdgeThreshold();
    float depthEdgeThreshold =
        ctx_->postEffectRenderer->GetDepthEdgeThreshold();
    if (ImGui::Begin("Post Effect")) {
        ImGui::RadioButton("None", &colorMode, 0);
        ImGui::RadioButton("Grayscale", &colorMode, 1);
        ImGui::RadioButton("Sepia", &colorMode, 2);
        ImGui::Checkbox("Vignetting", &enableVignetting);
        ImGui::Separator();
        ImGui::RadioButton("No Filter", &filterMode, 0);
        ImGui::RadioButton("3x3 Box Filter", &filterMode, 1);
        ImGui::RadioButton("5x5 Box Filter", &filterMode, 2);
        ImGui::RadioButton("3x3 Gaussian Filter", &filterMode, 3);
        ImGui::RadioButton("7x7 Gaussian Blur", &filterMode, 4);
        ImGui::Separator();
        ImGui::RadioButton("No Edge", &edgeMode, 0);
        ImGui::RadioButton("Luminance Edge", &edgeMode, 1);
        ImGui::RadioButton("Depth Edge", &edgeMode, 2);
        ImGui::SliderFloat("Luminance Threshold", &luminanceEdgeThreshold,
                           0.01f, 1.0f);
        ImGui::SliderFloat("Depth Threshold", &depthEdgeThreshold, 0.001f,
                           1.0f);
    }
    ImGui::End();

    ctx_->postEffectRenderer->SetColorMode(
        static_cast<PostEffectRenderer::ColorMode>(colorMode));
    ctx_->postEffectRenderer->SetFilterMode(
        static_cast<PostEffectRenderer::FilterMode>(filterMode));
    ctx_->postEffectRenderer->SetEdgeMode(
        static_cast<PostEffectRenderer::EdgeMode>(edgeMode));
    ctx_->postEffectRenderer->SetLuminanceEdgeThreshold(
        luminanceEdgeThreshold);
    ctx_->postEffectRenderer->SetDepthEdgeThreshold(depthEdgeThreshold);
    ctx_->postEffectRenderer->SetVignettingEnabled(enableVignetting);
#endif // _DEBUG
}

void GameScene::ResizeOffscreenIfNeeded() {
    if (!ctx_->renderTexture || !ctx_->postEffectRenderer) {
        return;
    }

    const int width = ctx_->winApp->GetWidth();
    const int height = ctx_->winApp->GetHeight();

    if (width <= 0 || height <= 0 ||
        (width == renderWidth_ && height == renderHeight_)) {
        return;
    }

    renderWidth_ = width;
    renderHeight_ = height;
    ctx_->renderTexture->Resize(renderWidth_, renderHeight_);
    ctx_->postEffectRenderer->Resize(renderWidth_, renderHeight_);
    camera_.SetAspect(static_cast<float>(renderWidth_) /
                      static_cast<float>(renderHeight_));
    camera_.UpdateMatrices();
}

void GameScene::DrawOffscreenScene() {
    if (!ctx_->skyboxRenderer) {
        return;
    }

    ModelRenderer *modelRenderer = ctx_->modelRenderer;
    const Model *model = ctx_->model->GetModel(modelId_);
    if (!model) {
        return;
    }

    ctx_->skyboxRenderer->Draw(skyboxModelId_, camera_);

    modelRenderer->PreDraw();
    modelRenderer->Draw(*model, modelTransform_, camera_, environmentTextureId_);
    modelRenderer->PostDraw();
}
