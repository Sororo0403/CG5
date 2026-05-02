#include "InputSystem.h"

#include "Input.h"
#include "InputComponent.h"
#include "InputSource.h"
#include "World.h"

#include <cmath>

void InputSystem::Update(World &world, const Input &input) {
    world.View<InputComponent, InputSource>(
        [&input](Entity, InputComponent &component, InputSource &source) {
            if (source.kind != InputSourceKind::KeyboardMouse ||
                source.playerIndex != 0) {
                return;
            }

            component.move = {0.0f, 0.0f};
            component.attack = false;
            component.guard = false;
            component.dash = false;

            if (input.IsKeyPress(DIK_A)) {
                component.move.x -= 1.0f;
            }
            if (input.IsKeyPress(DIK_D)) {
                component.move.x += 1.0f;
            }
            if (input.IsKeyPress(DIK_W)) {
                component.move.y += 1.0f;
            }
            if (input.IsKeyPress(DIK_S)) {
                component.move.y -= 1.0f;
            }

            const float length = std::sqrt(component.move.x * component.move.x +
                                           component.move.y * component.move.y);
            if (length > 0.0f) {
                component.move.x /= length;
                component.move.y /= length;
            }

            component.attack = input.IsMouseTrigger(0);
            component.guard = input.IsMousePress(1);
            component.dash = input.IsKeyPress(DIK_LSHIFT);
        });
}
