#include "GBuffer.h"
#include "DirectXCommon.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "SrvManager.h"
#include <array>
#include <stdexcept>

using namespace DxUtils;

namespace {
constexpr uint32_t ToIndex(GBuffer::Target target) {
    return static_cast<uint32_t>(target);
}

constexpr std::array<DXGI_FORMAT, static_cast<uint32_t>(GBuffer::Target::Count)>
    kTargetFormats = {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
    };

constexpr std::array<std::array<float, 4>,
                     static_cast<uint32_t>(GBuffer::Target::Count)>
    kClearColors = {{
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
} // namespace

GBuffer::~GBuffer() {
    if (!srvManager_) {
        return;
    }

    for (UINT &srvIndex : srvIndices_) {
        if (srvIndex != UINT_MAX) {
            srvManager_->Free(srvIndex);
            srvIndex = UINT_MAX;
        }
    }
}

void GBuffer::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                         int width, int height) {
    if (!dxCommon || !srvManager) {
        throw std::invalid_argument("GBuffer::Initialize requires valid managers");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("GBuffer::Initialize requires a positive size");
    }

    if (srvManager_ && srvManager_ != srvManager) {
        for (UINT &srvIndex : srvIndices_) {
            if (srvIndex != UINT_MAX) {
                srvManager_->Free(srvIndex);
                srvIndex = UINT_MAX;
            }
        }
    }

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    for (UINT &srvIndex : srvIndices_) {
        if (srvIndex == UINT_MAX) {
            srvIndex = srvManager_->Allocate();
        }
    }
    width_ = width;
    height_ = height;

    CreateResources();
}

void GBuffer::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) {
        return;
    }

    width_ = width;
    height_ = height;
    for (auto &resource : resources_) {
        resource.Reset();
    }
    rtvHeap_.Reset();

    CreateResources();
}

void GBuffer::BeginGeometryPass() {
    auto commandList = dxCommon_->GetCommandList();

    std::array<D3D12_RESOURCE_BARRIER, kTargetCount> barriers{};
    for (uint32_t targetIndex = 0; targetIndex < kTargetCount; ++targetIndex) {
        barriers[targetIndex] = CD3DX12_RESOURCE_BARRIER::Transition(
            resources_[targetIndex].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()),
                                 barriers.data());

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = width_;
    scissorRect.bottom = height_;

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kTargetCount> rtvHandles{};
    for (uint32_t targetIndex = 0; targetIndex < kTargetCount; ++targetIndex) {
        rtvHandles[targetIndex] =
            GetRtvHandle(static_cast<Target>(targetIndex));
    }
    auto dsvHandle = dxCommon_->GetDepthStencilView();

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->OMSetRenderTargets(static_cast<UINT>(rtvHandles.size()),
                                    rtvHandles.data(), FALSE, &dsvHandle);

    for (uint32_t targetIndex = 0; targetIndex < kTargetCount; ++targetIndex) {
        commandList->ClearRenderTargetView(rtvHandles[targetIndex],
                                           kClearColors[targetIndex].data(), 0,
                                           nullptr);
    }
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f,
                                       0, 0, nullptr);
}

void GBuffer::EndGeometryPass() {
    std::array<D3D12_RESOURCE_BARRIER, kTargetCount> barriers{};
    for (uint32_t targetIndex = 0; targetIndex < kTargetCount; ++targetIndex) {
        barriers[targetIndex] = CD3DX12_RESOURCE_BARRIER::Transition(
            resources_[targetIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    dxCommon_->GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()),
                                                 barriers.data());
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetGpuHandle(Target target) const {
    return srvManager_->GetGpuHandle(srvIndices_[ToIndex(target)]);
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetRtvHandle(Target target) const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(ToIndex(target)), static_cast<INT>(rtvDescriptorSize_));
    return handle;
}

void GBuffer::CreateResources() {
    auto device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = kTargetCount;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc,
                                               IID_PPV_ARGS(&rtvHeap_)),
                  "Create GBuffer RTV heap failed");

    rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    for (uint32_t targetIndex = 0; targetIndex < kTargetCount; ++targetIndex) {
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            kTargetFormats[targetIndex], static_cast<UINT64>(width_),
            static_cast<UINT>(height_), 1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = kTargetFormats[targetIndex];
        clearValue.Color[0] = kClearColors[targetIndex][0];
        clearValue.Color[1] = kClearColors[targetIndex][1];
        clearValue.Color[2] = kClearColors[targetIndex][2];
        clearValue.Color[3] = kClearColors[targetIndex][3];

        ThrowIfFailed(device->CreateCommittedResource(
                          &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                          &clearValue, IID_PPV_ARGS(&resources_[targetIndex])),
                      "Create GBuffer resource failed");

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = kTargetFormats[targetIndex];
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(
            resources_[targetIndex].Get(), &rtvDesc,
            GetRtvHandle(static_cast<Target>(targetIndex)));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = kTargetFormats[targetIndex];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(resources_[targetIndex].Get(), &srvDesc,
                                         srvManager_->GetCpuHandle(
                                             srvIndices_[targetIndex]));
    }
}
