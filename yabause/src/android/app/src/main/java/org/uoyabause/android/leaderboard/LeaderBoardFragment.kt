package org.uoyabause.android.leaderboard

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import org.devmiyax.yabasanshiro.R

class LeaderBoardFragment : Fragment() {
    private lateinit var viewModel: LeaderBoardViewModel
    private lateinit var recyclerView: RecyclerView
    private lateinit var adapter: LeaderBoardAdapter
    private lateinit var leaderboardSpinner: Spinner
    private lateinit var progressBar: FrameLayout
    private lateinit var emptyView: TextView
    private lateinit var textViewRank: TextView
    private lateinit var timeScoreHeaderTextView: TextView

    // クローズリスナーインターフェース
    interface OnLeaderboardCloseListener {
        fun onLeaderboardClose()
    }

    // クローズリスナープロパティ
    var closeListener: OnLeaderboardCloseListener? = null

    companion object {
        const val TAG = "LeaderBoardFragment"

        // ファクトリーメソッド
        fun newInstance(gameCode: String): LeaderBoardFragment {
            val fragment = LeaderBoardFragment()
            val args = Bundle()
            args.putString("gameCode", gameCode)
            fragment.arguments = args
            return fragment
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val view = inflater.inflate(R.layout.fragment_leader_board, container, false)

        // ビューの初期化
        recyclerView = view.findViewById(R.id.recyclerView)
        leaderboardSpinner = view.findViewById(R.id.leaderboardSpinner)
        progressBar = view.findViewById(R.id.loadingLayer) // ローディングレイヤー全体を参照
        emptyView = view.findViewById(R.id.emptyView)
        textViewRank = view.findViewById<TextView>(R.id.textViewRank)
        timeScoreHeaderTextView = view.findViewById<TextView>(R.id.timeScoreHeaderTextView)

        // ViewModelの初期化
        viewModel = ViewModelProvider(this).get(LeaderBoardViewModel::class.java)

        // アダプターの設定
        adapter = LeaderBoardAdapter()
        recyclerView.adapter = adapter

        // レイアウトマネージャーの設定
        val layoutManager = LinearLayoutManager(context)
        recyclerView.layoutManager = layoutManager

        // スクロールリスナーの設定（無限スクロール用）
        recyclerView.addOnScrollListener(
            object : RecyclerView.OnScrollListener() {
                override fun onScrolled(
                    recyclerView: RecyclerView,
                    dx: Int,
                    dy: Int,
                ) {
                    super.onScrolled(recyclerView, dx, dy)

                    val visibleItemCount = layoutManager.childCount
                    val totalItemCount = layoutManager.itemCount
                    val firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition()

                    // 下にスクロールして、最後に近づいたら次のページをロード
                    if (dy > 0 && (visibleItemCount + firstVisibleItemPosition) >= totalItemCount - 5) {
                        if (viewModel.hasMoreAfter.value == true && !viewModel.isLoading.value!!) {
                            viewModel.loadNextPage()
                        }
                    }

                    // 上にスクロールして、最初に近づいたら前のページをロード
                    if (dy < 0 && firstVisibleItemPosition < 5) {
                        if (viewModel.hasMoreBefore.value == true && !viewModel.isLoading.value!!) {
                            viewModel.loadPreviousPage()
                        }
                    }
                }
            },
        )

        // オブザーバーの設定
        setupObservers()

        // 初期データのロード
        val gameCode = arguments?.getString("gameCode") ?: "SATURNAPP"
        viewModel.loadLeaderboards(gameCode)

        return view
    }

    private fun setupObservers() {
        // リーダーボード一覧の監視
        viewModel.leaderboards.observe(viewLifecycleOwner) { leaderboards ->
            if (leaderboards.isNotEmpty()) {
                // スピナーにリーダーボード一覧をセット
                val adapter =
                    ArrayAdapter(
                        requireContext(),
                        android.R.layout.simple_spinner_item,
                        leaderboards.map { it.name ?: "Unknown" },
                    )
                adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
                leaderboardSpinner.adapter = adapter

                // スピナーの選択リスナー
                leaderboardSpinner.onItemSelectedListener =
                    object : AdapterView.OnItemSelectedListener {
                        override fun onItemSelected(
                            parent: AdapterView<*>?,
                            view: View?,
                            position: Int,
                            id: Long,
                        ) {
                            val selectedLeaderboardId = leaderboards[position].id
                            viewModel.selectLeaderboard(selectedLeaderboardId)

                            // ヘッダーテキストを更新
                            updateColumnHeader()
                        }

                        override fun onNothingSelected(parent: AdapterView<*>?) {}
                    }
            }
        }

        // スコアリストの監視
        viewModel.scores.observe(viewLifecycleOwner) { scores ->
            adapter.submitList(scores)

            if (scores.isEmpty()) {
                emptyView.visibility = View.VISIBLE
                recyclerView.visibility = View.GONE
            } else {
                emptyView.visibility = View.GONE
                recyclerView.visibility = View.VISIBLE
            }

            // スコアが読み込まれたタイミングでヘッダーを更新
            updateColumnHeader()
        }

        // ユーザー位置情報の監視
        viewModel.userPosition.observe(viewLifecycleOwner) { position ->
            // アダプターにユーザーIDを渡して、自分のスコアをハイライト表示
            adapter.setCurrentUserId(position?.userId)
        }

        // 前のページの最後にスクロールするフラグの監視
        viewModel.scrollToPreviousPageBottom.observe(viewLifecycleOwner) { position ->
            if (position >= 0) {
                val layoutManager = recyclerView.layoutManager as LinearLayoutManager
                layoutManager.scrollToPositionWithOffset(position, 0)

                // フラグをリセット
                viewModel.resetScrollToPreviousPageBottom()
            }
        }

        // 自分の順位へのスクロールフラグの監視
        viewModel.scrollToMyRank.observe(viewLifecycleOwner) { shouldScroll ->
            if (shouldScroll) {
                val position = viewModel.userPosition.value
                if (position != null) {
                    // 自分の順位が表示されるようにスクロール
                    val scores = viewModel.scores.value ?: return@observe
                    val myRank = position.rank

                    // スコアリスト内で自分の順位を探す
                    val indexInList = scores.indexOfFirst { it.rank == myRank }
                    if (indexInList >= 0) {
                        // リストの中央に表示されるようにスクロール
                        val layoutManager = recyclerView.layoutManager as LinearLayoutManager

                        // まず位置にスクロール
                        layoutManager.scrollToPosition(indexInList)

                        // レイアウトが完了した後に中央に調整
                        recyclerView.post {
                            val itemView = layoutManager.findViewByPosition(indexInList)
                            if (itemView != null) {
                                // アイテムが中央になるようにオフセットを計算
                                val offset = recyclerView.height / 2 - itemView.height / 2
                                layoutManager.scrollToPositionWithOffset(indexInList, offset)
                            }
                        }
                    }

                    // スクロールフラグをリセット
                    viewModel.resetScrollFlag()
                }
            }
        }

        // ローディング状態の監視
        viewModel.isLoading.observe(viewLifecycleOwner) { isLoading ->
            progressBar.visibility = if (isLoading) View.VISIBLE else View.GONE
        }

        // エラーの監視
        viewModel.error.observe(viewLifecycleOwner) { errorMessage ->
            if (errorMessage != null) {
                Toast.makeText(context, errorMessage, Toast.LENGTH_LONG).show()
                viewModel.clearError()
            }
        }

        // フラグメントを閉じるイベントの監視
        viewModel.closeFragment.observe(viewLifecycleOwner) { shouldClose ->
            if (shouldClose) {
                // 少し遅延させてメッセージが表示された後に閉じる
                view?.postDelayed({
                    close()
                    viewModel.resetCloseFragment()
                }, 2000) // 2秒後に閉じる
            }
        }

        // 自分の順位を監視
        viewModel.myRank.observe(viewLifecycleOwner) { myRank ->
            updateRankText(textViewRank, myRank, viewModel.totalScoreCount.value ?: 0)
        }

        // 総数を監視
        viewModel.totalScoreCount.observe(viewLifecycleOwner) { totalCount ->
            updateRankText(textViewRank, viewModel.myRank.value ?: 0, totalCount)
        }
    }

    // リーダーボードを閉じる
    fun close() {
        closeListener?.onLeaderboardClose()
    }

    // カラムヘッダーを更新するメソッド
    private fun updateColumnHeader() {
        val columnName = viewModel.getColumnName()
        timeScoreHeaderTextView.text = columnName
    }

    // 順位テキストを更新するメソッド
    private fun updateRankText(
        textView: TextView?,
        myRank: Int,
        totalCount: Int,
    ) {
        textView?.let {
            if (myRank > 0 && totalCount > 0) {
                // 自分の順位と総数が有効な場合は「自分の順位 / 総数」の形式で表示
                it.text = "$myRank / $totalCount"
                it.visibility = View.VISIBLE
            } else if (totalCount > 0) {
                // 自分の順位がないが総数がある場合は「- / 総数」と表示
                it.text = "- / $totalCount"
                it.visibility = View.VISIBLE
            } else {
                // 両方が無効な場合は非表示
                it.visibility = View.GONE
            }
        }
    }

    // リーダーボードアダプター
    inner class LeaderBoardAdapter : RecyclerView.Adapter<LeaderBoardAdapter.ViewHolder>() {
        private var scores: List<LeaderBoardRepository.ScoreEntry> = emptyList()
        private var currentUserId: String? = null

        fun submitList(newScores: List<LeaderBoardRepository.ScoreEntry>) {
            this.scores = newScores
            notifyDataSetChanged()
        }

        fun setCurrentUserId(userId: String?) {
            this.currentUserId = userId
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(
            parent: ViewGroup,
            viewType: Int,
        ): ViewHolder {
            val view =
                LayoutInflater
                    .from(parent.context)
                    .inflate(R.layout.item_leaderboard_score, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(
            holder: ViewHolder,
            position: Int,
        ) {
            val score = scores[position]
            holder.bind(score, score.userId == currentUserId)
        }

        override fun getItemCount(): Int = scores.size

        inner class ViewHolder(
            itemView: View,
        ) : RecyclerView.ViewHolder(itemView) {
            private val rankTextView: TextView = itemView.findViewById(R.id.rankTextView)
            private val nameTextView: TextView = itemView.findViewById(R.id.nameTextView)
            private val scoreTextView: TextView = itemView.findViewById(R.id.scoreTextView)
            private val diffView: TextView = itemView.findViewById(R.id.textViewDiff)
            private val userAvatarImageView: ImageView = itemView.findViewById(R.id.userAvatarImageView)

            fun bind(
                score: LeaderBoardRepository.ScoreEntry,
                isCurrentUser: Boolean,
            ) {
                rankTextView.text = score.rank.toString()
                nameTextView.text = score.name
                scoreTextView.text = viewModel.formatScore(score.score)
                diffView.text = viewModel.formatDiffScore(score.diff)

                // ユーザーアバターを表示
                if (score.photoUrl != null) {
                    Glide
                        .with(itemView.context)
                        .load(score.photoUrl)
                        .circleCrop()
                        .placeholder(R.drawable.account_circle_24px)
                        .into(userAvatarImageView)
                } else {
                    // デフォルトアバターを表示
                    userAvatarImageView.setImageResource(R.drawable.account_circle_24px)
                }

                // 自分のスコアをハイライト表示
                if (isCurrentUser) {
                    itemView.setBackgroundResource(R.drawable.bg_my_score)
                    // itemView.setBackgroundResource(R.drawable.bg_leaderboard_row_highlight)
                } else {
                    itemView.setBackgroundResource(R.drawable.bg_leaderboard_row)
                }
            }
        }
    }
}
