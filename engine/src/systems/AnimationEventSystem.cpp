#include "AnimationEventSystem.h"

#include "AnimationComponents.h"
#include "AnimationEvents.h"
#include "World.h"
#include "WorldCommandBuffer.h"

namespace {

bool Crossed(float previous, float current, float eventTime, bool looped) {
    if (!looped) {
        return previous < eventTime && eventTime <= current;
    }
    return eventTime > previous || eventTime <= current;
}

} // namespace

void AnimationEventSystem::Update(World &world) {
    WorldCommandBuffer commands;

    world.View<AnimationComponent, AnimationEventTrack>(
        [&world, &commands](Entity entity, AnimationComponent &animation,
                            AnimationEventTrack &track) {
            AnimationEventQueue *queue =
                world.TryGet<AnimationEventQueue>(entity);
            if (queue) {
                queue->events.clear();
            } else {
                commands.Add<AnimationEventQueue>(entity);
            }

            if (animation.currentAnimation.empty() || track.events.empty()) {
                return;
            }

            const bool looped =
                animation.loop && animation.time < animation.previousTime;
            for (const AnimationEvent &event : track.events) {
                if (!Crossed(animation.previousTime, animation.time,
                             event.time, looped)) {
                    continue;
                }

                if (queue) {
                    queue->events.push_back(event.name);
                } else {
                    commands.Enqueue(
                        [entity, name = event.name](World &targetWorld) {
                            AnimationEventQueue *targetQueue =
                                targetWorld.TryGet<AnimationEventQueue>(
                                    entity);
                            if (targetQueue) {
                                targetQueue->events.push_back(name);
                            }
                        });
                }
            }
        });

    commands.Playback(world);
}
