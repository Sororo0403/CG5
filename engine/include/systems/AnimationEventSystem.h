#pragma once

class World;

/// <summary>
/// AnimationComponentの再生時間に応じてAnimationEventQueueへイベントを積む。
/// </summary>
struct AnimationEventSystem {
    void Update(World &world);
};
