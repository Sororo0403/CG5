#include "model/MeshRenderer.h"

#include "graphics/DirectXCommon.h"
#include "graphics/DxHelpers.h"
#include "graphics/DxUtils.h"
#include "graphics/ShaderCompiler.h"
#include "graphics/ShaderPaths.h"
#include "graphics/SrvManager.h"
#include "model/Vertex.h"
#include "texture/TextureManager.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;
using namespace DxUtils;
using Microsoft::WRL::ComPtr;

namespace {

struct PerObjectConstBufferData {
    XMFLOAT4X4 matWVP;
    XMFLOAT4X4 matWorld;
    XMFLOAT4X4 matWorldInverseTranspose;
};

struct SceneConstBufferData {
    struct PointLightData {
        XMFLOAT4 positionRange;
        XMFLOAT4 colorIntensity;
    };

    XMFLOAT4 cameraPos;
    XMFLOAT4 keyLightDirection;
    XMFLOAT4 keyLightColor;
    XMFLOAT4 fillLightDirection;
    XMFLOAT4 fillLightColor;
    XMFLOAT4 ambientColor;
    PointLightData pointLights[2];
    XMFLOAT4 lightingParams;
    XMFLOAT4 fogColor;
    XMFLOAT4 fogParams;
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 lightViewProjection;
    XMFLOAT4 shadowParams;
    XMFLOAT4 shadowFilterParams;
    XMFLOAT4 customSceneParams0;
    XMFLOAT4 customSceneParams1;
};

XMMATRIX MakeWorldMatrix(const Transform &transform) {
    XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
    return XMMatrixScaling(transform.scale.x, transform.scale.y,
                           transform.scale.z) *
           XMMatrixRotationQuaternion(q) *
           XMMatrixTranslation(transform.position.x, transform.position.y,
                               transform.position.z);
}

XMMATRIX MakeWorldInverseTranspose(const XMMATRIX &world) {
    XMVECTOR determinant{};
    XMMATRIX inverse = XMMatrixInverse(&determinant, world);
    const float determinantValue = XMVectorGetX(determinant);
    if (!std::isfinite(determinantValue) ||
        std::abs(determinantValue) <= 0.000001f) {
        return XMMatrixIdentity();
    }
    return XMMatrixTranspose(inverse);
}

bool IsTransparentMaterial(const Material &material) {
    return material.blendMode == static_cast<int32_t>(BlendMode::Transparent) ||
           material.color.w < 1.0f;
}

D3D12_CULL_MODE ToD3D12CullMode(const MaterialCullMode mode) {
    switch (mode) {
    case MaterialCullMode::None:
        return D3D12_CULL_MODE_NONE;
    case MaterialCullMode::Front:
        return D3D12_CULL_MODE_FRONT;
    case MaterialCullMode::Back:
    default:
        return D3D12_CULL_MODE_BACK;
    }
}

size_t PipelineVariantIndex(bool transparent, MaterialCullMode cullMode,
                            bool depthWrite) {
    const size_t blendIndex = transparent ? 1 : 0;
    const size_t cullIndex = static_cast<size_t>(cullMode);
    const size_t depthIndex = depthWrite ? 1 : 0;
    return blendIndex * 6 + cullIndex * 2 + depthIndex;
}

size_t PipelineVariantIndex(const Material &material) {
    const Material drawMaterial = NormalizeMaterialForDraw(material);
    MaterialCullMode cullMode =
        static_cast<MaterialCullMode>(drawMaterial.cullMode);
    if (drawMaterial.cullMode < static_cast<int32_t>(MaterialCullMode::None) ||
        drawMaterial.cullMode > static_cast<int32_t>(MaterialCullMode::Back)) {
        cullMode = MaterialCullMode::Back;
    }
    return PipelineVariantIndex(IsTransparentMaterial(drawMaterial), cullMode,
                                drawMaterial.depthWrite != 0);
}

uint32_t ResolveNormalTextureId(TextureManager *textureManager,
                                uint32_t normalTextureId) {
    return normalTextureId == UINT32_MAX
               ? textureManager->GetDefaultNormalTextureId()
               : normalTextureId;
}

uint32_t ResolveBaseColorTextureId(const Material &material,
                                   uint32_t fallbackTextureId) {
    return material.baseColorTextureId == UINT32_MAX
               ? fallbackTextureId
               : material.baseColorTextureId;
}

uint32_t ResolveNormalTextureId(TextureManager *textureManager,
                                const Material &material,
                                uint32_t fallbackTextureId) {
    const uint32_t textureId = material.normalTextureId == UINT32_MAX
                                   ? fallbackTextureId
                                   : material.normalTextureId;
    return ResolveNormalTextureId(textureManager, textureId);
}

} // namespace

void MeshRenderer::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                              TextureManager *textureManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    textureManager_ = textureManager;

    CreateRootSignature();
    CreateShadowRootSignature();
    CreatePipelineStates();
    CreateShadowPipelineStates();
    CreateUploadBuffer();
}

void MeshRenderer::BeginFrame() {
    uploadBuffer_.BeginFrame();
}

void MeshRenderer::PreDraw() {
    auto *cmd = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    drawIndex_ = 0;
}

void MeshRenderer::PostDraw() {}

void MeshRenderer::PreDrawShadow() {
    auto *cmd = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
    drawIndex_ = 0;
}

void MeshRenderer::DrawMesh(const Mesh &mesh, const Material &material,
                            const Transform &transform, const Camera &camera,
                            uint32_t textureId, uint32_t normalTextureId) {
    if (drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const XMMATRIX world = MakeWorldMatrix(transform);
    const XMMATRIX worldInverseTranspose = MakeWorldInverseTranspose(world);
    const XMMATRIX wvp = world * camera.GetView() * camera.GetProj();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(wvp, world, worldInverseTranspose);
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);

    SetPipelineForMaterial(drawMaterial);
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));
    cmd->SetGraphicsRootDescriptorTable(4, shadowMapGpuHandle_);
    cmd->SetGraphicsRootDescriptorTable(
        5, textureManager_->GetGpuHandle(
               ResolveNormalTextureId(textureManager_, drawMaterial,
                                      normalTextureId)));
    cmd->IASetVertexBuffers(0, 1, &mesh.vbView);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

    ++drawIndex_;
}

void MeshRenderer::DrawMeshWithPipeline(
    uint32_t pipelineId, const Mesh &mesh, const Material &material,
    const Transform &transform, const Camera &camera, uint32_t textureId,
    uint32_t normalTextureId) {
    if (pipelineId >= customPipelines_.size() || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const XMMATRIX world = MakeWorldMatrix(transform);
    const XMMATRIX worldInverseTranspose = MakeWorldInverseTranspose(world);
    const XMMATRIX wvp = world * camera.GetView() * camera.GetProj();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(wvp, world, worldInverseTranspose);
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);

    SetPipelineForMaterial(customPipelines_[pipelineId].pipelineStates,
                           drawMaterial);
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));
    cmd->SetGraphicsRootDescriptorTable(4, shadowMapGpuHandle_);
    cmd->SetGraphicsRootDescriptorTable(
        5, textureManager_->GetGpuHandle(
               ResolveNormalTextureId(textureManager_, drawMaterial,
                                      normalTextureId)));
    cmd->IASetVertexBuffers(0, 1, &mesh.vbView);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

    ++drawIndex_;
}

void MeshRenderer::DrawMeshInstanced(const Mesh &mesh, const Material &material,
                                     const InstanceData *instances,
                                     uint32_t instanceCount,
                                     const Camera &camera,
                                     uint32_t textureId,
                                     uint32_t normalTextureId) {
    if (!instances || instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(instances, instanceCount);

    SetInstancedPipelineForMaterial(drawMaterial);
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));
    cmd->SetGraphicsRootDescriptorTable(4, shadowMapGpuHandle_);
    cmd->SetGraphicsRootDescriptorTable(
        5, textureManager_->GetGpuHandle(
               ResolveNormalTextureId(textureManager_, drawMaterial,
                                      normalTextureId)));
    D3D12_VERTEX_BUFFER_VIEW views[] = {mesh.vbView, instanceView};
    cmd->IASetVertexBuffers(0, 2, views);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);

    ++drawIndex_;
}

void MeshRenderer::DrawMeshInstancedWithPipeline(
    uint32_t pipelineId, const Mesh &mesh, const Material &material,
    const InstanceData *instances, uint32_t instanceCount, const Camera &camera,
    uint32_t textureId, uint32_t normalTextureId) {
    if (pipelineId >= customInstancedPipelines_.size() || !instances ||
        instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(instances, instanceCount);

    SetInstancedPipelineForMaterial(
        customInstancedPipelines_[pipelineId].pipelineStates, drawMaterial);
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));
    cmd->SetGraphicsRootDescriptorTable(4, shadowMapGpuHandle_);
    cmd->SetGraphicsRootDescriptorTable(
        5, textureManager_->GetGpuHandle(
               ResolveNormalTextureId(textureManager_, drawMaterial,
                                      normalTextureId)));
    D3D12_VERTEX_BUFFER_VIEW views[] = {mesh.vbView, instanceView};
    cmd->IASetVertexBuffers(0, 2, views);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);

    ++drawIndex_;
}

void MeshRenderer::DrawMeshShadow(
    const Mesh &mesh, const Transform &transform,
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    DrawMeshShadow(mesh, Material{}, transform, lightViewProjection, 0);
}

void MeshRenderer::DrawMeshShadow(
    const Mesh &mesh, const Material &material, const Transform &transform,
    const DirectX::XMFLOAT4X4 &lightViewProjection, uint32_t textureId) {
    if (drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const XMMATRIX world = MakeWorldMatrix(transform);
    const XMMATRIX lightVP = XMLoadFloat4x4(&lightViewProjection);
    const XMMATRIX wvp = world * lightVP;
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(wvp, world, XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr =
        WriteShadowSceneConstants(lightViewProjection);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);
    cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
    cmd->SetPipelineState(shadowPSO_.Get());
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));
    cmd->IASetVertexBuffers(0, 1, &mesh.vbView);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    ++drawIndex_;
}

void MeshRenderer::DrawMeshInstancedShadow(
    const Mesh &mesh, const InstanceData *instances, uint32_t instanceCount,
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    DrawMeshInstancedShadow(mesh, Material{}, instances, instanceCount,
                            lightViewProjection, 0);
}

void MeshRenderer::DrawMeshInstancedShadow(
    const Mesh &mesh, const Material &material, const InstanceData *instances,
    uint32_t instanceCount, const DirectX::XMFLOAT4X4 &lightViewProjection,
    uint32_t textureId) {
    if (!instances || instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr =
        WriteShadowSceneConstants(lightViewProjection);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(instances, instanceCount);

    cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
    cmd->SetPipelineState(instancedShadowPSO_.Get());
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));

    D3D12_VERTEX_BUFFER_VIEW views[] = {mesh.vbView, instanceView};
    cmd->IASetVertexBuffers(0, 2, views);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);
    ++drawIndex_;
}

void MeshRenderer::DrawMeshInstancedShadowWithPipeline(
    uint32_t pipelineId, const Mesh &mesh, const Material &material,
    const InstanceData *instances, uint32_t instanceCount,
    const DirectX::XMFLOAT4X4 &lightViewProjection, uint32_t textureId) {
    if (pipelineId >= customInstancedPipelines_.size() || !instances ||
        instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto *cmd = dxCommon_->GetCommandList();
    const Material drawMaterial = NormalizeMaterialForDraw(material);

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr =
        WriteShadowSceneConstants(lightViewProjection);
    const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
        WriteMaterialConstants(drawMaterial);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(instances, instanceCount);

    cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
    cmd->SetPipelineState(
        customInstancedPipelines_[pipelineId].shadowPipelineState.Get());
    cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
    cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
    cmd->SetGraphicsRootConstantBufferView(2, materialCbAddr);
    cmd->SetGraphicsRootDescriptorTable(
        3, textureManager_->GetGpuHandle(
               ResolveBaseColorTextureId(drawMaterial, textureId)));

    D3D12_VERTEX_BUFFER_VIEW views[] = {mesh.vbView, instanceView};
    cmd->IASetVertexBuffers(0, 2, views);
    cmd->IASetIndexBuffer(&mesh.ibView);
    cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
    cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);
    ++drawIndex_;
}

void MeshRenderer::CreateRootSignature() {
    CD3DX12_ROOT_PARAMETER params[6];
    params[0].InitAsConstantBufferView(0);
    params[1].InitAsConstantBufferView(1);
    params[2].InitAsConstantBufferView(2);

    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[3].InitAsDescriptorTable(1, &textureRange);

    CD3DX12_DESCRIPTOR_RANGE shadowRange;
    shadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    params[4].InitAsDescriptorTable(1, &shadowRange);

    CD3DX12_DESCRIPTOR_RANGE normalRange;
    normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    params[5].InitAsDescriptorTable(1, &normalRange);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 1, &sampler,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(MeshRenderer) failed");
    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&rootSignature_)),
                  "CreateRootSignature(MeshRenderer) failed");
}

void MeshRenderer::CreateShadowRootSignature() {
    CD3DX12_ROOT_PARAMETER params[4];
    params[0].InitAsConstantBufferView(0);
    params[1].InitAsConstantBufferView(1);
    params[2].InitAsConstantBufferView(2);

    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[3].InitAsDescriptorTable(1, &textureRange);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 1, &sampler,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(MeshShadow) failed");
    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&shadowRootSignature_)),
                  "CreateRootSignature(MeshShadow) failed");
}

void MeshRenderer::CreatePipelineStates() {
    auto *device = dxCommon_->GetDevice();
    auto vs = ShaderCompiler::Compile(ShaderPaths::MeshVS, "main", "vs_5_0");
    auto instancedVs =
        ShaderCompiler::Compile(ShaderPaths::MeshInstancedVS, "main",
                                "vs_5_0");
    auto ps = ShaderCompiler::Compile(ShaderPaths::MeshPS, "main", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC baseLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"CUSTOM", 0, DXGI_FORMAT_R32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_INPUT_ELEMENT_DESC instancedLayout[] = {
        baseLayout[0],
        baseLayout[1],
        baseLayout[2],
        baseLayout[3],
        baseLayout[4],
        baseLayout[5],
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCEFADE", 0, DXGI_FORMAT_R32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCESEED", 0, DXGI_FORMAT_R32_UINT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    auto makePso = [&](D3D12_SHADER_BYTECODE vertexShader,
                       D3D12_INPUT_LAYOUT_DESC inputLayout, bool transparent,
                       MaterialCullMode cullMode, bool depthWrite,
                       ComPtr<ID3D12PipelineState> &psoOut) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = rootSignature_.Get();
        pso.VS = vertexShader;
        pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.InputLayout = inputLayout;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DirectXCommon::kSceneColorFormat;
        pso.DSVFormat = DirectXCommon::kDepthStencilFormat;
        pso.SampleDesc.Count = 1;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.RasterizerState.CullMode = ToD3D12CullMode(cullMode);

        D3D12_BLEND_DESC blend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        blend.RenderTarget[0].BlendEnable = transparent ? TRUE : FALSE;
        blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.BlendState = blend;

        D3D12_DEPTH_STENCIL_DESC depth =
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                          : D3D12_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState = depth;

        ThrowIfFailed(device->CreateGraphicsPipelineState(
                          &pso, IID_PPV_ARGS(&psoOut)),
                      "CreateGraphicsPipelineState(MeshRenderer) failed");
    };

    for (bool transparent : {false, true}) {
        for (MaterialCullMode cullMode :
             {MaterialCullMode::None, MaterialCullMode::Front,
              MaterialCullMode::Back}) {
            for (bool depthWrite : {false, true}) {
                const size_t index =
                    PipelineVariantIndex(transparent, cullMode, depthWrite);
                makePso({vs->GetBufferPointer(), vs->GetBufferSize()},
                        {baseLayout, _countof(baseLayout)}, transparent,
                        cullMode, depthWrite, pipelineStates_[index]);
                makePso({instancedVs->GetBufferPointer(),
                         instancedVs->GetBufferSize()},
                        {instancedLayout, _countof(instancedLayout)},
                        transparent, cullMode, depthWrite,
                        instancedPipelineStates_[index]);
            }
        }
    }
}

uint32_t MeshRenderer::CreatePipeline(const std::wstring &vertexShaderPath,
                                      const std::wstring &pixelShaderPath) {
    auto *device = dxCommon_->GetDevice();
    PipelineSet pipelineSet{};
    auto vs = ShaderCompiler::Compile(vertexShaderPath, "main", "vs_5_0");
    auto ps = ShaderCompiler::Compile(pixelShaderPath, "main", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC baseLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"CUSTOM", 0, DXGI_FORMAT_R32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    auto makePso = [&](bool transparent, MaterialCullMode cullMode,
                       bool depthWrite,
                       ComPtr<ID3D12PipelineState> &psoOut) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = rootSignature_.Get();
        pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.InputLayout = {baseLayout, _countof(baseLayout)};
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DirectXCommon::kSceneColorFormat;
        pso.DSVFormat = DirectXCommon::kDepthStencilFormat;
        pso.SampleDesc.Count = 1;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.RasterizerState.CullMode = ToD3D12CullMode(cullMode);

        D3D12_BLEND_DESC blend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        blend.RenderTarget[0].BlendEnable = transparent ? TRUE : FALSE;
        blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.BlendState = blend;

        D3D12_DEPTH_STENCIL_DESC depth =
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                          : D3D12_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState = depth;

        ThrowIfFailed(device->CreateGraphicsPipelineState(
                          &pso, IID_PPV_ARGS(&psoOut)),
                      "CreateGraphicsPipelineState(CustomMesh) failed");
    };

    for (bool transparent : {false, true}) {
        for (MaterialCullMode cullMode :
             {MaterialCullMode::None, MaterialCullMode::Front,
              MaterialCullMode::Back}) {
            for (bool depthWrite : {false, true}) {
                const size_t index =
                    PipelineVariantIndex(transparent, cullMode, depthWrite);
                makePso(transparent, cullMode, depthWrite,
                        pipelineSet.pipelineStates[index]);
            }
        }
    }

    const uint32_t pipelineId = static_cast<uint32_t>(customPipelines_.size());
    customPipelines_.push_back(std::move(pipelineSet));
    return pipelineId;
}

uint32_t MeshRenderer::CreateInstancedPipeline(
    const std::wstring &vertexShaderPath, const std::wstring &pixelShaderPath,
    const std::wstring &shadowVertexShaderPath,
    const std::wstring &shadowPixelShaderPath) {
    auto *device = dxCommon_->GetDevice();
    InstancedPipelineSet pipelineSet{};
    auto instancedVs =
        ShaderCompiler::Compile(vertexShaderPath, "main", "vs_5_0");
    auto ps = ShaderCompiler::Compile(pixelShaderPath, "main", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC instancedLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"CUSTOM", 0, DXGI_FORMAT_R32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCEFADE", 0, DXGI_FORMAT_R32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCESEED", 0, DXGI_FORMAT_R32_UINT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    auto makePso = [&](bool transparent, MaterialCullMode cullMode,
                       bool depthWrite,
                       ComPtr<ID3D12PipelineState> &psoOut) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = rootSignature_.Get();
        pso.VS = {instancedVs->GetBufferPointer(),
                  instancedVs->GetBufferSize()};
        pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.InputLayout = {instancedLayout, _countof(instancedLayout)};
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DirectXCommon::kSceneColorFormat;
        pso.DSVFormat = DirectXCommon::kDepthStencilFormat;
        pso.SampleDesc.Count = 1;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.RasterizerState.CullMode = ToD3D12CullMode(cullMode);

        D3D12_BLEND_DESC blend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        blend.RenderTarget[0].BlendEnable = transparent ? TRUE : FALSE;
        blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.BlendState = blend;

        D3D12_DEPTH_STENCIL_DESC depth =
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                          : D3D12_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState = depth;

        ThrowIfFailed(device->CreateGraphicsPipelineState(
                          &pso, IID_PPV_ARGS(&psoOut)),
                      "CreateGraphicsPipelineState(CustomInstancedMesh) failed");
    };

    for (bool transparent : {false, true}) {
        for (MaterialCullMode cullMode :
             {MaterialCullMode::None, MaterialCullMode::Front,
              MaterialCullMode::Back}) {
            for (bool depthWrite : {false, true}) {
                const size_t index =
                    PipelineVariantIndex(transparent, cullMode, depthWrite);
                makePso(transparent, cullMode, depthWrite,
                        pipelineSet.pipelineStates[index]);
            }
        }
    }

    auto shadowInstancedVs =
        ShaderCompiler::Compile(shadowVertexShaderPath, "main", "vs_5_0");
    auto shadowPs =
        ShaderCompiler::Compile(shadowPixelShaderPath, "main", "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPso{};
    shadowPso.pRootSignature = shadowRootSignature_.Get();
    shadowPso.VS = {shadowInstancedVs->GetBufferPointer(),
                    shadowInstancedVs->GetBufferSize()};
    shadowPso.PS = {shadowPs->GetBufferPointer(), shadowPs->GetBufferSize()};
    shadowPso.InputLayout = {instancedLayout, _countof(instancedLayout)};
    shadowPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowPso.NumRenderTargets = 0;
    shadowPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    shadowPso.SampleDesc.Count = 1;
    shadowPso.SampleMask = UINT_MAX;
    shadowPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    shadowPso.RasterizerState.DepthBias = 1000;
    shadowPso.RasterizerState.SlopeScaledDepthBias = 1.5f;
    shadowPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    D3D12_DEPTH_STENCIL_DESC shadowDepth =
        CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    shadowDepth.DepthEnable = TRUE;
    shadowDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    shadowDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    shadowPso.DepthStencilState = shadowDepth;

    ThrowIfFailed(device->CreateGraphicsPipelineState(
                      &shadowPso,
                      IID_PPV_ARGS(&pipelineSet.shadowPipelineState)),
                  "CreateGraphicsPipelineState(CustomInstancedShadow) failed");

    const uint32_t pipelineId =
        static_cast<uint32_t>(customInstancedPipelines_.size());
    customInstancedPipelines_.push_back(std::move(pipelineSet));
    return pipelineId;
}

void MeshRenderer::CreateShadowPipelineStates() {
    auto *device = dxCommon_->GetDevice();
    auto vs =
        ShaderCompiler::Compile(ShaderPaths::MeshShadowVS, "main", "vs_5_0");
    auto instancedVs = ShaderCompiler::Compile(
        ShaderPaths::MeshShadowInstancedVS, "main", "vs_5_0");
    auto ps =
        ShaderCompiler::Compile(ShaderPaths::MeshShadowPS, "main", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC baseLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"CUSTOM", 0, DXGI_FORMAT_R32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_INPUT_ELEMENT_DESC instancedLayout[] = {
        baseLayout[0],
        baseLayout[1],
        baseLayout[2],
        baseLayout[3],
        baseLayout[4],
        baseLayout[5],
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCEFADE", 0, DXGI_FORMAT_R32_FLOAT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCESEED", 0, DXGI_FORMAT_R32_UINT, 1,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    auto makePso = [&](D3D12_SHADER_BYTECODE vertexShader,
                       D3D12_INPUT_LAYOUT_DESC inputLayout,
                       ComPtr<ID3D12PipelineState> &psoOut) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = shadowRootSignature_.Get();
        pso.VS = vertexShader;
        pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.InputLayout = inputLayout;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 0;
        pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pso.SampleDesc.Count = 1;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.RasterizerState.DepthBias = 1000;
        pso.RasterizerState.SlopeScaledDepthBias = 1.5f;
        pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        D3D12_DEPTH_STENCIL_DESC depth =
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState = depth;

        ThrowIfFailed(device->CreateGraphicsPipelineState(
                          &pso, IID_PPV_ARGS(&psoOut)),
                      "CreateGraphicsPipelineState(MeshShadow) failed");
    };

    makePso({vs->GetBufferPointer(), vs->GetBufferSize()},
            {baseLayout, _countof(baseLayout)}, shadowPSO_);
    makePso({instancedVs->GetBufferPointer(), instancedVs->GetBufferSize()},
            {instancedLayout, _countof(instancedLayout)}, instancedShadowPSO_);
}

void MeshRenderer::CreateUploadBuffer() {
    uploadBuffer_.Initialize(dxCommon_->GetDevice(), kUploadBytesPerFrame, 2);
}

D3D12_GPU_VIRTUAL_ADDRESS MeshRenderer::WriteObjectConstants(
    const XMMATRIX &wvp, const XMMATRIX &world,
    const XMMATRIX &worldInverseTranspose) {
    PerObjectConstBufferData data{};
    XMStoreFloat4x4(&data.matWVP, XMMatrixTranspose(wvp));
    XMStoreFloat4x4(&data.matWorld, XMMatrixTranspose(world));
    XMStoreFloat4x4(&data.matWorldInverseTranspose,
                    XMMatrixTranspose(worldInverseTranspose));
    return uploadBuffer_.Write(data).gpu;
}

D3D12_GPU_VIRTUAL_ADDRESS
MeshRenderer::WriteSceneConstants(const Camera &camera) {
    SceneConstBufferData data{};
    auto *sceneDst = &data;
    sceneDst->cameraPos = {camera.GetPosition().x, camera.GetPosition().y,
                           camera.GetPosition().z, 1.0f};
    sceneDst->keyLightDirection = {currentLighting_.keyLightDirection.x,
                                   currentLighting_.keyLightDirection.y,
                                   currentLighting_.keyLightDirection.z, 0.0f};
    sceneDst->keyLightColor = currentLighting_.keyLightColor;
    sceneDst->fillLightDirection = {currentLighting_.fillLightDirection.x,
                                    currentLighting_.fillLightDirection.y,
                                    currentLighting_.fillLightDirection.z,
                                    0.0f};
    sceneDst->fillLightColor = currentLighting_.fillLightColor;
    sceneDst->ambientColor = currentLighting_.ambientColor;
    for (size_t lightIndex = 0; lightIndex < currentLighting_.pointLights.size();
         ++lightIndex) {
        sceneDst->pointLights[lightIndex].positionRange =
            currentLighting_.pointLights[lightIndex].positionRange;
        sceneDst->pointLights[lightIndex].colorIntensity =
            currentLighting_.pointLights[lightIndex].colorIntensity;
    }
    sceneDst->lightingParams = currentLighting_.lightingParams;
    sceneDst->fogColor = currentFog_.color;
    sceneDst->fogParams = currentFog_.params;
    XMStoreFloat4x4(&sceneDst->viewProjection,
                    XMMatrixTranspose(camera.GetView() * camera.GetProj()));
    XMStoreFloat4x4(
        &sceneDst->lightViewProjection,
        XMMatrixTranspose(XMLoadFloat4x4(&shadowLightViewProjection_)));
    sceneDst->shadowParams = shadowParams_;
    sceneDst->shadowFilterParams = shadowFilterParams_;
    sceneDst->customSceneParams0 = customSceneParams0_;
    sceneDst->customSceneParams1 = customSceneParams1_;
    return uploadBuffer_.Write(data).gpu;
}

D3D12_GPU_VIRTUAL_ADDRESS
MeshRenderer::WriteShadowSceneConstants(
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    SceneConstBufferData data{};
    XMStoreFloat4x4(&data.viewProjection,
                    XMMatrixTranspose(XMLoadFloat4x4(&lightViewProjection)));
    data.customSceneParams0 = customSceneParams0_;
    data.customSceneParams1 = customSceneParams1_;
    return uploadBuffer_.Write(data).gpu;
}

D3D12_GPU_VIRTUAL_ADDRESS
MeshRenderer::WriteMaterialConstants(const Material &material) {
    return uploadBuffer_.Write(material).gpu;
}

D3D12_VERTEX_BUFFER_VIEW
MeshRenderer::WriteInstances(const InstanceData *instances,
                             uint32_t instanceCount) {
    const UploadAllocation allocation =
        uploadBuffer_.WriteArray(instances, instanceCount, alignof(InstanceData));
    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = allocation.gpu;
    view.SizeInBytes = static_cast<UINT>(allocation.size);
    view.StrideInBytes = sizeof(InstanceData);
    return view;
}

void MeshRenderer::SetPipelineForMaterial(const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        pipelineStates_[PipelineVariantIndex(material)].Get());
}

void MeshRenderer::SetPipelineForMaterial(
    const std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>,
                     kPipelineVariantCount> &pipelineStates,
    const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        pipelineStates[PipelineVariantIndex(material)].Get());
}

void MeshRenderer::SetInstancedPipelineForMaterial(const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        instancedPipelineStates_[PipelineVariantIndex(material)].Get());
}

void MeshRenderer::SetInstancedPipelineForMaterial(
    const std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>,
                     kPipelineVariantCount> &pipelineStates,
    const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        pipelineStates[PipelineVariantIndex(material)].Get());
}

void MeshRenderer::SetShadowMap(
    D3D12_GPU_DESCRIPTOR_HANDLE shadowMap,
    const DirectX::XMFLOAT4X4 &lightViewProjection,
    const SceneShadowSettings &settings) {
    shadowMapGpuHandle_ = shadowMap;
    shadowLightViewProjection_ = lightViewProjection;
    shadowParams_ = {1.0f, settings.bias,
                     (std::clamp)(settings.strength, 0.0f, 1.0f),
                     settings.normalBias};
    shadowFilterParams_ = {(std::max)(settings.filterRadius, 0.0f),
                           (std::max)(settings.depthSoftness, 0.0001f),
                           (std::max)(settings.edgeFade, 0.0f), 0.0f};
}

void MeshRenderer::SetCustomSceneParams(const DirectX::XMFLOAT4 &params0,
                                        const DirectX::XMFLOAT4 &params1) {
    customSceneParams0_ = params0;
    customSceneParams1_ = params1;
}
