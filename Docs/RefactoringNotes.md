# 改修候補メモ — 現状実装レビュー（初版 M4 完了時点 / 最終見直し M5 完了時点）

RemainingWork.md の機能追加を進める前後で直しておきたい箇所。
「いつ直すべきか」を優先度の代わりに書いている（早すぎる一般化はしない方針のため、
トリガーとなる機能実装より前に直す必要はない）。

**現状: M5 完了。次の実装対象は §3 固定タイムステップ → M6 タイルマップ / レベルロード。**
以下、M5 で解消済みの項目に対応済みマークを付け、未対応項目のトリガー時期を再確認した
（2026-07-08 見直し）。

## ステータス一覧（2026-07-08 見直し）

| #  | 項目 | 状態 | 対応時期 |
|----|------|------|----------|
| 1  | Engine::Init が失敗を握りつぶす | ✅ 済 | — |
| 2  | dt のスパイク対策 | ✅ 済（クランプ） | 固定ステップは §3 で |
| 3  | スプライト上限超過が黙って捨てられる | △ 警告は済 / 上限引き上げ残 | M6 直前 |
| 4  | 描画順がオブジェクト生成順に暗黙依存 | ✅ 済（SortKey） | — |
| 5  | テクスチャ解放経路が無い | ⬜ 未 | **M6 直前（近々）** |
| 6  | カメラ変換が CPU・スプライト毎 | ⬜ 未 | **M6 と同時（近々）** |
| 7  | Services に audio メンバが無い | ⬜ 未 | M8 |
| 8  | Spawn の 1 フレーム遅延 | ⬜ 未 | M6 の LoadLevel と同時 |
| 9  | クリア色が GameLoop にハードコード | ⬜ 未 | いつでも可 / 遅くとも M6 |
| 10 | GetComponent が dynamic_cast 線形探索 | ✅ 済（型IDタグ方式） | — |
| 11 | テストが 0 件 | ⬜ 未 | **§3 固定ステップで起点（適期接近）** |

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

### 3. スプライト上限 1024 超過 — △ 一部対応済み（警告ログは実装済み。上限引き上げは M6 残）
[D3D12Renderer.h:25](../Engine/Source/Rhi/D3D12/D3D12Renderer.h) —
`kMaxSpritesPerFrame = 1024`。タイルマップを 1 タイル = 1 SubmitSprite で描くと
1 画面分のタイル（視界が横 20〜30 タイル程度なら数百枚）+ 前後景レイヤーで簡単に超える。

- ✅ 超過時に一度だけ警告ログを出す（[D3D12Renderer.cpp:487](../Engine/Source/Rhi/D3D12/D3D12Renderer.cpp)
  `SubmitSprite`。`warnedOnce` フラグで 1 回だけ Warn）。黙って捨てられる事故は解消。
- ⬜ **M6 のタイルマップ実装時（次の実装対象）**: 上限引き上げ（頂点バッファを大きく取るだけ）と、
  カメラ範囲カリングをタイルマップ側で行う設計にする。**タイルマップ着手直前に対応**。

### 4. 描画順が「オブジェクト生成順」に暗黙依存 — ✅ 対応済み（M5）
[SpriteComponent.cpp:41](../Engine/Source/Graphics2D/SpriteComponent.cpp) の
`SortKey()` で `SpriteDrawDesc.sortKey`（space ビット + layer を 0x8000 バイアスの
昇順 uint16 に変換）を生成し、RHI 側が `DoFlushSprites` 前に安定ソートする方式で対応済み。
`SetLayer(int16_t)`（大きいほど手前）+ `SetSpace(SpriteSpace::Screen)`（HUD は常に
ワールドの手前）で前後関係を明示指定できる。RemainingWork §1 の「描画順制御」項目に対応。

- 更新（ロジック）と描画提出を分離する大掛かりなリファクタ（`Component::Render()`
  フェーズ新設など）は入れていない。ソートキーで足りる間はソートキーで済ませる方針を維持。
- ComponentScheduler のフェーズ分け（PreUpdate〜Render）は導入済みだが、動機は物理・
  カメラ追従のフレーム内順序保証であって描画順ではない。同一フェーズ内の実行順は契約上
  未規定なので、**同レイヤーの前後関係を更新順に頼らないこと**（この規約は引き続き有効）。

## タイルマップ / リソースが増える（M6）までに直すもの

> **再確認（M5 完了時点）**: この節（5・6）は「M6 直前が対応期限」の項目。
> 現在 M5 完了で **次が M6**（§3 固定タイムステップを挟む）なので、この 2 件は
> **近々の対応対象**に格上げ。M6 のタイルマップ着手と同時に入れる想定。

### 5. テクスチャ解放経路が存在しない — ⬜ 未対応（M6 直前で対応）
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

### 6. カメラ変換が CPU・スプライト毎 — ⬜ 未対応（M6 のタイルマップ実装と同時）
[SpriteComponent.cpp:71-79](../Engine/Source/Graphics2D/SpriteComponent.cpp) —
World 空間スプライトは `CameraManager::Active()` の `WorldToScreenX/Y` + `Zoom()` を
スプライト 1 枚ごとに CPU で適用している（Screen 空間はカメラを見ない）。
数十枚なら問題ないが、タイル数百〜千枚では無駄が大きい。

- M4 の設計メモどおり、ビュー変換（カメラのオフセット + ズーム）を
  定数バッファに入れて頂点シェーダ側で適用する形へ移行する。
  RHI 境界は `SetCamera(offset, zoom)` 相当の抽象を 1 つ足すだけで守れる。
  ※ Screen 空間 HUD はカメラ変換を通さないため、CB を分けるか描画パスを分ける必要がある。
- 移行タイミングはタイルマップ実装（M6）と同時が効率的（§5 と同じく M6 直前が期限）。
  それまでは現状維持でよい。

## 仕様とのズレ・小さな改善（機会があれば）

### 7. Services に audio メンバが無い — ⬜ 未対応（M8 オーディオで追加）
[Services.h](../Engine/Include/WitchEngine/Core/Services.h) —
CLAUDE.md のコア設計では `renderer / audio / input / resources / time` の
固定メンバとされているが、`audio` が未実装のため存在しない。
現状の実メンバは `logger / renderer / time / resources / input / cameras / vfs`
（`cameras` `vfs` `logger` は設計メモに無いが実装で追加された。ズレとして両向きに記録）。
M8（オーディオ）で `IAudio` と一緒に `audio` を追加する。優先度変わらず（M8 まで着手不要）。

### 8. Spawn の 1 フレーム遅延が初期化を複雑にする — ⬜ 未対応（M6 の LoadLevel と同時）
[EmptyScene.cpp:58](../Game/Source/Scenes/EmptyScene.cpp) のコメントにある通り、
`OnEnter` で Spawn したオブジェクトは最初の Update まで `Find` できない。
遅延 Spawn の設計自体は正しい（更新中の安全のため）が、
「シーン開始直後」だけは反映済みであってほしい場面が今後増える
（レベルロード直後にカメラをプレイヤーへ向ける等）。

- `Scene::OnEnter` 呼び出しの直後（[Engine::ApplyPendingSceneChange](../Engine/Source/Core/Engine.cpp)
  = Engine.cpp:254 の `currentScene_->OnEnter()` 直後）に生成反映フェーズだけを 1 回回す
  `FlushPendingSpawns()` を用意する案。LoadLevel 実装（M6）時に一緒に入れると綺麗。
  ※ 現状 `ApplyPendingSceneChange` は OnExit/OnEnter を呼ぶのみで flush は未実装。

### 9. クリア色が GameLoop にハードコード — ⬜ 未対応（いつでも可 / 実質 M6 で必要になる）
[GameLoop.cpp:86](../Engine/Source/Core/GameLoop.cpp) — `cmdList->Clear({kCornflowerBlue})`
で固定（定数は GameLoop.cpp:20）。マップごとの背景色（洞窟物語は黒背景が基本）を出すため、
Scene かカメラから指定できるようにする。数行の変更なのでいつでも可。
タイルマップ / レベルロード（M6）で「マップごとに背景色」が実際に要るので、遅くとも M6 で対応。

### 10. GetComponent が dynamic_cast の線形探索 — ✅ 対応済み（型 ID タグ方式, 2026-07-08）
プロファイルで `GetComponent` がホットに出たため、型 ID タグ方式へ置き換えた。
[Component.h](../Engine/Include/WitchEngine/Scene/Component.h) に `ComponentTypeId`
（各型の関数ローカル static のアドレス。RTTI 不要でリンク跨ぎでも一意）と仮想 `IsA(id)`
（自分の ID or 基底の `IsA` が真 → `is_base_of` 相当を再現）を追加し、派生は 1 行マクロ
`WITCH_COMPONENT(Self, Base)` で宣言する。[GameObject.h](../Engine/Include/WitchEngine/Scene/GameObject.h)
の `GetComponent<T>()` は `comp->IsA(T::StaticTypeId())` 判定 + `static_cast` に変更。
これで `dynamic_cast` の階層探索・RTTI を避けつつ **基底型引き（`GetComponent<基底>`）を維持**する。

- 一時期あった `typeid(T) == typeid(*comp)` の完全一致版は、基底型引きが黙って `nullptr` を
  返す後退があったため不採用（IsA 方式で解消済み）。
- **マクロ付け忘れの検出**: `WITCH_COMPONENT` を付け忘れると `T::StaticTypeId()` が親の ID に
  フォールバックし、無関係な型に誤マッチして不正な `static_cast`（未定義動作）になる。
  これを防ぐためマクロは `using ComponentSelfType = Self;` を各型に定義し、
  `GetComponent<T>` に `static_assert(kHasComponentTypeId<T>)`
  （`std::is_same_v<T::ComponentSelfType, T>`）を置く。付け忘れると親の `ComponentSelfType`
  が継承され `!= T` になるため、**多段継承（`B : A` で B が付け忘れ）でもコンパイル時に弾ける**。
- 規約は引き続き有効: **Update 内で毎フレーム GetComponent せず、OnSpawn/OnAttach で
  ポインタをキャッシュする**（AnimationComponent / AsepriteComponent は sprite_ をキャッシュ済み）。
  IsA は速いが、ホットパスでの取得回数そのものを減らす規約の方が本質。

### 11. テストが 0 件 — ⬜ 未対応（導入適期が接近: §3 固定ステップ / M6〜M7）
CI は `noTestsAction: ignore` で緑になっている。エンジンが「壊れると全部壊れる」
コア（Scene の 3 段階更新・遅延破棄、AABB 衝突解決、固定タイムステップ）を
持ち始める M6〜M7 あたりで、描画に依存しないロジックだけでも
ユニットテストを導入する価値が出てくる（衝突解決は特にテストと相性が良い）。
**再確認: 次の §3 固定タイムステップ（アキュムレータ）はまさに「描画非依存・ロジックのみ・
テストと相性が良い」最初の対象**。ここでテスト基盤（vcpkg で GoogleTest 等）を入れ、
固定ステップのアキュムレータ挙動を最初のテスト対象にするのが自然な起点。

---

## 直さなくてよいと判断したもの（記録として）

- **Scene::Find の線形探索** — CLAUDE.md の未決事項どおり「必要になってから」。
- **Engine の Meyers Singleton** — 設計意図どおり（サービスの生成順は Engine が握る）。
- **Camera2D の std::clamp 回避**（windows.h min/max マクロ対策）— 意図的な回避で妥当。
- **入力の 2 スナップショット方式と Update 順序** — コメントに順序契約が明記されており健全。
- **例外を境界で expected に翻訳する方針** — ResourceManager / IRenderer で守られている。
