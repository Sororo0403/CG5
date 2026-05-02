#include "RenderSystem.h"

#include "Camera.h"
#include "MeshRenderer.h"
#include "ModelAssets.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include "World.h"

void RenderSystem::Draw(World &world, const ModelAssets &modelAssets,
                        ModelRenderer &modelRenderer, const Camera &camera) {
    modelRenderer.PreDraw();
    world.View<Transform, MeshRenderer>(
        [&modelAssets, &modelRenderer, &camera](
            Entity, Transform &transform, MeshRenderer &meshRenderer) {
            if (!meshRenderer.visible) {
                return;
            }
            const Model *model = modelAssets.GetModel(meshRenderer.modelId);
            if (!model) {
                return;
            }
            modelRenderer.Draw(*model, transform, camera);
        });
    modelRenderer.PostDraw();
}
