#include "model/ModelRenderer.h"
#include "graphics/DirectXCommon.h"
#include "graphics/DxHelpers.h"
#include "graphics/DxUtils.h"
#include "graphics/ShaderCompiler.h"
#include "graphics/ShaderPaths.h"
#include "graphics/SrvManager.h"
#include "model/MaterialManager.h"
#include "model/MeshManager.h"
#include "model/Vertex.h"
#include "texture/TextureManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using namespace DirectX;
using namespace DxUtils;
using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kSkinningThreadCount = 1024u;

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

}

static XMFLOAT4X4 StoreMatrix(const XMMATRIX &matrix) {
    XMFLOAT4X4 result{};
    XMStoreFloat4x4(&result, matrix);
    return result;
}

static XMMATRIX MakeSafeInverseTranspose(const XMMATRIX &matrix) {
    const XMVECTOR determinant = XMMatrixDeterminant(matrix);
    const float determinantValue = XMVectorGetX(determinant);
    if (!std::isfinite(determinantValue) ||
        std::abs(determinantValue) <= 0.000001f) {
        return XMMatrixIdentity();
    }

    return XMMatrixTranspose(XMMatrixInverse(nullptr, matrix));
}

static void NormalizeInfluence(VertexInfluence &influence) {
    float totalWeight = 0.0f;
    for (float weight : influence.weights) {
        totalWeight += weight;
    }

    if (totalWeight <= 0.00001f) {
        return;
    }

    for (float &weight : influence.weights) {
        weight /= totalWeight;
    }
}

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
    XMFLOAT4 lightingModeParams;
    XMFLOAT4 fogColor;
    XMFLOAT4 fogParams;
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 lightViewProjection;
    XMFLOAT4 shadowParams;
    XMFLOAT4 shadowFilterParams;
};

void ModelRenderer::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                               MeshManager *meshManager,
                               TextureManager *textureManager,
                               MaterialManager *materialManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    meshManager_ = meshManager;
    textureManager_ = textureManager;
    materialManager_ = materialManager;

    CreateRootSignature();
    CreateShadowRootSignature();
    CreateSkinningRootSignature();
    CreatePipelineState();
    CreateShadowPipelineState();
    CreateSkinningPipelineState();
    CreateUploadBuffer();
}

void ModelRenderer::BeginFrame() {
    uploadBuffer_.BeginFrame();
    drawIndex_ = 0;
}

void ModelRenderer::PreDraw() {
    auto cmd = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->SetGraphicsRootSignature(rootSignature_.Get());

    drawIndex_ = 0;
}

void ModelRenderer::Draw(const Model &model, const Transform &transform,
                         const Camera &camera, uint32_t environmentTextureId) {
    if (drawIndex_ >= kMaxDraws) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();

    XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));

    XMMATRIX world =
        XMMatrixScaling(transform.scale.x, transform.scale.y,
                        transform.scale.z) *
        XMMatrixRotationQuaternion(q) *
        XMMatrixTranslation(transform.position.x, transform.position.y,
                            transform.position.z);

    if (model.hasRootAnimation) {
        world = XMLoadFloat4x4(&model.rootAnimationMatrix) * world;
    }

    XMMATRIX worldInverseTranspose = MakeSafeInverseTranspose(world);

    XMMATRIX wvp = world * camera.GetView() * camera.GetProj();

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
            WriteObjectConstants(wvp, world, worldInverseTranspose);
        D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);

        DispatchSkinning(subMesh);

        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);

        SetPipelineForMaterial(material);

        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;

        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
        cmd->SetGraphicsRootConstantBufferView(
            2, materialManager_->GetGPUVirtualAddress(subMesh.materialId));
        cmd->SetGraphicsRootDescriptorTable(
            3, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->SetGraphicsRootDescriptorTable(
            4, subMesh.skinCluster.paletteSrvGpuHandle);
        const bool hasPerDrawEnvironmentTexture =
            (environmentTextureId != UINT32_MAX);
        const bool useEnvironmentTexture =
            hasPerDrawEnvironmentTexture || hasEnvironmentTexture_;
        const uint32_t boundEnvironmentTextureId = hasPerDrawEnvironmentTexture
                                                       ? environmentTextureId
                                                       : environmentTextureId_;
        if (useEnvironmentTexture) {
            cmd->SetGraphicsRootDescriptorTable(
                5, textureManager_->GetGpuHandle(boundEnvironmentTextureId));
        }
        cmd->SetGraphicsRootDescriptorTable(6, shadowMapGpuHandle_);
        cmd->SetGraphicsRootDescriptorTable(
            7, textureManager_->GetGpuHandle(ResolveNormalTextureId(
                   textureManager_, material, subMesh.normalTextureId)));

        cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

        drawIndex_++;
    };

    if (!model.subMeshes.empty()) {
        for (const auto &subMesh : model.subMeshes) {
            drawSubMesh(subMesh);
            if (drawIndex_ >= kMaxDraws) {
                break;
            }
        }
    }
}

void ModelRenderer::DrawInstanced(const Model &model,
                                  const Transform *transforms,
                                  uint32_t instanceCount,
                                  const Camera &camera,
                                  uint32_t environmentTextureId) {
    if (!transforms || instanceCount == 0) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(model, transforms, instanceCount);

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        DispatchSkinning(subMesh);

        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);
        SetInstancedPipelineForMaterial(material);

        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;
        D3D12_VERTEX_BUFFER_VIEW views[] = {vertexBufferView, instanceView};

        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
        cmd->SetGraphicsRootConstantBufferView(
            2, materialManager_->GetGPUVirtualAddress(subMesh.materialId));
        cmd->SetGraphicsRootDescriptorTable(
            3, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->SetGraphicsRootDescriptorTable(
            4, subMesh.skinCluster.paletteSrvGpuHandle);

        const bool hasPerDrawEnvironmentTexture =
            (environmentTextureId != UINT32_MAX);
        const bool useEnvironmentTexture =
            hasPerDrawEnvironmentTexture || hasEnvironmentTexture_;
        const uint32_t boundEnvironmentTextureId = hasPerDrawEnvironmentTexture
                                                       ? environmentTextureId
                                                       : environmentTextureId_;
        if (useEnvironmentTexture) {
            cmd->SetGraphicsRootDescriptorTable(
                5, textureManager_->GetGpuHandle(boundEnvironmentTextureId));
        }
        cmd->SetGraphicsRootDescriptorTable(6, shadowMapGpuHandle_);
        cmd->SetGraphicsRootDescriptorTable(
            7, textureManager_->GetGpuHandle(ResolveNormalTextureId(
                   textureManager_, material, subMesh.normalTextureId)));

        cmd->IASetVertexBuffers(0, 2, views);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);

        ++drawIndex_;
    };

    for (const auto &subMesh : model.subMeshes) {
        drawSubMesh(subMesh);
        if (drawIndex_ >= kMaxDraws) {
            break;
        }
    }
}

void ModelRenderer::DrawInstanced(const Model &model,
                                  const InstanceData *instances,
                                  uint32_t instanceCount,
                                  const Camera &camera,
                                  uint32_t environmentTextureId) {
    if (!instances || instanceCount == 0) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();

    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(XMMatrixIdentity(), XMMatrixIdentity(),
                             XMMatrixIdentity());
    const D3D12_GPU_VIRTUAL_ADDRESS sceneCbAddr = WriteSceneConstants(camera);
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(model, instances, instanceCount);

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        DispatchSkinning(subMesh);

        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);
        SetInstancedPipelineForMaterial(material);

        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;
        D3D12_VERTEX_BUFFER_VIEW views[] = {vertexBufferView, instanceView};

        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, sceneCbAddr);
        cmd->SetGraphicsRootConstantBufferView(
            2, materialManager_->GetGPUVirtualAddress(subMesh.materialId));
        cmd->SetGraphicsRootDescriptorTable(
            3, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->SetGraphicsRootDescriptorTable(
            4, subMesh.skinCluster.paletteSrvGpuHandle);

        const bool hasPerDrawEnvironmentTexture =
            (environmentTextureId != UINT32_MAX);
        const bool useEnvironmentTexture =
            hasPerDrawEnvironmentTexture || hasEnvironmentTexture_;
        const uint32_t boundEnvironmentTextureId = hasPerDrawEnvironmentTexture
                                                       ? environmentTextureId
                                                       : environmentTextureId_;
        if (useEnvironmentTexture) {
            cmd->SetGraphicsRootDescriptorTable(
                5, textureManager_->GetGpuHandle(boundEnvironmentTextureId));
        }
        cmd->SetGraphicsRootDescriptorTable(6, shadowMapGpuHandle_);
        cmd->SetGraphicsRootDescriptorTable(
            7, textureManager_->GetGpuHandle(ResolveNormalTextureId(
                   textureManager_, material, subMesh.normalTextureId)));

        cmd->IASetVertexBuffers(0, 2, views);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);

        ++drawIndex_;
    };

    for (const auto &subMesh : model.subMeshes) {
        drawSubMesh(subMesh);
        if (drawIndex_ >= kMaxDraws) {
            break;
        }
    }
}

void ModelRenderer::PostDraw() {}

void ModelRenderer::SetShadowMap(
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

void ModelRenderer::PreDrawShadow() {
    auto cmd = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap *heaps[] = {srvManager_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
    cmd->SetPipelineState(shadowPSO_.Get());
}

void ModelRenderer::DrawShadow(
    const Model &model, const Transform &transform,
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    if (drawIndex_ >= kMaxDraws) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();
    XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
    XMMATRIX world =
        XMMatrixScaling(transform.scale.x, transform.scale.y,
                        transform.scale.z) *
        XMMatrixRotationQuaternion(q) *
        XMMatrixTranslation(transform.position.x, transform.position.y,
                            transform.position.z);

    if (model.hasRootAnimation) {
        world = XMLoadFloat4x4(&model.rootAnimationMatrix) * world;
    }

    const XMMATRIX lightVP = XMLoadFloat4x4(&lightViewProjection);
    const XMMATRIX wvp = world * lightVP;

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        DispatchSkinning(subMesh);
        cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
        cmd->SetPipelineState(shadowPSO_.Get());
        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;

        const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
            WriteObjectConstants(wvp, world, XMMatrixIdentity());
        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);
        const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
            materialManager_->GetGPUVirtualAddress(subMesh.materialId);
        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, materialCbAddr);
        cmd->SetGraphicsRootDescriptorTable(
            2, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        ++drawIndex_;
    };

    for (const auto &subMesh : model.subMeshes) {
        drawSubMesh(subMesh);
        if (drawIndex_ >= kMaxDraws) {
            break;
        }
    }
}

void ModelRenderer::DrawInstancedShadow(
    const Model &model, const Transform *transforms, uint32_t instanceCount,
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    if (!transforms || instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();
    const XMMATRIX lightVP = XMLoadFloat4x4(&lightViewProjection);
    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(lightVP, XMMatrixIdentity(), XMMatrixIdentity());
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(model, transforms, instanceCount);

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        DispatchSkinning(subMesh);
        cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
        cmd->SetPipelineState(instancedShadowPSO_.Get());
        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;
        D3D12_VERTEX_BUFFER_VIEW views[] = {vertexBufferView, instanceView};

        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);
        const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
            materialManager_->GetGPUVirtualAddress(subMesh.materialId);
        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, materialCbAddr);
        cmd->SetGraphicsRootDescriptorTable(
            2, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->IASetVertexBuffers(0, 2, views);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);
        ++drawIndex_;
    };

    for (const auto &subMesh : model.subMeshes) {
        drawSubMesh(subMesh);
        if (drawIndex_ >= kMaxDraws) {
            break;
        }
    }
}

void ModelRenderer::DrawInstancedShadow(
    const Model &model, const InstanceData *instances, uint32_t instanceCount,
    const DirectX::XMFLOAT4X4 &lightViewProjection) {
    if (!instances || instanceCount == 0 || drawIndex_ >= kMaxDraws) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();
    const XMMATRIX lightVP = XMLoadFloat4x4(&lightViewProjection);
    const D3D12_GPU_VIRTUAL_ADDRESS objectCbAddr =
        WriteObjectConstants(lightVP, XMMatrixIdentity(), XMMatrixIdentity());
    const D3D12_VERTEX_BUFFER_VIEW instanceView =
        WriteInstances(model, instances, instanceCount);

    auto drawSubMesh = [&](const ModelSubMesh &subMesh) {
        if (drawIndex_ >= kMaxDraws) {
            return;
        }

        DispatchSkinning(subMesh);
        cmd->SetGraphicsRootSignature(shadowRootSignature_.Get());
        cmd->SetPipelineState(instancedShadowPSO_.Get());
        const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView =
            subMesh.skinCluster.skinnedVertexResource
                ? subMesh.skinCluster.skinnedVertexBufferView
                : mesh.vbView;
        D3D12_VERTEX_BUFFER_VIEW views[] = {vertexBufferView, instanceView};

        const Material &material =
            materialManager_->GetMaterial(subMesh.materialId);
        const D3D12_GPU_VIRTUAL_ADDRESS materialCbAddr =
            materialManager_->GetGPUVirtualAddress(subMesh.materialId);
        cmd->SetGraphicsRootConstantBufferView(0, objectCbAddr);
        cmd->SetGraphicsRootConstantBufferView(1, materialCbAddr);
        cmd->SetGraphicsRootDescriptorTable(
            2, textureManager_->GetGpuHandle(
                   ResolveBaseColorTextureId(material, subMesh.textureId)));
        cmd->IASetVertexBuffers(0, 2, views);
        cmd->IASetIndexBuffer(&mesh.ibView);
        cmd->IASetPrimitiveTopology(mesh.primitiveTopology);
        cmd->DrawIndexedInstanced(mesh.indexCount, instanceCount, 0, 0, 0);
        ++drawIndex_;
    };

    for (const auto &subMesh : model.subMeshes) {
        drawSubMesh(subMesh);
        if (drawIndex_ >= kMaxDraws) {
            break;
        }
    }
}

void ModelRenderer::CreateSkinClusters(Model &model) {
    auto *device = dxCommon_->GetDevice();

    for (auto &subMesh : model.subMeshes) {
        SkinCluster &skinCluster = subMesh.skinCluster;

        const uint32_t jointCount =
            std::max<uint32_t>(1, static_cast<uint32_t>(model.bones.size()));

        skinCluster.inverseBindPoseMatrices.assign(
            jointCount, StoreMatrix(XMMatrixIdentity()));

        if (subMesh.vertexCount > 0) {
            const Mesh &mesh = meshManager_->GetMesh(subMesh.meshId);
            const UINT influenceBufferSize = static_cast<UINT>(
                sizeof(VertexInfluence) * subMesh.vertexCount);

            CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
            auto influenceDesc =
                CD3DX12_RESOURCE_DESC::Buffer(influenceBufferSize);

            ThrowIfFailed(device->CreateCommittedResource(
                              &uploadHeap, D3D12_HEAP_FLAG_NONE, &influenceDesc,
                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                              IID_PPV_ARGS(&skinCluster.influenceResource)),
                          "CreateCommittedResource(InfluenceBuffer) failed");

            ThrowIfFailed(
                skinCluster.influenceResource->Map(
                    0, nullptr,
                    reinterpret_cast<void **>(&skinCluster.mappedInfluence)),
                "InfluenceBuffer Map failed");

            skinCluster.influenceCount = subMesh.vertexCount;
            std::memset(skinCluster.mappedInfluence, 0,
                        sizeof(VertexInfluence) * skinCluster.influenceCount);

            const UINT inputVertexSrvIndex = srvManager_->Allocate();
            skinCluster.inputVertexSrvCpuHandle =
                srvManager_->GetCpuHandle(inputVertexSrvIndex);
            skinCluster.inputVertexSrvGpuHandle =
                srvManager_->GetGpuHandle(inputVertexSrvIndex);

            D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc{};
            vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
            vertexSrvDesc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            vertexSrvDesc.Buffer.FirstElement = 0;
            vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            vertexSrvDesc.Buffer.NumElements = subMesh.vertexCount;
            vertexSrvDesc.Buffer.StructureByteStride = sizeof(Vertex);
            device->CreateShaderResourceView(
                mesh.vertexBuffer.Get(), &vertexSrvDesc,
                skinCluster.inputVertexSrvCpuHandle);

            const UINT influenceSrvIndex = srvManager_->Allocate();
            skinCluster.influenceSrvCpuHandle =
                srvManager_->GetCpuHandle(influenceSrvIndex);
            skinCluster.influenceSrvGpuHandle =
                srvManager_->GetGpuHandle(influenceSrvIndex);

            D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
            influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
            influenceSrvDesc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            influenceSrvDesc.Buffer.FirstElement = 0;
            influenceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            influenceSrvDesc.Buffer.NumElements = subMesh.vertexCount;
            influenceSrvDesc.Buffer.StructureByteStride =
                sizeof(VertexInfluence);
            device->CreateShaderResourceView(
                skinCluster.influenceResource.Get(), &influenceSrvDesc,
                skinCluster.influenceSrvCpuHandle);

            const UINT skinnedVertexBufferSize =
                static_cast<UINT>(sizeof(Vertex) * subMesh.vertexCount);
            CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
            auto skinnedVertexDesc = CD3DX12_RESOURCE_DESC::Buffer(
                skinnedVertexBufferSize,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            ThrowIfFailed(
                device->CreateCommittedResource(
                    &defaultHeap, D3D12_HEAP_FLAG_NONE, &skinnedVertexDesc,
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr,
                    IID_PPV_ARGS(&skinCluster.skinnedVertexResource)),
                "CreateCommittedResource(SkinnedVertexBuffer) failed");

            skinCluster.skinnedVertexBufferView.BufferLocation =
                skinCluster.skinnedVertexResource->GetGPUVirtualAddress();
            skinCluster.skinnedVertexBufferView.SizeInBytes =
                skinnedVertexBufferSize;
            skinCluster.skinnedVertexBufferView.StrideInBytes = sizeof(Vertex);

            const UINT skinnedVertexUavIndex = srvManager_->Allocate();
            skinCluster.skinnedVertexUavCpuHandle =
                srvManager_->GetCpuHandle(skinnedVertexUavIndex);
            skinCluster.skinnedVertexUavGpuHandle =
                srvManager_->GetGpuHandle(skinnedVertexUavIndex);

            D3D12_UNORDERED_ACCESS_VIEW_DESC skinnedVertexUavDesc{};
            skinnedVertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
            skinnedVertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            skinnedVertexUavDesc.Buffer.FirstElement = 0;
            skinnedVertexUavDesc.Buffer.NumElements = subMesh.vertexCount;
            skinnedVertexUavDesc.Buffer.StructureByteStride = sizeof(Vertex);
            skinnedVertexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            device->CreateUnorderedAccessView(
                skinCluster.skinnedVertexResource.Get(), nullptr,
                &skinnedVertexUavDesc, skinCluster.skinnedVertexUavCpuHandle);
        }

        for (const auto &[jointName, jointWeightData] :
             subMesh.skinClusterData) {
            const auto jointIt = model.boneMap.find(jointName);
            if (jointIt == model.boneMap.end()) {
                continue;
            }

            const uint32_t jointIndex = jointIt->second;
            if (jointIndex >= skinCluster.inverseBindPoseMatrices.size()) {
                continue;
            }

            skinCluster.inverseBindPoseMatrices[jointIndex] =
                jointWeightData.inverseBindPoseMatrix;

            for (const VertexWeightData &vertexWeight :
                 jointWeightData.vertexWeights) {
                if (vertexWeight.vertexIndex >= skinCluster.influenceCount) {
                    continue;
                }

                VertexInfluence &influence =
                    skinCluster.mappedInfluence[vertexWeight.vertexIndex];

                for (uint32_t influenceIndex = 0;
                     influenceIndex < kNumMaxInfluence; ++influenceIndex) {
                    if (influence.weights[influenceIndex] == 0.0f) {
                        influence.weights[influenceIndex] = vertexWeight.weight;
                        influence.jointIndices[influenceIndex] =
                            static_cast<int32_t>(jointIndex);
                        break;
                    }
                }
            }
        }

        for (uint32_t vertexIndex = 0; vertexIndex < skinCluster.influenceCount;
             ++vertexIndex) {
            NormalizeInfluence(skinCluster.mappedInfluence[vertexIndex]);
        }

        const UINT paletteBufferSize =
            static_cast<UINT>(sizeof(WellForGPU) * jointCount);

        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        auto paletteDesc = CD3DX12_RESOURCE_DESC::Buffer(paletteBufferSize);

        ThrowIfFailed(device->CreateCommittedResource(
                          &uploadHeap, D3D12_HEAP_FLAG_NONE, &paletteDesc,
                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                          IID_PPV_ARGS(&skinCluster.paletteResource)),
                      "CreateCommittedResource(PaletteBuffer) failed");

        ThrowIfFailed(
            skinCluster.paletteResource->Map(
                0, nullptr,
                reinterpret_cast<void **>(&skinCluster.mappedPalette)),
            "PaletteBuffer Map failed");

        skinCluster.paletteCount = jointCount;
        for (uint32_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
            skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
                StoreMatrix(XMMatrixTranspose(XMMatrixIdentity()));
            skinCluster.mappedPalette[jointIndex]
                .skeletonSpaceInverseTransposeMatrix =
                StoreMatrix(XMMatrixTranspose(XMMatrixIdentity()));
        }

        const UINT srvIndex = srvManager_->Allocate();
        skinCluster.paletteSrvCpuHandle = srvManager_->GetCpuHandle(srvIndex);
        skinCluster.paletteSrvGpuHandle = srvManager_->GetGpuHandle(srvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = jointCount;
        srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

        device->CreateShaderResourceView(skinCluster.paletteResource.Get(),
                                         &srvDesc,
                                         skinCluster.paletteSrvCpuHandle);
    }

    UpdateSkinClusters(model);
}

void ModelRenderer::UpdateSkinClusters(Model &model) {
    for (auto &subMesh : model.subMeshes) {
        SkinCluster &skinCluster = subMesh.skinCluster;
        if (!skinCluster.mappedPalette || skinCluster.paletteCount == 0) {
            continue;
        }

        if (model.bones.empty() || model.skeletonSpaceMatrices.empty()) {
            skinCluster.mappedPalette[0].skeletonSpaceMatrix =
                StoreMatrix(XMMatrixTranspose(XMMatrixIdentity()));
            skinCluster.mappedPalette[0].skeletonSpaceInverseTransposeMatrix =
                StoreMatrix(XMMatrixTranspose(XMMatrixIdentity()));
            continue;
        }

        const uint32_t jointCount = std::min<uint32_t>(
            skinCluster.paletteCount,
            static_cast<uint32_t>(model.skeletonSpaceMatrices.size()));

        for (uint32_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
            XMMATRIX inverseBindPose = XMLoadFloat4x4(
                &skinCluster.inverseBindPoseMatrices[jointIndex]);
            XMMATRIX skeletonSpace =
                XMLoadFloat4x4(&model.skeletonSpaceMatrices[jointIndex]);
            XMMATRIX skinningMatrix = inverseBindPose * skeletonSpace;
            XMMATRIX skinningInverseTranspose =
                MakeSafeInverseTranspose(skinningMatrix);

            XMStoreFloat4x4(
                &skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix,
                XMMatrixTranspose(skinningMatrix));
            XMStoreFloat4x4(&skinCluster.mappedPalette[jointIndex]
                                 .skeletonSpaceInverseTransposeMatrix,
                            XMMatrixTranspose(skinningInverseTranspose));
        }
    }
}

void ModelRenderer::CreateUploadBuffer() {
    uploadBuffer_.Initialize(dxCommon_->GetDevice(), kUploadBytesPerFrame, 2);
}

D3D12_GPU_VIRTUAL_ADDRESS ModelRenderer::WriteObjectConstants(
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
ModelRenderer::WriteSceneConstants(const Camera &camera) {
    SceneConstBufferData data{};
    data.cameraPos = {camera.GetPosition().x, camera.GetPosition().y,
                      camera.GetPosition().z, 1.0f};
    data.keyLightDirection = {currentLighting_.keyLightDirection.x,
                              currentLighting_.keyLightDirection.y,
                              currentLighting_.keyLightDirection.z, 0.0f};
    data.keyLightColor = currentLighting_.keyLightColor;
    data.fillLightDirection = {currentLighting_.fillLightDirection.x,
                               currentLighting_.fillLightDirection.y,
                               currentLighting_.fillLightDirection.z, 0.0f};
    data.fillLightColor = currentLighting_.fillLightColor;
    data.ambientColor = currentLighting_.ambientColor;
    for (size_t lightIndex = 0;
         lightIndex < currentLighting_.pointLights.size(); ++lightIndex) {
        data.pointLights[lightIndex].positionRange =
            currentLighting_.pointLights[lightIndex].positionRange;
        data.pointLights[lightIndex].colorIntensity =
            currentLighting_.pointLights[lightIndex].colorIntensity;
    }
    data.lightingParams = currentLighting_.lightingParams;
    data.lightingModeParams = currentLighting_.lightingModeParams;
    data.fogColor = currentFog_.color;
    data.fogParams = currentFog_.params;
    XMStoreFloat4x4(&data.viewProjection,
                    XMMatrixTranspose(camera.GetView() * camera.GetProj()));
    XMStoreFloat4x4(
        &data.lightViewProjection,
        XMMatrixTranspose(XMLoadFloat4x4(&shadowLightViewProjection_)));
    data.shadowParams = shadowParams_;
    data.shadowFilterParams = shadowFilterParams_;
    return uploadBuffer_.Write(data).gpu;
}

D3D12_VERTEX_BUFFER_VIEW
ModelRenderer::WriteInstances(const Model &model, const Transform *transforms,
                              uint32_t instanceCount) {
    std::vector<InstanceData> instances(instanceCount);
    for (uint32_t index = 0; index < instanceCount; ++index) {
        const Transform &transform = transforms[index];
        XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
        const XMMATRIX world =
            XMMatrixScaling(transform.scale.x, transform.scale.y,
                            transform.scale.z) *
            XMMatrixRotationQuaternion(q) *
            XMMatrixTranslation(transform.position.x, transform.position.y,
                                transform.position.z);
        XMStoreFloat4x4(&instances[index].world, world);
    }

    return WriteInstances(model, instances.data(), instanceCount);
}

D3D12_VERTEX_BUFFER_VIEW
ModelRenderer::WriteInstances(const Model &model,
                              const InstanceData *sourceInstances,
                              uint32_t instanceCount) {
    std::vector<InstanceData> instances(instanceCount);
    const XMMATRIX root =
        model.hasRootAnimation ? XMLoadFloat4x4(&model.rootAnimationMatrix)
                               : XMMatrixIdentity();

    for (uint32_t index = 0; index < instanceCount; ++index) {
        instances[index] = sourceInstances[index];
        XMMATRIX world = XMLoadFloat4x4(&sourceInstances[index].world);
        if (model.hasRootAnimation) {
            world = root * world;
        }
        XMStoreFloat4x4(&instances[index].world, world);
    }

    const UploadAllocation allocation =
        uploadBuffer_.WriteArray(instances.data(), instances.size(),
                                 alignof(InstanceData));

    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = allocation.gpu;
    view.SizeInBytes =
        static_cast<UINT>(sizeof(InstanceData) * instances.size());
    view.StrideInBytes = sizeof(InstanceData);
    return view;
}

void ModelRenderer::SetPipelineForMaterial(const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        pipelineStates_[PipelineVariantIndex(material)].Get());
}

void ModelRenderer::SetInstancedPipelineForMaterial(const Material &material) {
    dxCommon_->GetCommandList()->SetPipelineState(
        instancedPipelineStates_[PipelineVariantIndex(material)].Get());
}

void ModelRenderer::CreateRootSignature() {
    CD3DX12_ROOT_PARAMETER params[8];

    params[0].InitAsConstantBufferView(0);
    params[1].InitAsConstantBufferView(1);
    params[2].InitAsConstantBufferView(2);

    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[3].InitAsDescriptorTable(1, &textureRange);

    CD3DX12_DESCRIPTOR_RANGE matrixPaletteRange;
    matrixPaletteRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[4].InitAsDescriptorTable(1, &matrixPaletteRange);

    CD3DX12_DESCRIPTOR_RANGE environmentRange;
    environmentRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[5].InitAsDescriptorTable(1, &environmentRange);

    CD3DX12_DESCRIPTOR_RANGE shadowRange;
    shadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    params[6].InitAsDescriptorTable(1, &shadowRange);

    CD3DX12_DESCRIPTOR_RANGE normalRange;
    normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    params[7].InitAsDescriptorTable(1, &normalRange);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 1, &sampler,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;

    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature failed");

    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&rootSignature_)),
                  "CreateRootSignature failed");
}

void ModelRenderer::CreateShadowRootSignature() {
    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstantBufferView(0);
    params[1].InitAsConstantBufferView(2);

    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[2].InitAsDescriptorTable(1, &textureRange);

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
                  "D3D12SerializeRootSignature(ModelShadow) failed");
    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&shadowRootSignature_)),
                  "CreateRootSignature(ModelShadow) failed");
}

void ModelRenderer::CreateSkinningRootSignature() {
    CD3DX12_ROOT_PARAMETER params[5];

    params[0].InitAsConstants(1, 0);

    CD3DX12_DESCRIPTOR_RANGE inputVertexRange;
    inputVertexRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &inputVertexRange);

    CD3DX12_DESCRIPTOR_RANGE influenceRange;
    influenceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &influenceRange);

    CD3DX12_DESCRIPTOR_RANGE matrixPaletteRange;
    matrixPaletteRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &matrixPaletteRange);

    CD3DX12_DESCRIPTOR_RANGE skinnedVertexRange;
    skinnedVertexRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    params[4].InitAsDescriptorTable(1, &skinnedVertexRange);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr);

    ComPtr<ID3DBlob> blob, error;

    ThrowIfFailed(D3D12SerializeRootSignature(
                      &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(Skinning) failed");

    ThrowIfFailed(dxCommon_->GetDevice()->CreateRootSignature(
                      0, blob->GetBufferPointer(), blob->GetBufferSize(),
                      IID_PPV_ARGS(&skinningRootSignature_)),
                  "CreateRootSignature(Skinning) failed");
}

void ModelRenderer::CreatePipelineState() {
    auto device = dxCommon_->GetDevice();

    auto vs =
        ShaderCompiler::Compile(ShaderPaths::ModelVS, "main", "vs_5_0");
    auto instancedVs =
        ShaderCompiler::Compile(ShaderPaths::ModelInstancedVS, "main",
                                "vs_5_0");

    auto ps =
        ShaderCompiler::Compile(ShaderPaths::ModelPS, "main", "ps_5_0");

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
    };

    D3D12_INPUT_ELEMENT_DESC instancedLayout[] = {
        baseLayout[0],
        baseLayout[1],
        baseLayout[2],
        baseLayout[3],
        baseLayout[4],
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
        pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
                      "CreateGraphicsPipelineState(ModelRenderer) failed");
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

void ModelRenderer::CreateShadowPipelineState() {
    auto device = dxCommon_->GetDevice();
    auto vs =
        ShaderCompiler::Compile(ShaderPaths::ModelShadowVS, "main", "vs_5_0");
    auto instancedVs = ShaderCompiler::Compile(
        ShaderPaths::ModelShadowInstancedVS, "main", "vs_5_0");
    auto ps =
        ShaderCompiler::Compile(ShaderPaths::ModelShadowPS, "main", "ps_5_0");

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
    };

    D3D12_INPUT_ELEMENT_DESC instancedLayout[] = {
        baseLayout[0],
        baseLayout[1],
        baseLayout[2],
        baseLayout[3],
        baseLayout[4],
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
                      "CreateGraphicsPipelineState(ModelShadow) failed");
    };

    makePso({vs->GetBufferPointer(), vs->GetBufferSize()},
            {baseLayout, _countof(baseLayout)}, shadowPSO_);
    makePso({instancedVs->GetBufferPointer(), instancedVs->GetBufferSize()},
            {instancedLayout, _countof(instancedLayout)},
            instancedShadowPSO_);
}

void ModelRenderer::CreateSkinningPipelineState() {
    auto cs =
        ShaderCompiler::Compile(ShaderPaths::SkinningCS, "main", "cs_5_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = skinningRootSignature_.Get();
    pso.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};

    ThrowIfFailed(dxCommon_->GetDevice()->CreateComputePipelineState(
                      &pso, IID_PPV_ARGS(&skinningPSO_)),
                  "CreateComputePipelineState(Skinning) failed");
}

void ModelRenderer::DispatchSkinning(const ModelSubMesh &subMesh) {
    const SkinCluster &skinCluster = subMesh.skinCluster;
    if (!skinCluster.skinnedVertexResource || subMesh.vertexCount == 0) {
        return;
    }

    auto cmd = dxCommon_->GetCommandList();

    auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
        skinCluster.skinnedVertexResource.Get(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &toUav);

    cmd->SetPipelineState(skinningPSO_.Get());
    cmd->SetComputeRootSignature(skinningRootSignature_.Get());
    cmd->SetComputeRoot32BitConstant(0, subMesh.vertexCount, 0);
    cmd->SetComputeRootDescriptorTable(1, skinCluster.inputVertexSrvGpuHandle);
    cmd->SetComputeRootDescriptorTable(2, skinCluster.influenceSrvGpuHandle);
    cmd->SetComputeRootDescriptorTable(3, skinCluster.paletteSrvGpuHandle);
    cmd->SetComputeRootDescriptorTable(4,
                                       skinCluster.skinnedVertexUavGpuHandle);

    const UINT threadGroupCount =
        (subMesh.vertexCount + kSkinningThreadCount - 1u) /
        kSkinningThreadCount;
    cmd->Dispatch(threadGroupCount, 1, 1);

    auto uavBarrier =
        CD3DX12_RESOURCE_BARRIER::UAV(skinCluster.skinnedVertexResource.Get());
    cmd->ResourceBarrier(1, &uavBarrier);

    auto toVertex = CD3DX12_RESOURCE_BARRIER::Transition(
        skinCluster.skinnedVertexResource.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd->ResourceBarrier(1, &toVertex);
}
