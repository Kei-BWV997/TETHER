# TETHER.esp — ESP フォーム仕様書

**SSEEdit** で以下の手順どおりに ESP を作成してくれ。中身は MCM に必要な最小構成：GlobalVariable × 16 と Quest × 1（MCMスクリプト付き）だけ。

**出力ファイル名**: `TETHER.esp`（**このファイル名固定**。SKSEプラグインが `LookupForm(FormID, "TETHER.esp")` で参照するので、違うと sliders が全部無効化される）。

**ESL フラグ**: OK（17 forms なので compact ESL の FormID 範囲に余裕で収まる）。

---

## Step 0 — 前提

1. **SkyUI 5.2 SE** インストール済み（あなたの MO2 スロット #0027 で入ってる）
2. **SkyUI SDK 5.2** インストール（Papyrus コンパイル用。SkyUI SE の Nexus ページで "Scripts" ファイルとして配布されてる）
3. `mcm/Scripts/Source/TETHER_MCM.psc` を `Data/Scripts/Source/TETHER_MCM.psc` にコピー

## Step 1 — SSEEdit で空 ESP を作る

1. SSEEdit 起動。`Skyrim.esm`、`Update.esm`、`SkyUI_SE.esp` をロード
2. ツリーのヘッダー領域で右クリック → **Add** → **New file** → 名前 `TETHER.esp`
3. 新しい `TETHER.esp` の File Header で masters を追加：`Skyrim.esm`、`Update.esm`、`SkyUI_SE.esp`（右クリック → Add Masters）

## Step 2 — GlobalVariable × 16 を追加

各行について `TETHER.esp` を右クリック → **Add** → **GlobalVariable**（既存を複製してリネームでもOK）。以下を設定：

- **EditorID**: 表の通り厳密に（**大文字小文字一致必須**、SKSEプラグインが名前で参照）
- **FNAM - Type**: 表の Type に従う（`Float` / `Short` / `Long`）
- **FLTV - Value**: 表のデフォルト値

| # | EditorID | Type | Default | 用途 |
|---|---|---|---|---|
| 1 | `TETHER_Hotkey` | `Short` | `35` | DIK scancode（整数）。35 = 'H' |
| 2 | `TETHER_OffsetRight` | `Float` | `0.0` | フォロワー右オフセット（マイナス=左） |
| 3 | `TETHER_OffsetBack` | `Float` | `-30.0` | フォロワー後方オフセット（マイナス=後ろ） |
| 4 | `TETHER_FollowRadius` | `Float` | `30.0` | KeepOffsetFromActor FollowRadius |
| 5 | `TETHER_CatchupRadius` | `Float` | `60.0` | KeepOffsetFromActor CatchUpRadius |
| 6 | `TETHER_TetherIdealDist` | `Float` | `40.0` | Elastic tether 近距離しきい値 |
| 7 | `TETHER_TetherSlackDist` | `Float` | `120.0` | Elastic tether 遠距離しきい値 |
| 8 | `TETHER_PlayerSpeedMultNear` | `Float` | `1.0` | プレイヤー速度（近距離時） |
| 9 | `TETHER_PlayerSpeedMultFar` | `Float` | `0.35` | プレイヤー速度（遠距離時=歩き速度） |
| 10 | `TETHER_FollowerSpeedMult` | `Float` | `1.30` | フォロワー速度ブースト |
| 11 | `TETHER_PalmOffset` | `Float` | `5.0` | constraint 掌へのめり込み量（bone local +Z 方向） |
| 12 | `TETHER_AutoRelWeapon` | `Short` | `1` | Bool: 抜刀時 auto-release（0/1） |
| 13 | `TETHER_AutoRelCombat` | `Short` | `0` | Bool: 戦闘発生時 auto-release（0/1） |
| 14 | `TETHER_AutoRelExtDist` | `Short` | `0` | Bool: 極距離時 auto-release（0/1） |
| 15 | `TETHER_ExtDistThreshold` | `Float` | `500.0` | 極距離 auto-release のしきい値 |
| 16 | `TETHER_ExtDistDuration` | `Float` | `3.0` | 極距離が続いてリリースするまでの秒数 |

**型の判断基準**：
- **Float**: 小数点あり or 負の値ありうる（距離、乗算、オフセット）
- **Short**: 整数のみ、範囲が小さい（Bool 0/1、DIK scancode 0〜300程度）
- **Long**: 大きい整数（TETHERでは不使用）

**Papyrus 側の互換性**：`GlobalVariable.GetValue()` は underlying の型に関わらず **常に Float を返す** ので、`TETHER_MCM.psc` は変更不要。underlying Short でも `!= 0.0` の bool 判定は正しく動作する。C++ 側も `TESGlobal::value` は float 一択なので同じ。

## Step 3 — Quest を作る

参考modの Bow Charge Plus / Quick Light の MCM quest 構造を踏襲。

### 3-1. Quest 本体

1. `TETHER.esp` を右クリック → **Add** → **Quest**
2. 設定：
   - **EditorID**: `TETHER_MCMQuest`
   - **FULL - Name**: 空でOK（MCM一覧の表示名は `ModName` string で決まるので不要。CK上での識別用に "TETHER" 入れても可）
   - **DNAM - General**:
     - **Flags**: **`Start Game Enabled` チェック** かつ **`Starts Enabled` チェック**（両方！参考modは両方ONで動いてる）
     - **Priority**: `0`
     - **Type**: `None`
   - **Objectives**: 空
   - **Stages**: 空でOK（参考modも stages なし）
   - **Conditions**: 空
   - **ANAM - Next Alias ID**: `1`

### 3-2. Script アタッチ（Quest本体に）

**VMAD - Virtual Machine Adapter (Script)**:
- Version `5`、ObjFormat `2`
- Script 追加：
  - **scriptName**: `TETHER_MCM`
  - **Status**: `Local`
- Properties × 16 を全部追加（Type = `object`、Status = `Edited`）。各 property 名と参照先 Global を以下の対応で紐付け：
    - `TETHER_Hotkey`               → Global `TETHER_Hotkey`
    - `TETHER_OffsetRight`          → Global `TETHER_OffsetRight`
    - `TETHER_OffsetBack`           → Global `TETHER_OffsetBack`
    - `TETHER_FollowRadius`         → Global `TETHER_FollowRadius`
    - `TETHER_CatchupRadius`        → Global `TETHER_CatchupRadius`
    - `TETHER_TetherIdealDist`      → Global `TETHER_TetherIdealDist`
    - `TETHER_TetherSlackDist`      → Global `TETHER_TetherSlackDist`
    - `TETHER_PlayerSpeedMultNear`  → Global `TETHER_PlayerSpeedMultNear`
    - `TETHER_PlayerSpeedMultFar`   → Global `TETHER_PlayerSpeedMultFar`
    - `TETHER_FollowerSpeedMult`    → Global `TETHER_FollowerSpeedMult`
    - `TETHER_PalmOffset`           → Global `TETHER_PalmOffset`
    - `TETHER_AutoRelWeapon`        → Global `TETHER_AutoRelWeapon`
    - `TETHER_AutoRelCombat`        → Global `TETHER_AutoRelCombat`
    - `TETHER_AutoRelExtDist`       → Global `TETHER_AutoRelExtDist`
    - `TETHER_ExtDistThreshold`     → Global `TETHER_ExtDistThreshold`
    - `TETHER_ExtDistDuration`      → Global `TETHER_ExtDistDuration`

**`ModName` プロパティ不要**（Papyrus 側の `OnConfigInit()` 内で `ModName = "TETHER"` を code から設定するので、CK/SSEEdit 側で property 追加する必要なし）。

SSEEditのコツ: Script container 追加した後、script name の行を右クリック → **Add** → **Property** を繰り返す。Property Type = `object` にしてから dropdown で該当 Global を選ぶ。

### 3-3. Player Alias を1つ追加（SkyUI 流儀）

参考mod（Bow Charge Plus）ではMCM quest に Player Alias を1つ持たせて、そこに `SKI_PlayerLoadGameAlias` を付ける。これは SkyUI 標準の "save load 時に MCM script に通知する" ヘルパー。**無くてもMCMは動く**が、参考modに揃えて正式流儀にする：

1. Quest の **Aliases** セクションで **Add** → **Alias**
2. Alias 設定：
   - **ALID - Alias Name**: `TETHER_MCMPlayerAlias`
   - **FNAM - Flags**: `Reserves Reference`（+ お好みで `Quest Object`）
   - **Fill Type**: `Forced Reference` （または `Specific Reference`）
     - **Forced Reference / Target**: `PlayerRef` (FormID `00000014`)
3. Alias に script アタッチ：
   - **VMAD** → Script 追加
     - **scriptName**: `SKI_PlayerLoadGameAlias`
     - **Status**: `Local`
     - Properties: 空でOK（このscript は property 不要）

（`SKI_PlayerLoadGameAlias.pex` は SkyUI SDK に含まれてる。既に SkyUI SDK 入れてれば `Data/Scripts/SKI_PlayerLoadGameAlias.pex` があるはず）

## Step 4 — TETHER_MCM.psc をコンパイル

Creation Kit を起動。Papyrus エディタ（Gameplay → Papyrus Script Manager）で：

1. `TETHER_MCM.psc` を開く
2. Compile（build）実行。masters がない系のエラーが出たら → SkyUI SDK が scripts source path に入ってるか確認
3. 出力: `Data/Scripts/TETHER_MCM.pex`

CK が `SKI_ConfigBase` を未定義と言うなら、SkyUI SDK の `SKI_ConfigBase.psc` が `Data/Scripts/Source/` に入ってるか確認してくれ。

## Step 5 — 動作確認

1. SSEEdit で ESP を save、SSEEdit 終了
2. MO2（またはあなたの mod manager）で `TETHER.esp` を有効化
3. ゲーム起動、セーブロード
4. **システム → Mod Configuration** を開く。一覧に **TETHER** が表示されるはず
5. TETHER を開く。3ページ（General / Position / Tension & Grip）が表示され、全項目が操作可能なはず
6. どれかスライダで **R キー** 押して個別リセット動作確認。General ページの **Reset All Settings** で全リセット確認

## Step 6 — 完了報告

MCMが表示されて全項目操作できることを確認したら教えてくれ。次に **M3**（C++プラグイン側で Globals を実行時に読んで `constexpr` を置換）に進む。M3 は CK作業なしの俺の担当。

---

### 命名リマインダー（バグ回避のため）

- ESP ファイル名は **必ず `TETHER.esp`**。C++ プラグインが `LookupForm(FormID, "TETHER.esp")` するので、違うファイル名だとプラグインは silently ハードコードデフォルトにフォールバック（＝MCMスライダで数値変えても効かない）
- Global の EditorID は **大文字小文字も含めて完全一致必須**。理由同上
- Script 名は **必ず `TETHER_MCM`**（ESPの Quest script alias がこのファイル名を参照）
