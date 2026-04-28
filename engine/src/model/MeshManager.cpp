#include "MeshManager.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace DxUtils;
using Microsoft::WRL::ComPtr;

void MeshManager::Initialize(DirectXCommon *dxCommon) {
    if (!dxCommon) {
        throw std::invalid_argument("MeshManager::Initialize requires dxCommon");
    }

    dxCommon_ = dxCommon;
    meshes_.clear();
}

uint32_t MeshManager::CreateMesh(const void *vertexData, uint32_t vertexStride,
                                 uint32_t vertexCount,
                                 const uint32_t *indexData,
                                 uint32_t indexCount) {
    if (!dxCommon_) {
        throw std::runtime_error("MeshManager is not initialized");
    }
    if (!vertexData || vertexStride == 0 || vertexCount == 0) {
        throw std::invalid_argument("MeshManager::CreateMesh requires vertices");
    }
    if (!indexData || indexCount == 0) {
        throw std::invalid_argument("MeshManager::CreateMesh requires indices");
    }

    Mesh mesh{};
    mesh.indexCount = indexCount;
    mesh.vertexStride = vertexStride;

    const uint64_t vbSize64 =
        static_cast<uint64_t>(vertexStride) * static_cast<uint64_t>(vertexCount);
    if (vbSize64 > (std::numeric_limits<UINT>::max)()) {
        throw std::overflow_error("Vertex buffer is too large");
    }
    const UINT vbSize = static_cast<UINT>(vbSize64);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&mesh.vertexBuffer)),
                  "Create VertexBuffer failed");

    void *vbMapped = nullptr;
    ThrowIfFailed(mesh.vertexBuffer->Map(0, nullptr, &vbMapped),
                  "Map VertexBuffer failed");
    std::memcpy(vbMapped, vertexData, vbSize);
    mesh.vertexBuffer->Unmap(0, nullptr);

    mesh.vbView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();

    mesh.vbView.SizeInBytes = vbSize;
    mesh.vbView.StrideInBytes = vertexStride;

    const uint64_t ibSize64 =
        static_cast<uint64_t>(sizeof(uint32_t)) *
        static_cast<uint64_t>(indexCount);
    if (ibSize64 > (std::numeric_limits<UINT>::max)()) {
        throw std::overflow_error("Index buffer is too large");
    }
    const UINT ibSize = static_cast<UINT>(ibSize64);

    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &heapProps, D3D12_HEAP_FLAG_NONE, &ibDesc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&mesh.indexBuffer)),
                  "Create IndexBuffer failed");

    void *ibMapped = nullptr;
    ThrowIfFailed(mesh.indexBuffer->Map(0, nullptr, &ibMapped),
                  "Map IndexBuffer failed");
    std::memcpy(ibMapped, indexData, ibSize);
    mesh.indexBuffer->Unmap(0, nullptr);

    mesh.ibView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();

    mesh.ibView.Format = DXGI_FORMAT_R32_UINT;
    mesh.ibView.SizeInBytes = ibSize;

    meshes_.push_back(mesh);
    uint32_t meshId = static_cast<uint32_t>(meshes_.size() - 1);

    return meshId;
}

const Mesh &MeshManager::GetMesh(uint32_t meshId) const {
    return meshes_.at(meshId);
}
