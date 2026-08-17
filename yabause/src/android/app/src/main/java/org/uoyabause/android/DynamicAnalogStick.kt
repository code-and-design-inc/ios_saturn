/*  Copyright 2024 devMiyax(smiyaxdev@gmail.com)

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

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PointF
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

/**
 * Dynamic center point analog stick following iOS AnalogStickView pattern.
 * Touch start position becomes the origin, subsequent movements are relative to that point.
 */
internal class DynamicAnalogStick(
    private val stickRadius: Float = 40f * 2.0f,
    private val defaultMaxDistance: Float = 200f,
) : PadButton() {
    // Public configurable property
    var maxDistance: Float = defaultMaxDistance

    // Default center point (shown when not touched)
    var defaultCenter: PointF = PointF(0f, 0f)

    // Dynamic center point (set when touch begins)
    private var dynamicCenter: PointF? = null

    // Active center point (dynamic when touched, default otherwise)
    private val activeCenterPoint: PointF
        get() = dynamicCenter ?: defaultCenter

    // Current stick offset from dynamic center
    private var stickOffset: PointF = PointF(0f, 0f)

    // Track if at max distance for haptic feedback
    private var isAtMaxDistance = false

    // Paint objects for drawing
    private val rangePaint =
        Paint().apply {
            style = Paint.Style.STROKE
            color = 0x4DFFFFFF // 30% white
            strokeWidth = 4f
            isAntiAlias = true
        }

    private val centerIndicatorPaint =
        Paint().apply {
            style = Paint.Style.FILL
            color = 0x66FFFFFF // 40% white
            isAntiAlias = true
        }

    private val stickPaint =
        Paint().apply {
            style = Paint.Style.FILL
            color = 0xCCFFFFFF.toInt() // 80% white
            isAntiAlias = true
        }

    private val stickBorderPaint =
        Paint().apply {
            style = Paint.Style.STROKE
            color = 0xFFFFFFFF.toInt()
            strokeWidth = 4f
            isAntiAlias = true
        }

    /**
     * Called when touch begins - sets the dynamic center point
     */
    fun onTouchBegin(
        x: Float,
        y: Float,
        pointerId: Int,
    ) {
        dynamicCenter = PointF(x, y)
        stickOffset = PointF(0f, 0f)
        isAtMaxDistance = false
        on(pointerId)
    }

    /**
     * Called when touch moves - calculates offset from dynamic center
     * Returns true if haptic feedback should trigger (reached max distance)
     */
    fun onTouchMove(
        x: Float,
        y: Float,
    ): Boolean {
        val center = dynamicCenter ?: return false

        var newOffsetX = x - center.x
        var newOffsetY = y - center.y

        // Calculate scaled max distance
        val scaledMaxDistance = maxDistance * scale

        // Clamp to max distance
        val distance = hypot(newOffsetX, newOffsetY)
        var shouldVibrate = false

        if (distance > scaledMaxDistance) {
            val angle = atan2(newOffsetY, newOffsetX)
            newOffsetX = scaledMaxDistance * cos(angle)
            newOffsetY = scaledMaxDistance * sin(angle)

            if (!isAtMaxDistance) {
                shouldVibrate = true
                isAtMaxDistance = true
            }
        } else {
            isAtMaxDistance = false
        }

        stickOffset = PointF(newOffsetX, newOffsetY)
        return shouldVibrate
    }

    /**
     * Called when touch ends - resets to center
     */
    fun onTouchEnd() {
        dynamicCenter = null
        stickOffset = PointF(0f, 0f)
        isAtMaxDistance = false
        off()
    }

    /**
     * Get X axis value (0-255, 128 = center)
     */
    fun getXvalue(): Int {
        val scaledMaxDistance = maxDistance * scale
        if (scaledMaxDistance == 0f) return 128
        val normalizedX = (stickOffset.x / scaledMaxDistance) * 128f + 128f
        return normalizedX.toInt().coerceIn(0, 255)
    }

    /**
     * Get Y axis value (0-255, 128 = center)
     */
    fun getYvalue(): Int {
        val scaledMaxDistance = maxDistance * scale
        if (scaledMaxDistance == 0f) return 128
        val normalizedY = (stickOffset.y / scaledMaxDistance) * 128f + 128f
        return normalizedY.toInt().coerceIn(0, 255)
    }

    /**
     * Check if the analog stick is currently active (being touched)
     */
    fun isActive(): Boolean = dynamicCenter != null

    /**
     * Check if this stick is currently tracking the given pointer
     */
    fun isTrackingPointer(pointerId: Int): Boolean = isOn() && hasPointer(pointerId)

    /**
     * Draw the analog stick (always visible at default or dynamic center)
     */
    fun draw(canvas: Canvas) {
        val center = activeCenterPoint

        val scaledMaxDistance = maxDistance * scale
        val scaledStickRadius = stickRadius * scale

        // Draw movable range circle (outline)
        canvas.drawCircle(center.x, center.y, scaledMaxDistance, rangePaint)

        // Draw center point indicator
        canvas.drawCircle(center.x, center.y, 8f * scale, centerIndicatorPaint)

        // Draw stick circle at current position
        val stickX = center.x + stickOffset.x
        val stickY = center.y + stickOffset.y
        canvas.drawCircle(stickX, stickY, scaledStickRadius, stickPaint)
        canvas.drawCircle(stickX, stickY, scaledStickRadius, stickBorderPaint)
    }

    /**
     * Reset the analog stick to initial state
     */
    fun reset() {
        dynamicCenter = null
        stickOffset = PointF(0f, 0f)
        isAtMaxDistance = false
        off()
    }
}
