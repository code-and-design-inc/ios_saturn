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
package org.uoyabause.android

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View

/**
 * Custom View for visualizing analog stick input values.
 * Shows both raw input and circle-clamped values.
 */
class AxisDebugView
    @JvmOverloads
    constructor(
        context: Context,
        attrs: AttributeSet? = null,
        defStyleAttr: Int = 0,
    ) : View(context, attrs, defStyleAttr) {
        private val circlePaint =
            Paint().apply {
                color = Color.WHITE
                style = Paint.Style.STROKE
                strokeWidth = 2f
                isAntiAlias = true
            }

        private val crosshairPaint =
            Paint().apply {
                color = Color.GRAY
                style = Paint.Style.STROKE
                strokeWidth = 1f
                isAntiAlias = true
            }

        private val rawPointPaint =
            Paint().apply {
                color = Color.RED
                style = Paint.Style.FILL
                isAntiAlias = true
            }

        private val clampedPointPaint =
            Paint().apply {
                color = Color.GREEN
                style = Paint.Style.FILL
                isAntiAlias = true
            }

        private val textPaint =
            Paint().apply {
                color = Color.WHITE
                textSize = 24f
                isAntiAlias = true
            }

        private val backgroundPaint =
            Paint().apply {
                color = Color.argb(180, 0, 0, 0)
                style = Paint.Style.FILL
            }

        private val viewSize = 200f
        private val circleRadius = 80f
        private val pointRadius = 8f

        override fun onMeasure(
            widthMeasureSpec: Int,
            heightMeasureSpec: Int,
        ) {
            val size = viewSize.toInt()
            setMeasuredDimension(size, size + 60) // Extra space for text
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)

            val centerX = viewSize / 2
            val centerY = viewSize / 2

            // Draw background
            canvas.drawRect(0f, 0f, viewSize, viewSize + 60, backgroundPaint)

            // Draw circle outline (valid input area)
            canvas.drawCircle(centerX, centerY, circleRadius, circlePaint)

            // Draw crosshairs
            canvas.drawLine(centerX - circleRadius, centerY, centerX + circleRadius, centerY, crosshairPaint)
            canvas.drawLine(centerX, centerY - circleRadius, centerX, centerY + circleRadius, crosshairPaint)

            // Get current values
            val rawX = AxisDebugValues.rawX
            val rawY = AxisDebugValues.rawY
            val clampedX = AxisDebugValues.axisX
            val clampedY = AxisDebugValues.axisY

            // Draw raw position (red) - raw values are -1 to 1
            val rawPosX = centerX + rawX * circleRadius
            val rawPosY = centerY + rawY * circleRadius
            canvas.drawCircle(rawPosX, rawPosY, pointRadius, rawPointPaint)

            // Draw clamped position (green) - clamped values are 0-255 with center at 128
            val clampedPosX = centerX + (clampedX - 128) / 128f * circleRadius
            val clampedPosY = centerY + (clampedY - 128) / 128f * circleRadius
            canvas.drawCircle(clampedPosX, clampedPosY, pointRadius + 2, clampedPointPaint)

            // Draw legend and values
            val legendY = viewSize + 20
            rawPointPaint.textSize = 20f
            clampedPointPaint.textSize = 20f

            canvas.drawCircle(10f, legendY - 5, 5f, rawPointPaint)
            canvas.drawText("Raw: %.2f, %.2f".format(rawX, rawY), 20f, legendY, textPaint)

            val legendY2 = viewSize + 45
            canvas.drawCircle(10f, legendY2 - 5, 5f, clampedPointPaint)
            canvas.drawText("Circle: %d, %d".format(clampedX, clampedY), 20f, legendY2, textPaint)
        }
    }
