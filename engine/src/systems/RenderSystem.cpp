#include "RenderSystem.h"

#include "AnimationComponents.h"
#include "Camera.h"
#include "MeshRenderer.h"
#include "ModelAssets.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include "TransformHierarchy.h"
#include "World.h"

void RenderSystem::Draw(World &world, ModelAssets &modelAssets,
                        ModelRenderer &modelRenderer, const Camera &camera) {
    modelRenderer.ReleaseUnusedInstanceSkinPalettes(world);
    modelRenderer.PreDraw();
    world.View<Transform, MeshRenderer>(
        [&world, &modelAssets, &modelRenderer, &camera](
            Entity entity, Transform &transform, MeshRenderer &meshRenderer) {
            if (!meshRenderer.visible) {
                return;
            }
            Model *model = modelAssets.GetModel(meshRenderer.modelId);
            if (!model) {
                return;
            }

            if (const auto *pose =
                    world.TryGet<SkeletonPoseComponent>(entity)) {
                if (const auto *worldMatrix =
                        world.TryGet<WorldTransformMatrix>(entity)) {
                    modelRenderer.Draw(*model, worldMatrix->matrix, camera,
                                       *pose, entity);
                    return;
                }
                modelRenderer.Draw(*model, transform, camera, *pose, entity);
                return;
            }

            if (const auto *worldMatrix =
                    world.TryGet<WorldTransformMatrix>(entity)) {
                modelRenderer.Draw(*model, worldMatrix->matrix, camera);
                return;
            }

            modelRenderer.Draw(*model, transform, camera);
        });
    modelRenderer.PostDraw();
}
