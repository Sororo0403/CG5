#include "GameScene.h"
#include "DirectXCommon.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <cmath>

using namespace DirectX;

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);

    renderWidth_ = ctx_->winApp->GetWidth();
    renderHeight_ = ctx_->winApp->GetHeight();

    renderTexture_.Initialize(ctx_->dxCommon, ctx_->srv, renderWidth_,
                              renderHeight_);
    postEffectRenderer_.Initialize(ctx_->dxCommon, ctx_->srv, renderWidth_,
                                   renderHeight_);

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
    environmentTextureId_ = ctx_->texture->CreateDebugCubemap();
    ctx_->dxCommon->EndUpload();
    ctx_->texture->ReleaseUploadBuffers();

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

    renderTexture_.BeginRender({0.02f, 0.04f, 0.08f, 1.0f});
    DrawOffscreenScene();
    renderTexture_.EndRender();

    ctx_->dxCommon->SetBackBufferRenderTarget(false);
    postEffectRenderer_.Draw(renderTexture_.GetGpuHandle());
}

void GameScene::ResizeOffscreenIfNeeded() {
    const int width = ctx_->winApp->GetWidth();
    const int height = ctx_->winApp->GetHeight();

    if (width <= 0 || height <= 0 ||
        (width == renderWidth_ && height == renderHeight_)) {
        return;
    }

    renderWidth_ = width;
    renderHeight_ = height;
    renderTexture_.Resize(renderWidth_, renderHeight_);
    postEffectRenderer_.Resize(renderWidth_, renderHeight_);
    camera_.SetAspect(static_cast<float>(renderWidth_) /
                      static_cast<float>(renderHeight_));
    camera_.UpdateMatrices();
}

void GameScene::DrawOffscreenScene() {
    SpriteRenderer *spriteRenderer = ctx_->spriteRenderer;
    spriteRenderer->PreDraw();

    Sprite background{};
    background.textureId = 0;
    background.position = {0.0f, 0.0f};
    background.size = {static_cast<float>(renderWidth_),
                       static_cast<float>(renderHeight_)};
    background.color = {0.04f, 0.08f, 0.14f, 1.0f};
    spriteRenderer->Draw(background);

    spriteRenderer->PostDraw();

    ModelRenderer *modelRenderer = ctx_->modelRenderer;
    const Model *model = ctx_->model->GetModel(modelId_);
    if (!model) {
        return;
    }

    modelRenderer->PreDraw();
    modelRenderer->Draw(*model, modelTransform_, camera_, environmentTextureId_);
    modelRenderer->PostDraw();
}
