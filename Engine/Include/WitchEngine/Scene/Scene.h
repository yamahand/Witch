#pragma once
#include "WitchEngine/Scene/ComponentScheduler.h"
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/GameObject.h"
#include <memory>
#include <string_view>
#include <vector>
#ifdef WITCH_DEBUG_UI
#include <span>
#endif

namespace witch {

/// オブジェクト所有ツリーの根。1 シーン = 1 画面を表す。
/// DrawDebugUI() は DebugUI 基底から継承する（WITCH_DEBUG_UI 定義時のみ存在）。
class Scene : public DebugUI {
public:
    virtual ~Scene() = default;

    /// シーンがアクティブになる直前に呼ばれる。
    virtual void OnEnter() {}
    /// シーンが非アクティブになる直前に呼ばれる。
    virtual void OnExit() {}

    /// 固定タイムステップ 1 回分の更新。GameLoop がフレーム内で 0〜N 回呼ぶ
    /// （dt は常に Time::FixedDeltaTime）。順序は厳守:
    /// 生成反映 → PreUpdate → GameObject::Update フック → Update → PostUpdate。
    /// ゲームロジック（と将来の Physics 系フェーズ）はこちらで回す。
    /// サブクラスがオーバーライドする場合は必ず Scene::FixedUpdate(fixedDt) を呼ぶ。
    /// 注: エッジ入力（WasPressed 等）は多重ステップフレームで二重発火するため
    /// FrameUpdate 側で読むこと（入力世代の更新はフレームに 1 回のため）。
    virtual void FixedUpdate(float fixedDt);

    /// フレームごとの更新。GameLoop が全 FixedUpdate の後に必ず 1 回呼ぶ
    /// （dt は可変のフレーム経過時間）。順序は厳守:
    /// 生成反映 → Animation → Camera → Render → 破棄回収。
    /// 生成反映を先頭で再度行うのは、固定ステップ 0 回のフレーム（高リフレッシュ
    /// レート環境）でも OnEnter 中の Spawn が同フレームの描画に乗るようにするため。
    /// 破棄回収がフレーム末（= ここの末尾）である契約は従来どおり。
    /// サブクラスがオーバーライドする場合は必ず Scene::FrameUpdate(dt) を呼ぶ。
    virtual void FrameUpdate(float dt);

#ifdef WITCH_DEBUG_UI
    /// 生存オブジェクトの DrawDebugUI（毎フレーム自由描画フック）を呼ぶ。
    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）に呼ぶこと。
    /// インスペクター表示（DrawInspector）はここではなく HierarchyWindow が
    /// 選択中オブジェクトに対してのみ行う。
    void DrawDebugUI() override;

    /// HierarchyWindow が GameObject を列挙するための読み取り専用ビュー。
    /// pendingSpawn_（未反映）は含まない。
    std::span<const std::unique_ptr<GameObject>> DebugObjects() const { return objects_; }
#endif

    /// 更新中に呼んでも安全。保留リストに積み、次の生成フェーズで反映する。
    template<typename T, typename... Args>
    T* Spawn(Args&&... args);

    /// ObjectId で線形探索する。現状 O(n) で許容している（最適化は必要になってから）。
    /// 見つからなければ nullptr を返す。
    GameObject* Find(ObjectId id) const;

    /// ObjectRegistry 経由でレベルファイルを実体化する（M6 実装予定）。
    void LoadLevel(std::string_view path);

private:
    /// GameObject::RegisterComponent（スポーン後の AddComponent）が scheduler_ に触るため。
    friend class GameObject;

    static ObjectId NextId();

    /// 生成反映（Stage 1）: pendingSpawn_ を OnSpawn → scheduler 登録 → objects_ へ移す。
    /// FixedUpdate / FrameUpdate 両方の先頭で呼ばれる。
    void FlushPendingSpawns();
    /// 破棄回収（Stage 3）: Destroyed なオブジェクトを scheduler から外し
    /// OnDespawn → delete する。FrameUpdate の末尾（= フレーム末）で呼ばれる。
    void CollectDestroyed();

    ComponentScheduler scheduler_;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::vector<std::unique_ptr<GameObject>> pendingSpawn_;
};

// ── Template implementation ──────────────────────────────────────────────────

template<typename T, typename... Args>
T* Scene::Spawn(Args&&... args) {
    static_assert(std::is_base_of_v<GameObject, T>,
                  "T must derive from witch::GameObject");
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    obj->id_ = NextId();
    obj->scene_ = this;
    T* ptr = obj.get();
    pendingSpawn_.push_back(std::move(obj));
    return ptr;
}

} // namespace witch
