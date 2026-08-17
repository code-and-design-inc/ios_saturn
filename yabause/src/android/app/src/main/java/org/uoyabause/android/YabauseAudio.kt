/*  Copyright 2013 Guillaume Duhamel

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    ============================================================================

    Copyright 2019 devMiyax(smiyaxdev@gmail.com)

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
package org.uoyabause.android

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioAttributes
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.AudioManager.OnAudioFocusChangeListener
import android.os.Build
import android.util.Log

class YabauseAudio internal constructor(
    private val activity: Activity,
) : OnAudioFocusChangeListener {
    private var muteFlags: Int
    private var muted: Boolean
    private val audioManager: AudioManager = activity.getSystemService(Activity.AUDIO_SERVICE) as AudioManager
    private var audioDeviceCallback: AudioDeviceCallback? = null
    private var headsetReceiver: BroadcastReceiver? = null

    // AudioFocusRequest for API 26+
    private var audioFocusRequest: AudioFocusRequest? = null

    init {
        activity.volumeControlStream = AudioManager.STREAM_MUSIC
        muteFlags = 0
        muted = false

        // Create AudioFocusRequest for API 26+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val audioAttributes =
                AudioAttributes
                    .Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build()

            audioFocusRequest =
                AudioFocusRequest
                    .Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(audioAttributes)
                    .setOnAudioFocusChangeListener(this)
                    .build()
        }

        setupAudioDeviceListener()
    }

    fun mute(flags: Int) {
        muted = true
        muteFlags = muteFlags or flags
        abandonAudioFocus()
        YabauseRunnable.setVolume(0)
    }

    fun unmute(flags: Int) {
        muteFlags = muteFlags and flags.inv()
        if (0 == muteFlags) {
            muted = false
            val result = requestAudioFocus()
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                YabauseRunnable.setVolume(0)
            } else {
                YabauseRunnable.setVolume(100)
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun requestAudioFocus(): Int = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        audioFocusRequest?.let { audioManager.requestAudioFocus(it) }
            ?: AudioManager.AUDIOFOCUS_REQUEST_FAILED
    } else {
        audioManager.requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
    }

    @Suppress("DEPRECATION")
    private fun abandonAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioFocusRequest?.let { audioManager.abandonAudioFocusRequest(it) }
        } else {
            audioManager.abandonAudioFocus(this)
        }
    }

    override fun onAudioFocusChange(focusChange: Int) {
        if (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) {
            mute(SYSTEM)
        } else if (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK) {
            YabauseRunnable.setVolume(50)
        } else if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
            if (muted) unmute(SYSTEM) else YabauseRunnable.setVolume(100)
        } else if (focusChange == AudioManager.AUDIOFOCUS_LOSS) {
            mute(SYSTEM)
        }
    }

    private fun setupAudioDeviceListener() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            audioDeviceCallback =
                object : AudioDeviceCallback() {
                    override fun onAudioDevicesAdded(addedDevices: Array<AudioDeviceInfo>) {
                        super.onAudioDevicesAdded(addedDevices)
                        Log.d(TAG, "Audio devices added")
                        handleAudioDeviceChange()
                    }

                    override fun onAudioDevicesRemoved(removedDevices: Array<AudioDeviceInfo>) {
                        super.onAudioDevicesRemoved(removedDevices)
                        Log.d(TAG, "Audio devices removed")
                        handleAudioDeviceChange()
                    }
                }
            audioManager.registerAudioDeviceCallback(audioDeviceCallback, null)
        }

        val filter =
            IntentFilter().apply {
                addAction(AudioManager.ACTION_HDMI_AUDIO_PLUG)
                addAction(AudioManager.ACTION_HEADSET_PLUG)
                addAction(Intent.ACTION_HEADSET_PLUG)
            }

        headsetReceiver =
            object : BroadcastReceiver() {
                override fun onReceive(
                    context: Context,
                    intent: Intent,
                ) {
                    when (intent.action) {
                        AudioManager.ACTION_HDMI_AUDIO_PLUG -> {
                            val state = intent.getIntExtra(AudioManager.EXTRA_AUDIO_PLUG_STATE, -1)
                            Log.d(TAG, "HDMI audio plug state changed: $state")
                            handleAudioDeviceChange()
                        }
                        AudioManager.ACTION_HEADSET_PLUG,
                        Intent.ACTION_HEADSET_PLUG,
                        -> {
                            val state = intent.getIntExtra("state", -1)
                            Log.d(TAG, "Headset plug state changed: $state")
                            handleAudioDeviceChange()
                        }
                    }
                }
            }
        activity.registerReceiver(headsetReceiver, filter)
    }

    private fun handleAudioDeviceChange() {
        Log.d(TAG, "Handling audio device change, restarting audio stream")
        Thread {
            YabauseRunnable.restartAudioStream()
            if (!muted) {
                YabauseRunnable.setVolume(100)
            }
        }.start()
    }

    fun destroy() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            audioDeviceCallback?.let {
                audioManager.unregisterAudioDeviceCallback(it)
            }
        }
        headsetReceiver?.let {
            activity.unregisterReceiver(it)
        }
    }

    companion object {
        const val SYSTEM = 1
        const val USER = 2
        private const val TAG = "YabauseAudio"
    }
}
