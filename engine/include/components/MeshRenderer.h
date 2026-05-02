#pragma once
#include <cstdint>

/// <summary>
/// モデル描画に必要な参照情報を保持するComponent。
/// </summary>
struct MeshRenderer {
    uint32_t modelId = 0;
    bool visible = true;
};
