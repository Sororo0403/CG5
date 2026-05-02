#include "RenderSystem.h"

#include "Camera.h"
#include "MeshRenderer.h"
#include "ModelManager.h"
#include "Transform.h"
#include "World.h"

void RenderSystem::Draw(World &world, ModelManager &modelManager,
                        const Camera &camera) {
    modelManager.PreDraw();
    world.View<Transform, MeshRenderer>(
        [&modelManager, &camera](Entity, Transform &transform,
                                 MeshRenderer &renderer) {
            if (!renderer.visible) {
                return;
            }
            modelManager.Draw(renderer.modelId, transform, camera);
        });
    modelManager.PostDraw();
}
