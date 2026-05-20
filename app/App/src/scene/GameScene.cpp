#include "scene/GameScene.h"

#include "core/WinApp.h"
#include "input/Input.h"

void GameScene::Update() {
    if (ctx_ && ctx_->systems.input && ctx_->systems.winApp &&
        ctx_->systems.input->IsKeyTrigger(DIK_ESCAPE)) {
        ctx_->systems.winApp->RequestClose();
    }
}

void GameScene::Draw() {}
