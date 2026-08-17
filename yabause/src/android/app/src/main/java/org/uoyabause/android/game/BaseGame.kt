package org.uoyabause.android.game

import android.os.Bundle
import android.util.Log
import android.widget.Toast
import com.google.android.gms.tasks.Tasks
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.Query
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication
import org.uoyabause.android.auth.AuthState

class LeaderBoard(
    val title: String,
    val id: String,
)

/**
 * RetroAchievements leaderboard configuration
 */
data class RetroAchievementsLeaderboardConfig(
    val type: String, // "time" | "score"
    val direction: String, // "ascending" | "descending"
    val webhookKey: String, // Discord webhook config key
    val displayFormat: String, // "time_ms" | "time_sec" | "number"
    val name: String? = null, // Leaderboard name (optional)
)

abstract interface GameUiEvent {
    abstract fun onNewRecord(leaderBoardId: String)
}

/**
 * Generic game implementation for RetroAchievements support
 */
class GenericGame(
    private val gameCode: String,
) : BaseGame() {
    init {
        // Initialize game ID from Firestore for RetroAchievements support
        kotlinx.coroutines.CoroutineScope(kotlinx.coroutines.Dispatchers.IO).launch {
            try {
                initGameId(gameCode)
                Log.d("GenericGame", "Initialized generic game: $gameCode -> gameId: $gameId")
            } catch (e: Exception) {
                Log.e("GenericGame", "Failed to initialize game ID for $gameCode", e)
            }
        }
    }

    override fun onBackUpUpdated(
        fname: String,
        before: ByteArray,
        after: ByteArray,
    ) {
        // Generic implementation - no specific backup handling needed
        Log.d("GenericGame", "Backup updated for game $gameCode: $fname")
    }
}

abstract class BaseGame {
    // gameCodeからgameIdの取得
    suspend fun initGameId(gameCode: String) =
        withContext(Dispatchers.IO) {
            val db = FirebaseFirestore.getInstance()
            val task =
                db
                    .collection("games")
                    .whereEqualTo("product_number", gameCode)
                    .get()

            // タスクが完了するまで待機
            val documents = Tasks.await(task)

            if (!documents.isEmpty) {
                val doc = documents.documents[0]

                // leaderboardIdフィールドがある場合はその値を使用
                if (doc.get("leaderboardId") != null) {
                    gameId = doc.getString("leaderboardId") ?: doc.id
                } else {
                    // なければドキュメントIDを使用
                    gameId = doc.id
                }

                // Load RetroAchievements leaderboard configurations
                loadRetroAchievementsLeaderboards(doc)
            }
        }

    var gameId: String = ""
    var leaderBoards: MutableList<LeaderBoard>? = null

    // RetroAchievements leaderboard configurations
    private var retroAchievementsLeaderboards: Map<String, RetroAchievementsLeaderboardConfig> = emptyMap()

    lateinit var uievent: GameUiEvent

    fun setUiEvent(uievent: GameUiEvent) {
        this.uievent = uievent
    }

    abstract fun onBackUpUpdated(
        fname: String,
        before: ByteArray,
        after: ByteArray,
    )

    /**
     * Load RetroAchievements leaderboard configurations from Firestore document
     */
    @Suppress("UNCHECKED_CAST")
    private fun loadRetroAchievementsLeaderboards(document: com.google.firebase.firestore.DocumentSnapshot) {
        try {
            val leaderboardsData = document.get("retroachievements_leaderboards") as? Map<String, Any>
            if (leaderboardsData != null) {
                val configs = mutableMapOf<String, RetroAchievementsLeaderboardConfig>()

                for ((leaderboardId, configData) in leaderboardsData) {
                    if (configData is Map<*, *>) {
                        val config =
                            RetroAchievementsLeaderboardConfig(
                                type = configData["type"] as? String ?: "score",
                                direction = configData["direction"] as? String ?: "descending",
                                webhookKey = configData["webhookKey"] as? String ?: "",
                                displayFormat = configData["displayFormat"] as? String ?: "number",
                                name = configData["name"] as? String,
                            )
                        configs[leaderboardId] = config
                    }
                }

                retroAchievementsLeaderboards = configs
                Log.d("BaseGame", "Loaded ${configs.size} RetroAchievements leaderboard configurations for game $gameId")
            } else {
                Log.d("BaseGame", "No RetroAchievements leaderboard configurations found for game $gameId")
            }
        } catch (e: Exception) {
            Log.e("BaseGame", "Error loading RetroAchievements leaderboard configurations", e)
            retroAchievementsLeaderboards = emptyMap()
        }
    }

    /**
     * Get RetroAchievements leaderboard configuration by leaderboard ID
     */
    fun getRetroAchievementsLeaderboardConfig(
        leaderboardId: String,
    ): RetroAchievementsLeaderboardConfig? = retroAchievementsLeaderboards[leaderboardId]

    /**
     * Check if new score is better than current score based on leaderboard configuration
     */
    private fun isNewScoreBetter(
        newScore: Long,
        currentScore: Long?,
        config: RetroAchievementsLeaderboardConfig?,
    ): Boolean {
        if (currentScore == null) return true

        return when (config?.direction) {
            "ascending" -> newScore < currentScore // For time-based leaderboards (lower is better)
            "descending" -> newScore > currentScore // For score-based leaderboards (higher is better)
            else -> newScore < currentScore // Default: assume time-based (ascending)
        }
    }

    // Shared leaderboard functionality
    protected fun submitScoreToFirestore(
        gameId: String,
        leaderboardId: String,
        score: Long,
        userName: String,
        webhookConfigKey: String,
        onSuccess: (() -> Unit)? = null,
        onFailure: ((Exception) -> Unit)? = null,
    ) {
        val db = FirebaseFirestore.getInstance()
        val currentUser =
            AuthState.realUser() ?: run {
                onFailure?.invoke(Exception("ユーザーが認証されていません"))
                return
            }
        val userId = currentUser.uid

        // ユーザーの画像URLを取得
        val photoUrl = currentUser.photoUrl?.toString()

        // Ensure we're using the Firebase user ID for the document ID
        // but still display the user's name (which might be from Discord)
        val scoreData =
            hashMapOf(
                "name" to userName,
                "score" to score,
                "timestamp" to System.currentTimeMillis(),
                "photoUrl" to photoUrl,
                "firebaseUid" to userId, // Store the Firebase UID explicitly
            )
        val scoreDocRef =
            db
                .collection("games/$gameId/leaderboards")
                .document(leaderboardId)
                .collection("scores")
                .document(userId)

        scoreDocRef
            .get()
            .addOnSuccessListener { document ->
                val currentScore = document.getLong("score")
                if (currentScore == null || score < currentScore) {
                    // 新記録（より短いタイム）の場合のみ上書き - この比較ロジックは後で設定ベースに変更予定
                    scoreDocRef
                        .set(scoreData)
                        .addOnSuccessListener {
                            // 新記録が登録されたら、それが1位かどうかを確認する
                            checkIfNewTopScore(gameId, leaderboardId, score, userName, photoUrl, webhookConfigKey)
                            onSuccess?.invoke()
                        }.addOnFailureListener { e -> onFailure?.invoke(e) }
                } else {
                    // 記録を更新しない場合も成功扱い
                    onSuccess?.invoke()
                }
            }.addOnFailureListener { e ->
                onFailure?.invoke(e)
            }
    }

    /**
     * 新しいスコアが1位かどうかを確認し、1位の場合はDiscordに投稿する
     *
     * @param gameId ゲームID
     * @param leaderboardId リーダーボードID
     * @param score スコア（タイム）
     * @param userName ユーザー名
     * @param photoUrl ユーザーのアバターURL
     * @param webhookConfigKey Remote Configのwebhook URLキー
     */
    private fun checkIfNewTopScore(
        gameId: String,
        leaderboardId: String,
        score: Long,
        userName: String,
        photoUrl: String?,
        webhookConfigKey: String,
    ) {
        val db = FirebaseFirestore.getInstance()
        val context = YabauseApplication.appContext

        // Firebase Remote ConfigからDiscord Webhook URLを取得
        val remoteConfig = com.google.firebase.remoteconfig.FirebaseRemoteConfig
            .getInstance()
        val webhookUrl = remoteConfig.getString(webhookConfigKey)

        // Webhook URLが空または無効な場合は処理を終了
        if (webhookUrl.isEmpty()) {
            Log.d("BaseGame", "Discord webhook URL is not set in Remote Config for key: $webhookConfigKey")
            return
        }

        // リーダーボードのスコアを取得して、新しいスコアが1位かどうかを確認
        db
            .collection("games/$gameId/leaderboards")
            .document(leaderboardId)
            .collection("scores")
            .orderBy("score", Query.Direction.ASCENDING) // タイムアタックなので昇順（小さい方が良い）
            .limit(1)
            .get()
            .addOnSuccessListener { querySnapshot ->
                if (querySnapshot.isEmpty) {
                    // スコアがない場合は、新しいスコアが1位
                    postNewTopScoreToDiscord(gameId, leaderboardId, score, userName, photoUrl, webhookUrl)
                    return@addOnSuccessListener
                }

                // 1位のスコアを取得
                val topScore = querySnapshot.documents[0].getLong("score") ?: Long.MAX_VALUE
                val topScoreUserId = querySnapshot.documents[0].id

                // 自分のスコアが1位かどうかを確認
                val currentUser = AuthState.realUser()
                if (currentUser != null && (score <= topScore || topScoreUserId == currentUser.uid)) {
                    // 新しいスコアが1位の場合、またはすでに自分が1位の場合
                    // リーダーボード名を取得
                    db
                        .collection("games/$gameId/leaderboards")
                        .document(leaderboardId)
                        .get()
                        .addOnSuccessListener { leaderboardDoc ->
                            val leaderboardName = leaderboardDoc.getString("name") ?: leaderboardId
                            postNewTopScoreToDiscord(gameId, leaderboardName, score, userName, photoUrl, webhookUrl)
                        }.addOnFailureListener { e ->
                            Log.e("BaseGame", "Error getting leaderboard name", e)
                        }
                } else {
                    Log.d("BaseGame", "New score is not the top score. Top: $topScore, New: $score")
                }
            }.addOnFailureListener { e ->
                Log.e("BaseGame", "Error checking if new score is top score", e)
            }
    }

    /**
     * 新記録（1位）をDiscordに投稿する
     *
     * @param gameId ゲームID
     * @param leaderboardName リーダーボード名
     * @param score スコア（タイム）
     * @param userName ユーザー名
     * @param photoUrl ユーザーのアバターURL
     * @param webhookUrl Discord WebhookのURL
     */
    private fun postNewTopScoreToDiscord(
        gameId: String,
        leaderboardName: String,
        score: Long,
        userName: String,
        photoUrl: String?,
        webhookUrl: String,
    ) {
        // バックグラウンドスレッドで実行
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val result =
                    org.uoyabause.android.util.DiscordWebhook.sendNewRecordMessage(
                        webhookUrl = webhookUrl,
                        gameId = gameId,
                        leaderboardName = leaderboardName,
                        userName = userName,
                        score = score,
                        avatarUrl = photoUrl,
                    )

                if (result) {
                    Log.d("BaseGame", "Successfully posted new top score to Discord")
                    // 成功通知を表示
                    withContext(Dispatchers.Main) {
                        val context = YabauseApplication.appContext
                        Toast
                            .makeText(
                                context,
                                context.getString(R.string.discord_webhook_notification_title) + ": " +
                                    context.getString(R.string.discord_webhook_notification_message),
                                Toast.LENGTH_SHORT,
                            ).show()
                    }
                } else {
                    Log.e("BaseGame", "Failed to post new top score to Discord")
                }
            } catch (e: Exception) {
                Log.e("BaseGame", "Error posting new top score to Discord", e)
            }
        }
    }

    /**
     * Submit RetroAchievements leaderboard score to Firebase with configuration support
     */
    fun submitRetroAchievementsScore(
        retroAchievementsLeaderboardId: String,
        score: Long,
        userName: String,
        title: String = "Leaderboard",
        onSuccess: (() -> Unit)? = null,
        onFailure: ((Exception) -> Unit)? = null,
    ) {
        // Get leaderboard configuration or create default
        val config =
            getRetroAchievementsLeaderboardConfig(retroAchievementsLeaderboardId) ?: run {
                Log.d("BaseGame", "No configuration found for RetroAchievements leaderboard $retroAchievementsLeaderboardId, using default")
                RetroAchievementsLeaderboardConfig(
                    type = "score",
                    direction = "descending",
                    webhookKey = "",
                    displayFormat = "number",
                    name = title,
                )
            }

        if (gameId.isEmpty()) {
            val error = Exception("Game ID not initialized. Call initGameId() first.")
            onFailure?.invoke(error)
            return
        }

        // Use RetroAchievements leaderboard ID as Firebase leaderboard ID
        val firebaseLeaderboardId = retroAchievementsLeaderboardId

        Log.d(
            "BaseGame",
            "Submitting RetroAchievements score: game=$gameId, leaderboard=$firebaseLeaderboardId, score=$score, type=${config.type}, direction=${config.direction}",
        )

        // Submit to Firebase with configuration-aware scoring
        submitScoreToFirestoreWithConfig(
            gameId = gameId,
            leaderboardId = firebaseLeaderboardId,
            score = score,
            userName = userName,
            config = config,
            leaderboardTitle = title,
            onSuccess = onSuccess,
            onFailure = onFailure,
        )
    }

    /**
     * Submit score to Firestore with RetroAchievements configuration support
     */
    private fun submitScoreToFirestoreWithConfig(
        gameId: String,
        leaderboardId: String,
        score: Long,
        userName: String,
        config: RetroAchievementsLeaderboardConfig,
        leaderboardTitle: String,
        onSuccess: (() -> Unit)? = null,
        onFailure: ((Exception) -> Unit)? = null,
    ) {
        val db = FirebaseFirestore.getInstance()
        val currentUser =
            AuthState.realUser() ?: run {
                onFailure?.invoke(Exception("ユーザーが認証されていません"))
                return
            }
        val userId = currentUser.uid

        // ユーザーの画像URLを取得
        val photoUrl = currentUser.photoUrl?.toString()

        // Ensure we're using the Firebase user ID for the document ID
        // but still display the user's name (which might be from Discord)
        val scoreData =
            hashMapOf(
                "name" to userName,
                "score" to score,
                "timestamp" to System.currentTimeMillis(),
                "photoUrl" to photoUrl,
                "firebaseUid" to userId, // Store the Firebase UID explicitly
            )

        // First, ensure leaderboard document exists with name
        val leaderboardDocRef =
            db
                .collection("games/$gameId/leaderboards")
                .document(leaderboardId)

        // Set leaderboard name (this will create the document if it doesn't exist)
        leaderboardDocRef
            .set(
                hashMapOf("name" to leaderboardTitle),
                com.google.firebase.firestore.SetOptions
                    .merge(),
            ).addOnFailureListener { e ->
                Log.w("BaseGame", "Failed to set leaderboard name", e)
            }

        val scoreDocRef = leaderboardDocRef.collection("scores").document(userId)

        scoreDocRef
            .get()
            .addOnSuccessListener { document ->
                val currentScore = document.getLong("score")
                if (isNewScoreBetter(score, currentScore, config)) {
                    // 新記録の場合のみ上書き (設定に基づく比較)
                    scoreDocRef
                        .set(scoreData)
                        .addOnSuccessListener {
                            // 新記録が登録されたら、それが1位かどうかを確認する
                            checkIfNewTopScoreWithConfig(gameId, leaderboardId, score, userName, photoUrl, config)
                            onSuccess?.invoke()
                        }.addOnFailureListener { e -> onFailure?.invoke(e) }
                } else {
                    // 記録を更新しない場合も成功扱い
                    onSuccess?.invoke()
                }
            }.addOnFailureListener { e ->
                onFailure?.invoke(e)
            }
    }

    /**
     * 新しいスコアが1位かどうかを確認し、1位の場合はDiscordに投稿する (設定対応版)
     */
    private fun checkIfNewTopScoreWithConfig(
        gameId: String,
        leaderboardId: String,
        score: Long,
        userName: String,
        photoUrl: String?,
        config: RetroAchievementsLeaderboardConfig,
    ) {
        val db = FirebaseFirestore.getInstance()
        val context = YabauseApplication.appContext

        // Firebase Remote ConfigからDiscord Webhook URLを取得
        val remoteConfig = com.google.firebase.remoteconfig.FirebaseRemoteConfig
            .getInstance()
        val webhookUrl = remoteConfig.getString(config.webhookKey)

        // Webhook URLが空または無効な場合は処理を終了
        if (webhookUrl.isEmpty()) {
            Log.d("BaseGame", "Discord webhook URL is not set in Remote Config for key: ${config.webhookKey}")
            return
        }

        // 設定に基づいた順序でリーダーボードのスコアを取得
        val orderDirection =
            when (config.direction) {
                "ascending" -> Query.Direction.ASCENDING // 時間系 (小さい方が良い)
                "descending" -> Query.Direction.DESCENDING // スコア系 (大きい方が良い)
                else -> Query.Direction.ASCENDING
            }

        // リーダーボードのスコアを取得して、新しいスコアが1位かどうかを確認
        db
            .collection("games/$gameId/leaderboards")
            .document(leaderboardId)
            .collection("scores")
            .orderBy("score", orderDirection)
            .limit(1)
            .get()
            .addOnSuccessListener { querySnapshot ->
                if (querySnapshot.isEmpty) {
                    // スコアがない場合は、新しいスコアが1位
                    postNewTopScoreToDiscord(gameId, config.name ?: leaderboardId, score, userName, photoUrl, webhookUrl)
                    return@addOnSuccessListener
                }

                // 1位のスコアを取得
                val topScore =
                    querySnapshot.documents[0].getLong("score") ?: when (config.direction) {
                        "ascending" -> Long.MAX_VALUE
                        "descending" -> Long.MIN_VALUE
                        else -> Long.MAX_VALUE
                    }
                val topScoreUserId = querySnapshot.documents[0].id

                // 自分のスコアが1位かどうかを確認
                val currentUser = AuthState.realUser()
                val isNewTopScore =
                    when (config.direction) {
                        "ascending" -> score <= topScore // 時間系: 新しいスコアが小さいか等しい
                        "descending" -> score >= topScore // スコア系: 新しいスコアが大きいか等しい
                        else -> score <= topScore
                    }

                if (currentUser != null && (isNewTopScore || topScoreUserId == currentUser.uid)) {
                    // 新しいスコアが1位の場合、またはすでに自分が1位の場合
                    postNewTopScoreToDiscord(gameId, config.name ?: leaderboardId, score, userName, photoUrl, webhookUrl)
                } else {
                    Log.d("BaseGame", "New score is not the top score. Top: $topScore, New: $score, Direction: ${config.direction}")
                }
            }.addOnFailureListener { e ->
                Log.e("BaseGame", "Error checking if new score is top score", e)
            }
    }

    /**
     * Firebase Analyticsにスコアイベントを送信する
     */
    protected fun logScoreEvent(
        score: Long,
        leaderboardId: String,
    ) {
        val context = YabauseApplication.appContext
        val bundle = Bundle()
        bundle.putLong(FirebaseAnalytics.Param.SCORE, score)
        bundle.putString("leaderboard_id", leaderboardId)
        val firebaseAnalytics = FirebaseAnalytics.getInstance(context)
        firebaseAnalytics.logEvent(FirebaseAnalytics.Event.POST_SCORE, bundle)
    }
}
