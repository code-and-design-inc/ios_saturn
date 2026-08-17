package org.uoyabause.android

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.Observer
import com.google.firebase.firestore.DocumentSnapshot
import io.mockk.*
import io.mockk.impl.annotations.MockK
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.uoyabause.android.leaderboard.LeaderBoardRepository
import org.uoyabause.android.leaderboard.LeaderBoardViewModel

@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [29])
class LeaderBoardViewModelTest {
    // LiveDataをテストするためのルール
    @get:Rule
    val instantTaskExecutorRule = InstantTaskExecutorRule()

    // テスト用のコルーチンディスパッチャー
    private val testDispatcher = StandardTestDispatcher()

    // テスト対象のViewModel
    private lateinit var viewModel: LeaderBoardViewModel

    // モック化するリポジトリ
    @MockK
    private lateinit var repository: LeaderBoardRepository

    // LiveDataのオブザーバーをモック化
    @MockK
    private lateinit var scoresObserver: Observer<List<LeaderBoardRepository.ScoreEntry>>

    @MockK
    private lateinit var errorObserver: Observer<String?>

    @MockK
    private lateinit var loadingObserver: Observer<Boolean>

    @MockK
    private lateinit var scrollToPreviousPageBottomObserver: Observer<Int>

    @Before
    fun setup() {
        // モックの初期化
        MockKAnnotations.init(this)
        Dispatchers.setMain(testDispatcher)

        // リポジトリのモックを設定
        coEvery { repository.getLeaderboards(any()) } returns emptyList()

        // ViewModelの初期化
        viewModel = LeaderBoardViewModel()
        // リポジトリをモックに置き換え
        viewModel.repository = repository

        // LiveDataのオブザーバーを設定
        viewModel.scores.observeForever(scoresObserver)
        viewModel.error.observeForever(errorObserver)
        viewModel.isLoading.observeForever(loadingObserver)
        viewModel.scrollToPreviousPageBottom.observeForever(scrollToPreviousPageBottomObserver)

        // オブザーバーの基本的な振る舞いを設定
        every { scoresObserver.onChanged(any()) } just Runs
        every { errorObserver.onChanged(any()) } just Runs
        every { loadingObserver.onChanged(any()) } just Runs
        every { scrollToPreviousPageBottomObserver.onChanged(any()) } just Runs
    }

    @After
    fun tearDown() {
        // オブザーバーの登録解除
        viewModel.scores.removeObserver(scoresObserver)
        viewModel.error.removeObserver(errorObserver)
        viewModel.isLoading.removeObserver(loadingObserver)
        viewModel.scrollToPreviousPageBottom.removeObserver(scrollToPreviousPageBottomObserver)

        // コルーチンディスパッチャーをリセット
        Dispatchers.resetMain()
    }

    @Test
    fun `loadLeaderboards should update leaderboards LiveData`() =
        runTest {
            // テストデータの準備
            val gameCode = "TEST_GAME"
            val leaderboards =
                listOf(
                    LeaderBoardRepository.LeaderboardInfo("id1", "Leaderboard 1"),
                    LeaderBoardRepository.LeaderboardInfo("id2", "Leaderboard 2"),
                )

            // リポジトリのモックを設定
            coEvery { repository.getLeaderboards(gameCode) } returns leaderboards

            // テスト対象のメソッドを呼び出し
            viewModel.loadLeaderboards(gameCode)

            // コルーチンの実行を待機
            testDispatcher.scheduler.advanceUntilIdle()

            // 検証
            verify { loadingObserver.onChanged(true) }
            verify { loadingObserver.onChanged(false) }

            // leaderboardsが更新されたことを確認
            assert(viewModel.leaderboards.value == leaderboards)
        }

    @Test
    fun `loadPreviousPage should update scores and set scrollToPreviousPageBottom`() =
        runTest {
            // テストデータの準備
            val leaderboardId = "test_leaderboard"
            val firstDoc = mockk<DocumentSnapshot>()
            val scores =
                listOf(
                    LeaderBoardRepository.ScoreEntry("user1", "User 1", 100, 123456789, 1, 0, "https://example.com/avatar1.png"),
                    LeaderBoardRepository.ScoreEntry("user2", "User 2", 90, 123456789, 2, 0, "https://example.com/avatar2.png"),
                )
            val pageResult =
                LeaderBoardRepository.PageResult(
                    scores = scores,
                    firstVisible = mockk(),
                    lastVisible = mockk(),
                    hasMoreBefore = true,
                    hasMoreAfter = true,
                )

            // リポジトリのモックを設定
            coEvery { repository.loadScoresBefore(leaderboardId, firstDoc, any(), any()) } returns pageResult

            // 初期状態を設定
            viewModel.selectLeaderboard(leaderboardId)
            testDispatcher.scheduler.advanceUntilIdle()

            // firstVisibleを設定（通常はloadFirstPageなどで設定される）
            val field = LeaderBoardViewModel::class.java.getDeclaredField("firstVisible")
            field.isAccessible = true
            field.set(viewModel, firstDoc)

            // テスト対象のメソッドを呼び出し
            viewModel.loadPreviousPage()

            // コルーチンの実行を待機
            testDispatcher.scheduler.advanceUntilIdle()

            // 検証
            verify { loadingObserver.onChanged(true) }
            verify { loadingObserver.onChanged(false) }
            verify { scoresObserver.onChanged(any()) }

            // scrollToPreviousPageBottomが正しく設定されたことを確認
            verify { scrollToPreviousPageBottomObserver.onChanged(scores.size - 1) }

            // hasMoreBeforeとhasMoreAfterが正しく設定されたことを確認
            assert(viewModel.hasMoreBefore.value == true)
            assert(viewModel.hasMoreAfter.value == true)
        }

    @Test
    fun `resetScrollToPreviousPageBottom should set value to -1`() {
        // テスト対象のメソッドを呼び出し
        viewModel.resetScrollToPreviousPageBottom()

        // 検証
        verify { scrollToPreviousPageBottomObserver.onChanged(-1) }
        assert(viewModel.scrollToPreviousPageBottom.value == -1)
    }

    @Test
    fun `loadNextPage should update scores correctly`() =
        runTest {
            // テストデータの準備
            val leaderboardId = "test_leaderboard"
            val lastDoc = mockk<DocumentSnapshot>()
            val scores =
                listOf(
                    LeaderBoardRepository.ScoreEntry("user3", "User 3", 80, 123456789, 3, 0, "https://example.com/avatar3.png"),
                    LeaderBoardRepository.ScoreEntry("user4", "User 4", 70, 123456789, 4, 0, "https://example.com/avatar4.png"),
                )
            val pageResult =
                LeaderBoardRepository.PageResult(
                    scores = scores,
                    firstVisible = mockk(),
                    lastVisible = mockk(),
                    hasMoreBefore = true,
                    hasMoreAfter = true,
                )

            // リポジトリのモックを設定
            coEvery { repository.loadScoreAfter(leaderboardId, lastDoc, any(), any()) } returns pageResult

            // 初期状態を設定
            viewModel.selectLeaderboard(leaderboardId)
            testDispatcher.scheduler.advanceUntilIdle()

            // lastVisibleを設定（通常はloadFirstPageなどで設定される）
            val field = LeaderBoardViewModel::class.java.getDeclaredField("lastVisible")
            field.isAccessible = true
            field.set(viewModel, lastDoc)

            // lastRankを設定
            val rankField = LeaderBoardViewModel::class.java.getDeclaredField("lastRank")
            rankField.isAccessible = true
            rankField.set(viewModel, 2)

            // テスト対象のメソッドを呼び出し
            viewModel.loadNextPage()

            // コルーチンの実行を待機
            testDispatcher.scheduler.advanceUntilIdle()

            // 検証
            verify { loadingObserver.onChanged(true) }
            verify { loadingObserver.onChanged(false) }
            verify { scoresObserver.onChanged(any()) }

            // hasMoreBeforeとhasMoreAfterが正しく設定されたことを確認
            assert(viewModel.hasMoreBefore.value == true)
            assert(viewModel.hasMoreAfter.value == true)
        }

    @Test
    fun `loadFirstPage should update scores correctly`() =
        runTest {
            // テストデータの準備
            val leaderboardId = "test_leaderboard"
            val scores =
                listOf(
                    LeaderBoardRepository.ScoreEntry("user1", "User 1", 100, 123456789, 1, 0, "https://example.com/avatar1.png"),
                    LeaderBoardRepository.ScoreEntry("user2", "User 2", 90, 123456789, 2, 0, "https://example.com/avatar2.png"),
                )
            val pageResult =
                LeaderBoardRepository.PageResult(
                    scores = scores,
                    firstVisible = mockk(),
                    lastVisible = mockk(),
                    hasMoreBefore = false,
                    hasMoreAfter = true,
                )

            // リポジトリのモックを設定
            coEvery { repository.loadPage(leaderboardId, 1, null, any()) } returns pageResult
            coEvery { repository.getTotalScoreCount(leaderboardId) } returns 100
            coEvery { repository.getTopScore(leaderboardId) } returns 100L

            // 初期状態を設定
            viewModel.selectLeaderboard(leaderboardId)
            testDispatcher.scheduler.advanceUntilIdle()

            // テスト対象のメソッドを呼び出し
            viewModel.loadFirstPage()

            // コルーチンの実行を待機
            testDispatcher.scheduler.advanceUntilIdle()

            // 検証
            verify { loadingObserver.onChanged(true) }
            verify { loadingObserver.onChanged(false) }
            verify { scoresObserver.onChanged(scores) }

            // hasMoreBeforeとhasMoreAfterが正しく設定されたことを確認
            assert(viewModel.hasMoreBefore.value == false)
            assert(viewModel.hasMoreAfter.value == true)
            assert(viewModel.totalScoreCount.value == 100)
        }

    @Test
    fun `clearError should set error to null`() {
        // エラーを設定
        val errorField = LeaderBoardViewModel::class.java.getDeclaredField("_error")
        errorField.isAccessible = true
        errorField.set(viewModel, androidx.lifecycle.MutableLiveData<String?>("テストエラー"))

        // テスト対象のメソッドを呼び出し
        viewModel.clearError()

        // 検証
        verify { errorObserver.onChanged(null) }
        assert(viewModel.error.value == null)
    }

    @Test
    fun `resetScrollFlag should set scrollToMyRank to false`() {
        // scrollToMyRankを設定
        val scrollField = LeaderBoardViewModel::class.java.getDeclaredField("_scrollToMyRank")
        scrollField.isAccessible = true
        scrollField.set(viewModel, androidx.lifecycle.MutableLiveData<Boolean>(true))

        // テスト対象のメソッドを呼び出し
        viewModel.resetScrollFlag()

        // 検証
        assert(viewModel.scrollToMyRank.value == false)
    }

    @Test
    fun `loadPreviousPage should handle error correctly`() =
        runTest {
            // テストデータの準備
            val leaderboardId = "test_leaderboard"
            val firstDoc = mockk<DocumentSnapshot>()
            val exception = Exception("テストエラー")

            // リポジトリのモックを設定（例外をスロー）
            coEvery { repository.loadScoresBefore(leaderboardId, firstDoc, any(), any()) } throws exception

            // 初期状態を設定
            viewModel.selectLeaderboard(leaderboardId)
            testDispatcher.scheduler.advanceUntilIdle()

            // firstVisibleを設定
            val field = LeaderBoardViewModel::class.java.getDeclaredField("firstVisible")
            field.isAccessible = true
            field.set(viewModel, firstDoc)

            // テスト対象のメソッドを呼び出し
            viewModel.loadPreviousPage()

            // コルーチンの実行を待機
            testDispatcher.scheduler.advanceUntilIdle()

            // 検証
            verify { loadingObserver.onChanged(true) }
            verify { loadingObserver.onChanged(false) }
            verify { errorObserver.onChanged(any()) }

            // エラーメッセージが設定されたことを確認
            assert(viewModel.error.value?.contains("前のページの読み込みに失敗") == true)
        }
}
