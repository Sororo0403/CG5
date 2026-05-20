#pragma once

#include "scene/BaseScene.h"

class GameScene final : public BaseScene {
  public:
    void Update() override;
    void Draw() override;
};
