# 残作業リスト — 洞窟物語ライクなゲームに必要な機能

現状（M4 完了時点）でできること:
ウィンドウ生成 / D3D12 スプライト描画（静止画・等倍矩形のみ）/ PNG テクスチャ読込 /
キーボード・マウス入力（エッジ検出付き）/ Camera2D（注視点・ズーム・座標変換）/
ImGui デバッグ UI / Tracy プロファイリング。

洞窟物語（横スクロール探索アクション）を成立させるには以下が不足している。
おおよそ着手すべき順に並べている（各節内は依存順）。

---

## 1. 描画の拡張（エンジン / Graphics2D + Rhi）— ✅ M5 完了

タイルマップ・アニメーション・HUD の前提になる基盤。最優先。

- [x] **ソースレクト指定**: `SpriteComponent::SetSourceRect(px矩形)` を追加済み。
      `LoadTexture` が `TextureInfo`（ハンドル + サイズ）を返し、px→UV 換算は
      SpriteComponent に集約。
- [x] **左右反転（flip）**: `SetFlip(h, v)`。提出時の UV スワップで実装（RHI 非関知）。
- [x] **カラー / アルファ（tint）**: `SetColor()`。頂点カラーでシェーダに渡し PS で乗算。
- [x] **描画順制御（レイヤー / ソートキー）**: `SetLayer(int16_t)`（大きいほど手前）。
      RHI は sortKey の安定ソートのみ行い、同値は提出順維持。
- [x] **回転の描画反映**: `Transform.rotation` を描画に反映。ピボット＝アンカー点、
      画面上で反時計回り正。頂点生成時に CPU で回転（rotation=0 は高速パス）。
- [x] **スクリーン空間描画（UI レイヤー）**: `SetSpace(SpriteSpace::Screen)`。
      transform を仮想座標として直接使い、常にワールドの手前に描画される。
- [x] **仮想解像度（固定視界）カメラ**: `IRenderer::SetVirtualResolution()`。
      ScreenCB に仮想サイズ + ビューポートをレターボックス内側矩形に絞る方式で、
      シェーダ変更なしに一様スケール写像を実現。黒帯は 2 段クリア。
      基準視界はゲーム側 `GameConfig.h` の 1920x1080（**仮置き** — アート方針決定時に確定）。
      マウスは `WindowToVirtualX/Y` → `ScreenToWorldX/Y` の 2 段変換（EmptyScene 参照）。

## 2. スプライトアニメーション（エンジン / Graphics2D）— ✅ M5 完了

- [x] **AnimationComponent**: グリッド式 `AnimationClip`（コマ px サイズ / 列数 /
      セル番号列 / fps / loop）。Play / Stop / IsFinished / SetClip（歩き・待機切替）。
      兄弟 SpriteComponent を OnAttach でキャッシュ（**Sprite を先に AddComponent**）。
- [x] アニメーション定義は C++ 直書きで開始（データ化はレベル形式実装と同時に再検討）。

## 3. 固定タイムステップ（エンジン / Core）— ✅ 完了（2026-07-09）

物理・衝突を入れる前に必ず決めること。可変 dt のまま物理を書くと後で全部壊れる。

- [x] **Update の固定タイムステップ化**（アキュムレータ方式、**60Hz 固定**）。
      `FixedStepAccumulator`（純ロジック）を `Time` が所有し、`GameLoop::Tick` が
      `FixedUpdate`（0〜N 回）→ `FrameUpdate`（必ず 1 回）で回す。
      フェーズ所属: PreUpdate / Update / PostUpdate = 固定側、
      Animation / Camera / Render = 毎フレーム側（Animation は見た目優先で可変と決定）。
      dt クランプ（0.25s）は固定ステップ数の上界（15 回）としても機能する。
- 決定事項: 描画補間は後回し（`Time::InterpolationAlpha()` だけ用意済み。
  高リフレッシュレートでの移動ジャダーが気になった時点で prev/current Transform
  スナップショット + α ブレンドを足す）。
- 注意（M7 で顕在化しうる）: エッジ入力（WasPressed）は入力世代がフレーム単位のため
  フレーム側で読む規約。ジャンプ入力等を固定側ロジックで扱いたくなったら
  入力スナップショット（ステップ跨ぎのエッジ保持）の仕組みが要る。

## 4. タイルマップ + レベルロード（エンジン / 新モジュール）— ✅ 実装済み（2026-07-10）

- [x] **レベルファイル形式**: 【決定: **Tiled または LDtk の JSON エクスポート**】
      パーサは nlohmann-json (vcpkg)。**LDtk ローダを実装済み**
      （`Engine/Source/Level/LdtkLoader.{h,cpp}`。JSON 型・例外はこの 1 TU に隔離し
      `std::expected` へ翻訳）。公開データ型 `LevelData`
      （`Engine/Include/WitchEngine/Level/LevelData.h`）はフォーマット中立の中間表現
      なので、**エディタ選定（Tiled / LDtk）自体はまだ未決定のまま**。Tiled に決まれば
      ローダを並置するだけで載る（`Scene::LoadLevel` は拡張子で分岐）。
      未対応で明示エラー: externalLevels（レベル別ファイル保存）。levels[0] のみ読む
      （複数レベル選択は §11 マップ遷移で必要になってから）。
- [x] **TilemapComponent**（`Graphics2D/TilemapComponent.{h,cpp}`）: 1 コンポーネント =
      LDtk の 1 タイルレイヤー。ctor で UV / フリップ / 不透明度を全解決し、
      Update はカメラ可視矩形カリング + SubmitSprite のみ。
      `kMaxSpritesPerFrame` は 16384 へ引き上げ済み（RefactoringNotes §3 完了）。
      ※ レイヤー描画順の正規化（LDtk 先頭 = 最前面 → LevelData 奥→手前）は
      サンプルが 1 レイヤーのため実害未検証。複数レイヤーのマップ作成時に目視確認する。
- [x] **Scene::LoadLevel 実装**: vfs → ParseLdtk → 背景色（Scene::ClearColor）→
      タイルレイヤーをルート GameObject + TilemapComponent 群で生成 →
      ObjectRegistry 経由のエンティティ配置（未登録名は警告スキップ。
      transform / Name は OnSpawn 前に設定）。返り値は `std::expected<void, std::string>`。
      LDtk の fieldInstances（エンティティのカスタムフィールド）注入はスコープ外
      （Factory が nullary のため。必要になったら設計）。px はピボット位置の生値を
      transform に入れる。デモ: `Game/Source/Scenes/StageScene`（Tab で EmptyScene と行き来）。
- [x] **マップ属性**: IntGrid（intGridCsv）を `LevelIntGrid` として保存済み
      （`Scene::CurrentLevel()` から取得。行優先・0 = 空）。衝突種別としての解釈は
      M7 物理側で行う。

## 5. 衝突判定 + プラットフォーマー物理（エンジン / 新モジュール Physics2D）

洞窟物語の核。ここの手触りがゲームの品質を決める。
**M7-a 実装済み（2026-07-11）**: 坂タイルを除く全項目。モジュールは
`Engine/Include/WitchEngine/Physics2D/`（Aabb / TileCollision / CollisionComponent /
CollisionWorld）。UpdatePhase に Physics（固定側、Update の後）を追加し、
Scene::FixedUpdate が Physics → 重なり検出 → コールバック発火 → PostUpdate で回す。

- [x] **AABB vs タイルマップの衝突解決**: 移動 → 軸ごとに押し戻し（X 掃引 → Y 掃引の
      純関数 `physics2d::MoveAabb`。セル境界を順に検査しトンネリングしない）。
      接地判定（onGround）は CollisionComponent が公開。衝突タイルは
      `CurrentLevel()` の**先頭 IntGrid**（`FindCollisionGrid` の規約）、0 = 空・
      それ以外 = ソリッド（解釈は `ShapeFromValue` に集約 = 坂対応の拡張点）。
      移動は**速度自動積分方式**: CollisionComponent が velocity を持ち、Physics
      フェーズで積分 + 押し戻し + transform 書き戻し。コントローラは速度の
      読み書きのみで制御する。
- [ ] **坂タイル対応**: 洞窟物語は 45° 坂が多用される。タイル衝突種別に坂を含める。
      【次の PR。ShapeFromValue に値 2/3（床坂）を足し、Y パスに足元中心点の
      坂スナップ + 下り吸着を追加、X パスは坂セル無視。LDtk 側の IntGrid 値定義と
      マップ編集を伴う】
- [x] **エンティティ同士の重なり判定**: CollisionWorld（Scene が所有、O(n²) 総当たり
      = 数十体想定で意図的に許容）。トリガーのみで押し戻しなし。
      layer / mask の uint32 で (自 mask & 相手 layer) の非対称フィルタ。
- [x] **CollisionComponent（AABB）+ 衝突通知**: 通知は 2 段構え —
      `Contacts()`（接触リスト = 一次ソース、PostUpdate から同一ステップ可読）+
      `SetOverlapCallback`（検出完了後の dispatch 段で発火。コールバック内の
      Destroy / Spawn は遅延契約で安全）。
- [x] キャラクターコントローラ: `Game/Source/Components/PlayerControllerComponent`
      （重力・左右加減速・Z ジャンプ + 可変ジャンプ高）。ジャンプのエッジ入力は
      固定ステップ内の IsDown 自前ラッチ（WasPressed の二重発火を回避。
      1 フレーム未満のタップは取りこぼしうる — 問題化したら §3 注記の入力
      スナップショットを設計）。`Game/Source/Entities/PlayerObject` を
      `WITCH_REGISTER_OBJECT_AS(PlayerObject, "Player")` で登録済み
      （レベルに Player エンティティを置けば LoadLevel から実体化される。
      現状サンプルには無いため StageScene が「立てるセル」を探して暫定 Spawn）。
      手触り定数（速度・重力・ジャンプ初速等）は WITCH_DEBUG_UI の
      DrawInspector でライブ調整できる。調整値の確定は坂 PR までに行う。

## 6. オーディオ（エンジン / 新モジュール Audio）

- [ ] **IAudio サービス**: `Services` に `audio` メンバを追加（CLAUDE.md の仕様には
      既に記載があるが実体が無い）。バックエンドは【決定: **miniaudio**（vcpkg）】。
      デコード・ミキシング・ループポイントを内蔵しており IAudio の実装が薄く済む。
      miniaudio 型はアダプタ実装の中に閉じ込め、外へ漏らさない（RHI と同じ発想）。
- [ ] **SE 再生**: 多重再生（同じ SE の重ね掛け）、音量指定。
- [ ] **BGM 再生**: ストリーミング + **ループポイント指定**
      （洞窟物語の BGM はイントロ付きループ。先頭からのループでは再現できない）。
- [ ] ResourceManager に音声アセットのロード / キャッシュを追加。

## 7. 入力の拡張（エンジン / Input）

- [ ] **ゲームパッド対応**（XInput。`Platform/Windows/` に閉じ込める）。
- [ ] **アクションマッピング**: `Key::Z` 直参照ではなく「Jump / Shoot / Left / …」の
      抽象アクションに割り当てる層。キーコンフィグとパッド両対応の前提になる。

## 8. カメラの拡張（エンジン / Graphics2D）

- [ ] **追従カメラ**: プレイヤーを遅れて追う（洞窟物語は先読み + スムージング）。
      CameraController としてゲーム側に置くか、エンジンにユーティリティを置くか。
- [ ] **マップ境界クランプ**: 部屋の端でカメラを止める（レベルデータのサイズが必要）。
- [ ] **画面揺れ（スクリーンシェイク）**: 爆発・ボス演出に必須級。

## 9. テキスト描画 + 会話システム

洞窟物語はメッセージ・イベントのゲームでもある。ボリュームは大きい。

- [ ] **ビットマップフォント描画**（エンジン側）: フォントテクスチャ + 文字送り。
      日本語を出すなら BMFont 等で事前生成したアトラスが現実的。
- [ ] **メッセージウィンドウ**（ゲーム側）: 1 文字ずつ表示・改ページ・顔グラフィック。
- [ ] **イベントスクリプト**（ゲーム側）: 洞窟物語の TSC 相当。
      「会話 → フラグ分岐 → アイテム付与 → マップ遷移」を記述する仕組み。
      【決定: **当面 C++ 直書き**】最初の 1〜2 マップは C++ でイベントを書き、
      イベント量が見えてからデータ駆動（Lua / 独自形式）への移行を再検討する。
      移行しやすいよう「イベントは 1 関数 = 1 イベント」の形で書き溜めておく。

## 10. ゲーム状態の永続化

- [ ] **ゲームフラグ管理**: イベント進行フラグの集合（ゲーム側サービスでよい）。
- [ ] **セーブ / ロード**: プレイヤー状態 + フラグ + 現在マップの書き出し / 復元。
      レベル形式が決まってからで良い。

## 11. シーン / 画面遷移まわり

- [ ] **フェードイン / アウト**（描画のカラー・UI レイヤーができれば単純な黒矩形で可）。
- [ ] **ドア / マップ遷移**: LoadLevel + プレイヤー配置 + フェードの組み合わせ。
      現状の `ChangeScene`（シーン全破棄）とは別に「同一シーン内でレベルだけ差し替え」の
      形にするか検討。
- [ ] **ポーズ**: Update 停止（インベントリ / メニュー画面の前提）。
- [ ] タイトル画面（ゲーム側シーン）。

## 12. ゲーム固有実装（Game/Source — エンジンが揃ってから）

- [ ] プレイヤー（移動・ジャンプ・ぶら下がり無し等の仕様決め込み）
- [ ] 武器 / 弾（洞窟物語式なら経験値による武器レベル）
- [ ] 敵（数種類 + 単純 AI）/ ダメージ・HP・ノックバック・無敵時間
- [ ] アイテム / 回復 / ハート増加
- [ ] HUD（HP バー・武器エネルギー）
- [ ] ボス（イベント + 敵の複合。最後でよい）

---

## 推奨マイルストーン割り（たたき台）

| M | 内容 | 対応節 |
|---|------|--------|
| ~~M5~~ | ✅ 完了: 描画拡張（ソースレクト・flip・tint・レイヤー・回転・HUD・仮想解像度）+ アニメーション | 1, 2 |
| ~~M6~~ | ✅ 完了: ~~固定タイムステップ~~ + タイルマップ描画 + LoadLevel（LDtk ローダ。エディタ選定は未決定のまま持ち越し = マップ制作開始時） | 3, 4 |
| M7 | △ M7-a 完了（2026-07-11）: タイル衝突 + AABB + トリガー + プレイヤー移動・ジャンプ。**残: 45° 坂（M7-b、次の PR）+ 手触り定数の確定** | 5 |
| M8 | オーディオ（SE + ループ BGM） | 6 |
| M9 | パッド + アクションマッピング / 追従カメラ + 境界クランプ | 7, 8 |
| M10 | フォント + メッセージウィンドウ + イベントスクリプト最小版 | 9 |
| M11 | 武器・敵・ダメージ / HUD / マップ遷移 | 11, 12 |
| M12 | セーブロード / タイトル / ポーズ | 10, 11 |

## 決定済み事項（2026-07-03 相談で確定）

| 項目 | 決定 |
|------|------|
| レベルファイル形式 | Tiled または LDtk の **JSON エクスポート**（エディタ選定はマップ制作開始時。**現状は LDtk ローダのみ実装済み**・中間表現 LevelData はフォーマット中立） |
| イベントスクリプト | **当面 C++ 直書き**（イベント量が見えたらデータ駆動化を再検討） |
| オーディオバックエンド | **miniaudio**（vcpkg。アダプタ層に閉じ込める） |
| 画面方式 | **ネイティブ解像度描画 + 仮想解像度（固定視界）カメラ**。レトロ風の低解像度 RT + 整数倍拡大は不採用（当初の 480x270 決定は撤回） |
| アート方針 | **未定・保留**（HD / ドット絵の別、基準視界の具体値、タイルの px 相当はアート方針決定時に確定） |
