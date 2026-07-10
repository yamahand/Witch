#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <string>
#include <vector>

namespace witch {

/// レベルデータの純データ型群。特定のレベルエディタ形式（LDtk / Tiled）に
/// 依存しないフォーマット中立な中間表現で、パーサ（Engine/Source/Level/ の
/// 各ローダ）がこの形へ正規化する。描画は TilemapComponent、エンティティ実体化は
/// Scene::LoadLevel が消費する。

/// タイル 1 枚。ロード時に描画情報へ解決済み（フリップ・不透明度は展開済み）。
struct LevelTile {
    int x = 0, y = 0;          ///< レベルローカル px（描画矩形の左上）
    int srcX = 0, srcY = 0;    ///< タイルセットテクスチャ上の px（左上）
    bool flipX = false;        ///< 左右反転
    bool flipY = false;        ///< 上下反転
    float alpha = 1.0f;        ///< タイル個別の不透明度（0.0〜1.0）
};

/// 描画するタイルレイヤー 1 枚。
struct LevelTileLayer {
    std::string identifier;    ///< レイヤー名
    int gridSize = 0;          ///< タイルの描画 px サイズ（正方形）
    float opacity = 1.0f;      ///< レイヤー全体の不透明度（0.0〜1.0）
    int offsetX = 0, offsetY = 0;  ///< レイヤー全体の px オフセット
    std::string tilesetPath;   ///< タイルセットテクスチャの VFS パス（解決済み）
    std::vector<LevelTile> tiles;
};

/// セル単位の整数値グリッド（衝突属性等）。M7 の物理が読む予定で、
/// 現時点では保存のみ（エンジンは値を解釈しない）。
struct LevelIntGrid {
    std::string identifier;    ///< レイヤー名
    int width = 0, height = 0; ///< セル数（横・縦）
    int gridSize = 0;          ///< 1 セルの px サイズ
    /// 行優先。index = cy * width + cx。0 = 空。値の意味はレベル側の定義に従う
    /// （Sample1_1.ldtk では 1 = walls）。
    std::vector<int> values;
};

/// エンティティ配置 1 件。identifier を ObjectRegistry のキーとして実体化する。
struct LevelEntity {
    std::string identifier;    ///< 型名（ObjectRegistry の登録名と一致させる）
    float x = 0, y = 0;        ///< 配置位置 px（エディタのピボット位置の生値）
    int width = 0, height = 0; ///< エディタ上のサイズ px（参考値）
};

/// レベル 1 面分。
struct LevelData {
    std::string identifier;    ///< レベル名
    int width = 0, height = 0; ///< レベル全体の px サイズ
    rhi::Color bgColor{0.0f, 0.0f, 0.0f, 1.0f};  ///< 背景クリア色
    /// 奥→手前の順（先頭を最初に描く）。パーサがこの順へ正規化する契約。
    std::vector<LevelTileLayer> tileLayers;
    std::vector<LevelIntGrid> intGrids;
    std::vector<LevelEntity> entities;
};

} // namespace witch
