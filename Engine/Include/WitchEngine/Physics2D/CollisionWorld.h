#pragma once
#include <vector>

namespace witch {

class CollisionComponent;
struct LevelIntGrid;

/// 衝突デバッグ表示（コライダー AABB + IntGrid の衝突形状）のグローバルスイッチ。
/// DebugMenu の "Show Collision" が切り替える。既定は OFF。
void SetCollisionDebugDrawEnabled(bool enabled);
bool CollisionDebugDrawEnabled();

/// シーン内の全 CollisionComponent の非所有レジストリと、エンティティ同士の
/// 重なり検出（トリガー。物理的押し戻しはしない）。Scene が値で 1 つ所有する。
///
/// 検出は per-object のフェーズモデルに合わない「全体パス」なので、Scene::FixedUpdate
/// が Physics フェーズ（全コライダーの移動確定）の直後に DetectOverlaps →
/// DispatchCallbacks を直接呼ぶ（FlushPendingSpawns と同格のシーン内ステージ）。
///
/// 総当たり O(n²) は登場エンティティ数十体の想定で意図的に許容している
/// （pragmatic 方針。空間分割はプロファイルで問題になってから）。
class CollisionWorld {
public:
    /// 登録（重複は無視）。CollisionComponent が初回 Update で遅延登録する。
    void Register(CollisionComponent* c);
    /// 解除。CollisionComponent::OnDetach（~GameObject 経由）が呼ぶ。
    void Unregister(CollisionComponent* c);

    /// 全コライダーの接触リストをクリアし、AABB 重なりを総当たりで検出して
    /// (self.mask & other.layer) が立つ側の接触リストへ積む（非対称可:
    /// 弾は敵に当たりたいが、敵は弾を無視できる）。
    /// 破棄フラグ済み・同一 GameObject 上のコライダー同士はスキップする。
    void DetectOverlaps();

    /// 接触リスト確定後にコールバックを発火する（検出パス中にゲームロジックを
    /// 走らせないための 2 段構え）。コールバック内の Destroy（遅延）/ Spawn（保留）
    /// は既存契約どおり安全。dispatch 中に破棄フラグが立っても、既に記録された
    /// 接触の発火はスキップしない（弾が自壊しても敵側の被弾処理は同ステップで走る）。
    /// 発火順は未規定（フェーズ内順序未規定の契約と同型。順序に依存しないこと）。
    void DispatchCallbacks();

    /// 衝突デバッグ表示（"Show Collision"）。全コライダーの AABB と、grid の
    /// 衝突形状（ソリッド = 矩形、坂 = 対角線）をカメラ可視範囲だけ DebugDraw で
    /// 描く。Scene::FrameUpdate が毎フレーム呼ぶ（トグル OFF 時は即 return。
    /// WITCH_DEBUG_DRAW が OFF のビルドでは DebugDraw 側が no-op）。
    /// @param grid 衝突用 IntGrid（無ければ nullptr = コライダーのみ描く）
    void DrawDebug(const LevelIntGrid* grid) const;

private:
    std::vector<CollisionComponent*> colliders_;  ///< 非所有（所有は GameObject 側）。
};

} // namespace witch
