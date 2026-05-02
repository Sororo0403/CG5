#pragma once
#include <cstdint>

enum class InputSourceKind {
    None,
    KeyboardMouse,
    JoyCon,
    AI,
};

/// <summary>
/// Entityがどの入力源から操作されるかを表すComponent。
/// </summary>
struct InputSource {
    InputSourceKind kind = InputSourceKind::None;
    uint32_t playerIndex = 0;
};
