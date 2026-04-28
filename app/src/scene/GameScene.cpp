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

    if (!ctx_->renderer.renderTexture || !ctx_->renderer.postEffectRenderer ||
        !ctx_->renderer.skyboxRenderer) {
        return;
    }

    renderWidth_ = ctx_->frame.width;
    renderHeight_ = ctx_->frame.height;

    ctx_->renderer.renderTexture->Initialize(ctx_->core.dxCommon, ctx_->core.srv,
                                             renderWidth_, renderHeight_);
    ctx_->renderer.postEffectRenderer->Initialize(
        ctx_->core.dxCommon, ctx_->core.srv, renderWidth_, renderHeight_);
    ctx_->renderer.postEffectRenderer->SetColorMode(
        PostEffectRenderer::ColorMode::None);
    ctx_->renderer.postEffectRenderer->SetFilterMode(
        PostEffectRenderer::FilterMode::None);
    ctx_->renderer.postEffectRenderer->SetEdgeMode(
        PostEffectRenderer::EdgeMode::Depth);
    ctx_->renderer.postEffectRenderer->SetVignettingEnabled(true);
    ctx_->renderer.postEffectRenderer->SetRadialBlurCenter(0.5f, 0.5f);
    ctx_->renderer.postEffectRenderer->SetRadialBlurStrength(0.18f);
    ctx_->renderer.postEffectRenderer->SetRadialBlurSampleCount(12);
    ctx_->renderer.postEffectRenderer->SetRandomMode(
        PostEffectRenderer::RandomMode::OverlayNoise);
    ctx_->renderer.postEffectRenderer->SetRandomStrength(randomNoiseStrength_);
    ctx_->renderer.postEffectRenderer->SetRandomScale(randomNoiseScale_);

    camera_.Initialize(static_cast<float>(renderWidth_) /
                       static_cast<float>(renderHeight_));
    camera_.SetPosition({0.0f, 1.1f, -4.0f});
    camera_.LookAt({0.0f, 0.85f, 0.0f});
    camera_.SetPerspectiveFovDeg(45.0f);
    camera_.UpdateMatrices();
    ctx_->renderer.postEffectRenderer->SetDepthParameters(camera_.GetNearZ(),
                                                          camera_.GetFarZ());

    modelTransform_.position = {0.0f, 0.0f, 0.0f};
    modelTransform_.scale = {1.0f, 1.0f, 1.0f};

    auto uploadContext = ctx_->core.dxCommon->BeginUploadContext();
    modelId_ = ctx_->assets.model->Load(uploadContext,
                                        L"resources/model/sneakWalk.gltf");
    skyboxModelId_ = ctx_->assets.texture->Load(
        uploadContext, L"resources/rostock_laage_airport_4k.dds");
    environmentTextureId_ = skyboxModelId_;
    dissolveNoiseTextureId_ =
        ctx_->assets.texture->CreateNoiseTexture(uploadContext, 256, 256);
    uploadContext.Finish();
    ctx_->assets.texture->ReleaseUploadBuffers();

    ctx_->renderer.skyboxRenderer->Initialize(
        ctx_->core.dxCommon, ctx_->core.srv, ctx_->assets.texture);
    ctx_->renderer.model->SetEnvironmentTexture(environmentTextureId_);
    ctx_->renderer.model->SetDissolveNoiseTexture(dissolveNoiseTextureId_);
    ApplyDissolveMaterial();
}

void GameScene::Update() {
    time_ += ctx_->frame.deltaTime;
    if (randomNoiseAnimate_) {
        randomNoiseTime_ += ctx_->frame.deltaTime;
    }
    if (ctx_->renderer.postEffectRenderer) {
        ctx_->renderer.postEffectRenderer->SetRandomTime(randomNoiseTime_);
    }

    ctx_->assets.model->UpdateAnimation(modelId_, ctx_->frame.deltaTime);
    if (dissolveAutoAnimate_) {
        dissolveThreshold_ = 0.5f + 0.5f * std::sin(time_ * 0.8f);
    }
    ApplyDissolveMaterial();
    UpdatePostEffectControls();

    const XMVECTOR rotation =
        XMQuaternionRotationRollPitchYaw(0.0f, time_ * 0.35f, 0.0f);
    XMStoreFloat4(&modelTransform_.rotation, rotation);
}

void GameScene::ApplyDissolveMaterial() {
    Model *model =
        ctx_->assets.model ? ctx_->assets.model->GetModel(modelId_) : nullptr;
    if (!model) {
        return;
    }

    for (const ModelSubMesh &subMesh : model->subMeshes) {
        Material material = ctx_->assets.model->GetMaterial(subMesh.materialId);
        material.enableDissolve = dissolveEnabled_ ? 1 : 0;
        material.dissolveThreshold = dissolveThreshold_;
        material.dissolveEdgeWidth = dissolveEdgeWidth_;
        material.dissolveEdgeColor = {dissolveEdgeColor_[0],
                                      dissolveEdgeColor_[1],
                                      dissolveEdgeColor_[2],
                                      dissolveEdgeColor_[3]};
        ctx_->assets.model->SetMaterial(subMesh.materialId, material);
    }
}

void GameScene::Draw() {
    DrawPostEffectControls();

    if (!ctx_->renderer.renderTexture || !ctx_->renderer.postEffectRenderer) {
        return;
    }

    ctx_->renderer.renderTexture->BeginRender({0.02f, 0.04f, 0.08f, 1.0f});
    DrawOffscreenScene();
    ctx_->renderer.renderTexture->EndRender();

    ctx_->core.dxCommon->TransitionDepthToShaderResource();
    ctx_->core.dxCommon->SetBackBufferRenderTarget(false, false);
    ctx_->renderer.postEffectRenderer->Draw(
        ctx_->renderer.renderTexture->GetGpuHandle(),
        ctx_->core.dxCommon->GetDepthStencilGpuHandle());
    ctx_->core.dxCommon->TransitionDepthToWrite();
}

void GameScene::UpdatePostEffectControls() {
    if (!ctx_->renderer.postEffectRenderer) {
        return;
    }

    if (ctx_->core.input) {
        if (ctx_->core.input->IsKeyTrigger(DIK_1)) {
            ctx_->renderer.postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::None);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_2)) {
            ctx_->renderer.postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::Grayscale);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_3)) {
            ctx_->renderer.postEffectRenderer->SetColorMode(
                PostEffectRenderer::ColorMode::Sepia);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_4)) {
            ctx_->renderer.postEffectRenderer->SetVignettingEnabled(
                !ctx_->renderer.postEffectRenderer->IsVignettingEnabled());
        } else if (ctx_->core.input->IsKeyTrigger(DIK_5)) {
            ctx_->renderer.postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Box3x3);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_6)) {
            ctx_->renderer.postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Box5x5);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_7)) {
            ctx_->renderer.postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::Gaussian3x3);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_8)) {
            ctx_->renderer.postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::GaussianBlur7x7);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_9)) {
            ctx_->renderer.postEffectRenderer->SetFilterMode(
                PostEffectRenderer::FilterMode::None);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_0)) {
            ctx_->renderer.postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::None);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_Q)) {
            ctx_->renderer.postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::Luminance);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_W)) {
            ctx_->renderer.postEffectRenderer->SetEdgeMode(
                PostEffectRenderer::EdgeMode::Depth);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_R)) {
            const float strength =
                ctx_->renderer.postEffectRenderer->GetRadialBlurStrength();
            ctx_->renderer.postEffectRenderer->SetRadialBlurStrength(
                strength > 0.0f ? 0.0f : 0.18f);
        } else if (ctx_->core.input->IsKeyTrigger(DIK_T)) {
            const int mode =
                (static_cast<int>(
                     ctx_->renderer.postEffectRenderer->GetRandomMode()) +
                 1) %
                3;
            ctx_->renderer.postEffectRenderer->SetRandomMode(
                static_cast<PostEffectRenderer::RandomMode>(mode));
        } else if (ctx_->core.input->IsKeyTrigger(DIK_Y)) {
            randomNoiseAnimate_ = !randomNoiseAnimate_;
        }
    }
}

void GameScene::DrawPostEffectControls() {
#ifdef _DEBUG
    if (!ctx_->renderer.postEffectRenderer) {
        return;
    }

    int colorMode =
        static_cast<int>(ctx_->renderer.postEffectRenderer->GetColorMode());
    int filterMode =
        static_cast<int>(ctx_->renderer.postEffectRenderer->GetFilterMode());
    int edgeMode =
        static_cast<int>(ctx_->renderer.postEffectRenderer->GetEdgeMode());
    bool enableVignetting =
        ctx_->renderer.postEffectRenderer->IsVignettingEnabled();
    float luminanceEdgeThreshold =
        ctx_->renderer.postEffectRenderer->GetLuminanceEdgeThreshold();
    float depthEdgeThreshold =
        ctx_->renderer.postEffectRenderer->GetDepthEdgeThreshold();
    const float *radialBlurCenter =
        ctx_->renderer.postEffectRenderer->GetRadialBlurCenter();
    float radialBlurCenterEdit[2] = {radialBlurCenter[0], radialBlurCenter[1]};
    float radialBlurStrength =
        ctx_->renderer.postEffectRenderer->GetRadialBlurStrength();
    int radialBlurSampleCount =
        ctx_->renderer.postEffectRenderer->GetRadialBlurSampleCount();
    int randomMode =
        static_cast<int>(ctx_->renderer.postEffectRenderer->GetRandomMode());
    float randomStrength =
        ctx_->renderer.postEffectRenderer->GetRandomStrength();
    float randomScale = ctx_->renderer.postEffectRenderer->GetRandomScale();
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
        ImGui::Separator();
        ImGui::SliderFloat2("Radial Blur Center", radialBlurCenterEdit, 0.0f,
                            1.0f);
        ImGui::SliderFloat("Radial Blur Strength", &radialBlurStrength, 0.0f,
                           1.0f);
        ImGui::SliderInt("Radial Blur Samples", &radialBlurSampleCount, 2, 32);
        ImGui::Separator();
        ImGui::RadioButton("No Random", &randomMode, 0);
        ImGui::RadioButton("Grayscale Random", &randomMode, 1);
        ImGui::RadioButton("Overlay Random", &randomMode, 2);
        ImGui::Checkbox("Random Animate", &randomNoiseAnimate_);
        ImGui::SliderFloat("Random Strength", &randomStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Random Scale", &randomScale, 1.0f, 1024.0f);
        ImGui::Separator();
        ImGui::Checkbox("Dissolve", &dissolveEnabled_);
        ImGui::Checkbox("Dissolve Auto", &dissolveAutoAnimate_);
        ImGui::SliderFloat("Dissolve Threshold", &dissolveThreshold_, 0.0f,
                           1.0f);
        ImGui::SliderFloat("Dissolve Edge Width", &dissolveEdgeWidth_, 0.001f,
                           0.25f);
        ImGui::ColorEdit4("Dissolve Edge Color", dissolveEdgeColor_);
    }
    ImGui::End();

    ctx_->renderer.postEffectRenderer->SetColorMode(
        static_cast<PostEffectRenderer::ColorMode>(colorMode));
    ctx_->renderer.postEffectRenderer->SetFilterMode(
        static_cast<PostEffectRenderer::FilterMode>(filterMode));
    ctx_->renderer.postEffectRenderer->SetEdgeMode(
        static_cast<PostEffectRenderer::EdgeMode>(edgeMode));
    ctx_->renderer.postEffectRenderer->SetLuminanceEdgeThreshold(
        luminanceEdgeThreshold);
    ctx_->renderer.postEffectRenderer->SetDepthEdgeThreshold(depthEdgeThreshold);
    ctx_->renderer.postEffectRenderer->SetVignettingEnabled(enableVignetting);
    ctx_->renderer.postEffectRenderer->SetRadialBlurCenter(
        radialBlurCenterEdit[0], radialBlurCenterEdit[1]);
    ctx_->renderer.postEffectRenderer->SetRadialBlurStrength(radialBlurStrength);
    ctx_->renderer.postEffectRenderer->SetRadialBlurSampleCount(
        radialBlurSampleCount);
    randomNoiseStrength_ = randomStrength;
    randomNoiseScale_ = randomScale;
    ctx_->renderer.postEffectRenderer->SetRandomMode(
        static_cast<PostEffectRenderer::RandomMode>(randomMode));
    ctx_->renderer.postEffectRenderer->SetRandomStrength(randomNoiseStrength_);
    ctx_->renderer.postEffectRenderer->SetRandomScale(randomNoiseScale_);
#endif // _DEBUG
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
    ctx_->renderer.postEffectRenderer->Resize(renderWidth_, renderHeight_);
    camera_.SetAspect(static_cast<float>(renderWidth_) /
                      static_cast<float>(renderHeight_));
    camera_.UpdateMatrices();
}

void GameScene::DrawOffscreenScene() {
    if (!ctx_->renderer.skyboxRenderer) {
        return;
    }

    ModelRenderer *modelRenderer = ctx_->renderer.model;
    const Model *model = ctx_->assets.model->GetModel(modelId_);
    if (!model) {
        return;
    }

    ctx_->renderer.skyboxRenderer->Draw(skyboxModelId_, camera_);

    modelRenderer->PreDraw();
    ApplyDissolveMaterial();
    modelRenderer->Draw(*model, modelTransform_, camera_, environmentTextureId_);
    modelRenderer->PostDraw();
}
