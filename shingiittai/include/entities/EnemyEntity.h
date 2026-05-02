#pragma once
#include "Entity.h"

#include <cstdint>

class World;

namespace AppEntities {

/// <summary>
/// 敵Entityを生成し、必要なComponentを追加する。
/// </summary>
/// <param name="world">Entityを生成するWorld。</param>
/// <param name="modelId">敵表示に使用するモデルID。</param>
/// <returns>生成された敵Entity ID。</returns>
Entity CreateEnemy(World &world, uint32_t modelId);

} // namespace AppEntities
