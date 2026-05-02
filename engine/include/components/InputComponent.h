#pragma once
#include <DirectXMath.h>

/// <summary>
/// 正規化された入力状態を保持するComponent。
/// </summary>
struct InputComponent {
    DirectX::XMFLOAT2 move{0.0f, 0.0f};
    bool attack = false;
    bool guard = false;
    bool dash = false;
};
