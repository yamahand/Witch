#include "WitchEngine/Physics2D/CollisionWorld.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Debug/DebugDraw.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Level/LevelData.h"
#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Physics2D/TileCollision.h"
#include "WitchEngine/Scene/GameObject.h"
#include <algorithm>
#include <cmath>

namespace witch {

namespace {

bool sCollisionDebugDraw = false;

// デバッグ表示の配色。
constexpr rhi::Color kTileColor{1.0f, 0.5f, 0.0f, 0.9f};     // ソリッド・坂（橙）
constexpr rhi::Color kBodyColor{0.0f, 1.0f, 0.0f, 1.0f};     // 通常コライダー（緑）
constexpr rhi::Color kTriggerColor{0.0f, 1.0f, 1.0f, 1.0f};  // トリガー専用（シアン）
constexpr rhi::Color kContactColor{1.0f, 0.0f, 0.0f, 1.0f};  // 接触中（赤）
constexpr rhi::Color kGroundColor{1.0f, 1.0f, 0.0f, 1.0f};   // 接地マーカー（黄）

} // namespace

void SetCollisionDebugDrawEnabled(bool enabled) {
    sCollisionDebugDraw = enabled;
}

bool CollisionDebugDrawEnabled() {
    return sCollisionDebugDraw;
}

void CollisionWorld::Register(CollisionComponent* c) {
    if (std::ranges::find(colliders_, c) == colliders_.end()) {
        colliders_.push_back(c);
    }
}

void CollisionWorld::Unregister(CollisionComponent* c) {
    std::erase(colliders_, c);
}

void CollisionWorld::DetectOverlaps() {
    for (CollisionComponent* c : colliders_) {
        c->contacts_.clear();
    }
    for (size_t i = 0; i < colliders_.size(); ++i) {
        CollisionComponent* a = colliders_[i];
        if (a->Owner()->IsDestroyed()) {
            continue;  // 検出前に破棄済みのものは接触に載せない
        }
        const Aabb boxA = a->WorldAabb();
        for (size_t j = i + 1; j < colliders_.size(); ++j) {
            CollisionComponent* b = colliders_[j];
            if (b->Owner()->IsDestroyed()) {
                continue;
            }
            if (a->Owner() == b->Owner()) {
                continue;  // 同一 GameObject 上のコライダー同士は対象外
            }
            if (!boxA.Overlaps(b->WorldAabb())) {
                continue;
            }
            if ((a->Mask() & b->Layer()) != 0) {
                a->contacts_.push_back({b->Owner()->Id(), b});
            }
            if ((b->Mask() & a->Layer()) != 0) {
                b->contacts_.push_back({a->Owner()->Id(), a});
            }
        }
    }
}

void CollisionWorld::DrawDebug(const LevelIntGrid* grid) const {
    if (!sCollisionDebugDraw) {
        return;
    }
    debug::DebugDraw* dd = Services::Instance().debugDraw;
    if (dd == nullptr) {
        return;
    }

    // ── IntGrid の衝突形状（カメラ可視範囲のセルのみ。TilemapComponent と同じ
    //    カリング。cameras 未登録時は全セル） ──
    if (grid != nullptr && grid->gridSize > 0) {
        const float gs = static_cast<float>(grid->gridSize);
        int cxMin = 0, cyMin = 0;
        int cxMax = grid->width - 1, cyMax = grid->height - 1;
        if (auto* cameras = Services::Instance().cameras) {
            const Camera2D& cam = cameras->Active();
            const float viewMinX = cam.ScreenToWorldX(0.0f);
            const float viewMinY = cam.ScreenToWorldY(0.0f);
            const float viewMaxX = cam.ScreenToWorldX(cam.ViewportWidth());
            const float viewMaxY = cam.ScreenToWorldY(cam.ViewportHeight());
            cxMin = std::max(cxMin, static_cast<int>(std::floor(viewMinX / gs)));
            cyMin = std::max(cyMin, static_cast<int>(std::floor(viewMinY / gs)));
            cxMax = std::min(cxMax, static_cast<int>(std::floor(viewMaxX / gs)));
            cyMax = std::min(cyMax, static_cast<int>(std::floor(viewMaxY / gs)));
        }
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            for (int cx = cxMin; cx <= cxMax; ++cx) {
                const size_t index = static_cast<size_t>(cy) *
                                         static_cast<size_t>(grid->width) +
                                     static_cast<size_t>(cx);
                if (index >= grid->values.size()) {
                    continue;
                }
                const float x = static_cast<float>(cx) * gs;
                const float y = static_cast<float>(cy) * gs;
                switch (physics2d::ShapeFromValue(grid->values[index])) {
                    case physics2d::TileShape::Solid:
                        dd->Rect(x, y, gs, gs, kTileColor);
                        break;
                    case physics2d::TileShape::SlopeUpRight:  // '/' 左下 → 右上
                        dd->Line(x, y + gs, x + gs, y, kTileColor);
                        dd->Line(x, y + gs, x + gs, y + gs, kTileColor);  // 底辺
                        break;
                    case physics2d::TileShape::SlopeUpLeft:   // '\' 左上 → 右下
                        dd->Line(x, y, x + gs, y + gs, kTileColor);
                        dd->Line(x, y + gs, x + gs, y + gs, kTileColor);  // 底辺
                        break;
                    case physics2d::TileShape::Empty:
                        break;
                }
            }
        }
    }

    // ── コライダー AABB ──
    for (const CollisionComponent* c : colliders_) {
        if (c->Owner()->IsDestroyed()) {
            continue;
        }
        const Aabb box = c->WorldAabb();
        const rhi::Color color = !c->Contacts().empty() ? kContactColor
                                 : c->SolidVsTiles()    ? kBodyColor
                                                        : kTriggerColor;
        dd->Rect(box.x, box.y, box.w, box.h, color);
        if (c->OnGround()) {
            // 接地中は下辺のすぐ下に黄線（接地状態の目視確認用）。
            dd->Line(box.x, box.Bottom() + 1.0f, box.Right(), box.Bottom() + 1.0f,
                     kGroundColor);
        }
    }
}

void CollisionWorld::DispatchCallbacks() {
    // colliders_ は dispatch 中に変化しない前提で直接イテレートする:
    // コールバック内の Spawn は保留リスト行き（新コライダーの登録は初回 Update =
    // 次ステップの Physics フェーズ）、Destroy は遅延フラグのみで Unregister は
    // フレーム末（~GameObject → OnDetach）まで起きない。
    for (CollisionComponent* c : colliders_) {
        if (!c->overlapCallback_) {
            continue;
        }
        // 破棄フラグ済みでもスキップしない: 既に記録された接触の発火は保証する
        // （弾がコールバック内で自壊しても、相手側の被弾処理は同ステップで走る）。
        for (const CollisionContact& contact : c->contacts_) {
            c->overlapCallback_(contact);
        }
    }
}

} // namespace witch
