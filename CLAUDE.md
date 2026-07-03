# Witch — プロジェクトガイド（Claude Code 用）

## これは何か
- プロジェクト名: **Witch**（2D ゲーム）
- エンジン名: **WitchEngine**（Witch を作りながら育てる 2D 専用ゲームエンジン）
- C++ でコードファーストに作る。実ゲームを 1 本作りながらエンジンを育てる（co-development）。
- 設計純度より「ゲームが完成すること・作りやすいこと」を優先する（pragmatic 路線）。

## 技術スタック
- 言語: C++23（CMake で cxx_std_23 を要求。MSVC は /std:c++23 相当）
- 開発環境: Windows 11 + Visual Studio 2026（CMake プロジェクトとして開く）
- ビルド: CMake（CMakePresets.json）。**.slnx / .vcxproj は直書きしない**
- 依存管理: vcpkg（マニフェストモード, vcpkg.json）
- グラフィックス: 現在は **Direct3D 12 のみ**。Vulkan / Metal は将来対応（枠だけ＝今は作らない）

## スコープ（重要）
- いま作るのは **Windows + D3D12 だけ**。
- Vulkan / Metal / macOS 等のフォルダや空クラスを「先回りで」作らない。
- 必要になってから足す。早すぎる一般化をしない。

## ディレクトリ構成
役割名フォルダ（Engine / Game）＋ `Include/WitchEngine/` 名前空間フォルダ。
```
Witch/                          ← リポジトリのルート
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── Engine/
│   ├── Include/WitchEngine/    ← 公開ヘッダ。include は "WitchEngine/..." で始まる
│   │   ├── Core/               ← Engine, Services, ObjectRegistry, Time, Logger
│   │   ├── Scene/              ← Scene, GameObject, Component
│   │   ├── Rhi/                ← IRenderer 等のインターフェースのみ
│   │   └── Graphics2D/         ← SpriteComponent 等
│   └── Source/                 ← 実装と非公開ヘッダ
│       ├── Core/
│       ├── Platform/Windows/   ← Win32。OS 差はここのファイル分割で吸収
│       ├── Rhi/D3D12/          ← D3D12 実装。D3D12 / dxgi 型はここから出さない
│       ├── Scene/
│       └── Graphics2D/
└── Game/
    └── Source/
        ├── Entities/           ← PlayerObject 等（GameObject を薄く継承）
        └── Components/         ← ゲーム専用 Component
```
- include は必ず `#include "WitchEngine/Scene/GameObject.h"` の形にする。
  接頭辞 `WitchEngine/` でエンジンのヘッダだと一目で分かり、ゲーム側ヘッダと混ざらない。

## アーキテクチャの鉄則（破ると後で破綻する）
1. 依存は **Game → Engine の一方向のみ**。Engine は Game を include しない／知らない。
2. 上位コードは **RHI 越しにしか描画しない**。D3D12 / dxgi の型・ヘッダは
   `Engine/Source/Rhi/D3D12/` の中だけ。その外に漏らさない
   （grep で D3D12 が漏れていない状態を保つ）。
3. プラットフォーム差異は `#ifdef` を散らさず、`Platform/<OS>/` の **ファイル分割**で吸収。
   CMake が OS ごとに対象ソースを選ぶ。
4. **継承は浅く保つ**。GameObject のサブクラスは「種別」だけを表し、
   振る舞いは Component に分解する。継承を縦・横に広げない。
5. サービス（Renderer / Audio / Input 等＝世界に 1 つ）は **Services ロケーター越し**に使う。
   エンティティ（ゲームの登場物＝数が変わる）は普通に Spawn する。Singleton にしない。
6. 所有は **一方向ツリー**: Scene → GameObject → Component（unique_ptr）。
   オブジェクト間参照は所有しない。ObjectId 経由で弱く持つ。
7. 破棄は即時 delete せず **遅延**（フレーム末に回収）。更新中の自己破棄で壊さない。
8. サービスの **生成順・破棄順は Engine が明示的に握る**（Singleton 遅延生成に任せない）。
   破棄は生成の逆順。

## コア設計（Claude Code はこの仕様で基底クラスをゼロから実装する）
- **Component**（Scene/Component.h）: 全コンポーネントの基底。
  仮想 `OnAttach()` / `Update(float dt)` / `OnDetach()`。
  `Owner()` で所有 GameObject に弱参照（所有しない。owner は必ず自分より長生き）。
- **GameObject**（Scene/GameObject.h）: 登場物の基底。サブクラスは薄く保つ。
  Component を `unique_ptr` で所有。`AddComponent<T>()` / `GetComponent<T>()`。
  `OnSpawn()` / `OnDespawn()` / `Update(float dt)`（既定で全 Component を更新）。
  `Destroy()` は遅延フラグを立てるだけ。`ObjectId Id()`、`GetScene()` で所属シーンに弱参照。
- **Scene**（Scene/Scene.h）: 所有ツリーの根。GameObject を `unique_ptr` で所有。
  `Spawn<T>()` は更新中に呼ばれても安全なよう保留リストに積む。
  `Update(float dt)` は **「生成反映 → 全更新 → 破棄回収」の 3 段階（順序厳守）**。
  `Find(ObjectId)` で弱参照を解決（今は線形で可）。
  `OnEnter()` / `OnExit()`。`LoadLevel(path)` は ObjectRegistry 経由で実体化。
- **ObjectRegistry**（Core/ObjectRegistry.h）: 文字列型名 → 生成関数のファクトリ。
  外部レベルエディタの "Enemy" 等から実体を作る（C++ にリフレクションが無いため必須）。
  登録マクロ `WITCH_REGISTER_OBJECT(Type)` を .cpp に 1 行で自動登録。Meyers Singleton で可。
- **Services**（Core/Services.h）: サービスロケーター。動的 map にせず固定メンバ
  （renderer / audio / input / resources / time）。インターフェース越しに引く。
- **Engine**（Core/Engine.h）: 本体。サービス実体を `unique_ptr` で所有し生成順を握る。
  `Init()` → `Run()`（メインループ）→ `Shutdown()`（逆順破棄）。
  `ChangeScene()` は次フレーム頭で切替（遷移中に新旧両方要る問題を避ける）。

## コーディング規約
- namespace は `witch`。
- ヘッダガードは `#pragma once`。
- インターフェースは `I` 接頭辞（`IRenderer` 等）。
- 公開ヘッダは `Engine/Include/WitchEngine/`、実装と非公開ヘッダは `Engine/Source/`。
- 1 ファイル 1 責務。巨大ファイルを作らない。
- デバッグ UI で ImGui を呼ぶコード（`DrawDebugUI()` の override 等）は必ず
  `#ifdef WITCH_DEBUG_UI` で囲む。OFF ビルド（release 等）では ImGui がリンクされない。

## エラー処理の方針
- 例外機能は **有効のまま**にする（`/EH-` や `-fno-exceptions` を付けない）。
  vcpkg の OSS や標準ライブラリは例外を投げる前提でビルドされており、
  無効化すると共存できず未定義動作になるため。
- 自前のエンジン／ゲームコードでは **`throw` を書かない**。
  失敗しうる処理は **`std::expected<T, E>`（C++23）を返す**。
  例: `std::expected<Texture, LoadError> LoadTexture(std::string_view path);`
- **例外を投げる OSS は、それを呼ぶアダプタ層の `try/catch` で受け、`std::expected`
  に翻訳する**。エンジン内部に例外を伝播させない（例外の存在範囲を境界に閉じ込める）。
  RHI で D3D12 を隔離するのと同じ発想。
- アサート（回復不能な前提違反）と `std::expected`（回復しうる失敗）は使い分ける。

## 命名規約（PascalCase で統一）
- **ディレクトリ名・ファイル名・クラス名すべて PascalCase**。
  構造フォルダのみ `Include` / `Source`（この 2 つは慣習に従う綴り）。
  例: `Engine/Source/Scene/GameObject.cpp` に `class GameObject`。ファイルとクラスは 1 対 1。
- 変数はローカル `camelCase`、メンバ末尾 `_`（`pendingDestroy_`）、定数 `kPascalCase`。
- **`#include` のパスは実ファイル名と大文字小文字まで完全一致させる**。
  Windows は不一致でも通すが Linux / macOS は通さない。将来の他 OS 展開で
  大量に壊れるのを防ぐため、ここは機械的に厳守する。

## まだ決めていないこと（勝手に決めない・必要時に相談）
- レベルエディタを Tiled と LDtk のどちらにするか（形式は JSON エクスポートで決定済み）
- アートの方向性（HD / ドット絵の別、基準視界の具体値、1 タイルの px 相当）
- オブジェクト間参照の高速化（今は線形 Find で良い）

※ 決定済み: Transform = GameObject の標準メンバ（M2）。レベル形式 = Tiled/LDtk の
JSON エクスポート、イベントスクリプト = 当面 C++ 直書き、オーディオ = miniaudio、
画面方式 = ネイティブ解像度描画 + 仮想解像度（固定視界）カメラ。見た目をレトロ風に
する意図はなく、低解像度 RT + 整数倍拡大は不採用
（詳細は Docs/RemainingWork.md の「決定済み事項」参照）。

## 作業の進め方
- **マイルストーン単位**で進める。各段階で必ずビルドが通る状態を保つ。
- コミットは細かくする。
- 大きく動く前に、**作成・変更するファイルの一覧を箇条書きで先に提示**する。
- コア設計の API を変える提案以外は、確認を待たず進めてよい。
- コア設計（上記の契約）に反する変更は、理由を述べて提案してから。
