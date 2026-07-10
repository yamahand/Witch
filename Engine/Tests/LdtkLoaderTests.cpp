// LDtk ローダ（非公開ヘッダ Level/LdtkLoader.h）のテスト。
// ParseLdtk はバイト列 → LevelData の純関数（RHI 非依存）。
// 実フィクスチャ（Content/Stage/Sample1_1.ldtk）と合成 JSON の両方で検証する。
#include "Level/LdtkLoader.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace {

using witch::LevelData;
using witch::ldtk::ParseLdtk;
using Catch::Approx;

std::vector<uint8_t> ToBytes(std::string_view text) {
    return {text.begin(), text.end()};
}

std::expected<LevelData, std::string> Parse(std::string_view jsonText,
                                            std::string_view sourceName = "Stage/Test.ldtk") {
    const auto bytes = ToBytes(jsonText);
    return ParseLdtk(bytes, sourceName);
}

TEST_CASE("ParseLdtk parses the Sample1_1 fixture", "[LdtkLoader]") {
    std::ifstream file(WITCH_TEST_CONTENT_DIR "/Stage/Sample1_1.ldtk", std::ios::binary);
    REQUIRE(file.is_open());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    const auto result = ParseLdtk(bytes, "Stage/Sample1_1.ldtk");
    REQUIRE(result.has_value());
    const LevelData& level = *result;

    CHECK(level.identifier == "AutoLayer");
    CHECK(level.width == 296);
    CHECK(level.height == 208);
    // __bgColor == "#474B67"
    CHECK(level.bgColor.r == Approx(0x47 / 255.0f));
    CHECK(level.bgColor.g == Approx(0x4B / 255.0f));
    CHECK(level.bgColor.b == Approx(0x67 / 255.0f));

    // IntGrid レイヤー 1 枚がオートタイルの描画レイヤーと IntGrid の両方になる。
    REQUIRE(level.tileLayers.size() == 1);
    const auto& layer = level.tileLayers[0];
    CHECK(layer.identifier == "IntGrid_layer");
    CHECK(layer.gridSize == 8);
    CHECK(layer.tilesetPath == "Stage/atlas/Cavernas_by_Adam_Saltsman.png");
    CHECK(layer.tiles.size() == 984);

    REQUIRE(level.intGrids.size() == 1);
    const auto& grid = level.intGrids[0];
    CHECK(grid.width == 37);
    CHECK(grid.height == 26);
    CHECK(grid.gridSize == 8);
    REQUIRE(grid.values.size() == 37u * 26u);
    // 値域は {0, 1}（1 = walls）で、両方の値が実際に現れる。
    CHECK(std::ranges::all_of(grid.values, [](int v) { return v == 0 || v == 1; }));
    CHECK(std::ranges::count(grid.values, 1) > 0);

    CHECK(level.entities.empty());
}

TEST_CASE("ParseLdtk translates parse failures to expected errors", "[LdtkLoader]") {
    // 壊れた JSON（例外がエラーへ翻訳されること）。
    CHECK_FALSE(Parse("{ not valid json").has_value());
    // JSON としては正しいが必須キーが無い。
    CHECK_FALSE(Parse("{}").has_value());
    CHECK_FALSE(Parse(R"({"levels": []})").has_value());
}

TEST_CASE("ParseLdtk rejects externalLevels", "[LdtkLoader]") {
    const auto result = Parse(R"({
        "externalLevels": true,
        "levels": [{"identifier": "L", "pxWid": 8, "pxHei": 8, "layerInstances": []}]
    })");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().find("externalLevels") != std::string::npos);
}

TEST_CASE("ParseLdtk expands flip bits and tile fields", "[LdtkLoader]") {
    const auto result = Parse(R"({
        "levels": [{
            "identifier": "L", "pxWid": 32, "pxHei": 8, "__bgColor": "#FF0080",
            "layerInstances": [{
                "__identifier": "T", "__type": "Tiles", "__gridSize": 8,
                "__opacity": 0.5, "__pxTotalOffsetX": 3, "__pxTotalOffsetY": -2,
                "__tilesetRelPath": "atlas/tiles.png",
                "gridTiles": [
                    {"px": [0, 0],  "src": [8, 16], "f": 0},
                    {"px": [8, 0],  "src": [0, 0],  "f": 1},
                    {"px": [16, 0], "src": [0, 0],  "f": 2, "a": 0.25},
                    {"px": [24, 0], "src": [0, 0],  "f": 3}
                ]
            }]
        }]
    })");
    REQUIRE(result.has_value());
    CHECK(result->bgColor.r == Approx(1.0f));
    CHECK(result->bgColor.g == Approx(0.0f));
    CHECK(result->bgColor.b == Approx(0x80 / 255.0f));

    REQUIRE(result->tileLayers.size() == 1);
    const auto& layer = result->tileLayers[0];
    CHECK(layer.opacity == Approx(0.5f));
    CHECK(layer.offsetX == 3);
    CHECK(layer.offsetY == -2);
    // sourceName "Stage/Test.ldtk" 基準で relPath が解決される。
    CHECK(layer.tilesetPath == "Stage/atlas/tiles.png");

    REQUIRE(layer.tiles.size() == 4);
    CHECK(layer.tiles[0].srcX == 8);
    CHECK(layer.tiles[0].srcY == 16);
    CHECK_FALSE(layer.tiles[0].flipX);
    CHECK_FALSE(layer.tiles[0].flipY);
    CHECK(layer.tiles[1].flipX);
    CHECK_FALSE(layer.tiles[1].flipY);
    CHECK_FALSE(layer.tiles[2].flipX);
    CHECK(layer.tiles[2].flipY);
    CHECK(layer.tiles[2].alpha == Approx(0.25f));
    CHECK(layer.tiles[3].flipX);
    CHECK(layer.tiles[3].flipY);
    CHECK(layer.tiles[3].alpha == Approx(1.0f));
}

TEST_CASE("ParseLdtk normalizes layer order back-to-front", "[LdtkLoader]") {
    // LDtk は配列先頭が最前面。LevelData は奥→手前なので逆順になる。
    const auto result = Parse(R"({
        "levels": [{
            "identifier": "L", "pxWid": 8, "pxHei": 8,
            "layerInstances": [
                {"__identifier": "Front", "__type": "Tiles", "__gridSize": 8,
                 "__tilesetRelPath": "a.png", "gridTiles": []},
                {"__identifier": "Back", "__type": "Tiles", "__gridSize": 8,
                 "__tilesetRelPath": "a.png", "gridTiles": []}
            ]
        }]
    })");
    REQUIRE(result.has_value());
    REQUIRE(result->tileLayers.size() == 2);
    CHECK(result->tileLayers[0].identifier == "Back");
    CHECK(result->tileLayers[1].identifier == "Front");
}

TEST_CASE("ParseLdtk parses entity instances", "[LdtkLoader]") {
    const auto result = Parse(R"({
        "levels": [{
            "identifier": "L", "pxWid": 64, "pxHei": 64,
            "layerInstances": [{
                "__identifier": "Entities", "__type": "Entities", "__gridSize": 8,
                "entityInstances": [
                    {"__identifier": "Enemy", "px": [24, 40], "width": 16, "height": 16},
                    {"__identifier": "Chest", "px": [8, 8], "width": 8, "height": 8}
                ]
            }]
        }]
    })");
    REQUIRE(result.has_value());
    REQUIRE(result->entities.size() == 2);
    CHECK(result->entities[0].identifier == "Enemy");
    CHECK(result->entities[0].x == Approx(24.0f));
    CHECK(result->entities[0].y == Approx(40.0f));
    CHECK(result->entities[0].width == 16);
    CHECK(result->entities[1].identifier == "Chest");
}

TEST_CASE("ParseLdtk resolves tileset relPath against the ldtk directory", "[LdtkLoader]") {
    const std::string_view json = R"({
        "levels": [{
            "identifier": "L", "pxWid": 8, "pxHei": 8,
            "layerInstances": [{
                "__identifier": "T", "__type": "Tiles", "__gridSize": 8,
                "__tilesetRelPath": "../Shared/tiles.png", "gridTiles": []
            }]
        }]
    })";
    // "Stage/Sub/Test.ldtk" + "../Shared/tiles.png" → "Stage/Shared/tiles.png"
    const auto ok = Parse(json, "Stage/Sub/Test.ldtk");
    REQUIRE(ok.has_value());
    REQUIRE(ok->tileLayers.size() == 1);
    CHECK(ok->tileLayers[0].tilesetPath == "Stage/Shared/tiles.png");

    // マウントルート直下の .ldtk から "../" はルート脱出 → レイヤーは警告スキップ。
    const auto escaped = Parse(json, "Test.ldtk");
    REQUIRE(escaped.has_value());
    CHECK(escaped->tileLayers.empty());
}

TEST_CASE("ParseLdtk skips hidden layers", "[LdtkLoader]") {
    const auto result = Parse(R"({
        "levels": [{
            "identifier": "L", "pxWid": 8, "pxHei": 8,
            "layerInstances": [{
                "__identifier": "T", "__type": "Tiles", "__gridSize": 8, "visible": false,
                "__tilesetRelPath": "a.png",
                "gridTiles": [{"px": [0, 0], "src": [0, 0]}]
            }]
        }]
    })");
    REQUIRE(result.has_value());
    CHECK(result->tileLayers.empty());
}

} // namespace
