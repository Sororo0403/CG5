#pragma once
#include "panels/EnemyTuningPanel.h"

struct EditorContext;

class GameplayPanel {
  public:
    void Draw(EditorContext &context);

  private:
    EnemyTuningPanel enemyTuningPanel_;
};
