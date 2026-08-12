# TETHER — Hand-Holding for Followers

Skyrim SE / AE 用 SKSE プラグイン。フォロワーとの継続的な hand-hold（歩行・走行・地形移動を通して手が離れない接続）を Havok ball-and-socket constraint と kinematic な位置制御の組み合わせで実装する。

- **バージョン**: 1.0.0
- **ソース**: https://github.com/Kei-BWV997/TETHER
- **配布**: LoversLab（無料 / オープンソース）
- **ライセンス**: MIT（ソース）／ CC0（ドキュメント）／ 第三者由来ファイルの扱いは `LICENSE` 参照

---

## 開発体制の開示 — このリポジトリと AI について

**このプラグインのコードは大部分が Claude Code (Anthropic) によって生成されている**。作者 (KEI997) の役割は次の通り：

- 仕様の決定・優先順位付け・トレードオフ判断
- 実機テストとバグ報告
- 参考にすべき他 mod ソースを AI に指示
- 生成コードのレビューと採否判断
- SSEEdit による ESP 作成、PCA SE による Papyrus コンパイル、OAR 用アニメーション制作

**AI に完全自動生成させたわけではない**が、C++ の細部（トランポリンフック、CommonLibSSE-NG API の使い方、Papyrus VM dispatch の書き方、OAR SDK 統合の詳細）は AI 出力そのままか軽度のレビュー修正で採用したものが多い。この点は先に開示する。

AI が参照した他 mod のソースコードは全て公開ライセンス下のもの。参照元は次節の対応表で明示する。もし対応表内の作者から「うちのソースをこの用途に参照するのは困る」と申し出があれば、該当機能を実装し直すかクレジットの扱いを変えるかで対応する。連絡先は LoversLab の PM か GitHub Issues。

---

## 実装別・参考ソース対応表

以下は AI が読み込んだ・参考にした他 mod / リポジトリ。技術的な由来は隠さずに書く。

| TETHER の機能 | 参考ソース | 借りた技術・パターン |
|---|---|---|
| SKSE プラグインフレーム | CommonLibSSE-NG (Ryan-rsm-McKenzie / powerof3) | ランタイム API 一式 |
| CMake / vcpkg / CMakePresets 構成 | [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347) (Ersh) | ビルド雛形（toolset ピン留めや vcpkg baseline を含む） |
| Post-behavior アニメーション層フック | FeetOfSkyrim (fenix31415) | `NiUpdateData` を用いた明示 world 伝播の書き方 |
| `KeepOffsetFromActor` の呼び出しと初期パラメータ | [FollowersAsCompanions](https://www.nexusmods.com/skyrimspecialedition/mods/27010) (Alek323) | Papyrus 関数のシグネチャと FollowRadius / CatchUpRadius の実績値 |
| フォロワー AI 管理系 mod との共存原則 | [FOMS](https://www.nexusmods.com/skyrimspecialedition/mods/94619) | 「一時上書き→復帰」の思想（実装はより単純） |
| `Actor::SetMaximumMovementSpeed` トランポリンフック | [Wade In Water Redone](https://www.nexusmods.com/skyrimspecialedition/mods/106054) / [Keep Up!](https://www.nexusmods.com/skyrimspecialedition/mods/173685) / [Dragonborn's Cohort](https://www.nexusmods.com/skyrimspecialedition/mods/184643)（すべて TESFAN） | `REL::RelocationID(37013, 37943) + REL::Relocate(0x1A, 0x51)` に `stl::write_thunk_call` するパターン |
| MCM（SkyUI `SKI_ConfigBase` 継承）の Papyrus 構造 | [Bow Charge Plus](https://www.nexusmods.com/skyrimspecialedition/mods/56537) / [Quick Light SE](https://www.nexusmods.com/skyrimspecialedition/mods/25439) / [Simple Recycling](https://www.nexusmods.com/skyrimspecialedition/mods/2130) | Quest + GlobalVariable + Player Alias with `SKI_PlayerLoadGameAlias` の構成 |
| OAR カスタム条件 SDK | [Open Animation Replacer](https://www.nexusmods.com/skyrimspecialedition/mods/92109) (Ersh) + [RaySense](https://www.nexusmods.com/skyrimspecialedition/mods/143856) (Smooth) | `src/API/*.h/cpp` を丸ごとコピー（OAR SDK として再配布を想定されている一式）。`AddCustomCondition<T>()` の使用 |
| OAR JSON 構造 / hkx ファイル配置慣習 | Gulan0 walk_run_sprint - OAR | `config.json` の書き方、priority 値、`male`/`female` サブフォルダの慣習 |
| `AddHavokBallAndSocketConstraint` を alive actor で機能させる方法 | asdsad121 の [Nexus フォーラム投稿 (2024)](https://forums.nexusmods.com/topic/6281756-attaching-an-object-to-an-actor/) | `ForceAddRagdollToWorld()` を両アクターに先に呼ぶと constraint が効くという発見。Papyrus wiki の "只の意識のあるアクターには効かない" 記述は古い |
| Constraint 方式採用の動機 | [@Ashihito1 のツイート動画](https://x.com/Ashihito1/status/2086362692328247323) | プレイヤーが NPC の腕を掴んで引っ張る挙動を実演した映像。kinematic IK では出せない「接着面が一致した握手感」を確認して方針転換の判断材料になった |
| HIGGS のフック層概念（参考のみ） | HIGGS (adamhynek) | 「アニメ確定後・NiNode::Update 直前」レイヤの概念。実際には kinematic IK を最終的に構造から外したため、当該フックは使用していない |

---

## 機能

### 1. Havok ball-and-socket constraint による手の物理接着

engage 時に次の順で Papyrus VM dispatch を発行：

```
player.ForceAddRagdollToWorld()
follower.ForceAddRagdollToWorld()
Game.AddHavokBallAndSocketConstraint(
    player,   "NPC L Hand [LHnd]",
    follower, "NPC R Hand [RHnd]",
    0, 0, palmOffset,   // player  local offset (Z = 手首から掌方向)
    0, 0, palmOffset)   // follower 同上
```

release 時：

```
Game.RemoveHavokConstraints(player, "NPC L Hand [LHnd]", follower, "NPC R Hand [RHnd]")
player.ForceRemoveRagdollFromWorld()
follower.ForceRemoveRagdollFromWorld()
```

`ForceAddRagdollToWorld` を先に呼ばないと constraint は生きたアクターには効かない（前述の asdsad121 の投稿を参照）。実装当初は kinematic な 2-bone IK で毎フレーム両腕を解いて接続していたが、接着面の座標が微妙にズレて「握っている感」が弱かったため constraint 方式に切り替えた（kinematic IK 側のコードは `#if 0` 相当のフラグで無効化して残置してある）。

### 2. Elastic tether — 距離依存の速度キャップ

`Actor::SetMaximumMovementSpeed` の呼び出しにトランポリンフックを刺し、tether 中のプレイヤーとフォロワーそれぞれに対して距離依存の乗算補正を返す。

- プレイヤーの max movement speed は、フォロワーとの距離に応じて **1.0x → 0.35x** (デフォルト値、MCM で可変) で線形補間
- フォロワーの max movement speed は tether 中常時 1.3x

`SetActorValue("SpeedMult")` や `ModActorValue` では効かなかった。engine の最終速度計算がこのメソッドで確定するため、そこにフックするのが確実。この技術は TESFAN の Wade In Water Redone から借りた。

急な加速でフォロワーが AI 反応遅延で置き去りになる問題への対策。プレイヤー側の脚を重くする方式にした理由は、フォロワーだけブーストしても sprint モードには追いつかないため、および「手を繋いだ状態で急ダッシュしたら相手はついていけない」という物理的な自然性。

### 3. `KeepOffsetFromActor` によるフォロワー位置制御

engage 時に Papyrus VM dispatch で `Actor.KeepOffsetFromActor(playerRef, offsetX, offsetY, 0, 0, 0, 0, catchUpRadius, followRadius)` を発行。release 時に `ClearKeepOffsetFromActor()`。

デフォルトはプレイヤーの真後ろ約 43 cm（Y = -30 unit）、FollowRadius = 30、CatchupRadius = 60。左右オフセットは 0（真後ろ）。全て MCM で可変。

FOMS 等のフォロワー AI 管理系 mod との共存メカニズム：`KeepOffsetFromActor` は engine 標準の「現在アクティブなパッケージ上に一時オフセット指示を積む」機構。TETHER は他 mod のパッケージ設定・カスタム AI 定義には触らず、engage 中だけこの指示を投げて release で明示解除する。他 mod は TETHER の存在を知らない状態で問題なく動作する。

### 4. OAR カスタム条件

プラグイン起動時（`SKSE::MessagingInterface::kPostLoad`）に Open Animation Replacer に 2 つのカスタム条件を登録：

- `TETHER_IsTetheredPlayer` — 現在の tether のプレイヤー側アクターで true
- `TETHER_IsTetheredFollower` — 現在の tether のフォロワー側アクターで true

`IsFemale` 等の OAR 標準条件と組み合わせて、最大 4 パターン（プレイヤー男 / プレイヤー女 × フォロワー男 / フォロワー女）まで演技を分けて配置できる仕組みを OAR 側 (`Data/meshes/actors/character/animations/OpenAnimationReplacer/TETHER_run/`) に用意している。

**v1.0.0 時点でモーションが実装されているのはプレイヤー男（引く側）のみ**。他 3 パターン用のフォルダには Gulan0 の `walk_run_sprint - OAR` からコピーしたバニラ準拠の dummy hkx が入っているだけで、tether 中に該当アクターが動いてもバニラの走行アニメが再生される。他パターンは今後の追加、または hkx を差し替えたコミュニティ pack 待ちになる。

### 5. 状態遷移の安全処理

Havok constraint はアクターの状態変化タイミング（セル移動、セーブロード、死亡、他 mod による ragdoll 解除）で処理を挟むと CTD しやすい。以下を検知して先制的に release する：

- worldspace 変化（外→室内、別 worldspace への fast travel）
- interior 境界跨ぎ（部屋→部屋のドア通過）
- `SKSE::MessagingInterface::kPreLoadGame`（セーブロード直前）
- プレイヤー / フォロワーの `Get3D()` が一時的に null

exterior→exterior の見えないセル境界跨ぎ（Skyrim の外世界は 4096 unit グリッドで区切られていて、普通に走ってると `parentCell` が頻繁に変わる）は 3D が valid なままなので release せず、engage 時の cell tracker を更新するだけ。

### 6. 自動 release トリガー（MCM で切替）

- **抜刀**（デフォルト ON）— プレイヤーが武器を抜くと release。intent が「戦う」に切り替わったと解釈
- **戦闘発生**（デフォルト OFF）— 戦闘中も手を引いて逃げるシチュエーションを残すためデフォルトでは無効
- **極距離継続**（デフォルト OFF）— 階段でフォロワーだけ落ちた等のケースは手動 release を優先

### 7. MCM Helper 統合

- ホットキー変更（キーボード / Xbox コントローラ両対応、LT / RT / A / B / X / Y / DPad / shoulder buttons）
- KeepOffset 位置、elastic tether の効きしろ、速度キャップ、掌接着深さ、自動 release トリガー等の全パラメータをスライダとトグルで露出
- R キーによる項目別デフォルト戻し（SkyUI 標準）と "Reset All Settings" ボタン

内部実装は SkyUI SDK の `SKI_ConfigBase` を継承した Papyrus クラス。MCM Helper 経由ではなくクラシックな SkyUI MCM 実装。C++ 側は `TESForm::LookupByEditorID` で ESP の GlobalVariable を参照する。EditorID 保持のため powerofthree's Tweaks が必須。

---

## 依存関係

### 必須
- Skyrim SE / AE（1.6.11xx 系推奨）
- SKSE64 / SKSE-AE
- Address Library for SKSE Plugins (AE 版)
- SkyUI 5.2 SE
- Open Animation Replacer (3.0 以降)
- powerofthree's Tweaks — EditorID 保持のため

### 推奨
- Keep Up! Follower Locomotion Fix (TESFAN) — フォロワーが階段や複雑地形で stuck する engine bug を修正。TETHER 単体でも動作するが、フォロワー置いてかれ問題を最小化するために強く推奨

### 動作確認済みの共存
- FOMS (Follower Organized Management System)
- True Directional Movement

---

## インストール

1. 依存関係を全て導入
2. TETHER パッケージ（DLL、ESP、MCM script、OAR パック）を mod manager で有効化
3. ゲーム起動、セーブロード
4. MCM に "TETHER" が表示されることを確認

正常起動時のログ (`Documents\My Games\Skyrim Special Edition\SKSE\TETHER.log`)：

```
TETHER v1-0-0-0
TETHER loaded
OAR condition register (TETHER_IsTetheredPlayer): OK
OAR condition register (TETHER_IsTetheredFollower): OK
Settings::Load: 16/16 globals bound from TETHER.esp
InputHandler registered ...
PlayerUpdateHook installed ...
SpeedHook installed ...
kDataLoaded: TETHER ready
```

`Settings::Load: 0/16` の場合は EditorID が読めていない。powerofthree's Tweaks の有効化と TETHER.esp の有効化を確認。

---

## 使い方

1. プレイヤーが納刀した状態で
2. フォロワーを正面に見て、ホットキー **H**（MCM で変更可）を押す
3. release も同じホットキー
4. 抜刀すると自動 release

---

## 既知の問題

- 一人称視点非対応
- 同時 tether は 1 本のみ
- 抜刀状態での手つなぎ走り非対応
- `ForceAddRagdollToWorld` の副作用で、アクター state の突発的変化（死亡、外部 mod による ragdoll 操作等）のタイミングによっては稀に CTD の可能性が残る。自動 release でカバーしきれない可能性がある
- **engage 時にフォロワーがその場から動かないことがある**。ホットキーを押して constraint が張られたのにフォロワーが停止したままになる症状。**一度 release して、少し距離を取ってから再度 engage** で解消する。原因未特定（`KeepOffsetFromActor` の適用前に他 mod のパッケージが優先されている可能性）。実用上のワークアラウンドは上記の通り

---

## MCM 設定項目

### General
- Toggle Key（キーボード / コントローラ）
- Auto-Release on Weapon Drawn（デフォルト ON）
- Auto-Release on Combat Start（デフォルト OFF）
- Auto-Release on Extreme Distance（デフォルト OFF）
- Distance Threshold / Duration（上記の詳細）
- Reset All Settings

### Position
- Right / Back offset
- Follow Radius / Catchup Radius

### Tension & Grip
- Ideal / Slack Distance
- Player Speed Near / Far
- Follower Speed Boost
- Palm Offset

---

## ライセンス

- ソースコード (`src/`, `mcm/`, `oar/*/config.json`) — MIT
- ドキュメント (`README.md`, `docs/`, `mcm/docs/`) — CC0
- `src/API/OpenAnimationReplacer-*.h/cpp` — Open Animation Replacer 側の SDK ファイルを取り込んだもの。OAR プロジェクトのライセンスに従う（Ersh の元プロジェクトが MIT 相当）

詳細は `LICENSE`。

---

## Changelog

### 1.0.0 (2026-08-12) — 初回リリース
- Havok ball-and-socket constraint による手の接続
- Elastic tether（距離依存プレイヤー速度キャップ）
- MCM Helper 統合（キーボード / コントローラ両対応）
- OAR カスタム条件 2 種の登録
- OAR パック（プレイヤー男 authored + 他 3 パターン用のフォルダ構造と dummy hkx）
- worldspace-aware auto-release による cell 遷移時の CTD 対策
