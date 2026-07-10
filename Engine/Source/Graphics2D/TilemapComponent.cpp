#include "WitchEngine/Graphics2D/TilemapComponent.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Graphics2D/LayerSortKey.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/GameObject.h"
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

TilemapComponent::TilemapComponent(const TextureInfo& texture, const LevelTileLayer& layer)
    : texture_(texture), tileSize_(static_cast<float>(layer.gridSize)) {
    // タイル座標・UV・不透明度をここで全解決し、毎フレームの Update は
    // カリング + 提出だけにする（データはロード後不変）。
    const float tw = static_cast<float>(texture_.width);
    const float th = static_cast<float>(texture_.height);
    tiles_.reserve(layer.tiles.size());
    bool first = true;
    for (const LevelTile& src : layer.tiles) {
        Tile tile;
        tile.x = static_cast<float>(src.x + layer.offsetX);
        tile.y = static_cast<float>(src.y + layer.offsetY);
        float u0 = static_cast<float>(src.srcX) / tw;
        float v0 = static_cast<float>(src.srcY) / th;
        float u1 = static_cast<float>(src.srcX + layer.gridSize) / tw;
        float v1 = static_cast<float>(src.srcY + layer.gridSize) / th;
        // フリップは UV スワップで焼き込む（SpriteComponent の SetFlip と同じ仕組み）。
        tile.u0 = src.flipX ? u1 : u0;
        tile.u1 = src.flipX ? u0 : u1;
        tile.v0 = src.flipY ? v1 : v0;
        tile.v1 = src.flipY ? v0 : v1;
        tile.alpha = src.alpha * layer.opacity;

        if (first) {
            minX_ = tile.x;
            minY_ = tile.y;
            maxX_ = tile.x + tileSize_;
            maxY_ = tile.y + tileSize_;
            first = false;
        } else {
            if (tile.x < minX_) minX_ = tile.x;
            if (tile.y < minY_) minY_ = tile.y;
            if (tile.x + tileSize_ > maxX_) maxX_ = tile.x + tileSize_;
            if (tile.y + tileSize_ > maxY_) maxY_ = tile.y + tileSize_;
        }
        tiles_.push_back(tile);
    }
}

void TilemapComponent::Update([[maybe_unused]] float dt) {
    auto* renderer = Services::Instance().renderer;
    if (!renderer || !texture_.IsValid() || tiles_.empty()) return;

    const Transform& t = Owner()->transform;

    // カメラの可視ワールド矩形。カメラは回転しないため AABB で正確に判定できる。
    // cameras 未登録時はカリングせず全提出する。
    float viewMinX = minX_ + t.x, viewMinY = minY_ + t.y;
    float viewMaxX = maxX_ + t.x, viewMaxY = maxY_ + t.y;
    if (auto* cameras = Services::Instance().cameras) {
        const Camera2D& cam = cameras->Active();
        viewMinX = cam.ScreenToWorldX(0.0f);
        viewMinY = cam.ScreenToWorldY(0.0f);
        viewMaxX = cam.ScreenToWorldX(cam.ViewportWidth());
        viewMaxY = cam.ScreenToWorldY(cam.ViewportHeight());
        // レイヤー全体の AABB が可視矩形外なら早期リジェクト。
        if (minX_ + t.x > viewMaxX || maxX_ + t.x < viewMinX ||
            minY_ + t.y > viewMaxY || maxY_ + t.y < viewMinY) {
#ifdef WITCH_DEBUG_UI
            lastSubmitted_ = 0;
#endif
            return;
        }
    }

    const uint32_t sortKey = LayerToSortKey(layer_);
    int submitted = 0;
    for (const Tile& tile : tiles_) {
        const float x = t.x + tile.x;
        const float y = t.y + tile.y;
        if (x > viewMaxX || x + tileSize_ < viewMinX ||
            y > viewMaxY || y + tileSize_ < viewMinY) {
            continue;
        }
        renderer->SubmitSprite({
            .texture = texture_.handle,
            .x = x,
            .y = y,
            .width = tileSize_,
            .height = tileSize_,
            .u0 = tile.u0, .v0 = tile.v0, .u1 = tile.u1, .v1 = tile.v1,
            .color = {1.0f, 1.0f, 1.0f, tile.alpha},
            .space = rhi::SpriteSpace::World,
            .sortKey = sortKey,
        });
        ++submitted;
    }
#ifdef WITCH_DEBUG_UI
    lastSubmitted_ = submitted;
#else
    (void)submitted;
#endif
}

#ifdef WITCH_DEBUG_UI
void TilemapComponent::DrawInspector() {
    ImGui::Text("texture: id=%u (%dx%d)", texture_.handle.id, texture_.width,
                texture_.height);
    ImGui::Text("tiles: %zu (submitted last frame: %d)", tiles_.size(), lastSubmitted_);
    ImGui::Text("tileSize: %.0f  bounds: (%.0f, %.0f)-(%.0f, %.0f)", tileSize_, minX_,
                minY_, maxX_, maxY_);
    int layer = layer_;
    if (ImGui::DragInt("layer", &layer, 1.0f, -32768, 32767))
        layer_ = static_cast<int16_t>(layer);
}
#endif

} // namespace witch
