#pragma once

class Enemy;
class Player;
struct SceneContext;

class LightingController {
  public:
    void Reset();
    void Update(const SceneContext &ctx, const Player &player,
                const Enemy &enemy, float deltaTime);

  private:
    float sceneLightTime_ = 0.0f;
};
