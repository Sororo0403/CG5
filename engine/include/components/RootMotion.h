#pragma once

#include <DirectXMath.h>
#include <string>

/// <summary>
/// root motionをTransformへ適用する設定。
/// </summary>
struct RootMotion {
    bool applyTranslation = true;
    bool applyVertical = false;
    float scale = 1.0f;
};

/// <summary>
/// root motion差分計算用の前回値。
/// </summary>
struct RootMotionState {
    std::string animationName;
    DirectX::XMFLOAT3 previousRootPosition{0.0f, 0.0f, 0.0f};
    bool initialized = false;
};
