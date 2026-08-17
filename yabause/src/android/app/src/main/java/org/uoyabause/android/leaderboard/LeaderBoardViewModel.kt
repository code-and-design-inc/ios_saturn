package org.uoyabause.android.leaderboard

import android.util.Log
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.DocumentSnapshot
import kotlinx.coroutines.launch
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.YabauseApplication

class LeaderBoardViewModel : ViewModel() {
    var repository = LeaderBoardRepository()
    private val pageSize = 100

    companion object {
        const val TAG = "LeaderBoardViewModel"
    }

    // リーダーボード一覧
    private val _leaderboards = MutableLiveData<List<LeaderBoardRepository.LeaderboardInfo>>()
    val leaderboards: LiveData<List<LeaderBoardRepository.LeaderboardInfo>> = _leaderboards

    // 現在のリーダーボードID
    private val _currentLeaderboardId = MutableLiveData<String>()
    val currentLeaderboardId: LiveData<String> = _currentLeaderboardId

    // 現在のリーダーボード情報（scoreType, rankingTypeを含む）
    private var currentLeaderboardInfo: LeaderBoardRepository.LeaderboardInfo? = null

    // UIで使用するためのscoreType公開プロパティ
    val currentScoreType: LeaderBoardRepository.ScoreType
        get() = currentLeaderboardInfo?.scoreType ?: LeaderBoardRepository.ScoreType.VALUE

    // UIで使用するためのrankingType公開プロパティ
    val currentRankingType: LeaderBoardRepository.RankingType
        get() = currentLeaderboardInfo?.rankingType ?: LeaderBoardRepository.RankingType.DESCENDING

    // 現在のユーザー位置情報
    private val _userPosition = MutableLiveData<LeaderBoardRepository.UserPosition?>()
    val userPosition: LiveData<LeaderBoardRepository.UserPosition?> = _userPosition

    // 総スコア数
    private val _totalScoreCount = MutableLiveData<Int>(0)
    val totalScoreCount: LiveData<Int> = _totalScoreCount

    // 自分の順位
    private val _myRank = MutableLiveData<Int>(0)
    val myRank: LiveData<Int> = _myRank

    // 現在表示中のスコアリスト
    private val _scores = MutableLiveData<List<LeaderBoardRepository.ScoreEntry>>()
    val scores: LiveData<List<LeaderBoardRepository.ScoreEntry>> = _scores

    // 前後のページがあるかどうか
    private val _hasMoreBefore = MutableLiveData<Boolean>(false)
    val hasMoreBefore: LiveData<Boolean> = _hasMoreBefore

    private val _hasMoreAfter = MutableLiveData<Boolean>(false)
    val hasMoreAfter: LiveData<Boolean> = _hasMoreAfter

    // 最初と最後のドキュメント（ページネーション用）
    private var firstVisible: DocumentSnapshot? = null
    private var lastVisible: DocumentSnapshot? = null

    // 最初と最後の順位情報
    private var firstRank: Int = 0
    private var lastRank: Int = 0
    private var topScore: Long = 0

    // ローディング状態
    private val _isLoading = MutableLiveData<Boolean>(false)
    val isLoading: LiveData<Boolean> = _isLoading

    // エラー状態
    private val _error = MutableLiveData<String?>()
    val error: LiveData<String?> = _error

    // 自分の順位にスクロールする必要があるかどうか
    private val _scrollToMyRank = MutableLiveData<Boolean>(false)
    val scrollToMyRank: LiveData<Boolean> = _scrollToMyRank

    // 前のページの最後にスクロールする必要があるかどうか
    private val _scrollToPreviousPageBottom = MutableLiveData<Int>(-1)
    val scrollToPreviousPageBottom: LiveData<Int> = _scrollToPreviousPageBottom

    // 前のページのスクロールフラグをリセット
    fun resetScrollToPreviousPageBottom() {
        _scrollToPreviousPageBottom.value = -1
    }

    // フラグメントを閉じるためのイベント
    private val _closeFragment = MutableLiveData<Boolean>(false)
    val closeFragment: LiveData<Boolean> = _closeFragment

    // フラグメントを閉じるイベントをリセット
    fun resetCloseFragment() {
        _closeFragment.value = false
    }

    // リーダーボード一覧を読み込む
    fun loadLeaderboards(gameCode: String) {
        viewModelScope.launch {
            _isLoading.value = true
            try {
                val result = repository.getLeaderboards(gameCode)
                _leaderboards.value = result

                if (result.isNotEmpty()) {
                    // 最初のリーダーボードを選択
                    selectLeaderboard(result[0].id)
                } else {
                    // リーダーボードが存在しない場合
                    _error.value = YabauseApplication.appContext.getString(R.string.leaderboard_not_supported)
                    _closeFragment.value = true
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading leaderboards", e)
                _error.value = YabauseApplication.appContext.getString(R.string.leaderboard_not_supported)
                _closeFragment.value = true
                _isLoading.value = false
            } finally {
            }
        }
    }

    // リーダーボードを選択
    fun selectLeaderboard(leaderboardId: String) {
        if (_currentLeaderboardId.value == leaderboardId) return

        _currentLeaderboardId.value = leaderboardId

        // 現在のリーダーボード情報を更新
        currentLeaderboardInfo = _leaderboards.value?.find { it.id == leaderboardId }

        // このリーダーボードのトップスコアを取得して保存
        viewModelScope.launch {
            try {
                topScore = repository.getTopScore(leaderboardId, currentLeaderboardInfo)
                Log.d(TAG, "Set top score for leaderboard $leaderboardId: $topScore")

                // ユーザーがログインしているかどうかを確認
                val userId = FirebaseAuth.getInstance().currentUser?.uid
                if (userId != null) {
                    // ログインしている場合は自分の順位を取得して表示
                    loadUserPositionAndPage()
                } else {
                    // ログインしていない場合は最初のページを表示
                    // topScoreは既に設定されているので、diffは正しく計算される
                    loadFirstPage()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error getting top score", e)
                topScore = 0L
                // エラーが発生した場合でも最初のページを表示
                loadFirstPage()
            }
        }
    }

    // 現在のユーザー位置を取得し、そのページを読み込む
    fun loadUserPositionAndPage() {
        val leaderboardId = _currentLeaderboardId.value ?: return
        val userId = FirebaseAuth.getInstance().currentUser?.uid

        viewModelScope.launch {
            _isLoading.value = true
            try {
                if (userId != null) {
                    // ユーザーIDを指定してページを取得（ユーザーのスコアが含まれるページが自動的に取得される）
                    val pageResult = repository.loadPage(leaderboardId, 0, userId, topScore, currentLeaderboardInfo)

                    // 結果を保存
                    _scores.value = pageResult.scores
                    firstVisible = pageResult.firstVisible
                    lastVisible = pageResult.lastVisible
                    _hasMoreBefore.value = pageResult.hasMoreBefore
                    _hasMoreAfter.value = pageResult.hasMoreAfter

                    // 順位情報を更新
                    if (pageResult.scores.isNotEmpty()) {
                        firstRank = pageResult.scores.first().rank
                        lastRank = pageResult.scores.last().rank
                    }

                    // ユーザーの順位を取得して設定
                    val userScore = pageResult.scores.find { it.userId == userId }

                    // 総数を取得
                    val totalCount = repository.getTotalScoreCount(leaderboardId)
                    _totalScoreCount.value = totalCount

                    if (userScore != null) {
                        // 自分の順位を設定
                        _myRank.value = userScore.rank

                        _userPosition.value =
                            LeaderBoardRepository.UserPosition(
                                userId = userId,
                                score = userScore.score,
                                rank = userScore.rank,
                                totalCount = totalCount,
                                pageNumber = 0,
                            )

                        // ユーザーの順位が中央になるようにスクロール
                        _scrollToMyRank.value = true
                    } else {
                        // ユーザーのスコアがページに含まれていない場合は、ユーザー位置をクリア
                        _userPosition.value = null
                        _myRank.value = 0
                    }
                } else {
                    // ユーザーIDがない場合は最初のページを表示
                    loadFirstPage()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error loading user position and page", e)
                _error.value = "ユーザー位置の読み込みに失敗しました: ${e.message}"
                // loadFirstPage() // エラー時は最初のページを表示
            } finally {
                _isLoading.value = false
            }
        }
    }

    // 最初のページを読み込む
    fun loadFirstPage() {
        val leaderboardId = _currentLeaderboardId.value ?: return

        viewModelScope.launch {
            _isLoading.value = true
            try {
                val pageResult = repository.loadPage(leaderboardId, 0, null, topScore, currentLeaderboardInfo)

                _scores.value = pageResult.scores
                firstVisible = pageResult.firstVisible
                lastVisible = pageResult.lastVisible
                _hasMoreBefore.value = pageResult.hasMoreBefore
                _hasMoreAfter.value = pageResult.hasMoreAfter

                // 順位情報を更新
                if (pageResult.scores.isNotEmpty()) {
                    firstRank = pageResult.scores.first().rank
                    lastRank = pageResult.scores.last().rank
                }

                // スクロールフラグをリセット
                _scrollToMyRank.value = false
            } catch (e: Exception) {
                Log.e(TAG, "Error loading first page", e)
                _error.value = "ページの読み込みに失敗しました: ${e.message}"
            } finally {
                _isLoading.value = false
            }
        }
    }

    // 次のページを読み込む
    fun loadNextPage() {
        val leaderboardId = _currentLeaderboardId.value ?: return
        val lastDoc = lastVisible ?: return

        viewModelScope.launch {
            _isLoading.value = true
            try {
                val pageResult = repository.loadScoreAfter(leaderboardId, lastDoc, lastRank, topScore, currentLeaderboardInfo)

                // 現在のスコアリストに新しいスコアを追加
                val currentScores = _scores.value ?: emptyList()

                // スコアを追加（Repository側で正しい順位が設定済み）
                if (pageResult.scores.isNotEmpty()) {
                    _scores.value = currentScores + pageResult.scores

                    // 最後の順位を更新（ページの最後のスコアの順位）
                    lastRank = pageResult.scores.last().rank
                } else {
                    _scores.value = currentScores + pageResult.scores
                }

                // 最後のドキュメントを更新
                if (pageResult.lastVisible != null) {
                    lastVisible = pageResult.lastVisible
                }

                // 前後のページがあるかどうかを更新
                _hasMoreBefore.value = true // 次のページを読み込んだので前のページは必ずある
                _hasMoreAfter.value = pageResult.hasMoreAfter
            } catch (e: Exception) {
                Log.e(TAG, "Error loading next page", e)
                _error.value = "次のページの読み込みに失敗しました: ${e.message}"
            } finally {
                _isLoading.value = false
            }
        }
    }

    // 前のページを読み込む
    fun loadPreviousPage() {
        val leaderboardId = _currentLeaderboardId.value ?: return
        val firstDoc = firstVisible ?: return

        viewModelScope.launch {
            _isLoading.value = true
            try {
                val pageResult = repository.loadScoresBefore(leaderboardId, firstDoc, firstRank, topScore, currentLeaderboardInfo)

                // 現在のスコアリストに新しいスコアを追加（前に追加）
                val currentScores = _scores.value ?: emptyList()

                // スコアを追加（Repository側で正しい順位が設定済み）
                if (pageResult.scores.isNotEmpty()) {
                    // 最初の順位を更新（ページの最初のスコアの順位）
                    firstRank = pageResult.scores.first().rank
                    _scores.value = pageResult.scores + currentScores

                    // 前のページの最後にスクロールするようにフラグを設定
                    _scrollToPreviousPageBottom.value = pageResult.scores.size - 1
                } else {
                    _scores.value = pageResult.scores + currentScores
                }

                // 最初のドキュメントを更新
                if (pageResult.firstVisible != null) {
                    firstVisible = pageResult.firstVisible
                }

                // 前後のページがあるかどうかを更新
                _hasMoreBefore.value = pageResult.hasMoreBefore
                _hasMoreAfter.value = true // 前のページを読み込んだので次のページは必ずある
            } catch (e: Exception) {
                Log.e(TAG, "Error loading previous page", e)
                _error.value = "前のページの読み込みに失敗しました: ${e.message}"
            } finally {
                _isLoading.value = false
            }
        }
    }

    // スクロールフラグをリセット
    fun resetScrollFlag() {
        _scrollToMyRank.value = false
    }

    // エラーメッセージをクリア
    fun clearError() {
        _error.value = null
    }

    // スコアフォーマット用のヘルパーメソッド
    fun formatScore(score: Long): String = repository.formatScore(score, currentScoreType)

    // 差分スコアフォーマット用のヘルパーメソッド
    fun formatDiffScore(diffScore: Long): String = repository.formatDiffScore(diffScore, currentScoreType)

    // カラム名取得用のヘルパーメソッド
    fun getColumnName(): String = repository.getColumnName(currentScoreType)

    // 時間表示かどうかの判定メソッド
    fun shouldDisplayAsTime(): Boolean = repository.shouldDisplayAsTime(currentScoreType)
}
