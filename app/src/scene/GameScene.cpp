#include "GameScene.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "PostEffectRenderer.h"
#include "RenderTexture.h"
#include "SpriteRenderer.h"
#include "SkyboxRenderer.h"
#include "TextureManager.h"
#include "WinApp.h"
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

    camera_.Initialize(static_cast<float>(renderWidth_) /
                       static_cast<float>(renderHeight_));
    camera_.SetPosition({0.0f, 1.1f, -4.0f});
    camera_.LookAt({0.0f, 0.85f, 0.0f});
    camera_.SetPerspectiveFovDeg(45.0f);
    camera_.UpdateMatrices();

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

    const XMVECTOR rotation =
        XMQuaternionRotationRollPitchYaw(0.0f, time_ * 0.35f, 0.0f);
    XMStoreFloat4(&modelTransform_.rotation, rotation);
}

void GameScene::Draw() {
    ResizeOffscreenIfNeeded();

    if (!ctx_->renderTexture || !ctx_->postEffectRenderer) {
        return;
    }

    ctx_->renderTexture->BeginRender({0.02f, 0.04f, 0.08f, 1.0f});
    DrawOffscreenScene();
    ctx_->renderTexture->EndRender();

    ctx_->dxCommon->SetBackBufferRenderTarget(false);
    ctx_->postEffectRenderer->Draw(ctx_->renderTexture->GetGpuHandle());
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
