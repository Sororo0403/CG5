#include "AnimationStateMachineSystem.h"

#include "AnimationComponents.h"
#include "AnimationStateMachine.h"
#include "World.h"

#include <algorithm>

void AnimationStateMachineSystem::Update(World &world, float deltaTime) {
    world.View<AnimationStateMachine, AnimationComponent>(
        [deltaTime](Entity, AnimationStateMachine &stateMachine,
                    AnimationComponent &animation) {
            stateMachine.stateTime += deltaTime;
            if (stateMachine.blendDuration > 0.0f) {
                stateMachine.blendTime =
                    (std::min)(stateMachine.blendTime + deltaTime,
                               stateMachine.blendDuration);
            }

            if (stateMachine.requestedState.empty() ||
                stateMachine.requestedState == stateMachine.state) {
                stateMachine.requestedState.clear();
                return;
            }

            stateMachine.previousState = stateMachine.state;
            stateMachine.state = stateMachine.requestedState;
            stateMachine.requestedState.clear();
            stateMachine.stateTime = 0.0f;
            stateMachine.blendTime = 0.0f;

            animation.currentAnimation = stateMachine.state;
            animation.time = 0.0f;
            animation.loop = stateMachine.loop;
            animation.playing = true;
            animation.finished = false;
        });
}
