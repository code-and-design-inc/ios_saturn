import Foundation
import FirebaseFirestore
import FirebaseAuth
import FirebaseRemoteConfig

/// リーダーボードの情報を保持するクラス
class LeaderBoard {
    let title: String
    let id: String

    init(title: String, id: String) {
        self.title = title
        self.id = id
    }
}

/// ゲームUIイベントを処理するプロトコル
protocol GameUiEvent {
    func onNewRecord(leaderBoardId: String)
}

/// ゲームの基本クラス
class BaseGame {

    var leaderBoards: [LeaderBoard]?
    
    // Game ID for Firebase integration (Android compatibility)
    // Made internal so subclasses can access and override the value as needed
    internal var gameId: String = ""

    var uiEvent: GameUiEvent?

    func setUiEvent(_ uiEvent: GameUiEvent) {
        self.uiEvent = uiEvent
    }
    
    /// Initialize game ID for Firebase integration (Android compatibility)
    /// - Parameter gameCode: Game product number/code
    func initGameId(_ gameCode: String) {
        // For RetroAchievements support, use a standardized game ID format
        // This ensures compatibility with Firebase leaderboard structure
        self.gameId = "retroachievements_\(gameCode)"
        NSLog("BaseGame: Initialized gameId: \(self.gameId)")
    }

    /// バックアップデータが更新されたときに呼ばれるメソッド
    /// - Parameters:
    ///   - fname: ファイル名
    ///   - before: 更新前のバイナリデータ
    ///   - after: 更新後のバイナリデータ
    func onBackUpUpdated(fname: String, before: Data, after: Data) {
        // サブクラスでオーバーライドする
    }
    
    /// Submit RetroAchievements leaderboard score to Firebase
    /// This method bridges RetroAchievements scores to the existing Firebase leaderboard system
    /// - Parameters:
    ///   - retroAchievementsLeaderboardId: RetroAchievements leaderboard ID (used as Firebase leaderboard ID)
    ///   - score: Numeric score value
    ///   - userName: User display name
    ///   - title: Leaderboard title
    ///   - onSuccess: Success callback
    ///   - onFailure: Failure callback with error
    func submitRetroAchievementsScore(
        retroAchievementsLeaderboardId: String,
        score: Int64,
        userName: String,
        title: String,
        onSuccess: (() -> Void)? = nil,
        onFailure: ((Error) -> Void)? = nil
    ) {
        // Use RetroAchievements leaderboard ID as Firebase leaderboard ID
        // This creates a direct mapping between RA leaderboards and Firebase leaderboards
        let firebaseLeaderboardId = retroAchievementsLeaderboardId
        
        NSLog("BaseGame: Submitting RetroAchievements score to Firebase - Leaderboard: \(firebaseLeaderboardId), Score: \(score), User: \(userName)")
        
        // Check if gameId is initialized (Android compatibility)
        guard !gameId.isEmpty else {
            NSLog("BaseGame: Game ID not initialized. Cannot submit RetroAchievements score.")
            let error = NSError(domain: "BaseGame", code: 1, userInfo: [NSLocalizedDescriptionKey: "Game ID not initialized"])
            onFailure?(error)
            return
        }
        
        // Submit to Firebase using the existing infrastructure
        // Note: This uses the existing SonicR.swift submitScoreToFirestore function
        NSLog("BaseGame: Calling submitScoreToFirestore with gameId: \(gameId), leaderboardId: \(firebaseLeaderboardId)")
        submitScoreToFirestore(
            gameId: gameId,
            leaderboardId: firebaseLeaderboardId,
            score: score,
            userName: userName,
            onSuccess: {
                NSLog("BaseGame: Successfully submitted RetroAchievements score to Firebase - Score: \(score)")
                onSuccess?()
            },
            onFailure: { error in
                NSLog("BaseGame: Failed to submit RetroAchievements score to Firebase: \(error.localizedDescription)")
                onFailure?(error)
            }
        )
    }
    
    // MARK: - Common Firebase Functions
    
    /// Firestoreにスコアを送信する共通関数
    /// - Parameters:
    ///   - gameId: Firebase上のゲームID
    ///   - leaderboardId: リーダーボードID
    ///   - score: スコア値
    ///   - userName: ユーザー名
    ///   - onSuccess: 成功コールバック
    ///   - onFailure: 失敗コールバック
    func submitScoreToFirestore(
        gameId: String,
        leaderboardId: String,
        score: Int64,
        userName: String,
        onSuccess: (() -> Void)? = nil,
        onFailure: ((Error) -> Void)? = nil
    ) {
        let db = Firestore.firestore()
        guard let userId = Auth.auth().currentUser?.uid else {
            onFailure?(NSError(domain: "ScoreSubmission", code: 1, userInfo: [NSLocalizedDescriptionKey: "ユーザーが認証されていません"]))
            return
        }

        // First get the leaderboard configuration to determine ranking type
        let leaderboardRef = db.collection("games/\(gameId)/leaderboards").document(leaderboardId)
        
        leaderboardRef.getDocument { leaderboardDoc, error in
            if let error = error {
                NSLog("Firebase: Failed to get leaderboard info: \(error.localizedDescription)")
                onFailure?(error)
                return
            }
            
            // Get ranking type from leaderboard configuration (default to DESCENDING for RetroAchievements)
            let rankingTypeString = leaderboardDoc?.get("rankingType") as? String ?? "DESCENDING"
            let isAscending = rankingTypeString.uppercased() == "ASCENDING"
            
            NSLog("Firebase: Leaderboard \(leaderboardId) ranking type: \(rankingTypeString) (ascending: \(isAscending))")

            // ユーザーのプロフィール画像URLを取得
            let photoURL = Auth.auth().currentUser?.photoURL?.absoluteString

            let scoreData: [String: Any] = [
                "name": userName,
                "score": score,
                "timestamp": Int(Date().timeIntervalSince1970 * 1000), // ミリ秒単位のタイムスタンプ
                "photoUrl": photoURL // ユーザーのアバター画像URL（nilの場合はFirestoreではnullとして保存される）
            ]

            let scoreDocRef = db.collection("games/\(gameId)/leaderboards")
                .document(leaderboardId)
                .collection("scores")
                .document(userId)

            scoreDocRef.getDocument { document, error in
                if let error = error {
                    onFailure?(error)
                    return
                }

                if let document = document, document.exists,
                   let currentScore = document.data()?["score"] as? Int64 {
                    
                    // Determine if this score is better based on ranking type
                    let shouldUpdate: Bool
                    if isAscending {
                        // ASCENDING: Lower scores are better (like race times)
                        shouldUpdate = score < currentScore
                    } else {
                        // DESCENDING: Higher scores are better (like points, play counts)
                        shouldUpdate = score > currentScore
                    }
                    
                    NSLog("Firebase: Current score: \(currentScore), New score: \(score), Ranking type: \(rankingTypeString), Should update: \(shouldUpdate)")
                    
                    if shouldUpdate {
                        // 新記録の場合のみ上書き
                        scoreDocRef.setData(scoreData) { error in
                            if let error = error {
                                NSLog("Firebase: Failed to update score: \(error.localizedDescription)")
                                onFailure?(error)
                            } else {
                                NSLog("Firebase: Successfully updated score from \(currentScore) to \(score)")
                                // スコア更新後、1位かどうかチェック
                                self.checkIfTopRankAndNotify(gameId: gameId, leaderboardId: leaderboardId, score: score, userName: userName, photoURL: photoURL)
                                onSuccess?()
                            }
                        }
                    } else {
                        // 記録を更新しない場合も成功扱い
                        NSLog("Firebase: Score \(score) not better than current \(currentScore), not updating")
                        onSuccess?()
                    }
                } else {
                    // 初めてのスコア登録
                    scoreDocRef.setData(scoreData) { error in
                        if let error = error {
                            onFailure?(error)
                        } else {
                            NSLog("Firebase: Successfully created first score: \(score)")
                            // スコア登録後、1位かどうかチェック
                            self.checkIfTopRankAndNotify(gameId: gameId, leaderboardId: leaderboardId, score: score, userName: userName, photoURL: photoURL)
                            onSuccess?()
                        }
                    }
                }
            }
        }
    }

    /// スコアが1位かどうかをチェックし、1位ならDiscordに通知する共通関数
    /// - Parameters:
    ///   - gameId: Firebase上のゲームID
    ///   - leaderboardId: リーダーボードID
    ///   - score: スコア値
    ///   - userName: ユーザー名
    ///   - photoURL: プロフィール画像URL
    private func checkIfTopRankAndNotify(gameId: String, leaderboardId: String, score: Int64, userName: String, photoURL: String?) {
        let db = Firestore.firestore()

        // First get the leaderboard configuration to determine ranking type
        let leaderboardRef = db.collection("games/\(gameId)/leaderboards").document(leaderboardId)
        
        leaderboardRef.getDocument { leaderboardDoc, error in
            if let error = error {
                print("Error getting leaderboard info: \(error.localizedDescription)")
                return
            }
            
            // Get ranking type from leaderboard configuration (default to DESCENDING for RetroAchievements)
            let rankingTypeString = leaderboardDoc?.get("rankingType") as? String ?? "DESCENDING"
            let isAscending = rankingTypeString.uppercased() == "ASCENDING"
            
            // 1位のスコアを取得するクエリ - Use appropriate sorting based on ranking type
            let query = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")
                .order(by: "score", descending: !isAscending)  // Descending for DESCENDING type, ascending for ASCENDING type
                .limit(to: 1)

            query.getDocuments { snapshot, error in
                guard let snapshot = snapshot, !snapshot.documents.isEmpty else {
                    print("Error checking top rank: \(error?.localizedDescription ?? "No documents")")
                    return
                }

                // 1位のスコアを取得
                if let topScore = snapshot.documents[0].get("score") as? Int64,
                   let topUserId = snapshot.documents[0].documentID as String? {

                    // 自分のスコアが1位と同じか、自分が1位になった場合
                    let isTopScore: Bool
                    if isAscending {
                        // ASCENDING: Lower or equal scores are considered top
                        isTopScore = score <= topScore
                    } else {
                        // DESCENDING: Higher or equal scores are considered top
                        isTopScore = score >= topScore
                    }
                    
                    if isTopScore && topUserId == Auth.auth().currentUser?.uid {
                        print("New record achieved! Score: \(score), Ranking type: \(rankingTypeString)")

                        // リーダーボード名を取得
                        if let leaderboardName = leaderboardDoc?.get("name") as? String {
                            // Discordに通知
                            self.notifyDiscord(gameId: gameId, leaderboardName: leaderboardName, score: score, userName: userName, photoURL: photoURL)
                        } else {
                            // リーダーボード名が取得できない場合はデフォルト名を使用
                            self.notifyDiscord(gameId: gameId, leaderboardName: "Leaderboard \(leaderboardId)", score: score, userName: userName, photoURL: photoURL)
                        }
                    }
                }
            }
        }
    }

    /// Discordに新記録を通知する共通関数（サブクラスでオーバーライド可能）
    /// - Parameters:
    ///   - gameId: Firebase上のゲームID
    ///   - leaderboardName: リーダーボード名
    ///   - score: スコア値
    ///   - userName: ユーザー名
    ///   - photoURL: プロフィール画像URL
    func notifyDiscord(gameId: String, leaderboardName: String, score: Int64, userName: String, photoURL: String?) {
        // デフォルトは何もしない（サブクラスでオーバーライドして具体的な通知処理を実装）
        NSLog("BaseGame: Discord notification requested for game \(gameId), leaderboard '\(leaderboardName)', score \(score)")
    }
}
