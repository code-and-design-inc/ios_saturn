import Foundation
import FirebaseFirestore
import FirebaseAuth

class LeaderBoardRepository {
    // MARK: - Properties
    private let db = Firestore.firestore()
    private var gameId: String = ""
    private let pageSize = 100

    // MARK: - Data Models
    
    // Score type definition
    enum ScoreType: String {
        case time = "TIME"     // Time-based score
        case value = "VALUE"   // Value-based score (default)
    }
    
    // Ranking type definition
    enum RankingType: String {
        case ascending = "ASCENDING"   // Lower values rank higher (times)
        case descending = "DESCENDING" // Higher values rank higher (scores)
    }

    struct LeaderboardInfo {
        let id: String
        let name: String?
        let scoreType: ScoreType
        let rankingType: RankingType
    }

    struct ScoreEntry {
        let userId: String
        let name: String
        let score: Int64
        let diff: Int64
        let timestamp: Date
        let rank: Int
        let photoURL: String?
    }

    struct UserPosition {
        let userId: String
        let rank: Int
        let score: Int64
    }

    struct PageResult {
        let scores: [ScoreEntry]
        let firstVisible: DocumentSnapshot?
        let lastVisible: DocumentSnapshot?
        let hasMoreBefore: Bool
        let hasMoreAfter: Bool
        let totalCount: Int
    }

    // MARK: - Public Methods

    // Get leaderboards for a game
    func getLeaderboards(gameCode: String) async throws -> [LeaderboardInfo] {
        do {
            // First get the gameId
            let gameQuery = db.collection("games").whereField("product_number", isEqualTo: gameCode)
            let gameDocuments = try await gameQuery.getDocuments()

            if gameDocuments.documents.isEmpty {
                print("No game found with product_number: \(gameCode)")
                return []
            }

            let gameDoc = gameDocuments.documents[0]

            // Check if there's a specific leaderboardId field
            if let leaderboardId = gameDoc.get("leaderboardId") as? String {
                self.gameId = leaderboardId
            } else {
                // Use the document ID as the gameId
                self.gameId = gameDoc.documentID
            }

            print("Found gameId: \(self.gameId)")

            // Get leaderboards for this game
            let leaderboardsQuery = db.collection("games/\(self.gameId)/leaderboards")
            let leaderboardsResult = try await leaderboardsQuery.getDocuments()

            if leaderboardsResult.documents.isEmpty {
                print("No leaderboards found for gameId: \(self.gameId)")
                return []
            }

            print("Retrieved \(leaderboardsResult.documents.count) leaderboards")

            // Map documents to LeaderboardInfo objects
            return leaderboardsResult.documents.map { doc in
                // Get scoreType and rankingType with defaults
                let scoreTypeString = doc.get("scoreType") as? String ?? "VALUE"
                let rankingTypeString = doc.get("rankingType") as? String ?? "DESCENDING"
                
                let scoreType = ScoreType(rawValue: scoreTypeString.uppercased()) ?? .value
                let rankingType = RankingType(rawValue: rankingTypeString.uppercased()) ?? .descending
                
                return LeaderboardInfo(
                    id: doc.documentID,
                    name: doc.get("name") as? String,
                    scoreType: scoreType,
                    rankingType: rankingType
                )
            }
        } catch {
            print("Error loading leaderboards: \(error)")
            throw error
        }
    }

    // Get top score for a leaderboard
    func getTopScore(leaderboardId: String, leaderboardInfo: LeaderboardInfo? = nil) async throws -> Int64 {
        do {
            if gameId.isEmpty {
                print("gameId is not set. Call getLeaderboards first.")
                return 0
            }

            print("Getting top score for leaderboardId: \(leaderboardId)")

            // Determine sort direction based on ranking type
            let rankingType = leaderboardInfo?.rankingType ?? .descending
            let isDescending = (rankingType == .descending)
            
            // Get the top score (1st place)
            // For ASCENDING: smallest value is best (descending: false)
            // For DESCENDING: largest value is best (descending: true)
            let query = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")
                .order(by: "score", descending: isDescending)
                .limit(to: 1)

            let result = try await query.getDocuments()

            if result.documents.isEmpty {
                print("No scores found for leaderboardId: \(leaderboardId)")
                return 0
            }

            // Get the top score
            if let score = result.documents[0].get("score") as? Int64 {
                return score
            }

            return 0
        } catch {
            print("Error getting top score: \(error)")
            throw error
        }
    }

    // Load a specific page of scores
    func loadPage(leaderboardId: String, pageNumber: Int, userId: String?, topScore: Int64, leaderboardInfo: LeaderboardInfo? = nil) async throws -> PageResult {
        do {
            if gameId.isEmpty {
                print("gameId is not set. Call getLeaderboards first.")
                throw NSError(domain: "LeaderBoardRepository", code: 1, userInfo: [NSLocalizedDescriptionKey: "Game ID not set"])
            }

            print("Loading page \(pageNumber) for leaderboardId: \(leaderboardId), userId: \(userId ?? "nil")")

            // Get total count
            let countQuery = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")

            let countSnapshot = try await countQuery.count.getAggregation(source: .server)
            let totalCount = Int(truncating: countSnapshot.count)

            // If userId is provided, find the user's position
            if let userId = userId {
                // ドキュメントIDがuserIdなので、ドキュメントを直接取得
                let userQuery = db.collection("games").document(gameId)
                    .collection("leaderboards").document(leaderboardId)
                    .collection("scores").document(userId)

                let userDoc = try await userQuery.getDocument()

                if userDoc.exists {
                    // User has a score, get their position

                    if let userScore = userDoc.get("score") as? Int64 {
                        // Count how many scores are better than the user's score
                        let rankingType = leaderboardInfo?.rankingType ?? .descending
                        let betterScoresQuery: Query
                        
                        if rankingType == .ascending {
                            // For ascending (times), better scores are less than user's
                            betterScoresQuery = db.collection("games").document(gameId)
                                .collection("leaderboards").document(leaderboardId)
                                .collection("scores")
                                .whereField("score", isLessThan: userScore)
                        } else {
                            // For descending (scores), better scores are greater than user's
                            betterScoresQuery = db.collection("games").document(gameId)
                                .collection("leaderboards").document(leaderboardId)
                                .collection("scores")
                                .whereField("score", isGreaterThan: userScore)
                        }

                        let betterScoresSnapshot = try await betterScoresQuery.count.getAggregation(source: .server)
                        let betterScoresCount = Int(truncating: betterScoresSnapshot.count)

                        // User's rank is betterScoresCount + 1
                        let userRank = betterScoresCount + 1

                        // Calculate which page the user is on
                        let userPage = (userRank - 1) / pageSize

                        // Load that page
                        return try await loadPageByNumber(
                            leaderboardId: leaderboardId,
                            pageNumber: userPage,
                            totalCount: totalCount,
                            topScore: topScore,
                            leaderboardInfo: leaderboardInfo
                        )
                    }
                }
            }

            // If no user ID or user not found, load the requested page
            return try await loadPageByNumber(
                leaderboardId: leaderboardId,
                pageNumber: pageNumber,
                totalCount: totalCount,
                topScore: topScore,
                leaderboardInfo: leaderboardInfo
            )
        } catch {
            print("Error loading page: \(error)")
            throw error
        }
    }

    // Load a page by page number
    func loadPageByNumber(leaderboardId: String, pageNumber: Int, totalCount: Int, topScore: Int64, leaderboardInfo: LeaderboardInfo? = nil) async throws -> PageResult {
        let offset = pageNumber * pageSize
        let hasMoreBefore = pageNumber > 0
        let hasMoreAfter = (offset + pageSize) < totalCount

        do {
            // Get scores for this page
            let rankingType = leaderboardInfo?.rankingType ?? .descending
            let isDescending = (rankingType == .descending)
            
            let query = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")
                .order(by: "score", descending: isDescending)  // DESCENDING: higher is better, ASCENDING: lower is better
                .order(by: "timestamp", descending: isDescending)
                .limit(to: pageSize)

            // Apply offset if needed
            let finalQuery: Query
            if offset > 0 {
                // This is a simplification - in a real app, you'd need to use startAfter with a document
                // For large offsets, Firestore doesn't support direct pagination
                // You'd need to implement cursor-based pagination
                finalQuery = query.limit(to: pageSize)
            } else {
                finalQuery = query
            }

            let result = try await finalQuery.getDocuments()

            // Process results
            var scores: [ScoreEntry] = []
            var firstVisible: DocumentSnapshot?
            var lastVisible: DocumentSnapshot?

            if !result.documents.isEmpty {
                firstVisible = result.documents.first
                lastVisible = result.documents.last

                // Map documents to ScoreEntry objects
                scores = result.documents.enumerated().compactMap { index, doc in
                    // ドキュメントIDがuserIdとして使用されている
                    let userId = doc.documentID
                    guard let name = doc.get("name") as? String,
                          let score = doc.get("score") as? Int64,
                          let timestamp = doc.get("timestamp") as? Int64 else {
                        return nil
                    }

                    // Calculate rank based on page number and position in results
                    let rank = offset + index + 1

                    // Calculate difference from top score
                    let diff = calculateDiff(score: score, topScore: topScore, rankingType: leaderboardInfo?.rankingType ?? .descending)

                    // エポックミリ秒からDateに変換
                    let date = Date(timeIntervalSince1970: TimeInterval(timestamp) / 1000.0)
                    // Get photoURL if available
                    let photoURL = doc.get("photoUrl") as? String

                    return ScoreEntry(
                        userId: userId,
                        name: name,
                        score: score,
                        diff: diff,
                        timestamp: date,
                        rank: rank,
                        photoURL: photoURL
                    )
                }
            }

            return PageResult(
                scores: scores,
                firstVisible: firstVisible,
                lastVisible: lastVisible,
                hasMoreBefore: hasMoreBefore,
                hasMoreAfter: hasMoreAfter,
                totalCount: totalCount
            )
        } catch {
            print("Error loading page by number: \(error)")
            throw error
        }
    }

    // Load scores after a specific document
    func loadScoresAfter(leaderboardId: String, lastVisible: DocumentSnapshot, lastRank: Int, topScore: Int64, leaderboardInfo: LeaderboardInfo? = nil) async throws -> PageResult {
        do {
            if gameId.isEmpty {
                print("gameId is not set. Call getLeaderboards first.")
                throw NSError(domain: "LeaderBoardRepository", code: 1, userInfo: [NSLocalizedDescriptionKey: "Game ID not set"])
            }

            print("Loading scores after for leaderboardId: \(leaderboardId)")

            // Get scores after the last visible document
            let rankingType = leaderboardInfo?.rankingType ?? .descending
            let isDescending = (rankingType == .descending)
            
            let query = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")
                .order(by: "score", descending: isDescending)
                .order(by: "timestamp", descending: isDescending)
                .start(afterDocument: lastVisible)
                .limit(to: pageSize)

            let result = try await query.getDocuments()

            // Process results
            var scores: [ScoreEntry] = []
            var newLastVisible: DocumentSnapshot?

            if !result.documents.isEmpty {
                newLastVisible = result.documents.last

                // Map documents to ScoreEntry objects
                scores = result.documents.enumerated().compactMap { index, doc in
                    // ドキュメントIDがuserIdとして使用されている
                    let userId = doc.documentID
                    guard let name = doc.get("name") as? String,
                          let score = doc.get("score") as? Int64,
                          let timestamp = doc.get("timestamp") as? Int64 else {
                        return nil
                    }

                    // Calculate difference from top score
                    let diff = calculateDiff(score: score, topScore: topScore, rankingType: leaderboardInfo?.rankingType ?? .descending)

                    // エポックミリ秒からDateに変換
                    let date = Date(timeIntervalSince1970: TimeInterval(timestamp) / 1000.0)
                    // Get photoURL if available
                    let photoURL = doc.get("photoUrl") as? String

                    return ScoreEntry(
                        userId: userId,
                        name: name,
                        score: score,
                        diff: diff,
                        timestamp: date,
                        rank: lastRank + index + 1,
                        photoURL: photoURL
                    )
                }
            }

            // Check if there are more scores after these
            let hasMoreAfter = result.documents.count >= pageSize

            return PageResult(
                scores: scores,
                firstVisible: nil,
                lastVisible: newLastVisible,
                hasMoreBefore: true,
                hasMoreAfter: hasMoreAfter,
                totalCount: 0 // Not needed for this query
            )
        } catch {
            print("Error loading scores after: \(error)")
            throw error
        }
    }

    // Load scores before a specific document
    func loadScoresBefore(leaderboardId: String, firstVisible: DocumentSnapshot, firstRank: Int, topScore: Int64, leaderboardInfo: LeaderboardInfo? = nil) async throws -> PageResult {
        do {
            if gameId.isEmpty {
                print("gameId is not set. Call getLeaderboards first.")
                throw NSError(domain: "LeaderBoardRepository", code: 1, userInfo: [NSLocalizedDescriptionKey: "Game ID not set"])
            }

            print("Loading scores before for leaderboardId: \(leaderboardId)")

            // Get scores before the first visible document
            // Note: Firestore doesn't have a direct "end before" query
            // This is a simplification - in a real app, you'd need a more complex approach
            let rankingType = leaderboardInfo?.rankingType ?? .descending
            let isDescending = (rankingType == .descending)
            
            let query = db.collection("games").document(gameId)
                .collection("leaderboards").document(leaderboardId)
                .collection("scores")
                .order(by: "score", descending: isDescending)
                .order(by: "timestamp", descending: isDescending)
                .limit(to: pageSize)

            let result = try await query.getDocuments()

            // Process results
            var scores: [ScoreEntry] = []
            var newFirstVisible: DocumentSnapshot?

            if !result.documents.isEmpty {
                newFirstVisible = result.documents.first

                // Map documents to ScoreEntry objects
                scores = result.documents.enumerated().compactMap { index, doc in
                    // ドキュメントIDがuserIdとして使用されている
                    let userId = doc.documentID
                    guard let name = doc.get("name") as? String,
                          let score = doc.get("score") as? Int64,
                          let timestamp = doc.get("timestamp") as? Int64 else {
                        return nil
                    }

                    // Calculate rank based on position in results
                    let rank = firstRank - (result.documents.count - index)

                    // Calculate difference from top score
                    let diff = calculateDiff(score: score, topScore: topScore, rankingType: leaderboardInfo?.rankingType ?? .descending)

                    // エポックミリ秒からDateに変換
                    let date = Date(timeIntervalSince1970: TimeInterval(timestamp) / 1000.0)
                    // Get photoURL if available
                    let photoURL = doc.get("photoUrl") as? String

                    return ScoreEntry(
                        userId: userId,
                        name: name,
                        score: score,
                        diff: diff,
                        timestamp: date,
                        rank: max(1, rank), // Ensure rank is at least 1
                        photoURL: photoURL
                    )
                }
            }

            // Check if there are more scores before these
            let hasMoreBefore = firstRank > pageSize

            return PageResult(
                scores: scores,
                firstVisible: newFirstVisible,
                lastVisible: nil,
                hasMoreBefore: hasMoreBefore,
                hasMoreAfter: true,
                totalCount: 0 // Not needed for this query
            )
        } catch {
            print("Error loading scores before: \(error)")
            throw error
        }
    }

    // Calculate score difference helper method
    private func calculateDiff(score: Int64, topScore: Int64, rankingType: RankingType) -> Int64 {
        if topScore > 0 && score > 0 {
            switch rankingType {
            case .ascending:
                return score - topScore  // For times: my time - best time
            case .descending:
                return topScore - score  // For scores: best score - my score
            }
        }
        return 0
    }
    
    // Format score based on type
    func formatScore(_ score: Int64, scoreType: ScoreType) -> String {
        switch scoreType {
        case .time:
            return formatTime(score)
        case .value:
            return String(score)
        }
    }
    
    // Format difference score based on type
    func formatDiffScore(_ diff: Int64, scoreType: ScoreType) -> String {
        switch scoreType {
        case .time:
            return formatDiffTime(diff)
        case .value:
            return diff > 0 ? "+\(diff)" : String(diff)
        }
    }
    
    // Get column name based on score type
    func getColumnName(scoreType: ScoreType) -> String {
        switch scoreType {
        case .time:
            return "Time"
        case .value:
            return "Score"
        }
    }
    
    // Format time for display
    func formatTime(_ time: Int64) -> String {
        let milliseconds = time % 1000
        let seconds = (time / 1000) % 60
        let minutes = (time / (1000 * 60)) % 60
        let hours = time / (1000 * 60 * 60)

        if hours > 0 {
            return String(format: "%d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds)
        } else {
            return String(format: "%02d:%02d.%03d", minutes, seconds, milliseconds)
        }
    }
    
    // Format time difference for display
    func formatDiffTime(_ diff: Int64) -> String {
        if diff == 0 {
            return "-"
        }
        
        let sign = diff > 0 ? "+" : "-"
        let absDiff = abs(diff)
        let milliseconds = absDiff % 1000
        let seconds = (absDiff / 1000) % 60
        let minutes = (absDiff / (1000 * 60)) % 60
        
        if minutes > 0 {
            return String(format: "%@%02d:%02d.%03d", sign, minutes, seconds, milliseconds)
        } else {
            return String(format: "%@%02d.%03d", sign, seconds, milliseconds)
        }
    }
}
