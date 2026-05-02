#include "ActionInputSystem.h"

#include "ActionInput.h"
#include "Input.h"
#include "InputSource.h"
#include "World.h"

#include <algorithm>
#include <cmath>

namespace {

void TickBuffer(float &time, float deltaTime) {
    time = (std::max)(0.0f, time - deltaTime);
}

void Normalize(DirectX::XMFLOAT2 &value) {
    const float length =
        std::sqrt(value.x * value.x + value.y * value.y);
    if (length > 0.0001f) {
        value.x /= length;
        value.y /= length;
    }
}

} // namespace

void ActionInputSystem::Update(World &world, const Input &input,
                               float deltaTime) {
    world.View<ActionInput, InputBuffer, InputSource>(
        [&input, deltaTime](Entity, ActionInput &action, InputBuffer &buffer,
                            InputSource &source) {
            TickBuffer(buffer.jumpTime, deltaTime);
            TickBuffer(buffer.attackTime, deltaTime);
            TickBuffer(buffer.dashTime, deltaTime);

            action = ActionInput{};
            if (source.kind != InputSourceKind::KeyboardMouse ||
                source.playerIndex != 0) {
                return;
            }

            if (input.IsKeyPress(DIK_A) || input.IsKeyPress(DIK_LEFT)) {
                action.move.x -= 1.0f;
            }
            if (input.IsKeyPress(DIK_D) || input.IsKeyPress(DIK_RIGHT)) {
                action.move.x += 1.0f;
            }
            if (input.IsKeyPress(DIK_W) || input.IsKeyPress(DIK_UP)) {
                action.move.y += 1.0f;
            }
            if (input.IsKeyPress(DIK_S) || input.IsKeyPress(DIK_DOWN)) {
                action.move.y -= 1.0f;
            }
            Normalize(action.move);

            action.look = {static_cast<float>(input.GetMouseDX()),
                           static_cast<float>(input.GetMouseDY())};
            action.jumpPressed = input.IsKeyTrigger(DIK_SPACE);
            action.jumpHeld = input.IsKeyPress(DIK_SPACE);
            action.attackPressed = input.IsMouseTrigger(0);
            action.guardHeld = input.IsMousePress(1);
            action.dashPressed = input.IsKeyTrigger(DIK_LSHIFT);
            action.lockOnPressed = input.IsKeyTrigger(DIK_TAB);

            if (action.jumpPressed) {
                buffer.jumpTime = buffer.bufferDuration;
            }
            if (action.attackPressed) {
                buffer.attackTime = buffer.bufferDuration;
            }
            if (action.dashPressed) {
                buffer.dashTime = buffer.bufferDuration;
            }
        });
}
