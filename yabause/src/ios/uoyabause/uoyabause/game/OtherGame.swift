import Foundation
import FirebaseFirestore
import FirebaseAuth
import FirebaseAnalytics
import FirebaseRemoteConfig
import UIKit

/// 未対応ゲーム（SEGA Rally、SonicR以外）のクラス
/// RetroAchievements統合とFirebase基本機能のみサポート
class OtherGame: BaseGame {

    init(gameCode: String) {
        super.init()
        
        NSLog("OtherGame: Initialized for game code: \(gameCode)")
        
        // RetroAchievements support用にgameIdを初期化
        initGameId(gameCode)
        
        // Firebase Remote Configの初期化（基本設定のみ）
        let remoteConfig = RemoteConfig.remoteConfig()
        let settings = RemoteConfigSettings()
        settings.minimumFetchInterval = 3600 // 1時間ごとに更新
        remoteConfig.configSettings = settings

        // デフォルト値の設定（汎用Discord webhook）
        let defaultValues: [String: NSObject] = [
            "discord_webhook_url_general": "" as NSObject
        ]
        remoteConfig.setDefaults(defaultValues)

        // Remote Configの値を取得
        remoteConfig.fetch { status, error in
            if status == .success {
                NSLog("OtherGame: Remote Config fetched successfully")
                remoteConfig.activate { _, error in
                    if let error = error {
                        NSLog("OtherGame: Error activating Remote Config: \(error.localizedDescription)")
                    } else {
                        NSLog("OtherGame: Remote Config activated successfully")
                        // Discord webhook URLの確認
                        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_general").stringValue ?? ""
                        NSLog("OtherGame: General Discord webhook URL: \(webhookUrl.isEmpty ? "Not set" : "Set")")
                    }
                }
            } else {
                NSLog("OtherGame: Error fetching Remote Config: \(error?.localizedDescription ?? "unknown error")")
            }
        }

        // Firestoreからゲーム情報を取得（共通処理）
        let db = Firestore.firestore()
        db.collection("games")
            .whereField("product_number", isEqualTo: gameCode)
            .getDocuments { [weak self] snapshot, error in
                guard let self = self, let snapshot = snapshot, !snapshot.documents.isEmpty else {
                    NSLog("OtherGame: No game found in Firebase for product_number: \(gameCode)")
                    return
                }

                // leaderboardIdフィールドがある場合はその値を使用
                if let leaderboardId = snapshot.documents[0].get("leaderboardId") as? String {
                    self.gameId = leaderboardId
                } else {
                    // なければドキュメントIDを使用
                    self.gameId = snapshot.documents[0].documentID
                }
                
                NSLog("OtherGame: Firebase gameId set to: \(self.gameId)")
            }
    }

    /// バックアップデータが更新されたときに呼ばれるメソッド（何もしない）
    override func onBackUpUpdated(fname: String, before: Data, after: Data) {
        // OtherGameでは特別な処理は行わない
        // RetroAchievements統合は別途処理される
        NSLog("OtherGame: Backup updated for \(fname) (no specific handling)")
    }
    
    /// Discordに新記録を通知する（汎用）
    override func notifyDiscord(gameId: String, leaderboardName: String, score: Int64, userName: String, photoURL: String?) {
        // Firebase Remote Configから汎用webhook URLを取得
        let remoteConfig = RemoteConfig.remoteConfig()
        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_general").stringValue ?? ""

        // webhook URLが空の場合は処理を中止
        guard !webhookUrl.isEmpty else {
            NSLog("OtherGame: General Discord webhook URL is empty. Skipping notification.")
            return
        }

        NSLog("OtherGame: Using general Discord webhook URL for notification")

        // Discordに新記録を送信
        DiscordWebhook.sendNewRecordMessage(
            webhookUrl: webhookUrl,
            gameId: gameId,
            leaderboardName: leaderboardName,
            userName: userName,
            score: score,
            avatarUrl: photoURL
        ) { success in
            if success {
                NSLog("OtherGame: Successfully posted record to Discord")
            } else {
                NSLog("OtherGame: Failed to post record to Discord")
            }
        }
    }
}