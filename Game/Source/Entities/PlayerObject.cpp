#include "Entities/PlayerObject.h"
#include "Components/PlayerControllerComponent.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/ObjectRegistry.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Physics2D/CollisionComponent.h"

namespace witch {

namespace {
/// 描画サイズ px（8px タイル基準の 2 タイル分。アート方針決定までの仮置き）。
constexpr float kSpriteSize = 16.0f;
constexpr const char* kTexturePath = "Witch.png";
} // namespace

PlayerObject::PlayerObject(float x, float y) {
    transform.x = x;
    transform.y = y;
}

void PlayerObject::OnSpawn() {
    SetName("Player");

    // AddComponent の順序契約: PlayerControllerComponent が OnAttach で兄弟
    // （Collision / Sprite）をキャッシュするため、Controller を必ず最後に足す
    // （AnimationComponent が Sprite を先に要求するのと同じ前例）。
    if (auto texture = Services::Instance().resources->LoadTexture(kTexturePath)) {
        AddComponent<SpriteComponent>(*texture, kSpriteSize, kSpriteSize);
    } else {
        // 描画は無くてもプレイ可能なので続行（コントローラは sprite 無しを許容する）。
        log::Error("PlayerObject: failed to load {}: {}", kTexturePath, texture.error());
    }
    AddComponent<CollisionComponent>(kPlayerHitboxWidth, kPlayerHitboxHeight);
    AddComponent<PlayerControllerComponent>();
}

/// レベルエディタの identifier "Player" で LoadLevel から実体化できるようにする。
WITCH_REGISTER_OBJECT_AS(PlayerObject, "Player");

} // namespace witch
