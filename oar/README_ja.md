# TETHER — OAR ロコモーションパック

TETHER のカスタム OAR 条件を使って、手つなぎ中のプレイヤーとフォロワーそれぞれに専用の walk/run/sprint モーションを差し替えるパック。

## 構造

```
Data/meshes/actors/character/animations/OpenAnimationReplacer/TETHER_run/
├── config.json              ← mod-level メタ
├── PlayerMale/
│   ├── config.json          ← 条件: NOT IsFemale + TETHER_IsTetheredPlayer
│   └── male/                ← .hkx はここ (mt_walkforward.hkx 等)
├── PlayerFemale/
│   ├── config.json          ← 条件: IsFemale + TETHER_IsTetheredPlayer
│   └── female/
├── FollowerMale/
│   ├── config.json          ← 条件: NOT IsFemale + TETHER_IsTetheredFollower
│   └── male/
└── FollowerFemale/
    ├── config.json          ← 条件: IsFemale + TETHER_IsTetheredFollower
    └── female/
```

## 4パターン × 11本 = 44本

各パターンフォルダに **11個のダミー .hkx** を配置済み（`Gulan0_walk_run_sprint` mod のバニラ準拠モーションをコピー）：

- mt_walkforward / mt_walkleft / mt_walkright / mt_walkforwardleft / mt_walkforwardright
- mt_runforward / mt_runleft / mt_runright / mt_runforwardleft / mt_runforwardright
- mt_sprintforward

**あなたが authored した .hkx で置換**することで、TETHER 中のポーズが有効化される。

## インストール手順

1. `oar/Data/` の中身を Skyrim の `Data/` にコピー（または MO2 で mod として追加）
2. TETHER.dll (v0.1.0.0以降) を導入 — カスタム条件 `TETHER_IsTetheredPlayer` / `TETHER_IsTetheredFollower` を登録
3. ゲーム起動 → tether engage → OAR が該当フォルダのモーションに差し替え

## 動作確認

TETHER.log に以下が出るはず：
```
OAR condition register (TETHER_IsTetheredPlayer): OK
OAR condition register (TETHER_IsTetheredFollower): OK
```

engage 後、OAR の in-game UI (デフォ Shift+O) で **Preview** ボタン押して見ると、PlayerMale/Female 等のサブモッドが Active 状態になってるはず。

## モーション差し替え時の注意

- ファイル名は **vanilla と一致**必須（`mt_walkforward.hkx` 等）
- **root motion 保持**（水平移動距離があるアニメじゃないとフォロワーが KeepOffset で置いてかれる）
- **T-pose ではなく手つなぎポーズ**（片手が握り点方向を向いてる）で authoring
- プレイヤーは左手、フォロワーは右手を「握り点」に向けて出したポーズ
- Constraint と kinematic 併用時: constraint が腕位置を最優先で決めるので、authored ポーズの腕位置は多少無視される可能性（未検証。テスト報告求む）

## 現状の条件

- **priority: 10** — vanilla（0）や一般 OAR mod より優先される
- **IsChild negated**: 子供アクターには適用しない（安全）
- **IsFemale**: 男女判定
- **TETHER_IsTetheredXxx**: TETHER プラグイン提供のカスタム条件

Priority や条件を調整したい場合は各 `config.json` を編集。
