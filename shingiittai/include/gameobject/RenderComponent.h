#pragma once
#include "Component.h"
#include "Transform.h"

#include <cstdint>
#include <functional>

class ModelManager;

class RenderComponent : public Component {
  public:
    using DrawCallback = std::function<void(ModelManager *, const Camera &)>;

    RenderComponent(ModelManager *modelManager, uint32_t modelId);
    RenderComponent(ModelManager *modelManager, uint32_t modelId,
                    DrawCallback drawCallback);

    void SetModel(ModelManager *modelManager, uint32_t modelId);
    void SetDrawCallback(DrawCallback drawCallback);
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }
    uint32_t GetModelId() const { return modelId_; }

    void Draw(const Camera &camera) override;

  private:
    ModelManager *modelManager_ = nullptr;
    uint32_t modelId_ = 0;
    bool visible_ = true;
    DrawCallback drawCallback_{};
};
