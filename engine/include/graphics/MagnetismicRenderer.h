#pragma once
#include <d3d12.h>
#include <dxgi.h>

#ifndef IMGUI_DISABLED
#include "imgui.h"
#include <algorithm>
#include <cmath>
#endif

struct MagnetVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MagnetVec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct MagnetMatrix4x4 {
    float m[4][4] = {};
};

struct MagnetismicInstance {
    MagnetVec3 position{};
    float time = 0.0f;
    float size = 1.0f;
    float intensity = 1.0f;
    float alpha = 1.0f;
    float swirlA = 0.0f;
    float swirlB = 0.0f;
    float noiseScale = 1.0f;
    float stepScale = 1.0f;
    MagnetVec4 colorA{};
    MagnetVec4 colorB{};
    float brightness = 1.0f;
    float distFade = 0.0f;
    float innerBoost = 0.0f;
};

class [[deprecated("Use EffectSystem with EffectPreset::MagneticField instead.")]]
MagnetismicRenderer {
  public:
    void Initialize(ID3D12Device *, DXGI_FORMAT, DXGI_FORMAT, const wchar_t *,
                    const wchar_t *) {
        initialized_ = true;
        elapsedTime_ = 0.0f;
    }
    void BeginFrame(float deltaTime) { elapsedTime_ += deltaTime; }
    void Draw(ID3D12GraphicsCommandList *, const MagnetismicInstance &instance,
              const MagnetMatrix4x4 &viewProj, const MagnetVec3 &,
              const MagnetVec3 &) {
#ifndef IMGUI_DISABLED
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        if (viewport == nullptr || drawList == nullptr) {
            return;
        }

        const float x = instance.position.x;
        const float y = instance.position.y;
        const float z = instance.position.z;
        const float clipX = x * viewProj.m[0][0] + y * viewProj.m[1][0] +
                            z * viewProj.m[2][0] + viewProj.m[3][0];
        const float clipY = x * viewProj.m[0][1] + y * viewProj.m[1][1] +
                            z * viewProj.m[2][1] + viewProj.m[3][1];
        const float clipZ = x * viewProj.m[0][2] + y * viewProj.m[1][2] +
                            z * viewProj.m[2][2] + viewProj.m[3][2];
        const float clipW = x * viewProj.m[0][3] + y * viewProj.m[1][3] +
                            z * viewProj.m[2][3] + viewProj.m[3][3];
        if (clipW <= 0.0001f || clipZ < 0.0f) {
            return;
        }

        const float ndcX = clipX / clipW;
        const float ndcY = clipY / clipW;
        const ImVec2 center(
            viewport->Pos.x + (ndcX * 0.5f + 0.5f) * viewport->Size.x,
            viewport->Pos.y + (-ndcY * 0.5f + 0.5f) * viewport->Size.y);
        const float pulse =
            0.5f + 0.5f * std::sin((instance.time + elapsedTime_) * 5.0f);
        const float radius = instance.size * 36.0f * (0.9f + pulse * 0.2f);
        const float alpha = std::clamp(instance.alpha * 0.55f, 0.0f, 1.0f);

        for (int i = 4; i >= 0; --i) {
            const float t = static_cast<float>(i + 1) / 5.0f;
            const ImU32 color = IM_COL32(
                static_cast<int>(std::clamp(instance.colorB.x * 120.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(instance.colorB.y * 120.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(instance.colorB.z * 120.0f, 0.0f, 255.0f)),
                static_cast<int>(alpha * (38.0f + 24.0f * pulse) * (1.0f - t * 0.08f)));
            drawList->AddCircle(center, radius * t, color, 72, 2.0f + i * 0.8f);
        }
        drawList->AddCircleFilled(
            center, radius * 0.18f,
            IM_COL32(255, 95, 190, static_cast<int>(alpha * 120.0f)), 36);
#else
        (void)instance;
        (void)viewProj;
#endif
    }

  private:
    bool initialized_ = false;
    float elapsedTime_ = 0.0f;
};

