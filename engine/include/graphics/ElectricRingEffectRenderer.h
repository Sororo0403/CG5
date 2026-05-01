#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

#ifndef IMGUI_DISABLED
#include "imgui.h"
#include <algorithm>
#endif

class DirectXCommon;
class SrvManager;

struct ElectricRingParamGPU {
    DirectX::XMFLOAT2 center = {0.5f, 0.5f};
    float radius = 0.0f;
    float time = 0.0f;
    float ringWidth = 0.0f;
    float distortionWidth = 0.0f;
    float distortionStrength = 0.0f;
    float swirlStrength = 0.0f;
    float cloudScale = 0.0f;
    float cloudIntensity = 0.0f;
    float brightness = 0.0f;
    float haloIntensity = 0.0f;
    DirectX::XMFLOAT2 aspectInvAspect = {1.0f, 1.0f};
    float innerFade = 0.0f;
    float outerFade = 0.0f;
    float enabled = 0.0f;
};

class ElectricRingEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *) { initialized_ = true; }
    void DrawDistortion(const ElectricRingParamGPU &param, D3D12_GPU_DESCRIPTOR_HANDLE,
                        D3D12_GPU_DESCRIPTOR_HANDLE source,
                        D3D12_GPU_DESCRIPTOR_HANDLE depth) {
        DrawPlasma(param, source, depth);
    }
    void DrawPlasma(const ElectricRingParamGPU &, D3D12_GPU_DESCRIPTOR_HANDLE,
                    D3D12_GPU_DESCRIPTOR_HANDLE);

  private:
    bool initialized_ = false;
};

inline void ElectricRingEffectRenderer::DrawPlasma(
    const ElectricRingParamGPU &param, D3D12_GPU_DESCRIPTOR_HANDLE,
    D3D12_GPU_DESCRIPTOR_HANDLE) {
#ifndef IMGUI_DISABLED
    if (param.enabled <= 0.5f) {
        return;
    }
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    if (viewport == nullptr || drawList == nullptr) {
        return;
    }
    const ImVec2 center(viewport->Pos.x + param.center.x * viewport->Size.x,
                        viewport->Pos.y + param.center.y * viewport->Size.y);
    const float radius =
        param.radius * (std::min)(viewport->Size.x, viewport->Size.y);
    const int alpha =
        static_cast<int>(std::clamp(param.brightness * 72.0f, 0.0f, 180.0f));
    drawList->AddCircle(center, radius, IM_COL32(80, 220, 255, alpha), 96,
                        (std::max)(2.0f, param.ringWidth * 420.0f));
#else
    (void)param;
#endif
}

