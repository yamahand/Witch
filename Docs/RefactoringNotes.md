# 改修候補メモ — 現状実装レビュー（初版 M4 完了時点 / 最終見直し M5 完了時点）

RemainingWork.md の機能追加を進める前後で直しておきたい箇所。
「いつ直すべきか」を優先度の代わりに書いている（早すぎる一般化はしない方針のため、
トリガーとなる機能実装より前に直す必要はない）。

**現状: M5 完了 + §3 固定タイムステップ済（2026-07-09）。次の実装対象は M6 タイルマップ / レベルロード。**
以下、M5 で解消済みの項目に対応済みマークを付け、未対応項目のトリガー時期を再確認した
（2026-07-08 見直し）。

## ステータス一覧（2026-07-08 見直し）

| #  | 項目 | 状態 | 対応時期 |
|----|------|------|----------|
| 1  | Engine::Init が失敗を握りつぶす | ✅ 済 | — |
| 2  | dt のスパイク対策 | ✅ 済（クランプ + 固定タイムステップ, 2026-07-09） | — |
| 3  | スプライト上限超過が黙って捨てられる | △ 警告は済 / 上限引き上げ残 | M6 直前 |
| 4  | 描画順がオブジェクト生成順に暗黙依存 | ✅ 済（SortKey） | — |
| 5  | テクスチャ解放経路が無い | ✅ 済（UnloadAll） | — |
| 6  | カメラ変換が CPU・スプライト毎 | ✅ 済（SetCamera + VS 適用） | — |
| 7  | Services に audio メンバが無い | ⬜ 未 | M8 |
| 8  | Spawn の 1 フレーム遅延 | ⬜ 未 | M6 の LoadLevel と同時 |
| 9  | クリア色が GameLoop にハードコード | ⬜ 未 | いつでも可 / 遅くとも M6 |
| 10 | GetComponent が dynamic_cast 線形探索 | ✅ 済（型IDタグ方式） | — |
| 11 | テストが 0 件 | △ 基盤導入済み（Catch2 + ctest, 2026-07-09）＋固定ステップのテスト済 | AABB は M7 で |

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
固定タイムステップ化も完了（2026-07-09、RemainingWork.md §3 参照）。
クランプは固定ステップ導入後も残し、1 フレームの固定ステップ数の上界
（0.25 / (1/60) = 15 回）を与えるスパイラル防止として機能している。

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
> → §5 は対応済み（2026-07-08）。§6 も対応済み（2026-07-09）。この節は完了。

### 5. テクスチャ解放経路が存在しない — ✅ 対応済み（2026-07-08）
[ResourceManager.cpp](../Engine/Source/Core/ResourceManager.cpp) に `UnloadAll()` を追加し、
両キャッシュ（`textureCache_` / `asepriteCache_`）の全テクスチャを
`IRenderer::DestroyTexture` で解放してキャッシュを空にする。呼び出し箇所は 2 つ:

- **シーン切替**: `Engine::ApplyPendingSceneChange` の旧シーン破棄後・新シーン
  `OnEnter()` 前。新シーンが同じアセットを使う場合はキャッシュミスして再ロードされるだけ。
- **終了時**: `~ResourceManager()`。Engine::Shutdown は renderer より先に
  ResourceManager を破棄する順序なので、デストラクタ時点で Services 経由の renderer が引ける。

補足・残課題:

- 旧記述の「スロット再利用もされない」は不正確だったので訂正: `CreateTexture` は
  解放済みスロットを線形スキャンで再利用する実装が元からあり、解放する経路が無かっただけ。
- `kMaxTextures` 枯渇時は `CreateTexture` が "Texture slot limit reached" を
  expected エラーで返すことを確認済み。
- `asepriteCache_` の `use_count > 1`（シーン跨ぎで `shared_ptr` を保持）は警告ログを
  出した上で破棄する。共有アセット（プレイヤー等）をシーン跨ぎで残したくなったら
  参照カウント化を検討（変わらず将来課題）。なお非所有の `TextureInfo`（SpriteComponent 等）
  の持ち越しは use_count 警告では検出できず黙って無効ハンドルになる。これが起きる設計に
  なりそうな時点が参照カウント化検討のトリガー。
- 動作確認用に EmptyScene へ G キー（シーン再入）を追加。連打しても
  スロットが枯渇しないことで解放経路を実証できる。

### 6. カメラ変換が CPU・スプライト毎 — ✅ 対応済み（2026-07-09）
ビュー変換（一様スケール + オフセット）を定数バッファに入れて頂点シェーダで
適用する形へ移行した。対応内容:

- **RHI 抽象**: `IRenderer::SetCamera(scale, offsetX, offsetY)` を追加
  （`screen = world * scale + offset`）。GameLoop がシーン更新後
  （Camera フェーズ確定後）に `Camera2D::ViewScale/ViewOffsetX/Y` から毎フレーム送る。
  カメラ未設定時は恒等。注視点・ビューポート等のカメラの内部事情は RHI に漏らさない。
- **HUD の分離**: `SpriteDrawDesc` に `SpriteSpace`（World/Screen）フィールドを追加し、
  「Screen はカメラ変換を受けず常に World の手前」を RHI の文書化された契約に昇格。
  D3D12 側は CB を 1 バッファ内 2 リージョン（offset 0 = World カメラ変換 /
  offset 256 = Screen 恒等）にし、(space, sortKey) ソート済みのドローループで
  space 切替時に Root CBV を差し替える（切り替えは高々 1 回）。
- **bit24 の撤廃**: `SpriteComponent::SortKey()` の空間ビット（bit 24）を廃止。
  sortKey は「同一 space 内の順序キー」となり、暗黙のビット割当依存が消えた。
- `SpriteComponent::Update` はワールド座標・ワールドサイズを無変換で提出するだけになり、
  per-sprite の CPU カメラ演算が消えた（タイル数百〜千枚に備えた §3 対応の前提が整った）。
- `Camera2D::WorldToScreenX/Y` / `ScreenToWorldX/Y` はマウスピック等の
  CPU 側単発変換用に残す（`ViewScale/ViewOffset` の合成形で書き直し、変換は同一）。

※ §3（スプライト上限 1024 引き上げ・カメラカリング）は未着手のまま。
M6 タイルマップ着手直前に対応する（変わらず）。

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
- **Base 引数の検証**: マクロの `Base` 取り違えのうち「基底でない型」は `Base::IsA` の
  修飾呼び出しが、「マクロ未適用の中間クラス」はマクロ内 static_assert
  （`Base::ComponentSelfType == Base`）が、それぞれコンパイルエラーで機械的に弾く。
  唯一「**中間クラスを飛ばして祖先を渡す**」だけは C++23 では検出不可能
  （直接基底を取るリフレクションが無く、is_base_of は間接基底でも真）。
  ここは規約（Base は必ず直接基底）で守る。継承を浅く保つ方針なら通常 Component 直付け。
- **ICF 対策**: 型 ID の実体 `kId` は意図的に**非 const**。const だとリンカの
  /OPT:ICF（Release 既定）が同一内容の読み取り専用データを畳み込み、異なる型の ID が
  同一アドレスになるリスクがある（書き込み可能データは畳み込まれない）。
  現行ツールチェーンでは const でも畳み込まれないことを実測済みだが、恒久対策として非 const にした。
- 規約は引き続き有効: **Update 内で毎フレーム GetComponent せず、OnSpawn/OnAttach で
  ポインタをキャッシュする**（AnimationComponent / AsepriteComponent は sprite_ をキャッシュ済み）。
  IsA は速いが、ホットパスでの取得回数そのものを減らす規約の方が本質。

### 11. テストが 0 件 — △ 基盤導入済み（Catch2 + ctest, 2026-07-09）
テスト基盤を導入した。フレームワークは Catch2（vcpkg の `tests` feature で取得）、
実行は既存配線の ctest（`catch_discover_tests` で各 TEST_CASE を自動登録）。

- **構成**: `Engine/Tests/`（WitchEngineTests 実行ファイル）。`WITCH_BUILD_TESTS`
  オプション（既定 OFF、debug / ci-* プリセットで ON）。配布用 release プリセットは
  テスト非依存のまま。ローカル実行は `ctest --preset test-debug`。
- **CI**: ci-windows-debug / ci-windows-release の両方でテストを実行し、
  `noTestsAction` を `ignore` → `error` に変更（テストが 1 件も無い後退を検出）。
  Release でも回すのは、ComponentTypeId の /OPT:ICF 畳み込み回帰（§10）が
  Release リンクでしか起きないため。
- **初期テスト**（描画非依存のコア契約のみ）: Scene の 3 段階更新
  （遅延 Spawn・遅延破棄・自己破棄安全・更新中 Spawn）、GetComponent 型 ID 方式
  （基底型引き・型 ID 一意性）、ComponentScheduler のフェーズ実行順
  （フック位置・フレーム途中 AddComponent の同一フレーム実行）、VfsPathUtil の純関数。
- ✅ **済（2026-07-09）**: §3 固定タイムステップのテストを追加
  （FixedStepAccumulator の剰余・上界契約、Scene の固定/毎フレーム分割契約
  — 0 ステップフレーム・多重ステップ・ステップ中 Spawn / Destroy）。
- ⬜ **残**: AABB 衝突解決（M7）もテスト対象に加える。

---

## 直さなくてよいと判断したもの（記録として）

- **Scene::Find の線形探索** — CLAUDE.md の未決事項どおり「必要になってから」。
- **Engine の Meyers Singleton** — 設計意図どおり（サービスの生成順は Engine が握る）。
- **Camera2D の std::clamp 回避**（windows.h min/max マクロ対策）— 意図的な回避で妥当。
- **入力の 2 スナップショット方式と Update 順序** — コメントに順序契約が明記されており健全。
- **例外を境界で expected に翻訳する方針** — ResourceManager / IRenderer で守られている。
