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

    /// シーンをアクティブにする非仮想の入口（Engine が呼ぶ。OnEnter を直接呼ばない）。
    /// OnEnter の間だけ Spawn が即時反映モードになる: Spawn<T>() の呼び出し中に
    /// OnSpawn まで完了し、直後から Find が通る（更新イテレーション外なので
    /// objects_ へ即時追加しても安全）。LoadLevel 直後のオブジェクト間配線を
    /// OnEnter 内で書けるようにするための契約（RefactoringNotes §8）。
    void Enter();
    /// シーンを非アクティブにする非仮想の入口（Engine が呼ぶ）。
    /// 現状 OnExit を呼ぶだけだが、Enter と対で入口を揃えておく。
    void Exit();

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

    /// GameObject を生成する。引数は T のコンストラクタへ完全転送される。
    /// 反映タイミングは 2 モード（どちらでも OnSpawn が見る状態が同じになるよう、
    /// OnSpawn は GameObject.h の自己完結契約に従うこと）:
    /// - OnEnter 中（Enter() 経由）: 即時反映。戻り時に OnSpawn 完了済みで Find が通る。
    /// - それ以外（更新中含む）: 保留リストに積み、次の生成フェーズで反映する
    ///   （更新イテレーション中の objects_ 変更を避けるため）。
    template<typename T, typename... Args>
    T* Spawn(Args&&... args);

    /// ObjectId で線形探索する。現状 O(n) で許容している（最適化は必要になってから）。
    /// 見つからなければ nullptr を返す。
    GameObject* Find(ObjectId id) const;

    /// ObjectRegistry 経由でレベルファイルを実体化する（M6 実装予定）。
    void LoadLevel(std::string_view path);

protected:
    /// シーンがアクティブになる直前に呼ばれる（Enter() 経由。直接呼ばない）。
    /// ここでの Spawn は即時反映される（Spawn の項参照）。
    virtual void OnEnter() {}
    /// シーンが非アクティブになる直前に呼ばれる（Exit() 経由。直接呼ばない）。
    virtual void OnExit() {}

private:
    /// GameObject::RegisterComponent（スポーン後の AddComponent）が scheduler_ に触るため。
    friend class GameObject;

    static ObjectId NextId();

    /// 1 体分の反映処理: OnSpawn → scheduler 登録 → objects_ へ移す。
    /// FlushPendingSpawns と即時モードの Spawn の両方から使う（挙動の単一ソース）。
    void CommitSpawn(std::unique_ptr<GameObject> obj);
    /// 生成反映（Stage 1）: pendingSpawn_ を CommitSpawn で objects_ へ移す。
    /// FixedUpdate / FrameUpdate 両方の先頭で呼ばれる。
    void FlushPendingSpawns();
    /// 破棄回収（Stage 3）: Destroyed なオブジェクトを scheduler から外し
    /// OnDespawn → delete する。FrameUpdate の末尾（= フレーム末）で呼ばれる。
    void CollectDestroyed();

    ComponentScheduler scheduler_;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::vector<std::unique_ptr<GameObject>> pendingSpawn_;
    bool inEnter_ = false;  ///< OnEnter 実行中（= Spawn 即時反映モード）。Enter() が管理。
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
    if (inEnter_) {
        // OnEnter 中は更新イテレーションが走っていないため即時反映できる。
        // OnSpawn 内の入れ子 Spawn はここへ同期再帰する（イテレーション無しで安全）。
        CommitSpawn(std::move(obj));
    } else {
        pendingSpawn_.push_back(std::move(obj));
    }
    return ptr;
}

} // namespace witch
