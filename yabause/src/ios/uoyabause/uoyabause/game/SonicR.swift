import Foundation
import FirebaseFirestore
import FirebaseAuth
import FirebaseAnalytics
import FirebaseRemoteConfig
import UIKit

/// ソニックRのレコード情報を保持するクラス
class SonicRRecord {
    var lapRecord: Int = 0
    var courseRecord: Int = 0
    var tagRecord: Int = 0
    var balloonRecord: Int = 0
}

/// ソニックRのバックアップデータを解析するクラス
class SonicRBackup {
    var records: [SonicRRecord] = []
    var totalTime: Int64 = 0

    init(bin: Data) {
        totalTime = 0
        for i in 0..<5 {
            let record = SonicRRecord()
            let si = i * 0x10 + 0x10

            // バイナリデータからレコード情報を抽出
            if si + 0x03 < bin.count {
                let lapRecordBytes = bin.subdata(in: si + 0x02..<si + 0x04)
                var lapRecordValue: UInt16 = 0
                _ = withUnsafeMutableBytes(of: &lapRecordValue) { lapRecordBytes.copyBytes(to: $0) }
                record.lapRecord = Int(Double(UInt16(bigEndian: lapRecordValue)) * 1.6666) * 10
            }

            if si + 0x07 < bin.count {
                let courseRecordBytes = bin.subdata(in: si + 0x06..<si + 0x08)
                var courseRecordValue: UInt16 = 0
                _ = withUnsafeMutableBytes(of: &courseRecordValue) { courseRecordBytes.copyBytes(to: $0) }
                record.courseRecord = Int(UInt16(bigEndian: courseRecordValue)) * 10
            }

            if si + 0x0B < bin.count {
                let tagRecordBytes = bin.subdata(in: si + 0x0A..<si + 0x0C)
                var tagRecordValue: UInt16 = 0
                _ = withUnsafeMutableBytes(of: &tagRecordValue) { tagRecordBytes.copyBytes(to: $0) }
                record.tagRecord = Int(UInt16(bigEndian: tagRecordValue)) * 10
            }

            if si + 0x0F < bin.count {
                let balloonRecordBytes = bin.subdata(in: si + 0x0E..<si + 0x10)
                var balloonRecordValue: UInt16 = 0
                _ = withUnsafeMutableBytes(of: &balloonRecordValue) { balloonRecordBytes.copyBytes(to: $0) }
                record.balloonRecord = Int(UInt16(bigEndian: balloonRecordValue)) * 10
            }

            records.append(record)
            totalTime += Int64(record.courseRecord)
        }
    }
}


/// ソニックRゲームクラス
class SonicR: BaseGame {

    init(gameCode: String) {
        super.init()

        // リーダーボードの初期化
        leaderBoards = [
            LeaderBoard(title: "Resort Island", id: "01"),
            LeaderBoard(title: "Radical City", id: "02"),
            LeaderBoard(title: "Regal Ruin", id: "03"),
            LeaderBoard(title: "Reactive Factory", id: "04"),
            LeaderBoard(title: "Radiant Emerald", id: "05")
        ]

        // Firebase Remote Configの初期化
        let remoteConfig = RemoteConfig.remoteConfig()
        let settings = RemoteConfigSettings()
        settings.minimumFetchInterval = 3600 // 1時間ごとに更新（開発中は0に設定可能）
        remoteConfig.configSettings = settings

        // デフォルト値の設定
        let defaultValues: [String: NSObject] = [
            "discord_webhook_url_sonicr": "" as NSObject
        ]
        remoteConfig.setDefaults(defaultValues)

        // Remote Configの値を取得
        remoteConfig.fetch { status, error in
            if status == .success {
                print("Remote Config fetched successfully")
                remoteConfig.activate { _, error in
                    if let error = error {
                        print("Error activating Remote Config: \(error.localizedDescription)")
                    } else {
                        print("Remote Config activated successfully")
                        // Discord webhook URLの確認
                        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_sonicr").stringValue ?? ""
                        print("Discord webhook URL: \(webhookUrl.isEmpty ? "Not set" : "Set")")
                    }
                }
            } else {
                print("Error fetching Remote Config: \(error?.localizedDescription ?? "unknown error")")
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
                            ("01", "Resort Island"),
                            ("02", "Radical City"),
                            ("03", "Regal Ruin"),
                            ("04", "Reactive Factory"),
                            ("05", "Radiant Emerald")
                        ]

                        for (id, name) in leaderboardsData {
                            let data = ["name": name]
                            leaderboardsRef.document(id).setData(data)
                        }
                    }
                }
            }
    }

    /// テスト用のダミーデータを挿入するメソッド
    func insertDummyLeaderboardData() {
        let db = Firestore.firestore()
        let gameId = "31"
        let leaderboardId = "01"
        let scoresRef = db.collection("games").document(gameId)
            .collection("leaderboards").document(leaderboardId)
            .collection("scores")

        // 1000件分のダミーデータを作成
        for i in 1...1000 {
            let userId = "dummy_user_\(i)"
            let name = "ダミー\(i)"
            let score = 100000 + i * 100 // 例: タイムアタックならミリ秒
            let timestamp = Int(Date().timeIntervalSince1970 * 1000 - Double(1000 * i))
            let data: [String: Any] = [
                "name": name,
                "score": score,
                "timestamp": timestamp,
                "photoUrl": "" // ダミーデータなのでphotoUrlはnilに設定
            ]
            scoresRef.document(userId).setData(data)
        }
    }

    override func onBackUpUpdated(fname: String, before: Data, after: Data) {
        if gameId.isEmpty { return }
        guard let currentUser = Auth.auth().currentUser else { return }

        let beforeRecord = SonicRBackup(bin: before)
        let afterRecord = SonicRBackup(bin: after)

        for i in 0..<5 {
            if i < afterRecord.records.count && i < beforeRecord.records.count &&
               afterRecord.records[i].courseRecord < beforeRecord.records[i].courseRecord {

                let score = Int64(afterRecord.records[i].courseRecord)

                if let gid = leaderBoards?[i].id {
                    let userName = currentUser.displayName ?? "Anonymous"
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
    
    /// Discordに新記録を通知する（SonicR専用）
    override func notifyDiscord(gameId: String, leaderboardName: String, score: Int64, userName: String, photoURL: String?) {
        // Firebase Remote Configからwebhook URLを取得
        let remoteConfig = RemoteConfig.remoteConfig()
        let webhookUrl = remoteConfig.configValue(forKey: "discord_webhook_url_sonicr").stringValue ?? ""

        // webhook URLが空の場合は処理を中止
        guard !webhookUrl.isEmpty else {
            print("Discord webhook URL is empty. Skipping notification.")
            return
        }

        print("Using Discord webhook URL from Remote Config for SonicR")

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
                print("Successfully posted SonicR record to Discord")
            } else {
                print("Failed to post SonicR record to Discord")
            }
        }
    }
}
