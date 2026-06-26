# Claude Code 実装プロンプト集（Witch / WitchEngine）

各マイルストーンの頭で、該当セクションを Claude Code に貼る。
どのプロンプトも前提として、まず `CLAUDE.md` を読ませること。
各段階の終わりで「ビルドが通る・動く」を必ず確認してから次へ進む。

---

## M0 — ビルド基盤・コア基底クラス・空ウィンドウ

```
まず CLAUDE.md を読んでから始めて。基底クラスは「コア設計」セクションの仕様に従い、
ゼロから実装する（既存コードは無い）。

# マイルストーン 0: ビルド基盤 + コア基底クラス + 空ウィンドウ

## ゴール
- CMake + vcpkg でプロジェクト全体がビルドできる。
- WitchEngine が静的ライブラリ、WitchGame がそれをリンクする実行ファイルになる。
- CLAUDE.md「コア設計」の基底クラスを実装し、コンパイルできる。
- WitchGame を実行すると Win32 の空ウィンドウが開き、×ボタンで閉じられる。
- シーンの Update が毎フレーム呼ばれ、Scene の「生成反映→全更新→破棄回収」の
  3 段階が動いていることをログで確認できる。

## やること
1. ルートに CMakeLists.txt / CMakePresets.json / vcpkg.json を作る。
   - Windows + MSVC + x64、Debug / Release のプリセット。
   - C++23 を要求する（target_compile_features に cxx_std_23、PUBLIC で WitchGame へ伝播）。
   - エラー処理は CLAUDE.md の方針に従う（例外有効のまま・自前は std::expected）。
2. ディレクトリ構成を CLAUDE.md の通りに用意する（Engine/Include/WitchEngine, Engine/Source 等）。
3. WitchEngine を STATIC ライブラリとして定義。
   - Engine/Include を公開インクルードに（→ include は "WitchEngine/..." で始まる）。
4. コア設計の基底クラスをゼロから実装する：
   - Scene/Component.h
   - Scene/GameObject.h（Component を所有、AddComponent/GetComponent、遅延 Destroy）
   - Scene/GameObject.cpp / Scene/Scene.{h,cpp}（Update の 3 段階・Spawn 保留・Find）
   - Core/ObjectRegistry.h（文字列→生成関数、WITCH_REGISTER_OBJECT マクロ）
   - Core/Services.h（固定メンバのロケーター）
   - Core/Engine.{h,cpp}（サービス所有・生成順・Run ループ・ChangeScene・逆順 Shutdown）
   - Core/Logger・Core/Time の最小実装。
   ※ この段階で実体の無いサービス（renderer 等）は前方宣言＋nullptr で良い。
5. Platform/Windows/ に Win32 ウィンドウ生成・メッセージループ・×で終了を実装。
   - #ifdef を散らさず、このファイルを Windows 専用ビルド対象にする。
   - ウィンドウの「閉じる」を Engine 側に伝えてループを止める。
6. WitchGame を実行ファイルとして定義し、WitchEngine をリンク。
   - Game/Source/Main.cpp で Engine を Init → ウィンドウ表示 → Run → Shutdown。
   - 仮の空シーン EmptyScene : witch::Scene を作り、起動時に ChangeScene で入れる。
   - EmptyScene::Update でフレーム番号をログ出力。

## 制約
- D3D12 はまだ触らない（描画は次のマイルストーン）。ウィンドウだけ。
- Vulkan / Metal / 他 OS のフォルダは作らない。
- コア設計の契約（所有・寿命・3 段階更新・サービスロケーター）を守る。

## 検証
- cmake のビルドが Debug / Release 両方通る。
- WitchGame.exe が起動し、空ウィンドウが出て、×で正常終了する。
- 終了時にサービスが生成の逆順で破棄されるログが出る。
- EmptyScene::Update のフレーム番号が増えていくログが出る。

進める前に、作成するファイル一覧と CMake ターゲット構成を箇条書きで先に提示して。
```

---

## M1 — RHI 境界と画面クリア

```
まず CLAUDE.md を読んで。M0 は完了している前提。

# マイルストーン 1: RHI インターフェースと D3D12 で画面クリア

## ゴール
- Include/WitchEngine/Rhi/ に描画抽象インターフェース（IRenderer ほか）を定義。
- Source/Rhi/D3D12/ にその D3D12 実装を置く。
- WitchGame のウィンドウが毎フレーム単色でクリアされる（例: コーンフラワーブルー）。

## 重要な設計方針（CLAUDE.md の鉄則 2 を厳守）
- D3D12 / dxgi の型・ヘッダは Source/Rhi/D3D12/ の中だけに閉じる。
  公開ヘッダ（Include 側）に D3D12 の型を一切出さない。
- IRenderer は Vulkan / Metal でも実装できる形にする：
  - 明示的なリソース状態遷移（バリア）を表に出す。
  - コマンドリスト記録 → サブミットの非同期前提。即時実行 API にしない。
  - リソース生成はディスクリプタ構造体渡し（引数羅列にしない）。
- Engine が D3D12Renderer を生成して Services::renderer に差す（上位に直接 new を書かない）。

## やること
1. Rhi/IRenderer.h に最小インターフェース（Init / BeginFrame / Clear / EndFrame / Present 相当）。
2. Source/Rhi/D3D12/ に D3D12Renderer を実装
   （デバイス、スワップチェーン、コマンドキュー、RTV、フェンス同期）。
3. Engine::Init で生成し Services に差す。Engine の描画パスで毎フレームクリア。

## 検証
- ウィンドウが単色で塗られ続け、リサイズしても破綻しない。
- 終了時に GPU リソースが解放される（D3D12 デバッグレイヤーで leak 警告が出ない）。

ファイル一覧を先に提示して。IRenderer の案も最初に見せて、合意してから実装して。
```

---

## M2 — スプライト 1 枚

```
CLAUDE.md を読んで。M1 完了前提。

# マイルストーン 2: テクスチャ付きスプライトを 1 枚描く

## ゴール
- PNG を読み込み、画面に 1 枚のスプライトとして描画する。

## やること
1. ResourceManager にテクスチャ読込（stb_image、vcpkg 経由）を実装し、
   IRenderer でテクスチャ／頂点バッファ／パイプラインを作る。
2. シェーダは HLSL を正本にする。Shaders/ に配置し、ビルド時 or 起動時にコンパイル。
3. Graphics2D/ に SpriteComponent（テクスチャ参照＋矩形）を作る。
   ※ Transform の置き場所をここで決める必要があるので、先に方針を相談すること。
4. Game 側で 1 つの GameObject に SpriteComponent を載せ、画面に出す。

## 検証
- PNG が正しい色・位置・サイズで 1 枚表示される。

Transform をどう持つか（Component か GameObject 標準メンバか）の案を最初に出して、
決めてから実装に入って。
```

---

## M3 以降（骨子のみ。詳細は各段階で詰める）

- **M3 スプライトバッチング**: 同一テクスチャをまとめて 1 ドロー。アトラス前提。
  大量表示で FPS が落ちないことを確認。
- **M4 カメラと座標系**: Camera2D、ワールド↔スクリーン変換、ピクセル単位の扱いを固める。
- **M5 入力**: IInput を RHI と同じ作法で抽象化し、ゲーム専用 Component を実動作させる。
- **M6 レベル読込**: 外部エディタのファイル形式を決め、Scene::LoadLevel と
  ObjectRegistry を繋いで、ファイルからオブジェクトを配置できるようにする。
- **M7 オーディオ / シーン遷移 / UI** … ゲームの要求が出てきた順に足す。

---

## 使い方のコツ
- 1 マイルストーン = 1 セッションを目安に。区切るほど Claude Code は安定する。
- 「設計を変えたい」と言われたら即採用せず、理由を聞いてから判断する。
  CLAUDE.md のコア設計・鉄則に反する変更は特に慎重に。
- 各マイルストーン完了後、CLAUDE.md の「まだ決めていないこと」を更新する
  （決まった項目を移す）。これが次セッションの精度を上げる。
