#pragma once

class Input;
class World;

/// <summary>
/// 物理入力をActionInputとInputBufferへ変換するSystem。
/// </summary>
struct ActionInputSystem {
    void Update(World &world, const Input &input, float deltaTime);
};
