#include "posteffect/pass/DistortionPass.h"
#ifndef IMGUI_DISABLED
#include "imgui.h"
#endif
#include <algorithm>
#include <cmath>

void DistortionPass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                                int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void DistortionPass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void DistortionPass::Update(float deltaTime) { (void)deltaTime; }

void DistortionPass::Request(const DistortionEffectParams &params) {
    requested_ = true;
    params_ = params;
}

void DistortionPass::Render(D3D12_GPU_DESCRIPTOR_HANDLE sourceTexture,
                            D3D12_GPU_DESCRIPTOR_HANDLE depthTexture) const {
    (void)sourceTexture;
    (void)depthTexture;
#ifndef IMGUI_DISABLED
    if (!requested_ || (!params_.hasOrigin && !params_.hasTarget)) {
        return;
    }

    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    if (mainViewport == nullptr) {
        return;
    }

    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const ImVec2 targetScreen(params_.targetScreen.x, params_.targetScreen.y);
    const ImVec2 originScreen(params_.originScreen.x, params_.originScreen.y);
    int brightR = 255;
    int brightG = 48;
    int brightB = 108;
    int softR = 120;
    int softG = 22;
    int softB = 70;
    int accentR = 255;
    int accentG = 215;
    int accentB = 235;
    switch (params_.preset) {
    case DistortionPreset::Shockwave:
        brightR = 245;
        brightG = 245;
        brightB = 255;
        softR = 90;
        softG = 120;
        softB = 180;
        accentR = 220;
        accentG = 235;
        accentB = 255;
        break;
    case DistortionPreset::UltimateBurst:
        brightR = 255;
        brightG = 215;
        brightB = 96;
        softR = 190;
        softG = 58;
        softB = 36;
        accentR = 255;
        accentG = 245;
        accentB = 200;
        break;
    case DistortionPreset::HeatHaze:
        brightR = 255;
        brightG = 155;
        brightB = 80;
        softR = 150;
        softG = 70;
        softB = 35;
        accentR = 255;
        accentG = 210;
        accentB = 150;
        break;
    case DistortionPreset::WaterRipple:
        brightR = 95;
        brightG = 210;
        brightB = 255;
        softR = 35;
        softG = 92;
        softB = 160;
        accentR = 180;
        accentG = 235;
        accentB = 255;
        break;
    case DistortionPreset::WarpPortal:
    default:
        break;
    }

    const ImU32 bright = IM_COL32(
        brightR, brightG, brightB, static_cast<int>(255.0f * params_.alpha));
    const ImU32 soft = IM_COL32(
        softR, softG, softB, static_cast<int>(255.0f * (params_.alpha * 0.82f)));
    const ImU32 slash = IM_COL32(
        accentR, accentG, accentB,
        static_cast<int>(255.0f * (params_.alpha * 0.78f)));
    const bool isStart = params_.phase == DistortionPhase::Start;
    const bool isSustain = params_.phase == DistortionPhase::Sustain;

    auto drawDistortionAt = [&](const ImVec2 &center, float radiusScale,
                                float rotationBias) {
        constexpr int kSegments = 28;
        ImVec2 points[kSegments + 1];
        for (int i = 0; i <= kSegments; ++i) {
            const float ratio =
                static_cast<float>(i) / static_cast<float>(kSegments);
            const float angle =
                ratio * DirectX::XM_2PI + params_.time * 6.0f + rotationBias;
            const float wave =
                std::sinf(angle * 3.0f + params_.time * 17.0f) * params_.jitter;
            const float radius = params_.baseRadius * radiusScale + wave;
            points[i] = ImVec2(center.x + std::cosf(angle) * radius,
                               center.y + std::sinf(angle) * radius);
        }

        drawList->AddPolyline(points, kSegments + 1, soft, true,
                              params_.thickness);
        drawList->AddCircle(center, params_.baseRadius * radiusScale * 0.62f,
                            bright, 24, params_.thickness * 0.7f);
        drawList->AddCircle(center, params_.baseRadius * radiusScale * 0.82f,
                            bright, 28, params_.thickness * 0.42f);

        for (int i = 0; i < 8; ++i) {
            const float ratio = static_cast<float>(i) / 8.0f;
            const float angle =
                ratio * DirectX::XM_2PI + params_.time * 8.5f + rotationBias;
            const float inner = params_.baseRadius * radiusScale * 0.42f;
            const float outer =
                inner + params_.lineLength *
                            (0.75f + 0.25f *
                                         std::sinf(params_.time * 18.0f + i));
            const ImVec2 a(center.x + std::cosf(angle) * inner,
                           center.y + std::sinf(angle) * inner);
            const ImVec2 b(center.x + std::cosf(angle) * outer,
                           center.y + std::sinf(angle) * outer);
            drawList->AddLine(a, b, bright, 1.6f);
        }
    };

    if (isStart && params_.hasOrigin) {
        ImVec2 sourceFoot = originScreen;
        sourceFoot.y += params_.footOffset;
        drawDistortionAt(sourceFoot, 0.88f, 0.0f);
    }

    if (isSustain && params_.hasOrigin && params_.hasTarget) {
        const ImVec2 mid((originScreen.x + targetScreen.x) * 0.5f,
                         (originScreen.y + targetScreen.y) * 0.5f +
                             params_.footOffset * 0.78f);
        drawDistortionAt(mid, 0.72f, 0.6f);
        drawList->AddLine(originScreen, targetScreen, soft, 0.8f);
    }

    if (params_.hasTarget) {
        ImVec2 arrivalCenter = targetScreen;
        arrivalCenter.y += params_.footOffset -
                           params_.previewOffset * (isStart ? 0.55f : 0.18f);
        drawDistortionAt(arrivalCenter, isSustain ? 1.12f : 1.0f, 1.2f);
    }

    float dirX = 0.0f;
    float dirY = -1.0f;
    if (params_.hasOrigin && params_.hasTarget) {
        dirX = targetScreen.x - originScreen.x;
        dirY = targetScreen.y - originScreen.y;
        const float dirLen = std::sqrtf(dirX * dirX + dirY * dirY);
        if (dirLen > 0.0001f) {
            dirX /= dirLen;
            dirY /= dirLen;
        } else {
            dirX = 0.0f;
            dirY = -1.0f;
        }
    }

    const float perpX = -dirY;
    const float perpY = dirX;
    const float slashLen = params_.baseRadius * (isSustain ? 0.92f : 1.10f);
    const float branchLen = slashLen * 0.76f;
    const float branchOffset = params_.baseRadius * 0.28f;

    if (params_.hasTarget) {
        ImVec2 arrivalCenter = targetScreen;
        arrivalCenter.y -= params_.previewOffset * (isStart ? 0.55f : 0.18f);
        const ImVec2 slashA(arrivalCenter.x - perpX * slashLen,
                            arrivalCenter.y - perpY * slashLen);
        const ImVec2 slashB(arrivalCenter.x + perpX * slashLen,
                            arrivalCenter.y + perpY * slashLen);
        const ImVec2 slashC(arrivalCenter.x - perpX * branchLen +
                                dirX * branchOffset,
                            arrivalCenter.y - perpY * branchLen +
                                dirY * branchOffset);
        const ImVec2 slashD(arrivalCenter.x + perpX * branchLen +
                                dirX * branchOffset,
                            arrivalCenter.y + perpY * branchLen +
                                dirY * branchOffset);
        const ImVec2 slashE(arrivalCenter.x - perpX * (branchLen * 0.58f) -
                                dirX * branchOffset * 0.72f,
                            arrivalCenter.y - perpY * (branchLen * 0.58f) -
                                dirY * branchOffset * 0.72f);
        const ImVec2 slashF(arrivalCenter.x + perpX * (branchLen * 0.58f) -
                                dirX * branchOffset * 0.72f,
                            arrivalCenter.y + perpY * (branchLen * 0.58f) -
                                dirY * branchOffset * 0.72f);

        drawList->AddLine(slashA, slashB, slash, params_.thickness * 0.82f);
        drawList->AddLine(slashC, slashD, bright, params_.thickness * 0.55f);
        drawList->AddLine(slashE, slashF, soft, params_.thickness * 0.42f);
    }
#endif
}
