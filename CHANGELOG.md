# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.19.0] - 2026-02-08

### Added
- Material Design 3 に準拠した全面的なUI刷新
- Navigation Drawer を Bottom Navigation Bar / NavigationRail に移行 (#77)
- 独立したバックアップマネージャー (#74)
- バックアップデータの共有機能
- ゲームフォルダ管理UI (Add Game Bottom Sheet) の追加 (#76)
- RetroAchievements ログインダイアログに「アカウント作成」リンクを追加

### Fixed
- v18→v19アップデート後にSettings画面がクラッシュする問題 (#79)
- ログイン成功時に「キャンセルされました」と誤表示される問題 (#78)
- 画面回転時にゲーム画面が透けて見える問題 (#82)
- 画面回転時にAddGameBottomSheetが重複表示される問題 (#81)
- 初回起動時にフォルダ追加後ゲームリストが更新されない問題 (#87)
- バックアップマネージャーでフリック時にタイトルバーが切り替わる問題 (#91)
- アラートダイアログのボタンにゲームパッドフォーカスが当たらない問題 (#93)
- フルスクリーンからアスペクト比変更時の黒画面・中央配置の問題 (#88)
- 小画面の横画面でボタンが見切れる問題の修正と縦画面レグレッション修正 (#89)

## [1.18.13] - 2025-12-21

### Added
- Initial changelog setup

### Changed
- Release workflow improvements

### Fixed
- Various bug fixes
