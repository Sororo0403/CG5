#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

#ifndef IMGUI_DISABLED
#include "imgui.h"
#include <algorithm>
#endif

class DirectXCommon;
class SrvManager;

struct WarpPostEffectParamGPU {
    DirectX::XMFLOAT2 center = {0.5f, 0.5f};
    float radius = 0.0f;
    float strength = 0.0f;
    DirectX::XMFLOAT2 center2 = {0.5f, 0.5f};
    float radius2 = 0.0f;
    float strength2 = 0.0f;
    float time = 0.0f;
    float enabled = 0.0f;
    DirectX::XMFLOAT2 slashStart = {0.5f, 0.5f};
    DirectX::XMFLOAT2 slashEnd = {0.5f, 0.5f};
    float slashThickness = 0.0f;
    float slashStrength = 0.0f;
    float slashEnabled = 0.0f;
};

class WarpPostEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *) { initialized_ = true; }
    void Draw(const WarpPostEffectParamGPU &param, D3D12_GPU_DESCRIPTOR_HANDLE) {
#ifndef IMGUI_DISABLED
        if (param.enabled <= 0.5f && param.slashEnabled <= 0.5f) {
            return;
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        if (viewport == nullptr || drawList == nullptr) {
            return;
        }
        auto toScreen = [&](const DirectX::XMFLOAT2 &uv) {
            return ImVec2(viewport->Pos.x + uv.x * viewport->Size.x,
                          viewport->Pos.y + uv.y * viewport->Size.y);
        };
        if (param.enabled > 0.5f) {
            const ImVec2 c0 = toScreen(param.center);
            const ImVec2 c1 = toScreen(param.center2);
            const float scale = (std::min)(viewport->Size.x, viewport->Size.y);
            const int alpha =
                static_cast<int>(std::clamp(param.strength * 255.0f, 0.0f, 140.0f));
            drawList->AddCircle(c0, param.radius * scale,
                                IM_COL32(255, 50, 100, alpha), 72, 3.0f);
            drawList->AddCircle(c1, param.radius2 * scale,
                                IM_COL32(80, 180, 255, alpha / 2), 72, 2.0f);
        }
        if (param.slashEnabled > 0.5f) {
            drawList->AddLine(toScreen(param.slashStart), toScreen(param.slashEnd),
                              IM_COL32(255, 230, 245, 180),
                              (std::max)(2.0f, param.slashThickness * 480.0f));
        }
#else
        (void)param;
#endif
    }

  private:
    bool initialized_ = false;
};

