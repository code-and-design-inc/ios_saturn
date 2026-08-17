# SEGA Rally Championship iOS Tests

このディレクトリには、iOS版SEGA Rally Championshipリーダーボード機能の単体テストが含まれています。

## ファイル構成

- `SegaRallyTests.swift` - メインのテストクラス
- `SegaRallyTestRunner.swift` - テスト実行とレポート生成
- `SegaRallyTests_README.md` - このファイル

## テスト対象

### 1. SegaRallyRecord クラス
- 初期化テスト
- 総ミリ秒変換テスト
- ゼロ値テスト

### 2. SegaRallyBackup クラス
- バイナリデータ解析テスト
- 3ステージ（Desert, Forest, Mountain）のデータ抽出テスト
- 小さなデータサイズでの動作テスト

### 3. SegaRally メインクラス
- 初期化テスト
- リーダーボード設定テスト
- バックアップ更新処理テスト

### 4. ユーティリティ関数
- タイムフォーマット関数テスト

### 5. パフォーマンステスト
- バックアップ解析のパフォーマンス測定
- 総ミリ秒変換のパフォーマンス測定

## テストの実行方法

### Xcodeでの実行

1. Xcodeでプロジェクトを開く
2. テストナビゲーターを開く（⌘+6）
3. `uoyabauseTests` > `SegaRallyTests` を選択
4. 個別のテストまたは全テストを実行

### コマンドラインでの実行

```bash
# プロジェクトディレクトリで実行
xcodebuild test -workspace YabaSnashiro.xcworkspace -scheme YabaSnashiro -destination 'platform=iOS Simulator,name=iPhone 14'
```

### テストランナーの使用

```swift
// 全テストを実行
SegaRallyTestRunner.runAllTests()

// カテゴリ別テスト実行
SegaRallyTestRunner.runCategoryTests(.record)
SegaRallyTestRunner.runCategoryTests(.backup)
SegaRallyTestRunner.runCategoryTests(.game)
SegaRallyTestRunner.runCategoryTests(.utility)

// パフォーマンステスト実行
SegaRallyTestRunner.runPerformanceTests()

// テストデータ検証
SegaRallyTestRunner.validateTestData()
```

## テストデータ

### バイナリデータ構造

SEGA Rallyのバックアップデータから以下のアドレスでタイム情報を抽出：

- **Desert Stage**: 
  - Minutes: 0x0909
  - Seconds: 0x090A
  - Milliseconds: 0x090B

- **Forest Stage**:
  - Minutes: 0x0BA9
  - Seconds: 0x0BAA
  - Milliseconds: 0x0BAB

- **Mountain Stage**:
  - Minutes: 0x0E49
  - Seconds: 0x0E4A
  - Milliseconds: 0x0E4B

### データ変換ロジック

```swift
record.minutes = minByte & 0x0F
record.seconds = ((secByte >> 4) & 0x0F) * 10 + (secByte & 0x0F)
record.milliseconds = (((msecByte >> 4) & 0x0F) * 10 + (msecByte & 0x0F)) * 10
```

## 期待される結果

### 正常なテスト実行結果

```
=== SEGA Rally Tests Starting ===
Running: testSegaRallyRecordInitialization
✅ PASSED: testSegaRallyRecordInitialization
Running: testSegaRallyRecordToTotalMilliseconds
✅ PASSED: testSegaRallyRecordToTotalMilliseconds
...
=== SEGA Rally Test Results ===
Total Tests: 10
Passed: 10
Failed: 0
Success Rate: 100.0%
🎉 All tests passed!
```

## トラブルシューティング

### よくある問題

1. **Firebase関連のエラー**
   - テスト環境ではFirebase認証が無効なため、認証が必要なテストは基本動作のみ確認

2. **メモリアドレスエラー**
   - テストデータのサイズが十分であることを確認
   - アドレス範囲がデータサイズ内であることを確認

3. **タイムフォーマットエラー**
   - 負の値や異常に大きな値の処理を確認

### デバッグ方法

1. **ログ出力の確認**
   ```swift
   print("Debug: \(backup.records[0].minutes):\(backup.records[0].seconds).\(backup.records[0].milliseconds)")
   ```

2. **バイナリデータの確認**
   ```swift
   for i in 0..<testData.count {
       print(String(format: "%04X: %02X", i, testData[i]))
   }
   ```

## 統合テストとの連携

このテストは単体テストです。統合テストでは以下を確認してください：

1. 実際のゲームプレイでのバックアップ更新
2. Firestoreへのスコア送信
3. Discord通知機能
4. リーダーボード表示

## 継続的インテグレーション

CI/CDパイプラインでこれらのテストを自動実行することを推奨します：

```yaml
# GitHub Actions例
- name: Run iOS Tests
  run: |
    xcodebuild test \
      -workspace YabaSnashiro.xcworkspace \
      -scheme YabaSnashiro \
      -destination 'platform=iOS Simulator,name=iPhone 14' \
      -only-testing:uoyabauseTests/SegaRallyTests
```

## 貢献

新しいテストケースを追加する場合：

1. `SegaRallyTests.swift`に新しいテストメソッドを追加
2. `SegaRallyTestRunner.swift`のテストメソッドリストを更新
3. このREADMEを更新
4. テストが通ることを確認してからプルリクエストを作成

## ライセンス

このテストコードは、メインプロジェクトと同じライセンスの下で提供されます。
