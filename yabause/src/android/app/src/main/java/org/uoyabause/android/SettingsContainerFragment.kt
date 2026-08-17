package org.uoyabause.android

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import org.devmiyax.yabasanshiro.R

/**
 * Wrapper Fragment that hosts SettingsFragment inside a MaterialToolbar layout.
 * Used in phone mode to keep the Navigation Bar visible while showing Settings.
 */
class SettingsContainerFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? = inflater.inflate(R.layout.fragment_settings_container, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val toolbar = view.findViewById<Toolbar>(R.id.toolbar_settings)
        toolbar.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        if (savedInstanceState == null) {
            childFragmentManager
                .beginTransaction()
                .replace(R.id.settings_fragment_container, SettingsActivity.SettingsFragment())
                .commit()
        }
    }

    companion object {
        fun newInstance(): SettingsContainerFragment = SettingsContainerFragment()
    }
}
