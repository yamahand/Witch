#pragma once
#include "WitchEngine/Core/TextureInfo.h"
#include "WitchEngine/Level/LevelData.h"
#include "WitchEngine/Scene/Component.h"
#include <cstdint>
#include <vector>

namespace witch {

/// タイルレイヤー 1 枚を描画するコンポーネント（1 コンポーネント = LevelTileLayer 1 枚）。
/// レベル全体は 1 つの GameObject に本コンポーネントを複数載せて表現する。
/// コンストラクタで全タイルの UV・フリップ・不透明度を解決済みデータに焼き込み、
/// 毎フレームはカメラ可視矩形でカリングして SubmitSprite するだけにする。
class TilemapComponent : public Component {
public:
    WITCH_COMPONENT(TilemapComponent, Component);

    /// @param texture タイルセットテクスチャ（ResourceManager::LoadTexture の結果。
    ///                UV 換算にテクスチャ実サイズを使う）
    /// @param layer   ロード済みレイヤーデータ。タイルの alpha × レイヤーの opacity を
    ///                この時点で焼き込む（不透明度の実行時変更は必要になってから）。
    TilemapComponent(const TextureInfo& texture, const LevelTileLayer& layer);

    /// カメラの可視ワールド矩形と交差するタイルだけを SubmitSprite する。
    void Update(float dt) override;

    /// 描画提出は Render フェーズ（SpriteComponent と同じ）。
    UpdatePhase Phase() const override { return UpdatePhase::Render; }

#ifdef WITCH_DEBUG_UI
    /// タイル数・レイヤー・直近フレームの提出数を表示する。
    void DrawInspector() override;
#endif

    /// 描画レイヤー。大きいほど手前（SpriteComponent と同一の体系・エンコード）。
    void SetLayer(int16_t layer) { layer_ = layer; }
    int16_t Layer() const { return layer_; }

private:
    /// 描画解決済みのタイル 1 枚。座標はレイヤーオフセット込みのローカル px。
    struct Tile {
        float x, y;              ///< タイル矩形の左上（オーナー transform からの相対）
        float u0, v0, u1, v1;    ///< フリップ焼き込み済み UV
        float alpha;             ///< tile.alpha × layer.opacity
    };

    TextureInfo texture_;
    float tileSize_ = 0.0f;      ///< 描画サイズ px（= gridSize、正方形）
    std::vector<Tile> tiles_;
    int16_t layer_ = 0;
    /// 全タイルのローカル AABB（カメラ外レイヤーの早期リジェクト用）。
    float minX_ = 0.0f, minY_ = 0.0f, maxX_ = 0.0f, maxY_ = 0.0f;
#ifdef WITCH_DEBUG_UI
    int lastSubmitted_ = 0;      ///< 直近フレームの提出数（インスペクタ表示用）
#endif
};

} // namespace witch
