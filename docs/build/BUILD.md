# TETHER — ビルド手順

対象ランタイム: **Skyrim SE/AE（開発ターゲットは最新 1.6.11xx / AE）**。CommonLibSSE-NG により1つのDLLがSE/AE/VR/GOGで動く。

## 必要なもの（あなたの環境）
- **Visual Studio 2022**（Desktop development with C++ / MSVC v143）
- **CMake** 3.22+
- **vcpkg**（`VCPKG_ROOT` 環境変数が設定済み）
- **CommonLibSSE-NG のチェックアウト**（OStimビルドで使ったものを流用可）
- テスト用ゲーム側: **SKSE64** ＋ **Address Library for SKSE Plugins（AE版）**

## 環境変数
| 変数 | 意味 | 例 |
|---|---|---|
| `VCPKG_ROOT` | vcpkgの場所 | `C:\dev\vcpkg` |
| `CommonLibSSEPath_NG` | **CommonLibSSE-NGのフォルダ**（必須） | `C:\dev\CommonLibSSE-NG` |
| `CompiledPluginsPath` | （任意）ビルド後にDLLをコピーする先。ゲームのData相当 | `C:\...\MO2\mods\TETHER` |

> `CommonLibSSEPath_NG` を持っていない場合のみ、一度 `git clone https://github.com/alandtse/CommonLibSSE-NG` してそのフォルダを指す。既にOStim用のがあるならそれでOK。

## ビルド
```bash
# 構成
cmake --preset vs2022-windows
# ビルド（Release）
cmake --build build --config Release
```
- 出力: `build/src/Release/TETHER.dll`
- 自動コピーしたい場合: `CompiledPluginsPath` を設定し、構成時に `-DCOPY_OUTPUT=ON` を付ける。
  ```bash
  cmake --preset vs2022-windows -DCOPY_OUTPUT=ON
  ```

## 動作確認（スキャフォールドのマイルストーン）
1. `TETHER.dll` を `<game>/Data/SKSE/Plugins/`（MO2ならmodフォルダ）に置く。
2. ゲーム起動 → ログを確認: `Documents/My Games/Skyrim Special Edition/SKSE/TETHER.log`
   ```
   TETHER v0.1.0 loading
   TETHER loaded
   kDataLoaded: TETHER scaffold ready (no behavior yet)
   ```
3. この3行が出れば、DLLの読み込み・ログ経路・CommonLibSSE-NG連携が通っている＝土台OK。

## CommonLibSSE-NG の取り込み方式について
本プロジェクトは Precision と同じ **`add_subdirectory($ENV{CommonLibSSEPath_NG})`** 方式（ローカル参照）。
- 利点: あなたのOStim用CommonLibを再利用、レジストリ設定不要。
- もし OStimのビルドが **vcpkgレジストリ方式** だった場合は、そちらに合わせて `src/CMakeLists.txt` の該当行と `vcpkg.json` を差し替える（1分の変更）。どちらの流儀か教えてくれれば揃える。
