package org.uoyabause.android.leaderboard

import android.util.Log
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.AggregateSource
import com.google.firebase.firestore.DocumentSnapshot
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.Query
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.tasks.await
import kotlinx.coroutines.withContext

class LeaderBoardRepository {
    private val db = FirebaseFirestore.getInstance()
    private val auth = FirebaseAuth.getInstance()
    private val pageSize = 100
    private var gameId: String = "" // ゲームIDを保持する変数

    companion object {
        const val TAG = "LeaderBoardRepository"
    }

    // スコアエントリーデータクラス
    data class ScoreEntry(
        val userId: String,
        val name: String,
        val score: Long,
        val timestamp: Long,
        val rank: Int,
        val diff: Long = 0, // トップスコアとの差分（自分のスコア - トップスコア）
        val photoUrl: String? = null, // ユーザーのアバター画像URL
    )

    // スコアタイプ定義
    enum class ScoreType {
        TIME, // 時間（タイム）
        VALUE, // 値（スコア）
    }

    // ランキングタイプ定義
    enum class RankingType {
        ASCENDING, // 値が小さいほど上位（タイムなど）
        DESCENDING, // 値が大きいほど上位（スコアなど）
    }

    // リーダーボードデータクラス
    data class LeaderboardInfo(
        val id: String,
        val name: String?,
        val scoreType: ScoreType = ScoreType.VALUE,
        val rankingType: RankingType = RankingType.DESCENDING,
    )

    // ページデータ結果クラス
    data class PageResult(
        val scores: List<ScoreEntry>,
        val lastVisible: DocumentSnapshot?,
        val firstVisible: DocumentSnapshot?,
        val hasMoreBefore: Boolean = false,
        val hasMoreAfter: Boolean = false,
    )

    // ユーザー位置情報クラス
    data class UserPosition(
        val userId: String,
        val score: Long,
        val rank: Int,
        val totalCount: Int,
        val pageNumber: Int,
    )

    // リーダーボードID一覧を取得
    suspend fun getLeaderboards(gameCode: String): List<LeaderboardInfo> =
        withContext(Dispatchers.IO) {
            try {
                // まずgameIdを取得
                val gameDocuments =
                    db
                        .collection("games")
                        .whereEqualTo("product_number", gameCode)
                        .get()
                        .await()

                if (gameDocuments.isEmpty) {
                    Log.d(TAG, "No game found with product_number: $gameCode")
                    return@withContext emptyList<LeaderboardInfo>()
                }

                if (gameDocuments.documents[0].getString("leaderboardId") != null) {
                    this@LeaderBoardRepository.gameId = gameDocuments.documents[0].get("leaderboardId").toString()
                } else {
                    // クラス変数にgameIdを保存
                    this@LeaderBoardRepository.gameId = gameDocuments.documents[0].id
                }

                Log.d(TAG, "Found gameId: ${this@LeaderBoardRepository.gameId}")

                // ゲームIDを使ってリーダーボード一覧を取得
                val leaderboardsResult =
                    db
                        .collection("games/${this@LeaderBoardRepository.gameId}/leaderboards")
                        .get()
                        .await()

                if (leaderboardsResult.isEmpty) {
                    Log.d(TAG, "No leaderboards found for gameId: ${this@LeaderBoardRepository.gameId}")
                    return@withContext emptyList<LeaderboardInfo>()
                }

                Log.d(TAG, "取得ドキュメント数: ${leaderboardsResult.documents.size}")

                // リーダーボード情報をマッピング
                return@withContext leaderboardsResult.documents.map { doc ->
                    // scoreTypeとrankingTypeを取得（デフォルト値を設定）
                    val scoreTypeString = doc.getString("scoreType") ?: "VALUE"
                    val rankingTypeString = doc.getString("rankingType") ?: "DESCENDING"

                    val scoreType =
                        try {
                            ScoreType.valueOf(scoreTypeString.uppercase())
                        } catch (e: IllegalArgumentException) {
                            Log.w(TAG, "Invalid scoreType: $scoreTypeString, using default VALUE")
                            ScoreType.VALUE
                        }

                    val rankingType =
                        try {
                            RankingType.valueOf(rankingTypeString.uppercase())
                        } catch (e: IllegalArgumentException) {
                            Log.w(TAG, "Invalid rankingType: $rankingTypeString, using default DESCENDING")
                            RankingType.DESCENDING
                        }

                    LeaderboardInfo(
                        id = doc.id,
                        name = doc.getString("name"),
                        scoreType = scoreType,
                        rankingType = rankingType,
                    )
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading leaderboards", e)
                return@withContext emptyList<LeaderboardInfo>()
            }
        }

    // 現在のユーザーのスコアから順位を取得
    suspend fun getCurrentPosition(leaderboardId: String): UserPosition? =
        withContext(Dispatchers.IO) {
            return@withContext getCurrentPosition(leaderboardId, null)
        }

    // LeaderboardInfo付きのgetCurrentPositionメソッド
    suspend fun getCurrentPosition(
        leaderboardId: String,
        leaderboardInfo: LeaderboardInfo?,
    ): UserPosition? =
        withContext(Dispatchers.IO) {
            try {
                val userId = auth.currentUser?.uid ?: return@withContext null

                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext null
                }

                Log.d(TAG, "Getting current position for leaderboardId: $leaderboardId, userId: $userId")

                // 総数を取得
                val countQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .count()
                val countSnapshot = countQuery.get(AggregateSource.SERVER).await()
                val totalCount = countSnapshot.count.toInt()

                // 自分のスコアを取得
                val myScoreDoc =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .document(userId)
                        .get()
                        .await()

                if (!myScoreDoc.exists()) {
                    Log.d(TAG, "User score document not found")
                    return@withContext null
                }

                val myScore = myScoreDoc.getLong("score") ?: 0L
                Log.d(TAG, "Found user score: $myScore")

                // 自分の順位を計算
                val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
                val betterScoresQuery =
                    when (rankingType) {
                        RankingType.ASCENDING ->
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .whereLessThan("score", myScore)
                                .count()
                        RankingType.DESCENDING ->
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .whereGreaterThan("score", myScore)
                                .count()
                    }
                val betterScoresCount = betterScoresQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()
                val myRank = betterScoresCount + 1

                // 自分の順位が含まれるページ番号を計算
                val pageNumber = (myRank - 1) / pageSize

                Log.d(TAG, "User position: rank=$myRank, totalCount=$totalCount, pageNumber=$pageNumber")

                return@withContext UserPosition(
                    userId = userId,
                    score = myScore,
                    rank = myRank,
                    totalCount = totalCount,
                    pageNumber = pageNumber,
                )
            } catch (e: Exception) {
                Log.e(TAG, "Error getting current position", e)
                return@withContext null
            }
        }

    // 指定されたページを取得（ユーザーIDが指定された場合はそのユーザーのスコアが含まれるページを取得）
    suspend fun loadPage(
        leaderboardId: String,
        pageNumber: Int,
        userId: String? = null,
        topScore: Long = 0,
    ): PageResult =
        withContext(Dispatchers.IO) {
            return@withContext loadPage(leaderboardId, pageNumber, userId, topScore, null)
        }

    // LeaderboardInfo付きのloadPageメソッド
    suspend fun loadPage(
        leaderboardId: String,
        pageNumber: Int,
        userId: String? = null,
        topScore: Long = 0,
        leaderboardInfo: LeaderboardInfo?,
    ): PageResult =
        withContext(Dispatchers.IO) {
            try {
                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = false,
                        hasMoreAfter = false,
                    )
                }

                Log.d(TAG, "Loading page $pageNumber for leaderboardId: $leaderboardId, userId: $userId")

                // 総数を取得
                val countQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .count()
                val totalCount = countQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()

                if (totalCount == 0) {
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = false,
                        hasMoreAfter = false,
                    )
                }

                // ユーザーIDが指定された場合
                if (userId != null) {
                    // ユーザーのスコアドキュメントを取得
                    val userScoreDoc =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .document(userId)
                            .get()
                            .await()

                    if (!userScoreDoc.exists()) {
                        Log.d(TAG, "User score document not found")
                        // ユーザーのスコアが見つからない場合は最初のページを返す
                        return@withContext loadPageByNumber(leaderboardId, 0, totalCount, topScore)
                    }

                    // ユーザーのスコアを取得
                    val userScore = userScoreDoc.getLong("score") ?: 0L
                    val userTimestamp = userScoreDoc.getLong("timestamp") ?: 0L
                    Log.d(TAG, "Found user score: $userScore, timestamp: $userTimestamp")

                    // ユーザーより良いスコアの数を取得して順位を計算
                    val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
                    val betterScoresQuery =
                        when (rankingType) {
                            RankingType.ASCENDING ->
                                db
                                    .collection("games")
                                    .document(gameId)
                                    .collection("leaderboards")
                                    .document(leaderboardId)
                                    .collection("scores")
                                    .whereLessThan("score", userScore)
                                    .count()
                            RankingType.DESCENDING ->
                                db
                                    .collection("games")
                                    .document(gameId)
                                    .collection("leaderboards")
                                    .document(leaderboardId)
                                    .collection("scores")
                                    .whereGreaterThan("score", userScore)
                                    .count()
                        }
                    val betterScoresCount = betterScoresQuery
                        .get(AggregateSource.SERVER)
                        .await()
                        .count
                        .toInt()
                    val userRank = betterScoresCount + 2

                    // ユーザーの順位からページ番号を計算
                    val pageNumber = (userRank - 1) / pageSize
                    val offset = pageNumber * pageSize

                    Log.d(TAG, "User rank: $userRank, pageNumber: $pageNumber, offset: $offset")

                    // ユーザーのスコアを中心にページサイズ分のデータを取得
                    // 前半分と後半分に分けて取得
                    val halfPageSize = pageSize / 2

                    val sortDirection = getSortDirection(rankingType)
                    val oppositeSortDirection =
                        when (rankingType) {
                            RankingType.ASCENDING -> Query.Direction.DESCENDING
                            RankingType.DESCENDING -> Query.Direction.ASCENDING
                        }

                    // ユーザーのスコアより上位のスコアを取得（最大halfPageSize件）
                    val beforeQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", oppositeSortDirection)
                            .orderBy("timestamp", oppositeSortDirection)
                            .startAt(userScore, userTimestamp)
                            .limit(halfPageSize.toLong())

                    // ユーザーのスコアより下位のスコアを取得（最大halfPageSize件）
                    val afterQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", sortDirection)
                            .orderBy("timestamp", sortDirection)
                            .startAfter(userScore, userTimestamp)
                            .limit(halfPageSize.toLong())

                    val beforeResult = beforeQuery.get().await()
                    val afterResult = afterQuery.get().await()

                    // 結果を結合してソート
                    val combinedScores = mutableListOf<ScoreEntry>()

                    // 前半分のスコアを追加（逆順になっているので逆転して追加）
                    beforeResult.documents.asReversed().forEachIndexed { index, doc ->
                        val rank = userRank - (beforeResult.documents.size - index)
                        if (rank > 0) { // 順位が0以下にならないようにする
                            val score = doc.getLong("score") ?: 0L
                            // トップスコアとの差分を計算
                            val diff = calculateDiff(score, topScore, rankingType)
                            combinedScores.add(
                                ScoreEntry(
                                    userId = doc.id,
                                    name = doc.getString("name") ?: "Unknown",
                                    score = score,
                                    timestamp = doc.getLong("timestamp") ?: 0L,
                                    diff = diff,
                                    rank = rank,
                                    photoUrl = doc.getString("photoUrl"),
                                ),
                            )
                        }
                    }

                    // 後半分のスコアを追加
                    afterResult.documents.forEachIndexed { index, doc ->
                        val score = doc.getLong("score") ?: 0L
                        // トップスコアとの差分を計算
                        val diff = calculateDiff(score, topScore, rankingType)
                        combinedScores.add(
                            ScoreEntry(
                                userId = doc.id,
                                name = doc.getString("name") ?: "Unknown",
                                score = score,
                                timestamp = doc.getLong("timestamp") ?: 0L,
                                diff = diff,
                                rank = userRank + index,
                                photoUrl = doc.getString("photoUrl"),
                            ),
                        )
                    }

                    // ユーザー自身のスコアを追加

/*
                combinedScores.add(
                    ScoreEntry(
                        userId = userId,
                        name = userScoreDoc.getString("name") ?: "Unknown",
                        score = userScore,
                        timestamp = userTimestamp,
                        rank = userRank,
                        photoUrl = userScoreDoc.getString("photoUrl")
                    )
                )
*/
                    // ランキングタイプに応じてソート（同じスコアの場合はタイムスタンプでソート）
                    when (rankingType) {
                        RankingType.ASCENDING -> combinedScores.sortWith(compareBy<ScoreEntry> { it.score }.thenBy { it.timestamp })
                        RankingType.DESCENDING ->
                            combinedScores.sortWith(
                                compareByDescending<ScoreEntry> { it.score }.thenByDescending { it.timestamp },
                            )
                    }

                    // ソート後に順位を再計算
                    // combinedScores.forEachIndexed { index, entry ->
                    //    combinedScores[index] = entry.copy(rank = index + 1)
                    // }

                    // 前後にページがあるか確認
                    val hasMoreBefore = userRank > halfPageSize
                    val hasMoreAfter = (userRank + halfPageSize) < totalCount

                    return@withContext PageResult(
                        scores = combinedScores,
                        firstVisible = beforeResult.documents.asReversed().firstOrNull(),
                        lastVisible = afterResult.documents.lastOrNull(),
                        hasMoreBefore = hasMoreBefore,
                        hasMoreAfter = hasMoreAfter,
                    )
                } else {
                    // ページ番号が指定された場合はそのページを取得
                    return@withContext loadPageByNumber(leaderboardId, pageNumber, totalCount, topScore, leaderboardInfo)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading page", e)
                return@withContext PageResult(
                    scores = emptyList(),
                    lastVisible = null,
                    firstVisible = null,
                    hasMoreBefore = pageNumber > 0,
                    hasMoreAfter = false,
                )
            }
        }

    // ページ番号を指定してページを取得
    suspend fun loadPageByNumber(
        leaderboardId: String,
        pageNumber: Int,
        totalCount: Int,
        topScore: Long = 0,
    ): PageResult =
        withContext(Dispatchers.IO) {
            return@withContext loadPageByNumber(leaderboardId, pageNumber, totalCount, topScore, null)
        }

    // LeaderboardInfo付きのloadPageByNumberメソッド
    suspend fun loadPageByNumber(
        leaderboardId: String,
        pageNumber: Int,
        totalCount: Int,
        topScore: Long = 0,
        leaderboardInfo: LeaderboardInfo?,
    ): PageResult =
        withContext(Dispatchers.IO) {
            val offset = pageNumber * pageSize
            val hasMoreBefore = pageNumber > 0
            val hasMoreAfter = (offset + pageSize) < totalCount

            // ページデータを取得
            val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
            val sortDirection = getSortDirection(rankingType)
            val query =
                db
                    .collection("games")
                    .document(gameId)
                    .collection("leaderboards")
                    .document(leaderboardId)
                    .collection("scores")
                    .orderBy("score", sortDirection)
                    .orderBy("timestamp", sortDirection)
                    .limit(pageSize.toLong())

            if (offset > 0) {
                // オフセット位置までスキップするクエリ
                val skipQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .orderBy("score", sortDirection)
                        .orderBy("timestamp", sortDirection)
                        .limit(offset.toLong())

                val skipResult = skipQuery.get().await()

                if (skipResult.isEmpty) {
                    // オフセット位置にデータがない場合は、最初のページを返す
                    val firstPageResult = query.get().await()
                    return@withContext processPageResult(firstPageResult, 0, false, hasMoreAfter, topScore)
                }

                // オフセットの最後のドキュメントを取得
                val lastSkippedDoc = skipResult.documents.lastOrNull()

                if (lastSkippedDoc != null) {
                    // 目的のページを取得
                    val pageQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", sortDirection)
                            .orderBy("timestamp", sortDirection)
                            .startAfter(lastSkippedDoc)
                            .limit(pageSize.toLong())

                    val pageResult = pageQuery.get().await()
                    return@withContext processPageResult(pageResult, offset, hasMoreBefore, hasMoreAfter, topScore, leaderboardInfo)
                } else {
                    // オフセットの最後のドキュメントが取得できない場合は、最初のページを返す
                    val firstPageResult = query.get().await()
                    return@withContext processPageResult(firstPageResult, 0, false, hasMoreAfter, topScore, leaderboardInfo)
                }
            } else {
                // 最初のページの場合は直接取得
                val pageResult = query.get().await()
                return@withContext processPageResult(pageResult, 0, false, hasMoreAfter, topScore, leaderboardInfo)
            }
        }

    // 指定されたユーザーのスコアが含まれるページを取得
    suspend fun loadPageForUser(
        leaderboardId: String,
        userId: String,
        topScore: Long = 0,
    ): PageResult =
        withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "Loading page for user: $userId in leaderboard: $leaderboardId")

                // ユーザーのスコアドキュメントを取得
                val userScoreDoc =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .document(userId)
                        .get()
                        .await()

                if (!userScoreDoc.exists()) {
                    Log.d(TAG, "User score document not found")
                    // ユーザーのスコアが見つからない場合は最初のページを返す
                    val firstPageQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", Query.Direction.ASCENDING)
                            .orderBy("timestamp")
                            .limit(pageSize.toLong())

                    val firstPageResult = firstPageQuery.get().await()
                    return@withContext processPageResult(firstPageResult, 0, false, true)
                }

                // ユーザーのスコアを取得
                val userScore = userScoreDoc.getLong("score") ?: 0L
                Log.d(TAG, "Found user score: $userScore")

                // 総数を取得
                val countQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .count()
                val totalCount = countQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()

                // ユーザーより良いスコアの数を取得して順位を計算
                val betterScoresQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .whereLessThan("score", userScore)
                        .count()
                val betterScoresCount = betterScoresQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()
                val userRank = betterScoresCount + 1

                // ユーザーの順位からページ番号を計算
                val pageNumber = (userRank - 1) / pageSize
                val offset = pageNumber * pageSize

                Log.d(TAG, "User rank: $userRank, pageNumber: $pageNumber, offset: $offset")

                // ユーザーのスコアを含むページを取得
                if (offset > 0) {
                    // オフセットがある場合は、その分だけスキップする必要がある
                    val offsetQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", Query.Direction.ASCENDING)
                            .orderBy("timestamp")
                            .limit(offset.toLong())

                    val offsetResult = offsetQuery.get().await()

                    if (offsetResult.isEmpty) {
                        // オフセット位置にデータがない場合は、最初のページを返す
                        Log.d(TAG, "No data found at offset, returning first page")
                        val firstPageQuery =
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .orderBy("score", Query.Direction.ASCENDING)
                                .orderBy("timestamp")
                                .limit(pageSize.toLong())

                        val firstPageResult = firstPageQuery.get().await()
                        return@withContext processPageResult(firstPageResult, 0, false, true)
                    }

                    // オフセットの最後のドキュメントを取得
                    val offsetLastDoc = offsetResult.documents.lastOrNull()

                    if (offsetLastDoc == null) {
                        Log.e(TAG, "offsetLastDoc is null, cannot use startAfter")
                        // 最初のページを返す
                        val firstPageQuery =
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .orderBy("score", Query.Direction.ASCENDING)
                                .orderBy("timestamp")
                                .limit(pageSize.toLong())

                        val firstPageResult = firstPageQuery.get().await()
                        return@withContext processPageResult(firstPageResult, 0, false, true)
                    }

                    // 目的のページを取得
                    val pageQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", Query.Direction.ASCENDING)
                            .orderBy("timestamp")
                            .startAfter(offsetLastDoc)
                            .limit(pageSize.toLong())

                    val pageResult = pageQuery.get().await()

                    if (pageResult.isEmpty) {
                        // 次のページにデータがない場合は、最初のページを返す
                        Log.d(TAG, "No data found in user page, returning first page")
                        val firstPageQuery =
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .orderBy("score", Query.Direction.ASCENDING)
                                .orderBy("timestamp")
                                .limit(pageSize.toLong())

                        val firstPageResult = firstPageQuery.get().await()
                        return@withContext processPageResult(firstPageResult, 0, false, true)
                    }

                    val hasMoreBefore = pageNumber > 0
                    val hasMoreAfter = (offset + pageSize) < totalCount

                    return@withContext processPageResult(pageResult, offset, hasMoreBefore, hasMoreAfter)
                } else {
                    // 最初のページの場合は直接取得
                    val pageQuery =
                        db
                            .collection("games")
                            .document(gameId)
                            .collection("leaderboards")
                            .document(leaderboardId)
                            .collection("scores")
                            .orderBy("score", Query.Direction.ASCENDING)
                            .orderBy("timestamp")
                            .limit(pageSize.toLong())

                    val pageResult = pageQuery.get().await()
                    return@withContext processPageResult(pageResult, 0, false, (pageSize < totalCount))
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading page for user", e)
                return@withContext PageResult(
                    scores = emptyList(),
                    lastVisible = null,
                    firstVisible = null,
                    hasMoreBefore = false,
                    hasMoreAfter = false,
                )
            }
        }

    // ページ結果を処理するヘルパーメソッド
    private fun processPageResult(
        pageResult: com.google.firebase.firestore.QuerySnapshot,
        offset: Int,
        hasMoreBefore: Boolean,
        hasMoreAfter: Boolean,
        topScore: Long = 0,
    ): PageResult = processPageResult(pageResult, offset, hasMoreBefore, hasMoreAfter, topScore, null)

    // LeaderboardInfo付きのprocessPageResultメソッド
    private fun processPageResult(
        pageResult: com.google.firebase.firestore.QuerySnapshot,
        offset: Int,
        hasMoreBefore: Boolean,
        hasMoreAfter: Boolean,
        topScore: Long = 0,
        leaderboardInfo: LeaderboardInfo?,
    ): PageResult {
        if (pageResult.isEmpty) {
            return PageResult(
                scores = emptyList(),
                lastVisible = null,
                firstVisible = null,
                hasMoreBefore = hasMoreBefore,
                hasMoreAfter = false,
            )
        }

        // ページデータをマッピング
        val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
        val scores =
            pageResult.documents.mapIndexed { index, doc ->
                val score = doc.getLong("score") ?: 0L
                // トップスコアとの差分を計算
                val diff = calculateDiff(score, topScore, rankingType)

                ScoreEntry(
                    userId = doc.id,
                    name = doc.getString("name") ?: "Unknown",
                    score = score,
                    timestamp = doc.getLong("timestamp") ?: 0L,
                    rank = offset + index + 1,
                    diff = diff,
                    photoUrl = doc.getString("photoUrl"),
                )
            }

        return PageResult(
            scores = scores,
            firstVisible = pageResult.documents.firstOrNull(),
            lastVisible = pageResult.documents.lastOrNull(),
            hasMoreBefore = hasMoreBefore,
            hasMoreAfter = hasMoreAfter,
        )
    }

    // 次のページを取得
    suspend fun loadScoreAfter(
        leaderboardId: String,
        lastVisible: DocumentSnapshot,
        lastRank: Int = 0,
        topScore: Long = 0,
    ): PageResult =
        withContext(Dispatchers.IO) {
            return@withContext loadScoreAfter(leaderboardId, lastVisible, lastRank, topScore, null)
        }

    // LeaderboardInfo付きのloadScoreAfterメソッド
    suspend fun loadScoreAfter(
        leaderboardId: String,
        lastVisible: DocumentSnapshot,
        lastRank: Int = 0,
        topScore: Long = 0,
        leaderboardInfo: LeaderboardInfo?,
    ): PageResult =
        withContext(Dispatchers.IO) {
            try {
                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = true,
                        hasMoreAfter = false,
                    )
                }

                Log.d(TAG, "Loading scores after for leaderboardId: $leaderboardId")

                // 最後のドキュメントの後から次のページを取得
                val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
                val sortDirection = getSortDirection(rankingType)
                val query =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .orderBy("score", sortDirection)
                        .orderBy("timestamp", sortDirection)
                        .startAfter(lastVisible)
                        .limit(pageSize.toLong())

                val result = query.get().await()

                if (result.isEmpty) {
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = true,
                        hasMoreAfter = false,
                    )
                }

                // ViewModelから渡された最後の順位を使用
                val startRank = if (lastRank > 0) lastRank + 1 else 1

                // ページデータをマッピング
                val scores =
                    result.documents.mapIndexed { index, doc ->
                        val score = doc.getLong("score") ?: 0L
                        // トップスコアとの差分を計算
                        val diff = calculateDiff(score, topScore, rankingType)
                        ScoreEntry(
                            userId = doc.id,
                            name = doc.getString("name") ?: "Unknown",
                            score = score,
                            timestamp = doc.getLong("timestamp") ?: 0L,
                            rank = startRank + index,
                            diff = diff,
                            photoUrl = doc.getString("photoUrl"),
                        )
                    }

                // 総数を取得して、次のページがあるか確認
                val countQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .count()
                val totalCount = countQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()

                val lastRankInCurrentPage = startRank + scores.size - 1
                val hasMoreAfter = lastRankInCurrentPage < totalCount

                return@withContext PageResult(
                    scores = scores,
                    firstVisible = result.documents.firstOrNull(),
                    lastVisible = result.documents.lastOrNull(),
                    hasMoreBefore = true,
                    hasMoreAfter = hasMoreAfter,
                )
            } catch (e: Exception) {
                Log.e(TAG, "Error loading scores after", e)
                return@withContext PageResult(
                    scores = emptyList(),
                    lastVisible = null,
                    firstVisible = null,
                    hasMoreBefore = true,
                    hasMoreAfter = false,
                )
            }
        }

    // 前のページを取得
    suspend fun loadScoresBefore(
        leaderboardId: String,
        firstVisible: DocumentSnapshot,
        firstRank: Int = 0,
        topScore: Long = 0,
    ): PageResult =
        withContext(Dispatchers.IO) {
            return@withContext loadScoresBefore(leaderboardId, firstVisible, firstRank, topScore, null)
        }

    // LeaderboardInfo付きのloadScoresBeforeメソッド
    suspend fun loadScoresBefore(
        leaderboardId: String,
        firstVisible: DocumentSnapshot,
        firstRank: Int = 0,
        topScore: Long = 0,
        leaderboardInfo: LeaderboardInfo?,
    ): PageResult =
        withContext(Dispatchers.IO) {
            try {
                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = false,
                        hasMoreAfter = true,
                    )
                }

                Log.d(TAG, "Loading scores before for leaderboardId: $leaderboardId")

                // 最初のドキュメントのスコアを取得
                val firstScore = firstVisible.getLong("score") ?: 0L

                // 前のページを取得（limitToLastで最後のpageSize件を取得）
                val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
                val sortDirection = getSortDirection(rankingType)
                val query =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .orderBy("score", sortDirection)
                        .orderBy("timestamp", sortDirection)
                        .let { query ->
                            when (rankingType) {
                                RankingType.ASCENDING -> query.whereLessThan("score", firstScore)
                                RankingType.DESCENDING -> query.whereGreaterThan("score", firstScore)
                            }
                        }.limitToLast(pageSize.toLong())

                val result = query.get().await()

                if (result.isEmpty) {
                    return@withContext PageResult(
                        scores = emptyList(),
                        lastVisible = null,
                        firstVisible = null,
                        hasMoreBefore = false,
                        hasMoreAfter = true,
                    )
                }

                // ViewModelから渡された最初の順位を使用
                // 前のページの最初の順位を計算
                val startRank =
                    if (firstRank > 0) {
                        // 前のページの最初の順位からページサイズ分引く
                        val calculatedRank = firstRank - result.documents.size
                        if (calculatedRank < 1) 1 else calculatedRank
                    } else {
                        // firstRankが指定されていない場合はドキュメント数から推定
                        val totalCount =
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .count()
                                .get(AggregateSource.SERVER)
                                .await()
                                .count
                                .toInt()

                        val betterScoresQuery =
                            db
                                .collection("games")
                                .document(gameId)
                                .collection("leaderboards")
                                .document(leaderboardId)
                                .collection("scores")
                                .let { query ->
                                    val firstScore = firstVisible.getLong("score") ?: 0L
                                    when (rankingType) {
                                        RankingType.ASCENDING -> query.whereLessThan("score", firstScore)
                                        RankingType.DESCENDING -> query.whereGreaterThan("score", firstScore)
                                    }
                                }.count()
                        val betterScoresCount = betterScoresQuery
                            .get(AggregateSource.SERVER)
                            .await()
                            .count
                            .toInt()
                        betterScoresCount + 1 - result.documents.size
                    }

                // ページデータをマッピング
                val scores =
                    result.documents.mapIndexed { index, doc ->
                        val score = doc.getLong("score") ?: 0L
                        // トップスコアとの差分を計算
                        val diff = calculateDiff(score, topScore, rankingType)
                        ScoreEntry(
                            userId = doc.id,
                            name = doc.getString("name") ?: "Unknown",
                            score = score,
                            timestamp = doc.getLong("timestamp") ?: 0L,
                            rank = startRank + index,
                            diff = diff,
                            photoUrl = doc.getString("photoUrl"),
                        )
                    }

                // 前のページがあるか確認
                val hasMoreBefore = firstRank > 1

                return@withContext PageResult(
                    scores = scores,
                    firstVisible = result.documents.firstOrNull(),
                    lastVisible = result.documents.lastOrNull(),
                    hasMoreBefore = hasMoreBefore,
                    hasMoreAfter = true,
                )
            } catch (e: Exception) {
                Log.e(TAG, "Error loading scores before", e)
                return@withContext PageResult(
                    scores = emptyList(),
                    lastVisible = null,
                    firstVisible = null,
                    hasMoreBefore = false,
                    hasMoreAfter = true,
                )
            }
        }

    // フォーマット関数
    fun formatTime(msec: Long): String {
        val min = msec / 60000
        val sec = (msec % 60000) / 1000
        val ms = msec % 1000
        return String.format("%d:%02d.%03d", min, sec, ms)
    }

    // 差分時間のフォーマット関数（"+"記号付き）
    fun formatDiffTime(msec: Long): String {
        if (msec <= 0) return "0:00.000"
        val min = msec / 60000
        val sec = (msec % 60000) / 1000
        val ms = msec % 1000
        return String.format("+%d:%02d.%03d", min, sec, ms)
    }

    // 指定されたリーダーボードの総スコア数を取得
    suspend fun getTotalScoreCount(leaderboardId: String): Int =
        withContext(Dispatchers.IO) {
            try {
                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext 0
                }

                Log.d(TAG, "Getting total score count for leaderboardId: $leaderboardId")

                // スコアの総数を取得
                val countQuery =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .count()

                val totalCount = countQuery
                    .get(AggregateSource.SERVER)
                    .await()
                    .count
                    .toInt()
                Log.d(TAG, "Total score count for leaderboardId: $leaderboardId is $totalCount")

                return@withContext totalCount
            } catch (e: Exception) {
                Log.e(TAG, "Error getting total score count", e)
                return@withContext 0
            }
        }

    // 指定されたリーダーボードのトップスコア（最高スコア）を取得
    suspend fun getTopScore(leaderboardId: String): Long =
        withContext(Dispatchers.IO) {
            return@withContext getTopScore(leaderboardId, null)
        }

    // LeaderboardInfo付きのgetTopScoreメソッド
    suspend fun getTopScore(
        leaderboardId: String,
        leaderboardInfo: LeaderboardInfo?,
    ): Long =
        withContext(Dispatchers.IO) {
            try {
                if (gameId.isEmpty()) {
                    Log.e(TAG, "gameId is not set. Call getLeaderboards first.")
                    return@withContext 0L
                }

                Log.d(TAG, "Getting top score for leaderboardId: $leaderboardId")

                // ランキングタイプに応じてソート方向を決定
                val rankingType = leaderboardInfo?.rankingType ?: RankingType.DESCENDING
                val sortDirection = getSortDirection(rankingType)

                // トップスコアを取得するためには、ランキングの最上位（1位）のスコアを取得
                // ASCENDING（小さい方が上位）の場合は最小値、DESCENDING（大きい方が上位）の場合は最大値
                val query =
                    db
                        .collection("games")
                        .document(gameId)
                        .collection("leaderboards")
                        .document(leaderboardId)
                        .collection("scores")
                        .orderBy(
                            "score",
                            when (rankingType) {
                                RankingType.ASCENDING -> Query.Direction.ASCENDING // 最小値が1位
                                RankingType.DESCENDING -> Query.Direction.DESCENDING // 最大値が1位
                            },
                        ).limit(1)

                val result = query.get().await()

                if (result.isEmpty) {
                    Log.d(TAG, "No scores found for leaderboardId: $leaderboardId")
                    return@withContext 0L
                }

                // トップスコア（最小値）を取得
                val topScore = result.documents.firstOrNull()?.getLong("score") ?: 0L
                Log.d(TAG, "Top score for leaderboardId: $leaderboardId is $topScore")

                return@withContext topScore
            } catch (e: Exception) {
                Log.e(TAG, "Error getting top score", e)
                return@withContext 0L
            }
        }

    // スコアの差分を計算するヘルパーメソッド
    fun calculateDiff(
        score: Long,
        topScore: Long,
        rankingType: RankingType,
    ): Long = if (topScore > 0 && score > 0) {
        when (rankingType) {
            RankingType.ASCENDING -> score - topScore // タイムの場合：自分のタイム - 最高タイム
            RankingType.DESCENDING -> topScore - score // スコアの場合：最高スコア - 自分のスコア
        }
    } else {
        0L
    }

    // 既存のメソッドとの互換性のため（デフォルトはASCENDING）
    fun calculateDiff(
        score: Long,
        topScore: Long,
    ): Long = calculateDiff(score, topScore, RankingType.ASCENDING)

    // ソート方向を取得するヘルパーメソッド
    private fun getSortDirection(rankingType: RankingType): Query.Direction = when (rankingType) {
        RankingType.ASCENDING -> Query.Direction.ASCENDING
        RankingType.DESCENDING -> Query.Direction.DESCENDING
    }

    // スコア表示形式を判定するヘルパーメソッド
    fun shouldDisplayAsTime(scoreType: ScoreType): Boolean = scoreType == ScoreType.TIME

    // スコアを適切な形式でフォーマットするメソッド
    fun formatScore(
        score: Long,
        scoreType: ScoreType,
    ): String = when (scoreType) {
        ScoreType.TIME -> formatTime(score) // 時間形式
        ScoreType.VALUE -> score.toString() // 数値形式
    }

    // 差分スコアを適切な形式でフォーマットするメソッド
    fun formatDiffScore(
        diffScore: Long,
        scoreType: ScoreType,
    ): String = when (scoreType) {
        ScoreType.TIME -> formatDiffTime(diffScore) // 時間差分形式
        ScoreType.VALUE -> {
            if (diffScore > 0) "+$diffScore" else diffScore.toString() // 数値差分形式
        }
    }

    // カラム名を取得するメソッド
    fun getColumnName(scoreType: ScoreType): String = when (scoreType) {
        ScoreType.TIME -> "Time"
        ScoreType.VALUE -> "Score"
    }
}
