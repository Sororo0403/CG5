#pragma once
#include "Entity.h"

#include <cstdint>

class World;

namespace AppEntities {

/// <summary>
/// プレイヤーEntityを生成し、必要なComponentを追加する。
/// </summary>
/// <param name="world">Entityを生成するWorld。</param>
/// <param name="modelId">プレイヤー表示に使用するモデルID。</param>
/// <returns>生成されたプレイヤーEntity ID。</returns>
Entity CreatePlayer(World &world, uint32_t modelId);

} // namespace AppEntities
