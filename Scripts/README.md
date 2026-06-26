# Scripts

開発を補助するスクリプト。`.ps1` が本体で、ダブルクリック用に同名の `.bat` ラッパーを併設している。
いずれも引数で構成（`debug` / `release`、既定は `debug`）を指定できる。

| スクリプト | 役割 |
|---|---|
| `Generate` | `cmake --preset` でプロジェクト（VS ソリューション等）を `build/<config>` に生成 |
| `Run` | （必要なら生成し）ビルドして `WitchGame.exe` を実行 |
| `OpenSolution` | 生成済みの `build/<config>/Witch.slnx` を Visual Studio で開く |

## 使い方

PowerShell から:

```powershell
.\Scripts\Generate.ps1                # debug を生成
.\Scripts\Generate.ps1 -Config release

.\Scripts\Run.ps1                     # debug をビルドして実行
.\Scripts\Run.ps1 -Config release
.\Scripts\Run.ps1 -NoBuild            # ビルドせず既存 exe を起動

.\Scripts\OpenSolution.ps1            # build/debug/Witch.slnx を開く
```

コマンドプロンプト / ダブルクリックから（`.bat`、第 1 引数が構成）:

```bat
Scripts\Generate.bat
Scripts\Run.bat release
Scripts\OpenSolution.bat
```

## メモ

- Visual Studio はマルチ構成ジェネレータなので、1 つの `.slnx` 内で Debug/Release を切り替えられる。
  `Run` は MSBuild の構成名（`Debug` / `Release`）に変換して `cmake --build --config` に渡す。
- 出力は `build/<config>/<Config>/WitchGame.exe`（例: `build/debug/Debug/WitchGame.exe`）。
- `Run` は exe のあるディレクトリで起動するので、隣にコピーされる `Shaders/` `Assets/` を参照できる。
