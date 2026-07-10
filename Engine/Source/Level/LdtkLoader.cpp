#include "Level/LdtkLoader.h"
#include "WitchEngine/Core/Logger.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <format>
#include <utility>

// nlohmann-json はこの TU に完全隔離する（公開ヘッダ・他 TU へ型も例外も
// 漏らさない）。パース全体を 1 つの try/catch で包み、json の型不一致・
// キー欠落等の例外を std::expected に翻訳する。

namespace witch::ldtk {

namespace {

using nlohmann::json;

/// "#RRGGBB" を rhi::Color へ変換する。不正な文字列は警告して黒を返す
/// （見た目で分かる回復可能な失敗であり、ロード全体を止めない）。
rhi::Color ParseHexColor(const std::string& hex, std::string_view sourceName) {
    if (hex.size() == 7 && hex[0] == '#') {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int v[6];
        bool ok = true;
        for (int i = 0; i < 6; ++i) {
            v[i] = nibble(hex[1 + i]);
            if (v[i] < 0) ok = false;
        }
        if (ok) {
            return {static_cast<float>(v[0] * 16 + v[1]) / 255.0f,
                    static_cast<float>(v[2] * 16 + v[3]) / 255.0f,
                    static_cast<float>(v[4] * 16 + v[5]) / 255.0f, 1.0f};
        }
    }
    log::Warn("LdtkLoader: invalid color '{}' in {} (using black)", hex, sourceName);
    return {0.0f, 0.0f, 0.0f, 1.0f};
}

/// タイルセット relPath（.ldtk の所在ディレクトリ基準）を VFS パスへ解決する。
/// 正規化後にマウントルートを脱出する（".." が残る）場合は空を返す。
std::string ResolveTilesetPath(std::string_view sourceName, const std::string& relPath) {
    namespace fs = std::filesystem;
    const auto slash = sourceName.find_last_of("/\\");
    const std::string_view dir =
        (slash == std::string_view::npos) ? std::string_view{} : sourceName.substr(0, slash);
    const std::string joined = (fs::path(dir) / relPath).lexically_normal().generic_string();
    if (joined.starts_with("..")) return {};
    return joined;
}

/// gridTiles / autoLayerTiles のタイル配列を LevelTile へ変換する
/// （両者はフィールド構造が同一）。
std::vector<LevelTile> ParseTiles(const json& tiles) {
    std::vector<LevelTile> result;
    result.reserve(tiles.size());
    for (const auto& t : tiles) {
        LevelTile tile;
        tile.x = t.at("px").at(0).get<int>();
        tile.y = t.at("px").at(1).get<int>();
        tile.srcX = t.at("src").at(0).get<int>();
        tile.srcY = t.at("src").at(1).get<int>();
        const int f = t.value("f", 0);
        tile.flipX = (f & 1) != 0;
        tile.flipY = (f & 2) != 0;
        tile.alpha = t.value("a", 1.0f);
        result.push_back(tile);
    }
    return result;
}

/// タイルを持つレイヤーインスタンス 1 枚を LevelTileLayer へ変換する。
/// タイルセットパスが未解決なら nullopt（呼び出し側が警告してスキップ）。
std::optional<LevelTileLayer> ParseTileLayer(const json& li, const json& tiles,
                                             std::string_view sourceName) {
    const std::string relPath = li.value("__tilesetRelPath", "");
    if (relPath.empty()) return std::nullopt;
    std::string tilesetPath = ResolveTilesetPath(sourceName, relPath);
    if (tilesetPath.empty()) {
        log::Warn("LdtkLoader: tileset path '{}' escapes mount root in {}", relPath,
                  sourceName);
        return std::nullopt;
    }
    LevelTileLayer layer;
    layer.identifier = li.at("__identifier").get<std::string>();
    layer.gridSize = li.at("__gridSize").get<int>();
    layer.opacity = li.value("__opacity", 1.0f);
    layer.offsetX = li.value("__pxTotalOffsetX", 0);
    layer.offsetY = li.value("__pxTotalOffsetY", 0);
    layer.tilesetPath = std::move(tilesetPath);
    layer.tiles = ParseTiles(tiles);
    return layer;
}

} // namespace

std::expected<LevelData, std::string> ParseLdtk(std::span<const uint8_t> bytes,
                                                std::string_view sourceName) {
    try {
        const json root = json::parse(bytes.begin(), bytes.end());

        if (root.value("externalLevels", false)) {
            return std::unexpected(std::format(
                "{}: externalLevels (レベル別ファイル保存) は未対応", sourceName));
        }
        const json& levels = root.at("levels");
        if (levels.empty()) {
            return std::unexpected(std::format("{}: levels が空", sourceName));
        }

        // 先頭レベルのみ読む（複数レベルの選択は必要になってから）。
        const json& lv = levels.at(0);
        LevelData level;
        level.identifier = lv.at("identifier").get<std::string>();
        level.width = lv.at("pxWid").get<int>();
        level.height = lv.at("pxHei").get<int>();
        level.bgColor =
            ParseHexColor(lv.value("__bgColor", root.value("bgColor", "")), sourceName);

        // LDtk の layerInstances は先頭が最前面。LevelData は奥→手前の契約なので
        // 逆順に走査して正規化する。
        const json& layerInstances = lv.value("layerInstances", json::array());
        for (auto it = layerInstances.rbegin(); it != layerInstances.rend(); ++it) {
            const json& li = *it;
            const std::string type = li.at("__type").get<std::string>();
            const std::string identifier = li.at("__identifier").get<std::string>();
            if (!li.value("visible", true)) {
                log::Info("LdtkLoader: layer '{}' is hidden — skipped ({})", identifier,
                          sourceName);
                continue;
            }

            if (type == "IntGrid" || type == "AutoLayer") {
                if (type == "IntGrid") {
                    LevelIntGrid grid;
                    grid.identifier = identifier;
                    grid.width = li.at("__cWid").get<int>();
                    grid.height = li.at("__cHei").get<int>();
                    grid.gridSize = li.at("__gridSize").get<int>();
                    grid.values = li.at("intGridCsv").get<std::vector<int>>();
                    level.intGrids.push_back(std::move(grid));
                }
                // IntGrid はオートタイルルール付きなら描画タイルも持つ。
                const json& tiles = li.value("autoLayerTiles", json::array());
                if (!tiles.empty()) {
                    if (auto layer = ParseTileLayer(li, tiles, sourceName)) {
                        level.tileLayers.push_back(std::move(*layer));
                    } else {
                        log::Warn("LdtkLoader: layer '{}' has tiles but no tileset — "
                                  "skipped ({})",
                                  identifier, sourceName);
                    }
                }
            } else if (type == "Tiles") {
                const json& tiles = li.value("gridTiles", json::array());
                if (auto layer = ParseTileLayer(li, tiles, sourceName)) {
                    level.tileLayers.push_back(std::move(*layer));
                } else {
                    log::Warn("LdtkLoader: layer '{}' has no tileset — skipped ({})",
                              identifier, sourceName);
                }
            } else if (type == "Entities") {
                for (const auto& ei : li.at("entityInstances")) {
                    LevelEntity entity;
                    entity.identifier = ei.at("__identifier").get<std::string>();
                    entity.x = static_cast<float>(ei.at("px").at(0).get<int>());
                    entity.y = static_cast<float>(ei.at("px").at(1).get<int>());
                    entity.width = ei.value("width", 0);
                    entity.height = ei.value("height", 0);
                    level.entities.push_back(std::move(entity));
                }
            } else {
                log::Warn("LdtkLoader: unknown layer type '{}' ('{}') — skipped ({})",
                          type, identifier, sourceName);
            }
        }
        return level;
    } catch (const std::exception& e) {
        // json の型不一致・キー欠落・パース失敗はすべてここへ来る。
        return std::unexpected(std::format("{}: LDtk parse error: {}", sourceName, e.what()));
    }
}

} // namespace witch::ldtk
