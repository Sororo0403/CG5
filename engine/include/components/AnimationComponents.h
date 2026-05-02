#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>

/// <summary>
/// Entityごとのアニメーション再生状態。
/// </summary>
struct AnimationComponent {
    std::string currentAnimation;
    float time = 0.0f;
    bool loop = true;
    bool playing = true;
    bool finished = false;
};

/// <summary>
/// Entityごとのスケルトン姿勢。
/// </summary>
struct SkeletonPoseComponent {
    std::vector<DirectX::XMFLOAT4X4> skeletonSpaceMatrices;
    std::vector<DirectX::XMFLOAT4X4> finalBoneMatrices;

    bool hasRootAnimation = false;
    DirectX::XMFLOAT4X4 rootAnimationMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f, //
        0.0f, 1.0f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.0f, 0.0f, 0.0f, 1.0f  //
    };
};
