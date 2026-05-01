#pragma once
#include <DirectXMath.h>

class DirectXCommon;

#ifndef IMGUI_DISABLED
#include "imgui.h"
#include <algorithm>
#include <cmath>
#endif

struct VignetteParams {
    DirectX::XMFLOAT3 color = {0.0f, 0.0f, 0.0f};
    float intensity = 0.0f;
    float innerRadius = 0.0f;
    float power = 1.0f;
    float roundness = 1.0f;
};

class VignetteRenderer {
  public:
    void Initialize(DirectXCommon *) { initialized_ = true; }
    void Draw(const VignetteParams &params) {
#ifndef IMGUI_DISABLED
        if (params.intensity <= 0.001f) {
            return;
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        if (viewport == nullptr) {
            return;
        }
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        if (drawList == nullptr) {
            return;
        }

        const ImVec2 min = viewport->Pos;
        const ImVec2 max = ImVec2(viewport->Pos.x + viewport->Size.x,
                                  viewport->Pos.y + viewport->Size.y);
        const ImVec2 center = ImVec2((min.x + max.x) * 0.5f,
                                     (min.y + max.y) * 0.5f);
        const float maxRadius =
            std::sqrt(viewport->Size.x * viewport->Size.x +
                      viewport->Size.y * viewport->Size.y) *
            0.5f;
        const int layers = 18;

        for (int i = layers; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(layers);
            const float ring = std::clamp((t - params.innerRadius) /
                                              (std::max)(0.001f,
                                                       1.0f - params.innerRadius),
                                          0.0f, 1.0f);
            const float alpha =
                std::pow(ring, params.power) * params.intensity * 0.16f;
            if (alpha <= 0.001f) {
                continue;
            }
            const ImU32 col = IM_COL32(
                static_cast<int>(std::clamp(params.color.x, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(params.color.y, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(params.color.z, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
            drawList->AddCircleFilled(center, maxRadius * t, col, 96);
        }
#else
        (void)params;
#endif
    }

  private:
    bool initialized_ = false;
};

