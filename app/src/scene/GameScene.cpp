#include "GameScene.h"
#include "DirectXCommon.h"
#include "SpriteRenderer.h"
#include "WinApp.h"
#include <cmath>

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);

    renderWidth_ = ctx_->winApp->GetWidth();
    renderHeight_ = ctx_->winApp->GetHeight();

    renderTexture_.Initialize(ctx_->dxCommon, ctx_->srv, renderWidth_,
                              renderHeight_);
    postEffectRenderer_.Initialize(ctx_->dxCommon, ctx_->srv, renderWidth_,
                                   renderHeight_);
}

void GameScene::Update() { time_ += ctx_->deltaTime; }

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

    const float centerX = static_cast<float>(renderWidth_) * 0.5f;
    const float centerY = static_cast<float>(renderHeight_) * 0.5f;
    const float wave = (std::sinf(time_ * 2.0f) + 1.0f) * 0.5f;

    Sprite panel{};
    panel.textureId = 0;
    panel.position = {centerX - 300.0f, centerY - 170.0f};
    panel.size = {600.0f, 340.0f};
    panel.color = {0.95f, 0.30f + 0.25f * wave, 0.12f, 1.0f};
    spriteRenderer->Draw(panel);

    Sprite viewport{};
    viewport.textureId = 0;
    viewport.position = {centerX - 230.0f, centerY - 110.0f};
    viewport.size = {460.0f, 220.0f};
    viewport.color = {0.13f, 0.37f, 0.62f, 1.0f};
    spriteRenderer->Draw(viewport);

    Sprite marker{};
    marker.textureId = 0;
    marker.position = {centerX - 70.0f + 140.0f * wave, centerY - 52.0f};
    marker.size = {140.0f, 104.0f};
    marker.color = {0.92f, 0.95f, 1.0f, 1.0f};
    spriteRenderer->Draw(marker);

    spriteRenderer->PostDraw();
}
