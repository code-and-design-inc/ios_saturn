# Pro/サブスクリプション限定機能一覧

## 判定ロジック

`YabauseApplication.isPro()` が中心的なゲート関数。以下のいずれかで `true` を返す:

| 条件 | 説明 |
|------|------|
| `isSubscriptionDisabledByRemoteConfig` | Remote Config でサブスクリプション無効化時は全員Pro扱い |
| `BuildConfig.BUILD_TYPE == "pro"` | Pro版ビルドバリアント |
| `SharedPreferences["donated"] == true` | レガシー寄付フラグ |
| `SharedPreferences["subscribed"] == true` | Google Play サブスクリプション有効 |

**定義箇所**: `YabauseApplication.kt:200-218`

---

## サブスクリプション商品

| 商品ID | 名称 | 用途 |
|--------|------|------|
| `premium` | Pro Annual | 年間サブスクリプション (主力商品) |
| `cloud_functions` | Basic | 基本サブスクリプション | <- 削除
| `up_premium_sub` | Premium | プレミアムサブスクリプション | <- 削除

**定義箇所**: `BillingClientWrapper.kt:260-264`

---

## 機能制限一覧

### 1. ゲームライブラリ上限

| | Free | Pro |
|---|---|---|
| 登録可能ゲーム数 | **3本** | **無制限** |

- `BuildConfig.MAX_FREE_GAMES = 3`
- **Phone**: `GameSelectFragmentPhone.kt:1151`, `GameSelectViewModel.kt:229-241`
- **TV**: 同様のロジックあり

### 2. ゲームフォルダスキャン

| | Free | Pro |
|---|---|---|
| 全フォルダスキャン | 不可 | 可能 |
| フォルダ個別スキャン | 不可 | 可能 |
| フォルダ追加 | 不可 | 可能 |

- Free版ではスキャンセクション全体が半透明 (`alpha = 0.5f`) で表示
- **箇所**: `AddGameBottomSheetFragment.kt:136, 149, 168, 189`

### 3. ゲームインストール (ファイル選択)

| | Free | Pro |
|---|---|---|
| インストール回数 | **3回まで** (`InstallCount`) | **無制限** |

- 残り回数0でファイル選択時に `checkDonated()` ダイアログ表示
- 設定画面に「残りインストール回数」を表示 (`SettingsActivity.kt:324-337`)
- **箇所**: `GameSelectFragmentPhone.kt:275-288`, `SettingsActivity.kt:324`

### 4. クラウドチート

| | Free | Pro |
|---|---|---|
| クラウドチート閲覧・使用 | 不可 | 可能 |

- Firebase Auth ログインも必須
- **箇所**: `CloudCheatItemFragment.kt:86`

### 5. アクションリプレイ (ゲーム内チート)

| | Free | Pro |
|---|---|---|
| アクションリプレイ | 不可 | 可能 |

- ゲーム実行中メニューの「ACP」項目
- **箇所**: `Yabause.kt:1191-1196`

### 6. クラウドバックアップ

|                  | Free                 | Pro      |
|------------------|----------------------|----------|
| クラウドバック検索・閲覧     | 可能                   | 可能       |
| クラウドバックアップへの保存   | 不可（ローカルデバイス同士は可能にする) | **256件** |
| バックアップデータへの共有    | 不可                   | 可能   | 
| クラウドバックアップからのコピー | 不可                   | 可能       |　 


- `DEFAULT_MAX_BACKUP_COUNT = 3`, `PRO_MAX_BACKUP_COUNT = 256`
- バックアップリスト画面でアクション実行時にゲート
- **箇所**: `FirebaseBackupDataSource.kt:67-68, 175-182`, `BackupListFragment.kt:542`
- **箇所 (レガシー)**: `GameSelectPresenter.kt:229-237`

### 7. 共有バックアップ (コミュニティ)

|               | Free | Pro |
|---------------|---|---|
| 共有バックアップ検索・閲覧 | 可能 | 可能 |
| 共有バックアップの評価   | 不可 | 可能 |　<- 追加
| 共有バックアップのコピー  | 不可 | 可能 |　<- 追加

- **箇所**: `SharedBackupSearchFragment.kt:494`

### 8. 広告

| | Free | Pro |
|---|---|---|
| バナー広告 | あり | **なし** |

#### バナー広告
- ゲーム選択画面にAdMobバナーを表示
- **箇所**: `GameSelectActivityPhone.kt:196, 438`

#### インタースティシャル広告 (全画面広告)
- ゲーム終了後にランダム確率で表示
- **Phone**: 30%でAdActivity、30%でAdActivity、残りでレビュー依頼候補 (`GameSelectFragmentPhone2.kt`) <- 変更

### 外部アプリからの起動

|                                                 | Free | Pro |
|-------------------------------------------------|------|-----|
| org.uoyabause.android.FileNameUriを使った外部アプリからの起動 | 不可  | 可能 | <- 追加

### 9. クロスデバイス同期 (PIN 生成)

| | Free | Pro |
|---|---|---|
| PIN 生成 (Android → Qt 版ログイン) | 不可 | 可能 |
| PIN 再発行 | 不可 | 可能 |

- Qt 版 (Windows / Mac / Linux) のバックアップマネージャー等ログイン必須機能は、Android で生成した PIN によるペアリングが前提。PIN 生成自体を Pro 限定にすることで Qt 側のログインを実質的に閉じる。
- 既にペアリング済みの Qt クライアントは、Pro 解約後もセッションが残る限り使用可能 (クライアント側ゲートのみ)。
- **箇所 (ペアリング PIN ボタンクリック)**: `auth/ui/AccountManagementFragment.kt:988, 1065`, `auth/ui/SimpleAccountManagementActivity.kt:1074, 1179`
- **箇所 (ペアリング PIN 表示 defense-in-depth)**: `auth/ui/AccountManagementFragment.kt` `showGeneratedPin()` 冒頭, `auth/ui/SimpleAccountManagementActivity.kt` `showGeneratedPin()` 冒頭 — Pro でなければ即 `clearGeneratedPin()` で破棄。
- **箇所 (deep link `//login` の login PIN)**: `phone/GameSelectFragmentPhone2.kt:298`, `phone/GameSelectFragmentPhone.kt:1486`, `tv/GameSelectFragment.kt:476` — `StartupActivity` の `showPin=true` extra で `ShowPinInFragment` を表示する経路。`isPro()` チェックを挟み、Free なら `checkDonated()` のみ表示して PIN fetch 自体を skip。

---

## ゲートUI

Pro未購入ユーザーが制限機能にアクセスした場合、`YabauseApplication.checkDonated()` が `MaterialAlertDialog` を表示し、サブスクリプション購入を促す。

**定義箇所**: `YabauseApplication.kt:289`

---

## コメント付きで無効化されている旧制限 (参考)

| 機能 | 箇所 | 状態 |
|------|------|------|
| ゲーム削除の制限 | `GameItemAdapter.kt:691-708` | コメントアウト済み |
| Yabause内バナー広告 | `Yabause.kt:1902-1917` | コメントアウト済み |
| TV版バックアップ最大数 | `tv/GameSelectFragment.kt:396-397, 648-649` | コメントアウト済み |
