#include "BillboardRenderer.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "ShaderCompiler.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;
using namespace DxUtils;

void BillboardRenderer::Initialize(DirectXCommon *dxCommon) {
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreatePipelineState();
    CreateVertexBuffer();
    CreateConstantBuffer();
}

void BillboardRenderer::PreDraw(const Camera &camera) {
    auto *cmd = dxCommon_->GetCommandList();

    XMMATRIX viewProjection = camera.GetView() * camera.GetProj();
    ConstantBufferData *mapped = nullptr;
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    XMStoreFloat4x4(&mapped->viewProjection, XMMatrixTranspose(viewProjection));
    constantBuffer_->Unmap(0, nullptr);

    XMMATRIX billboard = camera.GetView();
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    billboard = XMMatrixInverse(nullptr, billboard);
    XMStoreFloat3(&cameraRight_, billboard.r[0]);
    XMStoreFloat3(&cameraUp_, billboard.r[1]);
    XMStoreFloat3(&cameraForward_, billboard.r[2]);

    cmd->SetPipelineState(pipelineState_.Get());
    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    cmd->SetGraphicsRootConstantBufferView(
        0, constantBuffer_->GetGPUVirtualAddress());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vertexBufferView_);
}

void BillboardRenderer::PostDraw() {}

void BillboardRenderer::DrawDisc(const XMFLOAT3 &center, float radius,
                                 const XMFLOAT4 &color, float roll) {
    if (radius <= 0.0f || color.w <= 0.0f) {
        return;
    }

    const float s = std::sinf(roll);
    const float c = std::cosf(roll);
    auto corner = [&](float x, float y) {
        const float rx = x * c - y * s;
        const float ry = x * s + y * c;
        return XMFLOAT3{center.x + (cameraRight_.x * rx + cameraUp_.x * ry) *
                                       radius,
                        center.y + (cameraRight_.y * rx + cameraUp_.y * ry) *
                                       radius,
                        center.z + (cameraRight_.z * rx + cameraUp_.z * ry) *
                                       radius};
    };

    Vertex vertices[6] = {
        {corner(-1.0f, 1.0f), {0.0f, 0.0f}, color, 0.0f},
        {corner(1.0f, 1.0f), {1.0f, 0.0f}, color, 0.0f},
        {corner(-1.0f, -1.0f), {0.0f, 1.0f}, color, 0.0f},
        {corner(-1.0f, -1.0f), {0.0f, 1.0f}, color, 0.0f},
        {corner(1.0f, 1.0f), {1.0f, 0.0f}, color, 0.0f},
        {corner(1.0f, -1.0f), {1.0f, 1.0f}, color, 0.0f},
    };
    DrawQuad(vertices);
}

void BillboardRenderer::DrawBeam(const XMFLOAT3 &start, const XMFLOAT3 &end,
                                 float width, const XMFLOAT4 &color) {
    if (width <= 0.0f || color.w <= 0.0f) {
        return;
    }

    XMVECTOR a = XMLoadFloat3(&start);
    XMVECTOR b = XMLoadFloat3(&end);
    XMVECTOR tangent = XMVector3Normalize(b - a);
    XMVECTOR forward = XMLoadFloat3(&cameraForward_);
    XMVECTOR normal = XMVector3Cross(forward, tangent);
    if (XMVectorGetX(XMVector3LengthSq(normal)) <= 0.000001f) {
        normal = XMLoadFloat3(&cameraRight_);
    } else {
        normal = XMVector3Normalize(normal);
    }
    normal *= width * 0.5f;

    XMFLOAT3 n{};
    XMStoreFloat3(&n, normal);
    Vertex vertices[6] = {
        {{start.x + n.x, start.y + n.y, start.z + n.z},
         {0.0f, 0.0f},
         color,
         1.0f},
        {{end.x + n.x, end.y + n.y, end.z + n.z},
         {1.0f, 0.0f},
         color,
         1.0f},
        {{start.x - n.x, start.y - n.y, start.z - n.z},
         {0.0f, 1.0f},
         color,
         1.0f},
        {{start.x - n.x, start.y - n.y, start.z - n.z},
         {0.0f, 1.0f},
         color,
         1.0f},
        {{end.x + n.x, end.y + n.y, end.z + n.z},
         {1.0f, 0.0f},
         color,
         1.0f},
        {{end.x - n.x, end.y - n.y, end.z - n.z},
         {1.0f, 1.0f},
         color,
         1.0f},
    };
    DrawQuad(vertices);
}

void BillboardRenderer::CreateRootSignature() {
    CD3DX12_ROOT_PARAMETER params[1]{};
    params[0].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC desc{};
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "Serialize Billboard root signature failed");
    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&rootSignature_)),
                  "Create Billboard root signature failed");
}

void BillboardRenderer::CreatePipelineState() {
    auto vs = ShaderCompiler::Compile(
        L"engine/resources/shaders/billboard/BillboardVS.hlsl", "main",
        "vs_5_0");
    auto ps = ShaderCompiler::Compile(
        L"engine/resources/shaders/billboard/BillboardPS.hlsl", "main",
        "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32_FLOAT, 0, 36,
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
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

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
                  "Create Billboard pipeline state failed");
}

void BillboardRenderer::CreateVertexBuffer() {
    const UINT size = sizeof(Vertex) * 6;
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heap, D3D12_HEAP_FLAG_NONE, &desc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&vertexBuffer_)),
                  "Create Billboard vertex buffer failed");

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = size;
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
}

void BillboardRenderer::CreateConstantBuffer() {
    const UINT size = Align256(sizeof(ConstantBufferData));
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heap, D3D12_HEAP_FLAG_NONE, &desc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&constantBuffer_)),
                  "Create Billboard constant buffer failed");
}

void BillboardRenderer::DrawQuad(const Vertex (&vertices)[6]) {
    Vertex *mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);
    dxCommon_->GetCommandList()->DrawInstanced(6, 1, 0, 0);
}
