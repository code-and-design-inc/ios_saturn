import Foundation
import FirebaseAuth
import FirebaseFirestore

class LeaderBoardViewModel {
    // MARK: - Properties
    let repository = LeaderBoardRepository()
    private let pageSize = 100

    // Leaderboards list
    private(set) var leaderboards: [LeaderBoardRepository.LeaderboardInfo]? {
        didSet {
            onLeaderboardsChanged?(leaderboards ?? [])
        }
    }

    // Current leaderboard info
    private(set) var currentLeaderboardInfo: LeaderBoardRepository.LeaderboardInfo?
    
    // Current leaderboard ID
    private(set) var currentLeaderboardId: String? {
        didSet {
            if oldValue != currentLeaderboardId, let leaderboardId = currentLeaderboardId {
                // Get top score for this leaderboard and save it
                Task {
                    do {
                        // Find current leaderboard info
                        currentLeaderboardInfo = leaderboards?.first { $0.id == leaderboardId }
                        topScore = try await repository.getTopScore(leaderboardId: leaderboardId, leaderboardInfo: currentLeaderboardInfo)
                        print("Set top score for leaderboard \(leaderboardId): \(topScore)")

                        // Get user position and load page
                        await loadUserPositionAndPage()
                    } catch {
                        print("Error getting top score: \(error)")
                        topScore = 0
                    }
                }
            }
        }
    }

    // User position
    private(set) var userPosition: LeaderBoardRepository.UserPosition? {
        didSet {
            onUserPositionChanged?(userPosition)
        }
    }

    // Total score count
    private(set) var totalScoreCount: Int = 0 {
        didSet {
            onTotalScoreCountChanged?(totalScoreCount)
        }
    }

    // User's rank
    private(set) var myRank: Int = 0 {
        didSet {
            onMyRankChanged?(myRank)
        }
    }

    // Scores list
    private(set) var scores: [LeaderBoardRepository.ScoreEntry]? {
        didSet {
            onScoresChanged?(scores ?? [])
        }
    }

    // Loading state
    private(set) var isLoading: Bool = false {
        didSet {
            onLoadingStateChanged?(isLoading)
        }
    }

    // Error message
    private(set) var error: String? {
        didSet {
            onErrorReceived?(error)
        }
    }

    // Close fragment flag
    private(set) var closeFragment: Bool = false {
        didSet {
            onCloseRequested?(closeFragment)
        }
    }

    // Scroll to my rank flag
    private(set) var scrollToMyRank: Bool = false {
        didSet {
            if scrollToMyRank {
                onScrollToMyRankRequested?(true)
            }
        }
    }

    // Pagination flags
    private(set) var hasMoreBefore: Bool = false
    private(set) var hasMoreAfter: Bool = false

    // Pagination documents
    private var firstVisible: DocumentSnapshot?
    private var lastVisible: DocumentSnapshot?

    // Rank tracking
    private var firstRank: Int = 0
    private var lastRank: Int = 0

    // Top score for the current leaderboard
    private var topScore: Int64 = 0

    // MARK: - Callbacks

    var onLeaderboardsChanged: (([LeaderBoardRepository.LeaderboardInfo]) -> Void)?
    var onScoresChanged: (([LeaderBoardRepository.ScoreEntry]) -> Void)?
    var onUserPositionChanged: ((LeaderBoardRepository.UserPosition?) -> Void)?
    var onLoadingStateChanged: ((Bool) -> Void)?
    var onErrorReceived: ((String?) -> Void)?
    var onCloseRequested: ((Bool) -> Void)?
    var onMyRankChanged: ((Int) -> Void)?
    var onTotalScoreCountChanged: ((Int) -> Void)?
    var onScrollToMyRankRequested: ((Bool) -> Void)?

    // MARK: - Public Methods

    // Load leaderboards for a game
    func loadLeaderboards(gameCode: String) {
        Task {
            await MainActor.run {
                isLoading = true
            }
            do {
                let result = try await repository.getLeaderboards(gameCode: gameCode)

                await MainActor.run {
                    self.leaderboards = result

                    if !result.isEmpty {
                        // Select first leaderboard
                        selectLeaderboard(leaderboardId: result[0].id)
                    } else {
                        // No leaderboards available
                        self.error = NSLocalizedString("Leaderboard not supported for this game", comment: "Leaderboard not supported error")
                        self.closeFragment = true
                    }
                }
            } catch {
                print("Error loading leaderboards: \(error)")
                await MainActor.run {
                    self.error = NSLocalizedString("Leaderboard not supported for this game", comment: "Leaderboard not supported error")
                    self.closeFragment = true
                }
            }
            await MainActor.run {
                isLoading = false
            }
        }
    }

    // Select a leaderboard
    func selectLeaderboard(leaderboardId: String) {
        if currentLeaderboardId == leaderboardId { return }

        currentLeaderboardId = leaderboardId
    }

    // Load user position and the page containing it
    func loadUserPositionAndPage() async {
        guard let leaderboardId = currentLeaderboardId else { return }
        let userId = Auth.auth().currentUser?.uid

        await MainActor.run {
            isLoading = true
        }

        do {
            if let userId = userId {
                // Get page with user's score
                let pageResult = try await repository.loadPage(
                    leaderboardId: leaderboardId,
                    pageNumber: 0,
                    userId: userId,
                    topScore: topScore,
                    leaderboardInfo: currentLeaderboardInfo
                )

                // Save results on the main thread
                await MainActor.run {
                    self.scores = pageResult.scores
                    self.firstVisible = pageResult.firstVisible
                    self.lastVisible = pageResult.lastVisible
                    self.hasMoreBefore = pageResult.hasMoreBefore
                    self.hasMoreAfter = pageResult.hasMoreAfter
                    self.totalScoreCount = pageResult.totalCount

                    // Update rank information
                    if let firstScore = pageResult.scores.first, let lastScore = pageResult.scores.last {
                        self.firstRank = firstScore.rank
                        self.lastRank = lastScore.rank
                    }

                    // Find user's score in the list
                    if let userScore = pageResult.scores.first(where: { $0.userId == userId }) {
                        self.userPosition = LeaderBoardRepository.UserPosition(
                            userId: userId,
                            rank: userScore.rank,
                            score: userScore.score
                        )
                        self.myRank = userScore.rank
                    }

                    // Set flag to scroll to user's position
                    self.scrollToMyRank = true
                }
            } else {
                // No user logged in, load first page
                await loadFirstPage()
            }
        } catch {
            print("Error loading user position: \(error)")
            await MainActor.run {
                self.error = NSLocalizedString("Failed to load leaderboard: \(error.localizedDescription)", comment: "Leaderboard loading error")
            }
        }

        await MainActor.run {
            isLoading = false
        }
    }

    // Load first page of scores
    func loadFirstPage() async {
        guard let leaderboardId = currentLeaderboardId else { return }

        await MainActor.run {
            isLoading = true
        }

        do {
            let pageResult = try await repository.loadPage(
                leaderboardId: leaderboardId,
                pageNumber: 0,
                userId: nil,
                topScore: topScore,
                leaderboardInfo: currentLeaderboardInfo
            )

            await MainActor.run {
                self.scores = pageResult.scores
                self.firstVisible = pageResult.firstVisible
                self.lastVisible = pageResult.lastVisible
                self.hasMoreBefore = pageResult.hasMoreBefore
                self.hasMoreAfter = pageResult.hasMoreAfter
                self.totalScoreCount = pageResult.totalCount

                // Update rank information
                if let firstScore = pageResult.scores.first, let lastScore = pageResult.scores.last {
                    self.firstRank = firstScore.rank
                    self.lastRank = lastScore.rank
                }

                // Reset scroll flag
                self.scrollToMyRank = false
            }
        } catch {
            print("Error loading first page: \(error)")
            await MainActor.run {
                self.error = NSLocalizedString("Failed to load page: \(error.localizedDescription)", comment: "Page loading error")
            }
        }

        await MainActor.run {
            isLoading = false
        }
    }

    // Load next page of scores
    func loadNextPage() {
        guard let leaderboardId = currentLeaderboardId,
              let lastDoc = lastVisible,
              !isLoading,
              hasMoreAfter else { return }

        Task {
            await MainActor.run {
                isLoading = true
            }

            do {
                let pageResult = try await repository.loadScoresAfter(
                    leaderboardId: leaderboardId,
                    lastVisible: lastDoc,
                    lastRank: lastRank,
                    topScore: topScore,
                    leaderboardInfo: currentLeaderboardInfo
                )

                await MainActor.run {
                    // Add new scores to the current list
                    var currentScores = self.scores ?? []

                    if !pageResult.scores.isEmpty {
                        // Update ranks for the next page
                        let nextPageScores = pageResult.scores.enumerated().map { index, score in
                            return LeaderBoardRepository.ScoreEntry(
                                userId: score.userId,
                                name: score.name,
                                score: score.score,
                                diff: score.diff,
                                timestamp: score.timestamp,
                                rank: lastRank + index + 1,
                                photoURL: score.photoURL
                            )
                        }

                        currentScores.append(contentsOf: nextPageScores)
                        self.scores = currentScores

                        // Update last rank
                        self.lastRank = self.lastRank + pageResult.scores.count
                    }

                    // Update last visible document
                    if let lastVisible = pageResult.lastVisible {
                        self.lastVisible = lastVisible
                    }

                    // Update pagination flags
                    self.hasMoreAfter = pageResult.hasMoreAfter
                }
            } catch {
                print("Error loading next page: \(error)")
                await MainActor.run {
                    self.error = NSLocalizedString("Failed to load more scores: \(error.localizedDescription)", comment: "Load more error")
                }
            }

            await MainActor.run {
                isLoading = false
            }
        }
    }

    // Load previous page of scores
    func loadPreviousPage() {
        guard let leaderboardId = currentLeaderboardId,
              let firstDoc = firstVisible,
              !isLoading,
              hasMoreBefore else { return }

        Task {
            await MainActor.run {
                isLoading = true
            }

            do {
                let pageResult = try await repository.loadScoresBefore(
                    leaderboardId: leaderboardId,
                    firstVisible: firstDoc,
                    firstRank: firstRank,
                    topScore: topScore,
                    leaderboardInfo: currentLeaderboardInfo
                )

                await MainActor.run {
                    // Add new scores to the beginning of the current list
                    var currentScores = self.scores ?? []

                    if !pageResult.scores.isEmpty {
                        // Update first rank
                        self.firstRank = pageResult.scores.first?.rank ?? self.firstRank

                        // Insert scores at the beginning
                        currentScores.insert(contentsOf: pageResult.scores, at: 0)
                        self.scores = currentScores
                    }

                    // Update first visible document
                    if let firstVisible = pageResult.firstVisible {
                        self.firstVisible = firstVisible
                    }

                    // Update pagination flags
                    self.hasMoreBefore = pageResult.hasMoreBefore
                }
            } catch {
                print("Error loading previous page: \(error)")
                await MainActor.run {
                    self.error = NSLocalizedString("Failed to load previous scores: \(error.localizedDescription)", comment: "Load previous error")
                }
            }

            await MainActor.run {
                isLoading = false
            }
        }
    }

    // MARK: - Helper Methods

    func clearError() {
        error = nil
    }

    func resetCloseFragment() {
        closeFragment = false
    }

    func resetScrollFlag() {
        scrollToMyRank = false
    }
    
    // MARK: - Score Formatting Methods
    
    func formatScore(_ score: Int64) -> String {
        let scoreType = currentLeaderboardInfo?.scoreType ?? .value
        return repository.formatScore(score, scoreType: scoreType)
    }
    
    func formatDiffScore(_ diff: Int64) -> String {
        let scoreType = currentLeaderboardInfo?.scoreType ?? .value
        return repository.formatDiffScore(diff, scoreType: scoreType)
    }
    
    func getColumnName() -> String {
        let scoreType = currentLeaderboardInfo?.scoreType ?? .value
        return repository.getColumnName(scoreType: scoreType)
    }
}
