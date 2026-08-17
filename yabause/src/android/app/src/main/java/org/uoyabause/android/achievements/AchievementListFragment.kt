package org.uoyabause.android.achievements

import android.os.Bundle
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.ProgressBar
import android.widget.Switch
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import org.devmiyax.yabasanshiro.R

/**
 * Fragment for displaying RetroAchievements achievement list
 */
class AchievementListFragment : Fragment() {
    companion object {
        const val TAG = "AchievementListFragment"
        const val TYPE_HEADER = 0
        const val TYPE_ACHIEVEMENT = 1

        fun newInstance(): AchievementListFragment = AchievementListFragment()
    }

    private var rootView: View? = null
    private var recyclerView: RecyclerView? = null
    private var emptyView: TextView? = null
    private var hardcoreModeSwitch: Switch? = null
    private var achievementAdapter: AchievementAdapter? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        rootView = inflater.inflate(R.layout.fragment_achievement_list, container, false)

        initializeViews()
        setupHardcoreModeSwitch()
        loadAchievements()

        return rootView
    }

    private fun initializeViews() {
        recyclerView = rootView?.findViewById(R.id.achievement_recycler_view)
        emptyView = rootView?.findViewById(R.id.empty_view)
        hardcoreModeSwitch = rootView?.findViewById(R.id.hardcore_mode_switch)

        recyclerView?.layoutManager = LinearLayoutManager(context)
        achievementAdapter = AchievementAdapter()
        recyclerView?.adapter = achievementAdapter

        // Enable gamepad navigation for RecyclerView
        recyclerView?.isFocusable = true
        recyclerView?.isFocusableInTouchMode = true
        recyclerView?.requestFocus()

        // Set up key event handling for gamepad navigation
        recyclerView?.setOnKeyListener { _, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN) {
                when (keyCode) {
                    KeyEvent.KEYCODE_DPAD_UP -> {
                        val layoutManager = recyclerView?.layoutManager as? LinearLayoutManager
                        val currentPosition = layoutManager?.findFirstVisibleItemPosition() ?: 0
                        if (currentPosition > 0) {
                            recyclerView?.scrollToPosition(currentPosition - 1)
                        }
                        true
                    }
                    KeyEvent.KEYCODE_DPAD_DOWN -> {
                        val layoutManager = recyclerView?.layoutManager as? LinearLayoutManager
                        val currentPosition = layoutManager?.findLastVisibleItemPosition() ?: 0
                        val totalItems = achievementAdapter?.itemCount ?: 0
                        if (currentPosition < totalItems - 1) {
                            recyclerView?.scrollToPosition(currentPosition + 1)
                        }
                        true
                    }
                    else -> false
                }
            } else {
                false
            }
        }
    }

    private fun setupHardcoreModeSwitch() {
        val retroAchievementsManager = context?.let { RetroAchievementsManager.getInstance(it) }

        if (retroAchievementsManager != null) {
            // Set initial state
            hardcoreModeSwitch?.isChecked = retroAchievementsManager.isHardcoreEnabled()

            // Set up listener
            hardcoreModeSwitch?.setOnCheckedChangeListener { _, isChecked ->
                retroAchievementsManager.setHardcoreEnabled(isChecked)
                // Optionally reload achievements to reflect hardcore mode changes
                loadAchievements()
            }
        } else {
            // If auth manager is not available, disable the switch and set to default (hardcore enabled)
            hardcoreModeSwitch?.isEnabled = false
            hardcoreModeSwitch?.isChecked = true
        }
    }

    private fun loadAchievements() {
        // Check if user is logged in to RetroAchievements
        val retroAchievementsManager = context?.let { RetroAchievementsManager.getInstance(it) }

        if (retroAchievementsManager == null || !retroAchievementsManager.isUserLoggedIn()) {
            showEmptyState(getString(R.string.achievements_not_logged_in))
            return
        }

        // Get achievement list from native code
        val achievementList = RetroAchievementsManager.getAchievementListNative().toList()

        if (achievementList.isEmpty()) {
            showEmptyState(getString(R.string.achievements_no_game_loaded))
        } else {
            showAchievements(achievementList)
        }
    }

    private fun showEmptyState(message: String) {
        recyclerView?.visibility = View.GONE
        emptyView?.visibility = View.VISIBLE
        emptyView?.text = message
    }

    private fun showAchievements(achievements: List<AchievementItem>) {
        recyclerView?.visibility = View.VISIBLE
        emptyView?.visibility = View.GONE
        achievementAdapter?.updateAchievements(achievements)
    }

    /**
     * Data class for achievement items
     */
    data class AchievementItem(
        val id: Int,
        val title: String,
        val description: String,
        val points: Int,
        val badgeUrl: String?,
        val state: AchievementState,
        val category: String,
        val progress: String?,
        val measuredPercent: Int = 0,
    )

    enum class AchievementState {
        LOCKED,
        UNLOCKED,
        UNSUPPORTED,
    }

    /**
     * Adapter for achievement list
     */
    private inner class AchievementAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
        private val achievements = mutableListOf<Any>() // Mix of headers and achievements

        fun updateAchievements(newAchievements: List<AchievementItem>) {
            achievements.clear()

            // Group achievements by category
            val groupedAchievements = newAchievements.groupBy { it.category }

            for ((category, categoryAchievements) in groupedAchievements) {
                achievements.add(category) // Add header
                achievements.addAll(categoryAchievements) // Add achievements
            }

            notifyDataSetChanged()
        }

        override fun getItemViewType(position: Int): Int = if (achievements[position] is String) TYPE_HEADER else TYPE_ACHIEVEMENT

        override fun onCreateViewHolder(
            parent: ViewGroup,
            viewType: Int,
        ): RecyclerView.ViewHolder = when (viewType) {
            TYPE_HEADER -> {
                val view =
                    LayoutInflater
                        .from(parent.context)
                        .inflate(R.layout.item_achievement_header, parent, false)
                HeaderViewHolder(view)
            }
            else -> {
                val view =
                    LayoutInflater
                        .from(parent.context)
                        .inflate(R.layout.item_achievement, parent, false)
                AchievementViewHolder(view)
            }
        }

        override fun onBindViewHolder(
            holder: RecyclerView.ViewHolder,
            position: Int,
        ) {
            when (holder) {
                is HeaderViewHolder -> {
                    holder.bind(achievements[position] as String)
                }
                is AchievementViewHolder -> {
                    holder.bind(achievements[position] as AchievementItem)
                }
            }
        }

        override fun getItemCount(): Int = achievements.size
    }

    /**
     * ViewHolder for category headers
     */
    private inner class HeaderViewHolder(
        itemView: View,
    ) : RecyclerView.ViewHolder(itemView) {
        private val headerText: TextView = itemView.findViewById(R.id.header_text)

        fun bind(category: String) {
            headerText.text = category
        }
    }

    /**
     * ViewHolder for achievement items
     */
    private inner class AchievementViewHolder(
        itemView: View,
    ) : RecyclerView.ViewHolder(itemView) {
        private val iconImageView: ImageView = itemView.findViewById(R.id.achievement_icon)
        private val titleText: TextView = itemView.findViewById(R.id.achievement_title)
        private val descriptionText: TextView = itemView.findViewById(R.id.achievement_description)
        private val pointsText: TextView = itemView.findViewById(R.id.achievement_points)
        private val statusText: TextView = itemView.findViewById(R.id.achievement_status)
        private val progressBar: ProgressBar? = itemView.findViewById(R.id.achievement_progress)

        fun bind(achievement: AchievementItem) {
            titleText.text = achievement.title
            descriptionText.text = achievement.description
            pointsText.text = "${achievement.points} pts"

            // Set status and color based on state
            when (achievement.state) {
                AchievementState.UNLOCKED -> {
                    statusText.text = getString(R.string.achievements_unlocked)
                    statusText.setTextColor(resources.getColor(android.R.color.holo_green_dark, null))
                    itemView.alpha = 1.0f
                }
                AchievementState.LOCKED -> {
                    if (achievement.measuredPercent > 0) {
                        statusText.text = getString(R.string.achievements_progress, achievement.measuredPercent)
                        progressBar?.progress = achievement.measuredPercent
                        progressBar?.visibility = View.VISIBLE
                    } else {
                        statusText.text = getString(R.string.achievements_locked)
                        progressBar?.visibility = View.GONE
                    }
                    statusText.setTextColor(resources.getColor(android.R.color.darker_gray, null))
                    itemView.alpha = 0.7f
                }
                AchievementState.UNSUPPORTED -> {
                    statusText.text = getString(R.string.achievements_unsupported)
                    statusText.setTextColor(resources.getColor(android.R.color.holo_red_dark, null))
                    itemView.alpha = 0.5f
                }
            }

            // Load achievement badge image
            if (!achievement.badgeUrl.isNullOrEmpty()) {
                context?.let { ctx ->
                    Glide
                        .with(ctx)
                        .load(achievement.badgeUrl)
                        .placeholder(R.drawable.trophy_24px)
                        .error(R.drawable.trophy_24px)
                        .into(iconImageView)
                }
            } else {
                iconImageView.setImageResource(R.drawable.trophy_24px)
            }
        }
    }
}
