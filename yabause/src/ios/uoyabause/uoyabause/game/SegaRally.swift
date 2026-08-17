import Foundation
import FirebaseFirestore
import FirebaseAuth
import FirebaseAnalytics
import FirebaseRemoteConfig
import UIKit

/// SEGA Rallyのレコード情報を保持するクラス
class SegaRallyRecord {
    var minutes: Int = 0
    var seconds: Int = 0
    var milliseconds: Int = 0

    /// 比較と保存のために総ミリ秒に変換
    func toTotalMilliseconds() -> Int64 {
        return Int64(minutes * 60000) + Int64(seconds * 1000) + Int64(milliseconds)
    }
}

/// SEGA Rallyのバックアップデータを解析するクラス
class SegaRallyBackup {
    var records: [SegaRallyRecord] = []

    init(bin: Data) {
        // 各ステージのタイミングデータを抽出
        extractStageRecord(bin: bin, minAddr: 0x0909, secAddr: 0x090A, msecAddr: 0x090B) // Desert
        extractStageRecord(bin: bin, minAddr: 0x0BA9, secAddr: 0x0BAA, msecAddr: 0x0BAB) // Forest
        extractStageRecord(bin: bin, minAddr: 0x0E49, secAddr: 0x0E4A, msecAddr: 0x0E4B) // Mountain
    }

    private func extractStageRecord(bin: Data, minAddr: Int, secAddr: Int, msecAddr: Int) {
        let record = SegaRallyRecord()

        if minAddr < bin.count && secAddr < bin.count && msecAddr < bin.count {
            let minByte = Int(bin[minAddr]) & 0xFF
            let secByte = Int(bin[secAddr]) & 0xFF
            let msecByte = Int(bin[msecAddr]) & 0xFF

            // 提供された式を使用してタイミングデータを抽出
            record.minutes = minByte & 0x0F
            record.seconds = ((secByte >> 4) & 0x0F) * 10 + (secByte & 0x0F)
            record.milliseconds = (((msecByte >> 4) & 0x0F) * 10 + (msecByte & 0x0F)) * 10
        }

        records.append(record)
    }
}




/// タイムをフォーマットする関数（SEGA Rally用）
func formatSegaRallyTime(msec: Int64) -> String {
    let min = msec / 60000
    let sec = (msec % 60000) / 1000
    let ms = msec % 1000
    return String(format: "%d:%02d.%03d", min, sec, ms)
}

/// SEGA Rallyゲームクラス
class SegaRally: BaseGame {


    init(gameCode: String) {
        super.init()

        // リーダーボードの初期化
        leaderBoards = [
            LeaderBoard(title: "Desert", id: "01"),
            LeaderBoard(title: "Forest", id: "02"),
            LeaderBoard(title: "Mountain", id: "03")
        ]

        // Firebase Remote Configの初期化
        let remoteConfig = RemoteConfig.remoteConfig()
        let settings = RemoteConfigSettings()
        settings.minimumFetchInterval = 3600 // 1時間ごとに更新（開発中は0に設定可能）
        remoteConfig.configSettings = settings

        // デフォルト値の設定
        let defaultValues: [String: NSObject] = [
            "discord_webhook_url_segarally": "" as NSObject
        ]
        remoteConfig.setDefaults(defaultValues)

        // Remote Configの値を取得
        remoteConfig.fetch { status, error in
            if status == .success {
                print("Remote Config fetched successfully for SEGA Rally")
                remoteConfig.activate { _, error in
                    if let error = error {
                        print("Error activating Remote Config: \(error.localizedDescription)")
                    } else {
                        print("Remote Config activated successfully for SEGA Rally")
                        // Discord webhook URLの確認
                        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_segarally").stringValue ?? ""
                        print("SEGA Rally Discord webhook URL: \(webhookUrl.isEmpty ? "Not set" : "Set")")
                    }
                }
            } else {
                print("Error fetching Remote Config for SEGA Rally: \(error?.localizedDescription ?? "unknown error")")
            }
        }

        // Firestoreからゲーム情報を取得
        let db = Firestore.firestore()
        db.collection("games")
            .whereField("product_number", isEqualTo: gameCode)
            .getDocuments { [weak self] snapshot, error in
                guard let self = self, let snapshot = snapshot, !snapshot.documents.isEmpty else {
                    return
                }

                // leaderboardIdフィールドがある場合はその値を使用
                if let leaderboardId = snapshot.documents[0].get("leaderboardId") as? String {
                    self.gameId = leaderboardId
                } else {
                    // なければドキュメントIDを使用
                    self.gameId = snapshot.documents[0].documentID
                }

                // leaderboardsコレクションが空なら初期データ投入
                let leaderboardsRef = db.collection("games").document(self.gameId).collection("leaderboards")
                leaderboardsRef.getDocuments { snapshot, error in
                    guard let snapshot = snapshot else { return }

                    if snapshot.documents.isEmpty {
                        let leaderboardsData = [
                            ("01", "Desert"),
                            ("02", "Forest"),
                            ("03", "Mountain")
                        ]

                        for (id, name) in leaderboardsData {
                            let data = ["name": name]
                            leaderboardsRef.document(id).setData(data)
                        }
                    }
                }
            }
    }
    
    /// Discordに新記録を通知する（SEGA Rally用）
    override func notifyDiscord(gameId: String, leaderboardName: String, score: Int64, userName: String, photoURL: String?) {
        // Firebase Remote Configからwebhook URLを取得
        let remoteConfig = RemoteConfig.remoteConfig()
        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_segarally").stringValue ?? ""

        // webhook URLが空の場合は処理を中止
        guard !webhookUrl.isEmpty else {
            print("Discord webhook URL is empty. Skipping notification.")
            return
        }

        print("Using Discord webhook URL from Remote Config for SEGA Rally")

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
                print("Successfully posted SEGA Rally record to Discord")
            } else {
                print("Failed to post SEGA Rally record to Discord")
            }
        }
    }

    override func onBackUpUpdated(fname: String, before: Data, after: Data) {
        print("SEGA Rally onBackUpUpdated called fname=\(fname)")

        if gameId.isEmpty { return }

        // SEGARALLY_0 または RALLYPLUS_0 のときのみスコア評価処理を実行
        if fname != "SEGARALLY_0" && fname != "RALLYPLUS_0" { return }

        guard let currentUser = Auth.auth().currentUser else { return }

        let beforeRecord = SegaRallyBackup(bin: before)
        let afterRecord = SegaRallyBackup(bin: after)

        print("Desert \(String(format: "%02d:%02d.%03d", afterRecord.records[0].minutes, afterRecord.records[0].seconds, afterRecord.records[0].milliseconds))")
        print("Forest \(String(format: "%02d:%02d.%03d", afterRecord.records[1].minutes, afterRecord.records[1].seconds, afterRecord.records[1].milliseconds))")
        print("Mountain \(String(format: "%02d:%02d.%03d", afterRecord.records[2].minutes, afterRecord.records[2].seconds, afterRecord.records[2].milliseconds))")

        for i in 0..<3 { // 3 stages: Desert, Forest, Mountain
            let beforeTime = beforeRecord.records[i].toTotalMilliseconds()
            let afterTime = afterRecord.records[i].toTotalMilliseconds()

            // 新記録かどうかをチェック（より短いタイムで、ゼロでない）
            if afterTime > 0 && (beforeTime == 0 || afterTime < beforeTime) {
                let score = afterTime
                if let gid = leaderBoards?[i].id {
                    // 表示名が利用可能な場合は使用するが、ドキュメントIDにはFirebase UIDを使用することを確認
                    let userName = currentUser.displayName ?? "Anonymous"

                    // デバッグ用のユーザー情報をログ出力
                    print("Submitting SEGA Rally score for user: \(currentUser.uid), display name: \(userName), stage: \(leaderBoards?[i].title ?? ""), time: \(formatSegaRallyTime(msec: score))")

                    submitScoreToFirestore(gameId: gameId, leaderboardId: gid, score: score, userName: userName)

                    // Analyticsイベントの記録
                    Analytics.logEvent(AnalyticsEventPostScore, parameters: [
                        AnalyticsParameterScore: score,
                        "leaderboard_id": gid
                    ])

                    // 新記録通知
                    self.uiEvent?.onNewRecord(leaderBoardId: gid)
                }
            }
        }
    }
}
