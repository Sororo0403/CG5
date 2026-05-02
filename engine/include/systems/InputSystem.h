#pragma once

class Input;
class World;

/// <summary>
/// Engine入力をECSのInputComponentへ変換するSystem。
/// </summary>
struct InputSystem {
    /// <summary>
    /// キーボード入力をInputComponentへ反映する。
    /// </summary>
    /// <param name="world">更新対象のWorld。</param>
    /// <param name="input">Engine入力管理オブジェクト。</param>
    void Update(World &world, const Input &input);
};
