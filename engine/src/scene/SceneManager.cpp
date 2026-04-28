#include "SceneManager.h"
#include "BaseScene.h"
#include <stdexcept>

void SceneManager::Initialize(const SceneContext &ctx) { ctx_ = &ctx; }

void SceneManager::ChangeScene(std::unique_ptr<BaseScene> nextScene) {
    if (!ctx_) {
        throw std::runtime_error("SceneManager is not initialized");
    }
    if (!nextScene) {
        throw std::invalid_argument("SceneManager::ChangeScene received null");
    }

    currentScene_.reset();

    currentScene_ = std::move(nextScene);
    currentScene_->SetSceneManager(this);
    currentScene_->Initialize(*ctx_);
}
void SceneManager::Update() {
    if (currentScene_) {
        currentScene_->Update();
    }
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::Resize(int width, int height) {
    if (currentScene_) {
        currentScene_->OnResize(width, height);
    }
}
