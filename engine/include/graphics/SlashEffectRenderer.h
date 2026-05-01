#pragma once
#include <DirectXMath.h>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

#ifndef IMGUI_DISABLED
#include "Camera.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#endif

class [[deprecated("Use EffectSystem with EffectPreset::SlashArc instead.")]]
SlashEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *) {
        initialized_ = true;
    }
    void DrawEnemySlash(const Camera &, const DirectX::XMFLOAT3 &,
                        const DirectX::XMFLOAT3 &, float, float, bool);

  private:
    bool initialized_ = false;
};

#ifndef IMGUI_DISABLED
inline bool ProjectSlashPoint(const Camera &camera, const DirectX::XMFLOAT3 &world,
                              ImVec2 &out) {
    using namespace DirectX;
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return false;
    }
    XMVECTOR pos = XMVectorSet(world.x, world.y, world.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(pos, camera.GetView() * camera.GetProj());
    const float w = XMVectorGetW(clip);
    if (w <= 0.0001f) {
        return false;
    }
    const float ndcX = XMVectorGetX(clip) / w;
    const float ndcY = XMVectorGetY(clip) / w;
    const float ndcZ = XMVectorGetZ(clip) / w;
    if (ndcZ < 0.0f || ndcZ > 1.0f) {
        return false;
    }
    out.x = viewport->Pos.x + (ndcX * 0.5f + 0.5f) * viewport->Size.x;
    out.y = viewport->Pos.y + (-ndcY * 0.5f + 0.5f) * viewport->Size.y;
    return true;
}
#endif

inline void SlashEffectRenderer::DrawEnemySlash(
    const Camera &camera, const DirectX::XMFLOAT3 &startWorld,
    const DirectX::XMFLOAT3 &endWorld, float phaseAlpha, float actionTime,
    bool isSweep) {
#ifndef IMGUI_DISABLED
    ImVec2 start{};
    ImVec2 end{};
    if (!ProjectSlashPoint(camera, startWorld, start) ||
        !ProjectSlashPoint(camera, endWorld, end)) {
        return;
    }
    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.001f) {
        return;
    }
    const float invLen = 1.0f / len;
    const float px = -dy * invLen;
    const float py = dx * invLen;
    const float curve = (isSweep ? 44.0f : 26.0f) *
                        (0.85f + 0.15f * std::sin(actionTime * 30.0f));
    const ImVec2 control((start.x + end.x) * 0.5f + px * curve,
                         (start.y + end.y) * 0.5f + py * curve);

    constexpr int kSegments = 14;
    ImVec2 points[kSegments + 1];
    for (int i = 0; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const float u = 1.0f - t;
        points[i].x = u * u * start.x + 2.0f * u * t * control.x + t * t * end.x;
        points[i].y = u * u * start.y + 2.0f * u * t * control.y + t * t * end.y;
    }

    const int alpha = static_cast<int>(std::clamp(phaseAlpha, 0.0f, 1.0f) * 255.0f);
    const float thick = (isSweep ? 18.0f : 13.0f) * (0.8f + phaseAlpha * 0.4f);
    drawList->AddPolyline(points, kSegments + 1, IM_COL32(70, 0, 12, alpha),
                          false, thick * 2.0f);
    drawList->AddPolyline(points, kSegments + 1, IM_COL32(230, 24, 54, alpha),
                          false, thick);
    drawList->AddPolyline(points, kSegments + 1, IM_COL32(255, 238, 245, alpha),
                          false, thick * 0.32f);
    drawList->AddCircleFilled(end, thick * 0.78f, IM_COL32(255, 180, 210, alpha));
#else
    (void)camera;
    (void)startWorld;
    (void)endWorld;
    (void)phaseAlpha;
    (void)actionTime;
    (void)isSweep;
#endif
}

