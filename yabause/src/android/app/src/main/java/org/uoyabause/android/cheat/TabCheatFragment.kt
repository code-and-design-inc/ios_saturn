/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
package org.uoyabause.android.cheat

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.fragment.app.Fragment
import androidx.viewpager2.adapter.FragmentStateAdapter
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator
import org.devmiyax.yabasanshiro.R
import org.uoyabause.android.Yabause
import java.util.ArrayList

internal class CheatViewPagerAdapter(
    fragment: Fragment,
    private val gameId: String?,
) : FragmentStateAdapter(fragment) {
    override fun getItemCount(): Int = 2

    override fun createFragment(position: Int): Fragment = when (position) {
        PAGE_LOCAL -> LocalCheatItemFragment.newInstance(gameId, 1)
        PAGE_CLOUD -> CloudCheatItemFragment.newInstance(gameId, 1)
        else -> Fragment()
    }

    fun getPageTitle(position: Int): CharSequence = when (position) {
        PAGE_LOCAL -> "Local"
        PAGE_CLOUD -> "Shared"
        else -> ""
    }

    companion object {
        const val PAGE_LOCAL = 0
        const val PAGE_CLOUD = 1
    }
}

class TabCheatFragment : Fragment() {
    private var gameId: String? = null
    private var listener: OnFragmentInteractionListener? = null
    lateinit var activeCheats: ArrayList<String>
    var mainView: View? = null
    var tabLayout: TabLayout? = null
    private var viewPager: ViewPager2? = null
    private var tabLayoutMediator: TabLayoutMediator? = null
    private var toolbar: MaterialToolbar? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        activeCheats = ArrayList()
        gameId = requireArguments().getString(ARG_GAME_ID)
        val currentCheatCode = requireArguments().getStringArray(ARG_CURRENT_CHEAT)
        if (currentCheatCode != null) {
            for (i in currentCheatCode.indices) {
                activeCheats.add(currentCheatCode[i])
            }
        }
    }

    fun addActiveCheat(v: String) {
        if (!activeCheats.contains(v)) {
            activeCheats.add(v)
        }
    }

    fun removeActiveCheat(v: String) {
        activeCheats.remove(v)
    }

    fun isActive(v: String): Boolean {
        for (string in activeCheats) {
            if (string == v) {
                return true
            }
        }
        return false
    }

    fun switchToPreviousTab() {
        viewPager?.let { it.currentItem = it.currentItem - 1 }
    }

    fun switchToNextTab() {
        viewPager?.let { it.currentItem = it.currentItem + 1 }
    }

    fun moveCursor(direction: Int) {
        getCurrentFragment()?.moveCursor(direction)
    }

    fun selectCurrentItem() {
        getCurrentFragment()?.selectCurrentItem()
    }

    private fun getCurrentFragment(): BaseCheatItemFragment? =
        childFragmentManager.findFragmentByTag("f${viewPager?.currentItem}") as? BaseCheatItemFragment

    fun sendCheatListToYabause() {
        val cntChoice = activeCheats.size
        if (cntChoice > 0) {
            val cheatitem = arrayOfNulls<String>(cntChoice)
            var cindex = 0
            for (i in 0 until cntChoice) {
                cheatitem[cindex] = activeCheats[cindex]
                cindex++
            }
            val activity = activity as Yabause?
            activity!!.updateCheatCode(cheatitem)
        } else {
            val activity = activity as Yabause?
            activity!!.updateCheatCode(null)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        mainView = inflater.inflate(R.layout.fragment_tab_cheat, container, false)
        if (mainView == null) {
            return null
        }

        toolbar = mainView?.findViewById(R.id.toolbar_cheat)
        toolbar?.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        tabLayout = mainView?.findViewById<View>(R.id.tab_cheat_source) as TabLayout
        viewPager = mainView?.findViewById<View>(R.id.view_pager_cheat) as ViewPager2
        viewPager?.defaultFocusHighlightEnabled = false
        // Disable focus highlight on ViewPager2's internal RecyclerView
        val innerRecyclerView = viewPager?.getChildAt(0)
        innerRecyclerView?.defaultFocusHighlightEnabled = false

        val adapter = CheatViewPagerAdapter(this, gameId)
        viewPager?.adapter = adapter

        tabLayoutMediator =
            TabLayoutMediator(tabLayout!!, viewPager!!) { tab, position ->
                tab.text = adapter.getPageTitle(position)
            }
        tabLayoutMediator?.attach()

        return mainView
    }

    fun onButtonPressed(uri: Uri?) {
        if (listener != null) {
            listener!!.onFragmentInteraction(uri)
        }
    }

    fun enableFullScreen() {
        val window = requireActivity().window
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    fun disableFullScreen() {
        val window = requireActivity().window
        WindowCompat.setDecorFitsSystemWindows(window, true)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.show(WindowInsetsCompat.Type.systemBars())
    }

    override fun onDestroyView() {
        tabLayoutMediator?.detach()
        tabLayoutMediator = null
        viewPager?.adapter = null
        viewPager = null
        toolbar = null
        super.onDestroyView()
    }

    override fun onAttach(context: Context) {
        // disableFullScreen();
        super.onAttach(context)
        if (context is OnFragmentInteractionListener) {
            listener = context
        } else {
        }
    }

    override fun onDetach() {
        // enableFullScreen();
        sendCheatListToYabause()
        super.onDetach()
        listener = null
    }

    interface OnFragmentInteractionListener {
        fun onFragmentInteraction(uri: Uri?)
    }

    companion object {
        const val TAG = "TabCheatFragment"
        private const val ARG_GAME_ID = "gameid"
        private const val ARG_CURRENT_CHEAT = "current_cheats"

        fun newInstance(
            gameid: String?,
            currentCheatCode: Array<String?>?,
        ): TabCheatFragment {
            val fragment = TabCheatFragment()
            val args = Bundle()
            args.putString(ARG_GAME_ID, gameid)
            args.putStringArray(ARG_CURRENT_CHEAT, currentCheatCode)
            fragment.arguments = args
            return fragment
        }
    }
}
