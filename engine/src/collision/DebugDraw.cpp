#include "DebugDraw.h"
#include "Camera.h"
#include "ModelAssets.h"
#include "ModelRenderer.h"

void DebugDraw::DrawOBB(const ModelAssets *modelAssets,
                        ModelRenderer *modelRenderer, const OBB &obb,
                        const Camera &camera) {
    if (!modelAssets || !modelRenderer) {
        return;
    }

    const Model *model = modelAssets->GetModel(boxModelId_);
    if (!model) {
        return;
    }

    Transform tf;
    tf.position = obb.center;
    tf.rotation = obb.rotation;
    tf.scale = {obb.size.x * 2.0f, obb.size.y * 2.0f, obb.size.z * 2.0f};
    modelRenderer->Draw(*model, tf, camera);
}

void DebugDraw::DrawAABB(const ModelAssets *modelAssets,
                         ModelRenderer *modelRenderer, const AABB &aabb,
                         const Camera &camera) {
    if (!modelAssets || !modelRenderer) {
        return;
    }

    const Model *model = modelAssets->GetModel(boxModelId_);
    if (!model) {
        return;
    }

    Transform tf;
    tf.position = {(aabb.min.x + aabb.max.x) * 0.5f,
                   (aabb.min.y + aabb.max.y) * 0.5f,
                   (aabb.min.z + aabb.max.z) * 0.5f};
    tf.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    tf.scale = {aabb.max.x - aabb.min.x, aabb.max.y - aabb.min.y,
                aabb.max.z - aabb.min.z};
    modelRenderer->Draw(*model, tf, camera);
}
