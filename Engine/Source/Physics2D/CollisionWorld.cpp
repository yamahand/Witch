#include "WitchEngine/Physics2D/CollisionWorld.h"
#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include <algorithm>

namespace witch {

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
