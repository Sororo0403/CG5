#include "SpriteRenderer.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "ShaderCompiler.h"
#include "Sprite.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace DxUtils;

struct SpriteVertex {
    XMFLOAT3 pos;
    XMFLOAT2 uv;
    XMFLOAT4 color;
};

struct SpriteConstBuffer {
    XMFLOAT4X4 mat;
};

void SpriteRenderer::Initialize(DirectXCommon *dxCommon,
                                TextureManager *textureManager,
                                SrvManager *srvManager, int width, int height) {
    dxCommon_ = dxCommon;
    textureManager_ = textureManager;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreatePipelineState();
    CreateVertexBuffer();
    CreateConstantBuffer();
    UpdateProjection(width, height);
}

void SpriteRenderer::Draw(const Sprite &sprite) {
    auto cmd = dxCommon_->GetCommandList();

    // 頂点生成
    float l = sprite.position.x;
    float t = sprite.position.y;
    float r = sprite.position.x + sprite.size.x;
    float b = sprite.position.y + sprite.size.y;

    SpriteVertex vertices[6] = {
        {{l, t, 0.0f}, {0.0f, 0.0f}, sprite.color},
        {{r, t, 0.0f}, {1.0f, 0.0f}, sprite.color},
        {{l, b, 0.0f}, {0.0f, 1.0f}, sprite.color},

        {{l, b, 0.0f}, {0.0f, 1.0f}, sprite.color},
        {{r, t, 0.0f}, {1.0f, 0.0f}, sprite.color},
        {{r, b, 0.0f}, {1.0f, 1.0f}, sprite.color},
    };

    // VB 更新
    SpriteVertex *mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    // テクスチャ
    cmd->SetGraphicsRootDescriptorTable(
        1, textureManager_->GetGpuHandle(sprite.textureId));

    cmd->DrawInstanced(6, 1, 0, 0);
}

void SpriteRenderer::DrawFilledCircle(const XMFLOAT2 &center, float radius,
                                      const XMFLOAT4 &color,
                                      uint32_t segments) {
    if (radius <= 0.0f) {
        return;
    }
    segments = (std::max)(segments, 3u);
    constexpr float kTwoPi = 6.28318530717958647692f;
    for (uint32_t i = 0; i < segments; ++i) {
        const float a0 = kTwoPi * static_cast<float>(i) /
                         static_cast<float>(segments);
        const float a1 = kTwoPi * static_cast<float>(i + 1) /
                         static_cast<float>(segments);
        DrawPrimitiveTriangle(
            center, {center.x + std::cosf(a0) * radius,
                     center.y + std::sinf(a0) * radius},
            {center.x + std::cosf(a1) * radius,
             center.y + std::sinf(a1) * radius},
            color);
    }
}

void SpriteRenderer::DrawCircle(const XMFLOAT2 &center, float radius,
                                const XMFLOAT4 &color, float thickness,
                                uint32_t segments) {
    if (radius <= 0.0f || thickness <= 0.0f) {
        return;
    }
    segments = (std::max)(segments, 3u);
    constexpr float kTwoPi = 6.28318530717958647692f;
    XMFLOAT2 prev{center.x + radius, center.y};
    for (uint32_t i = 1; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) /
                            static_cast<float>(segments);
        XMFLOAT2 next{center.x + std::cosf(angle) * radius,
                      center.y + std::sinf(angle) * radius};
        DrawLine(prev, next, color, thickness);
        prev = next;
    }
}

void SpriteRenderer::DrawLine(const XMFLOAT2 &start, const XMFLOAT2 &end,
                              const XMFLOAT4 &color, float thickness) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrtf(dx * dx + dy * dy);
    if (length <= 0.0001f || thickness <= 0.0f) {
        return;
    }

    const float half = thickness * 0.5f;
    const float nx = -dy / length * half;
    const float ny = dx / length * half;

    DrawPrimitiveQuad({start.x + nx, start.y + ny}, {end.x + nx, end.y + ny},
                      {end.x - nx, end.y - ny}, {start.x - nx, start.y - ny},
                      color);
}

void SpriteRenderer::DrawPolyline(const XMFLOAT2 *points, size_t pointCount,
                                  const XMFLOAT4 &color, bool closed,
                                  float thickness) {
    if (points == nullptr || pointCount < 2) {
        return;
    }

    for (size_t i = 1; i < pointCount; ++i) {
        DrawLine(points[i - 1], points[i], color, thickness);
    }
    if (closed) {
        DrawLine(points[pointCount - 1], points[0], color, thickness);
    }
}

void SpriteRenderer::DrawConvexPolygon(const XMFLOAT2 *points,
                                       size_t pointCount,
                                       const XMFLOAT4 &color) {
    if (points == nullptr || pointCount < 3) {
        return;
    }

    for (size_t i = 2; i < pointCount; ++i) {
        DrawPrimitiveTriangle(points[0], points[i - 1], points[i], color);
    }
}

void SpriteRenderer::PreDraw() {
    auto cmd = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->SetPipelineState(pipelineState_.Get());
    cmd->SetGraphicsRootSignature(rootSignature_.Get());

    cmd->SetGraphicsRootConstantBufferView(
        0, constBuffer_->GetGPUVirtualAddress());

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbView_);
}

void SpriteRenderer::PostDraw() {}

void SpriteRenderer::DrawPrimitiveTriangle(const XMFLOAT2 &a, const XMFLOAT2 &b,
                                           const XMFLOAT2 &c,
                                           const XMFLOAT4 &color) {
    auto cmd = dxCommon_->GetCommandList();

    SpriteVertex vertices[6] = {
        {{a.x, a.y, 0.0f}, {0.0f, 0.0f}, color},
        {{b.x, b.y, 0.0f}, {0.0f, 0.0f}, color},
        {{c.x, c.y, 0.0f}, {0.0f, 0.0f}, color},
        {{c.x, c.y, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
        {{c.x, c.y, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
        {{c.x, c.y, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
    };

    SpriteVertex *mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    cmd->SetGraphicsRootDescriptorTable(1, textureManager_->GetGpuHandle(0));
    cmd->DrawInstanced(3, 1, 0, 0);
}

void SpriteRenderer::DrawPrimitiveQuad(const XMFLOAT2 &a, const XMFLOAT2 &b,
                                       const XMFLOAT2 &c, const XMFLOAT2 &d,
                                       const XMFLOAT4 &color) {
    auto cmd = dxCommon_->GetCommandList();

    SpriteVertex vertices[6] = {
        {{a.x, a.y, 0.0f}, {0.0f, 0.0f}, color},
        {{b.x, b.y, 0.0f}, {0.0f, 0.0f}, color},
        {{d.x, d.y, 0.0f}, {0.0f, 0.0f}, color},
        {{d.x, d.y, 0.0f}, {0.0f, 0.0f}, color},
        {{b.x, b.y, 0.0f}, {0.0f, 0.0f}, color},
        {{c.x, c.y, 0.0f}, {0.0f, 0.0f}, color},
    };

    SpriteVertex *mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    cmd->SetGraphicsRootDescriptorTable(1, textureManager_->GetGpuHandle(0));
    cmd->DrawInstanced(6, 1, 0, 0);
}

void SpriteRenderer::CreateVertexBuffer() {
    UINT size = sizeof(SpriteVertex) * 6;

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heap, D3D12_HEAP_FLAG_NONE, &desc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&vertexBuffer_)),
                  "Create sprite VB failed");

    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = size;
    vbView_.StrideInBytes = sizeof(SpriteVertex);
}

void SpriteRenderer::CreateConstantBuffer() {
    UINT size = Align256(sizeof(SpriteConstBuffer));

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heap, D3D12_HEAP_FLAG_NONE, &desc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&constBuffer_)),
                  "Create sprite CB failed");

    SpriteConstBuffer *mapped = nullptr;
    constBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    mapped->mat = matProjection_;
    constBuffer_->Unmap(0, nullptr);
}

void SpriteRenderer::UpdateProjection(int width, int height) {
    XMMATRIX ortho = XMMatrixOrthographicOffCenterLH(
        0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f,
        1.0f);

    XMStoreFloat4x4(&matProjection_, XMMatrixTranspose(ortho));

    SpriteConstBuffer *mapped = nullptr;
    constBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    mapped->mat = matProjection_;
    constBuffer_->Unmap(0, nullptr);
}

void SpriteRenderer::CreateRootSignature() {
    CD3DX12_ROOT_PARAMETER params[2]{};
    params[0].InitAsConstantBufferView(0);

    CD3DX12_DESCRIPTOR_RANGE range{};
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &range);

    CD3DX12_STATIC_SAMPLER_DESC sampler{};
    sampler.Init(0);

    CD3DX12_ROOT_SIGNATURE_DESC desc{};
    desc.Init(_countof(params), params, 1, &sampler,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "SerializeRootSignature failed");

    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&rootSignature_)),
                  "CreateRootSignature failed");
}

void SpriteRenderer::CreatePipelineState() {
    auto vs = ShaderCompiler::Compile(L"engine/resources/shaders/sprite/SpriteVS.hlsl",
                                      "main", "vs_5_0");
    auto ps = ShaderCompiler::Compile(L"engine/resources/shaders/sprite/SpritePS.hlsl",
                                      "main", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    desc.InputLayout = {layout, _countof(layout)};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    D3D12_DEPTH_STENCIL_DESC depth =
        CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState = depth;

    D3D12_BLEND_DESC blend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto &rt = blend.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState = blend;

    ThrowIfFailed(dxCommon_->GetDevice()->CreateGraphicsPipelineState(
                      &desc, IID_PPV_ARGS(&pipelineState_)),
                  "CreateGraphicsPipelineState failed");
}
