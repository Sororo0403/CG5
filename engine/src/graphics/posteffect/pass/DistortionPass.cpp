#include "posteffect/pass/DistortionPass.h"
#include "SpriteRenderer.h"
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
                            D3D12_GPU_DESCRIPTOR_HANDLE depthTexture,
                            SpriteRenderer *spriteRenderer) const {
    (void)sourceTexture;
    (void)depthTexture;
    if (!requested_ || (!params_.hasOrigin && !params_.hasTarget) ||
        spriteRenderer == nullptr) {
        return;
    }

    const DirectX::XMFLOAT2 targetScreen{params_.targetScreen.x,
                                         params_.targetScreen.y};
    const DirectX::XMFLOAT2 originScreen{params_.originScreen.x,
                                         params_.originScreen.y};
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

    auto makeColor = [](int r, int g, int b, float a) {
        return DirectX::XMFLOAT4{static_cast<float>(r) / 255.0f,
                                 static_cast<float>(g) / 255.0f,
                                 static_cast<float>(b) / 255.0f,
                                 std::clamp(a, 0.0f, 1.0f)};
    };
    const DirectX::XMFLOAT4 bright =
        makeColor(brightR, brightG, brightB, params_.alpha);
    const DirectX::XMFLOAT4 soft =
        makeColor(softR, softG, softB, params_.alpha * 0.82f);
    const DirectX::XMFLOAT4 slash =
        makeColor(accentR, accentG, accentB, params_.alpha * 0.78f);
    const bool isStart = params_.phase == DistortionPhase::Start;
    const bool isSustain = params_.phase == DistortionPhase::Sustain;

    auto drawDistortionAt = [&](const DirectX::XMFLOAT2 &center,
                                float radiusScale, float rotationBias) {
        constexpr int kSegments = 28;
        DirectX::XMFLOAT2 points[kSegments + 1];
        for (int i = 0; i <= kSegments; ++i) {
            const float ratio =
                static_cast<float>(i) / static_cast<float>(kSegments);
            const float angle =
                ratio * DirectX::XM_2PI + params_.time * 6.0f + rotationBias;
            const float wave =
                std::sinf(angle * 3.0f + params_.time * 17.0f) * params_.jitter;
            const float radius = params_.baseRadius * radiusScale + wave;
            points[i] = {center.x + std::cosf(angle) * radius,
                         center.y + std::sinf(angle) * radius};
        }

        spriteRenderer->DrawPolyline(points, kSegments + 1, soft, true,
                                     params_.thickness);
        spriteRenderer->DrawCircle(center, params_.baseRadius * radiusScale * 0.62f,
                                  bright, params_.thickness * 0.7f, 24);
        spriteRenderer->DrawCircle(center, params_.baseRadius * radiusScale * 0.82f,
                                  bright, params_.thickness * 0.42f, 28);

        for (int i = 0; i < 8; ++i) {
            const float ratio = static_cast<float>(i) / 8.0f;
            const float angle =
                ratio * DirectX::XM_2PI + params_.time * 8.5f + rotationBias;
            const float inner = params_.baseRadius * radiusScale * 0.42f;
            const float outer =
                inner + params_.lineLength *
                            (0.75f + 0.25f *
                                         std::sinf(params_.time * 18.0f + i));
            const DirectX::XMFLOAT2 a{center.x + std::cosf(angle) * inner,
                                      center.y + std::sinf(angle) * inner};
            const DirectX::XMFLOAT2 b{center.x + std::cosf(angle) * outer,
                                      center.y + std::sinf(angle) * outer};
            spriteRenderer->DrawLine(a, b, bright, 1.6f);
        }
    };

    if (isStart && params_.hasOrigin) {
        DirectX::XMFLOAT2 sourceFoot = originScreen;
        sourceFoot.y += params_.footOffset;
        drawDistortionAt(sourceFoot, 0.88f, 0.0f);
    }

    if (isSustain && params_.hasOrigin && params_.hasTarget) {
        const DirectX::XMFLOAT2 mid{
            (originScreen.x + targetScreen.x) * 0.5f,
            (originScreen.y + targetScreen.y) * 0.5f +
                params_.footOffset * 0.78f};
        drawDistortionAt(mid, 0.72f, 0.6f);
        spriteRenderer->DrawLine(originScreen, targetScreen, soft, 0.8f);
    }

    if (params_.hasTarget) {
        DirectX::XMFLOAT2 arrivalCenter = targetScreen;
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
        DirectX::XMFLOAT2 arrivalCenter = targetScreen;
        arrivalCenter.y -= params_.previewOffset * (isStart ? 0.55f : 0.18f);
        const DirectX::XMFLOAT2 slashA{arrivalCenter.x - perpX * slashLen,
                                       arrivalCenter.y - perpY * slashLen};
        const DirectX::XMFLOAT2 slashB{arrivalCenter.x + perpX * slashLen,
                                       arrivalCenter.y + perpY * slashLen};
        const DirectX::XMFLOAT2 slashC{
            arrivalCenter.x - perpX * branchLen + dirX * branchOffset,
            arrivalCenter.y - perpY * branchLen + dirY * branchOffset};
        const DirectX::XMFLOAT2 slashD{
            arrivalCenter.x + perpX * branchLen + dirX * branchOffset,
            arrivalCenter.y + perpY * branchLen + dirY * branchOffset};
        const DirectX::XMFLOAT2 slashE{
            arrivalCenter.x - perpX * (branchLen * 0.58f) -
                dirX * branchOffset * 0.72f,
            arrivalCenter.y - perpY * (branchLen * 0.58f) -
                dirY * branchOffset * 0.72f};
        const DirectX::XMFLOAT2 slashF{
            arrivalCenter.x + perpX * (branchLen * 0.58f) -
                dirX * branchOffset * 0.72f,
            arrivalCenter.y + perpY * (branchLen * 0.58f) -
                dirY * branchOffset * 0.72f};

        spriteRenderer->DrawLine(slashA, slashB, slash,
                                 params_.thickness * 0.82f);
        spriteRenderer->DrawLine(slashC, slashD, bright,
                                 params_.thickness * 0.55f);
        spriteRenderer->DrawLine(slashE, slashF, soft,
                                 params_.thickness * 0.42f);
    }
}
