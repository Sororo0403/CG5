#pragma once
#include <string>
#include <vector>

/// <summary>
/// アニメーション中の特定時刻に発生するイベント。
/// </summary>
struct AnimationEvent {
    std::string name;
    float time = 0.0f;
};

/// <summary>
/// Entityごとのアニメーションイベント定義。
/// </summary>
struct AnimationEventTrack {
    std::vector<AnimationEvent> events;
};

/// <summary>
/// 直近フレームで発火したアニメーションイベント。
/// </summary>
struct AnimationEventQueue {
    std::vector<std::string> events;
};
