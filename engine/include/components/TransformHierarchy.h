#pragma once
#include "Entity.h"
#include "Transform.h"

/// <summary>
/// 親Entityを参照するComponent。
/// </summary>
struct ParentComponent {
    Entity parent = kInvalidEntity;
};

/// <summary>
/// 親からの相対Transformを保持するComponent。
/// </summary>
struct LocalTransform {
    Transform transform{};
};
