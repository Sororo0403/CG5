#include "PreviousTransformSystem.h"

#include "GroundingComponents.h"
#include "Transform.h"
#include "World.h"
#include "WorldCommandBuffer.h"

void PreviousTransformSystem::Update(World &world) {
    WorldCommandBuffer commands;
    world.View<Transform>([&world, &commands](Entity entity,
                                              Transform &transform) {
        PreviousTransform *previous = world.TryGet<PreviousTransform>(entity);
        if (previous) {
            previous->transform = transform;
        } else {
            commands.Add<PreviousTransform>(entity,
                                            PreviousTransform{transform});
        }
    });
    commands.Playback(world);
}
