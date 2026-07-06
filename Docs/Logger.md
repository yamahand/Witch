# Logging Framework（Core/Log）

WitchEngine の Logging Framework の要件と確定した設計。C++23 のみ・外部ライブラリ不使用。

## 要件

- Logger は Services（サービスロケーター）へ登録できる
- Logger と ImGui Log Viewer は完全分離（Logger 側は ImGui 依存ゼロ）
- Sink / Filter / Formatter パターン
- LogRecord（Logger 内部専用）と LogView（Sink へ渡す読み取り専用ビュー）を分離。
  **Sink は LogRecord を知らない**
- メッセージ・カテゴリ文字列は `std::string` で持たず巨大 TextBuffer へ保存し、
  LogView 生成時に `string_view` へ変換する
- TextBuffer（1 面 100MB 程度）はダブルバッファ、LogRecord 配列は 100,000 件程度で分離
- Sink 毎に Immediate / Deferred を宣言。Viewer / DebugOutput は Immediate、
  Console / File は Deferred（`Flush()` でのみ出力）
- Thread Safe（初期実装は `std::mutex`、将来 LockFree へ置換しやすい設計）
- `std::source_location` 対応（呼び出し元の file / function / line を自動捕捉）

## ディレクトリ構成

```
Engine/Include/WitchEngine/Core/
├── Logger.h                  ← ファサード（log::Info 等）+ Logger クラス。既存 include パス維持
└── Log/
    ├── LogLevel.h            ← enum LogLevel（Trace/Info/Warn/Error/Fatal）+ ToString
    ├── Category.h            ← Category 構造体 + constexpr FNV-1a ハッシュ
    ├── LogView.h             ← Sink へ渡す読み取り専用ビュー
    ├── ILogSink.h            ← Sink インターフェース + SinkMode（Immediate/Deferred）
    ├── ILogFilter.h          ← Accept(const LogView&) を持つフィルタ
    ├── ILogFormatter.h       ← LogView → 1 行文字列（Sink 毎に差し替え可）
    ├── LevelFilter.h         ← レベル閾値フィルタ（header-only）
    ├── CategoryFilter.h      ← カテゴリハッシュの許可/拒否フィルタ（header-only）
    ├── PlainTextFormatter.h  ← [HH:MM:SS.mmm][LEVEL][Cat] msg（Console/DebugOutput 既定）
    ├── DetailedTextFormatter.h ← フレーム・スレッド・file:line 付き（File 既定）
    ├── ViewerSink.h          ← Immediate。ImGui 非依存のリングバッファ + Snapshot()
    ├── ConsoleSink.h         ← Deferred。stdout へバッチ書き出し
    ├── DebugOutputSink.h     ← Immediate。OutputDebugStringA（非 Win は no-op）
    └── FileSink.h            ← Deferred。Create() が std::expected を返す

Engine/Source/Core/
├── Logger.cpp                ← Logger 実装（pimpl。mutex / バッファ / records を隠蔽）
└── Log/
    ├── LogRecord.h           ← 内部専用（公開ヘッダに出さない）
    ├── TextBuffer.h/.cpp     ← 追記専用フラットバッファ
    ├── TimestampFormat.h     ← ローカル時刻整形の共有ヘルパ
    └── 各 Sink / Formatter の .cpp
```

## クラス責務

| クラス | 責務 |
|---|---|
| `Logger` | 全ログの唯一の入口 `Log()`。TextBuffer×2 / LogRecord 配列 / Sink 群を所有。ロックは pimpl 内部に完全に閉じる |
| `LogRecord` | 内部専用 1 件分。文字列は TextBuffer への offset+length で参照 |
| `LogView` | Sink 向けビュー。`Write()` 呼び出し中のみ有効な `string_view` を持つ |
| `TextBuffer` | 追記専用バッファ。ロックは持たない（Logger のロック下で使う前提） |
| `ILogSink` | `Mode()` / `Write(LogView)` / `Flush()`。Filter/Formatter を unique_ptr で所有（コンストラクタ注入） |
| `ViewerSink` | Write 時に文字列を自前コピーしてリング保持。表示側（ImGui）は `Snapshot()` でポーリング。**描画は一切知らない** |
| `FileSink` | `Create(path)` が `std::expected<unique_ptr<FileSink>, string>` を返す（例外なし） |

所有関係: `Engine → unique_ptr<Logger>`、`Services::logger` は非所有ポインタ、
`Logger → vector<unique_ptr<ILogSink>>`、`Sink → unique_ptr<ILogFormatter/ILogFilter>`。

## データフロー

```
log::Info("hp={}", hp)
  ↓ FormatWithLocation の consteval コンストラクタが「呼び出し元で」source_location を捕捉
  ↓ std::format で整形 → Logger::Log(level, category, msg, loc)   [mutex 下]
  ├─ 容量逼迫（TextBuffer 満杯 or records 上限）なら先に強制 Flush
  ├─ TextBuffer(active) へ category / msg を追記 → LogRecord(offset/length) を構築
  ├─ LogView 生成 → Immediate Sink（Viewer / DebugOutput）は即 Write
  └─ LogRecord は records に蓄積（Deferred 向け）
Engine::Run のフレーム末: logger->SetFrameNumber(...) → logger->Flush()
  ├─ records を LogView 化し Deferred Sink（Console / File）へ Write → 各 Sink の Flush
  └─ ドレイン完了後に active バッファを Reset してスワップ
      （読み取り中のバッファをリセットする瞬間が構造上存在しない）
```

- Logger 未登録の間（`Engine::Init` 前 / `Shutdown` 後）はファサードが
  stdout + OutputDebugStringA へフォールバックする。
- Deferred Sink の Write/Flush は Logger のロック下で直列に呼ばれるため自前ロック不要。
  ViewerSink のみ、表示スレッドから `Snapshot()` が呼ばれるため自前 mutex を持つ。

## スレッド安全と将来の LockFree 化

初期実装は Logger 内部の単一 `std::mutex`。ロック操作は公開 API に一切漏れていないため、
将来 `TextBuffer::Append` をアトミック bump allocator に、records を MPSC キューに
置き換えても Logger.cpp / TextBuffer.cpp の内部変更だけで済む（呼び出し側・Sink は無変更）。

## 使い方

```cpp
// 既存 API（無変更で動く）
log::Info("Engine init start.");
log::Warn("Failed: {}", error);

// カテゴリ付き
constexpr log::Category kPhysics{"Physics"};
log::Info(kPhysics, "collision resolved dt={}", dt);

// Sink 構成（Engine::Init 参照）
logger->AddSink(std::make_unique<log::ConsoleSink>(
    nullptr, std::make_unique<log::LevelFilter>(log::LogLevel::Warn)));

// ImGui Viewer 側（将来・WITCH_DEBUG_UI 内）: ViewerSink::Snapshot() を描画するだけ
```
