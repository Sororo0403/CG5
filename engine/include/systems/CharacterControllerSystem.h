#pragma once

class World;

/// <summary>
/// ActionInputからアクションゲーム向けVelocityを生成するSystem。
/// </summary>
struct CharacterControllerSystem {
    void Update(World &world, float deltaTime);
};
