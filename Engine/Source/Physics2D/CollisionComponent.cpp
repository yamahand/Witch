#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Physics2D/TileCollision.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"

namespace witch {

CollisionComponent::CollisionComponent(float width, float height,
                                       float offsetX, float offsetY)
    : width_(width), height_(height), offsetX_(offsetX), offsetY_(offsetY) {}

Aabb CollisionComponent::WorldAabb() const {
    const Transform& t = Owner()->transform;
    return Aabb{t.x + offsetX_ - width_ * 0.5f,
                t.y + offsetY_ - height_ * 0.5f,
                width_, height_};
}

void CollisionComponent::RefreshGridCache() {
    const LevelData* level = Owner()->GetScene()->CurrentLevel();
    if (level == cachedLevel_) {
        return;
    }
    cachedLevel_ = level;
    grid_ = level ? physics2d::FindCollisionGrid(*level) : nullptr;
}

void CollisionComponent::Update(float dt) {
    RefreshGridCache();

    Aabb box = WorldAabb();
    const float dx = velX_ * dt;
    const float dy = velY_ * dt;

    if (solidVsTiles_ && grid_ != nullptr) {
        const physics2d::MoveResult r = physics2d::MoveAabb(*grid_, box, dx, dy);
        onGround_ = r.onGround;
        hitHead_ = r.hitHead;
        hitLeft_ = r.hitLeft;
        hitRight_ = r.hitRight;
        // 遮られた軸の速度成分は 0 に（着地で落下停止・壁で水平停止）。
        if (r.hitLeft || r.hitRight) {
            velX_ = 0.0f;
        }
        if (r.hitHead || r.onGround) {
            velY_ = 0.0f;
        }
        box.x = r.x;
        box.y = r.y;
    } else {
        box.x += dx;
        box.y += dy;
        onGround_ = false;
        hitHead_ = false;
        hitLeft_ = false;
        hitRight_ = false;
    }

    // AABB 左上 → transform（AABB 中心 - offset）へ書き戻し。
    Transform& t = Owner()->transform;
    t.x = box.x + width_ * 0.5f - offsetX_;
    t.y = box.y + height_ * 0.5f - offsetY_;
}

} // namespace witch
