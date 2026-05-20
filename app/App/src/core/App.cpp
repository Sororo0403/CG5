#include "App.h"

#include "Engine.h"
#include "scene/GameScene.h"

#include <memory>

namespace {

std::unique_ptr<BaseScene> CreateGameScene() {
    return std::make_unique<GameScene>();
}

EngineRuntimeConfig CreateRuntimeConfig() {
    EngineRuntimeConfig config{};
    config.title = L"CG5";
    return config;
}

} // namespace

int App::Run(HINSTANCE instance, int showCommand) {
    EngineRuntime engine;
    return engine.Run(instance, showCommand, CreateGameScene(),
                      CreateRuntimeConfig());
}
