#pragma once
#include "Sprite.h"
#include <DirectXMath.h>
#include <cstddef>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class TextureManager;
class SrvManager;

/// <summary>
/// 2Dスプライト描画パイプラインを管理する
/// </summary>
class SpriteRenderer {
  public:
    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="dxCommon">DirectXCommonインスタンス</param>
    /// <param name="textureManager">TextureManagerインスタンス</param>
    /// <param name="srvManager">SrvManagerインスタンス</param>
    /// <param name="width">クライアント領域の幅</param>
    /// <param name="height">クライアント領域の高さ</param>
    void Initialize(DirectXCommon *dxCommon, TextureManager *textureManager,
                    SrvManager *srvManager, int width, int height);

    /// <summary>
    /// 描画処理
    /// </summary>
    /// <param name="sprite">描画するスプライト</param>
    void Draw(const Sprite &sprite);

    void DrawFilledCircle(const DirectX::XMFLOAT2 &center, float radius,
                          const DirectX::XMFLOAT4 &color,
                          uint32_t segments = 48);
    void DrawCircle(const DirectX::XMFLOAT2 &center, float radius,
                    const DirectX::XMFLOAT4 &color, float thickness,
                    uint32_t segments = 48);
    void DrawLine(const DirectX::XMFLOAT2 &start, const DirectX::XMFLOAT2 &end,
                  const DirectX::XMFLOAT4 &color, float thickness);
    void DrawPolyline(const DirectX::XMFLOAT2 *points, size_t pointCount,
                      const DirectX::XMFLOAT4 &color, bool closed,
                      float thickness);
    void DrawConvexPolygon(const DirectX::XMFLOAT2 *points, size_t pointCount,
                           const DirectX::XMFLOAT4 &color);

    /// <summary>
    /// 描画前処理
    /// </summary>
    void PreDraw();

    /// <summary>
    /// 描画後処理
    /// </summary>
    void PostDraw();

    /// <summary>
    /// 投影行列を更新する
    /// </summary>
    /// <param name="width">描画領域の幅</param>
    /// <param name="height">描画領域の高さ</param>
    void UpdateProjection(int width, int height);

  private:
    struct SpriteVertex {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT4 color;
    };

    /// <summary>
    /// ルートシグネチャを生成する
    /// </summary>
    void CreateRootSignature();
    /// <summary>
    /// パイプラインステートを生成する
    /// </summary>
    void CreatePipelineState();
    /// <summary>
    /// 頂点バッファを生成する
    /// </summary>
    void CreateVertexBuffer();
    /// <summary>
    /// 定数バッファを生成する
    /// </summary>
    void CreateConstantBuffer();
    void DrawPrimitiveTriangle(const DirectX::XMFLOAT2 &a,
                               const DirectX::XMFLOAT2 &b,
                               const DirectX::XMFLOAT2 &c,
                               const DirectX::XMFLOAT4 &color);
    void DrawPrimitiveQuad(const DirectX::XMFLOAT2 &a,
                           const DirectX::XMFLOAT2 &b,
                           const DirectX::XMFLOAT2 &c,
                           const DirectX::XMFLOAT2 &d,
                           const DirectX::XMFLOAT4 &color);
    void DrawVertices(const SpriteVertex *vertices, UINT vertexCount);

    static constexpr UINT kMaxDynamicVertices = 65536;

    DirectXCommon *dxCommon_ = nullptr;
    TextureManager *textureManager_ = nullptr;
    SrvManager *srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    SpriteVertex *vertexMapped_ = nullptr;
    UINT vertexCursor_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;

    DirectX::XMFLOAT4X4 matProjection_{};
};
