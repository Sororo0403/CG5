#pragma once
#include <string>

/// <summary>
/// AnimationComponentへ再生状態を流し込む軽量ステートマシン。
/// </summary>
struct AnimationStateMachine {
    std::string state;
    std::string requestedState;
    std::string previousState;
    float stateTime = 0.0f;
    float blendDuration = 0.0f;
    float blendTime = 0.0f;
    bool loop = true;
};
