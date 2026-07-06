# 改修候補メモ — 現状実装レビュー（M4 完了時点）

RemainingWork.md の機能追加を進める前後で直しておきたい箇所。
「いつ直すべきか」を優先度の代わりに書いている（早すぎる一般化はしない方針のため、
トリガーとなる機能実装より前に直す必要はない）。

---

## 今すぐ直してよいもの（小さく、放置すると事故る）

### 1. Engine::Init が失敗を握りつぶす — ✅ 対応済み（2026-07-03）
`Engine::Init` が `std::expected<void, std::string>` を返すようになり、
ウィンドウ生成失敗・レンダラ初期化失敗をエラーとして返す。
`Application::Run` が失敗時に `platform::ShowErrorDialog`（MessageBox）で表示して
終了コード 1 を返す（Main は `return game.Run()`）。
GameLoop の `if (renderer_)` null 分岐は「Init 成功時のみ生成される」前提で撤去し、
コンストラクタの assert に置き換えた（headless 実行が必要になったらその時に戻す）。

### 2. dt のスパイク対策がない — ✅ 対応済み（本ノート作成時点で実装済みだった）
[Time.cpp](../Engine/Source/Core/Time.cpp) の `Tick` が `kMaxDelta = 0.25f` で
クランプ済み（ノート作成時に Time.h だけ見て実装を見落としていた）。
本命の固定タイムステップ化（RemainingWork.md §3, M6 予定）は引き続き残作業。
クランプは固定ステップ導入後もスパイラル防止として残す。

## 描画拡張（M5）の前に直すもの

### 3. スプライト上限 1024 超過が黙って捨てられる
[D3D12Renderer.h:25](../Engine/Source/Rhi/D3D12/D3D12Renderer.h) —
`kMaxSpritesPerFrame = 1024`。タイルマップを 1 タイル = 1 SubmitSprite で描くと
1 画面分のタイル（視界が横 20〜30 タイル程度なら数百枚）+ 前後景レイヤーで簡単に超える。
超過時は無警告で描画されない（原因調査が地獄になるタイプの不具合）。

- 最低限: 超過時に一度だけ警告ログを出す。
- M6 のタイルマップ実装時: 上限引き上げ（頂点バッファを大きく取るだけ）と、
  カメラ範囲カリングをタイルマップ側で行う設計にする。

### 4. 描画順が「オブジェクト生成順」に暗黙依存
[SpriteComponent.cpp:45](../Engine/Source/Graphics2D/SpriteComponent.cpp) —
`SubmitSprite` は Scene::Update 中に呼ばれ、描画順 = objects_ の並び順で決まる。
背景 / キャラ / UI の前後関係を保証できない。

- `SpriteDrawDesc` にソートキー（layer + 層内順）を追加し、
  `DoFlushSprites` 前に安定ソートする。SubmitSprite の蓄積型の設計は
  そのまま活きるので変更は小さい。
- 更新（ロジック）と描画提出を分離する大掛かりなリファクタ
  （`Component::Render()` フェーズ新設など）は今はしない。
  ソートキーで足りる間はソートキーで済ませる（pragmatic 路線）。
- **追記（ComponentScheduler 導入後）**: フェーズ分け（PreUpdate〜Render）は
  ComponentScheduler として導入済み。ただし動機は物理（M7）・カメラ追従（M8）の
  フレーム内順序保証であって描画順ではない。描画の前後関係の保証は引き続き
  ソートキー（`SetLayer`）で行う。同一フェーズ内の実行順は契約上未規定なので、
  同レイヤーの前後関係を更新順に頼らないこと。

## タイルマップ / リソースが増える（M6）までに直すもの

### 5. テクスチャ解放経路が存在しない
[ResourceManager.cpp](../Engine/Source/Core/ResourceManager.cpp) —
`textureCache_` は積む一方で、`IRenderer::DestroyTexture` を呼ぶコードがどこにもない。
かつ [D3D12Renderer.h:21](../Engine/Source/Rhi/D3D12/D3D12Renderer.h) の
`kMaxTextures = 64` は固定でスロット再利用もされないため、
マップを跨いでテクスチャを読み続けるといずれ CreateTexture が失敗する。

- ResourceManager に `UnloadAll()`（or シーン遷移時の一括解放）を追加し、
  Engine のシーン切替（`ApplyPendingSceneChange`）で呼ぶのが最小構成。
  共有アセット（プレイヤー等）を残したくなったら参照カウント化を検討。
- `kMaxTextures` はアトラス運用なら 64 で足りる可能性が高い。
  枯渇時に明確なエラーを返すことだけ確認しておく。

### 6. カメラ変換が CPU・スプライト毎
[SpriteComponent.cpp:35](../Engine/Source/Graphics2D/SpriteComponent.cpp) —
WorldToScreen をスプライト 1 枚ごとに CPU で適用している。
数十枚なら問題ないが、タイル数百〜千枚では無駄が大きい。

- M4 の設計メモどおり、ビュー変換（カメラのオフセット + ズーム）を
  定数バッファに入れて頂点シェーダ側で適用する形へ移行する。
  RHI 境界は `SetCamera(offset, zoom)` 相当の抽象を 1 つ足すだけで守れる。
- 移行タイミングはタイルマップ実装（M6）と同時が効率的。
  それまでは現状維持でよい。

## 仕様とのズレ・小さな改善（機会があれば）

### 7. Services に audio メンバが無い
[Services.h](../Engine/Include/WitchEngine/Core/Services.h) —
CLAUDE.md のコア設計では `renderer / audio / input / resources / time` の
固定メンバとされているが、`audio` が未実装のため存在しない。
M8（オーディオ）で `IAudio` と一緒に追加する。ズレとして認識だけしておく。

### 8. Spawn の 1 フレーム遅延が初期化を複雑にする
[EmptyScene.cpp:58](../Game/Source/Scenes/EmptyScene.cpp) のコメントにある通り、
`OnEnter` で Spawn したオブジェクトは最初の Update まで `Find` できない。
遅延 Spawn の設計自体は正しい（更新中の安全のため）が、
「シーン開始直後」だけは反映済みであってほしい場面が今後増える
（レベルロード直後にカメラをプレイヤーへ向ける等）。

- `Scene::OnEnter` 呼び出しの直後（Engine::ApplyPendingSceneChange 内）に
  生成反映フェーズだけを 1 回回す `FlushPendingSpawns()` を用意する案。
  LoadLevel 実装（M6）時に一緒に入れると綺麗。

### 9. クリア色が GameLoop にハードコード
[GameLoop.cpp:13](../Engine/Source/Core/GameLoop.cpp) — kCornflowerBlue 固定。
マップごとの背景色（洞窟物語は黒背景が基本）を出すため、
Scene かカメラから指定できるようにする。数行の変更なのでいつでも可。

### 10. GetComponent が dynamic_cast の線形探索
[GameObject.h:79](../Engine/Include/WitchEngine/Scene/GameObject.h) —
現状の規模では問題ないが、毎フレームのホットパス
（衝突応答で相手の Component を引く等）で多用し始めると効きだす。
「Update 内で毎フレーム GetComponent しない（OnSpawn/OnAttach でポインタを
キャッシュする）」を規約にしておけば、当面この実装のままでよい。
最適化（型 ID 化等）はプロファイルで問題が出てから。

### 11. テストが 0 件
CI は `noTestsAction: ignore` で緑になっている。エンジンが「壊れると全部壊れる」
コア（Scene の 3 段階更新・遅延破棄、AABB 衝突解決、固定タイムステップ）を
持ち始める M6〜M7 あたりで、描画に依存しないロジックだけでも
ユニットテストを導入する価値が出てくる（衝突解決は特にテストと相性が良い）。

---

## 直さなくてよいと判断したもの（記録として）

- **Scene::Find の線形探索** — CLAUDE.md の未決事項どおり「必要になってから」。
- **Engine の Meyers Singleton** — 設計意図どおり（サービスの生成順は Engine が握る）。
- **Camera2D の std::clamp 回避**（windows.h min/max マクロ対策）— 意図的な回避で妥当。
- **入力の 2 スナップショット方式と Update 順序** — コメントに順序契約が明記されており健全。
- **例外を境界で expected に翻訳する方針** — ResourceManager / IRenderer で守られている。
