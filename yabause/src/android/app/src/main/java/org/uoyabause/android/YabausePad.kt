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

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.AttributeSet
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.View.OnTouchListener
import androidx.preference.PreferenceManager
import org.devmiyax.yabasanshiro.R
import java.lang.Math.PI
import java.lang.Math.atan2
import java.lang.Math.sqrt
import kotlin.math.pow

internal open class PadButton {
    protected var rect: RectF

    // Multi-touch support: Track multiple pointer IDs
    protected val pointIdSet: MutableSet<Int> = mutableSetOf()

    // Legacy property for backward compatibility
    val pointId: Int
        get() = pointIdSet.firstOrNull() ?: -1
    protected var pointidInternal: Int
    var back: Paint? = null
    var scale: Float
    var centerX: Float = 0.0f
    var centerY: Float = 0.0f

    val pushPaint =
        Paint().apply {
            style = Paint.Style.FILL // 塗りつぶしスタイルを設定
            color = 0x80FFFFFF.toInt() // 色を設定（この例では緑）
        }

    open fun recalc() {
    }

    fun updateRect(
        x1: Int,
        y1: Int,
        x2: Int,
        y2: Int,
    ) {
        rect[x1.toFloat(), y1.toFloat(), x2.toFloat()] = y2.toFloat()
        centerX = rect.centerX()
        centerY = rect.centerY()
        recalc()
    }

    fun updateRect(
        matrix: Matrix,
        x1: Int,
        y1: Int,
        x2: Int,
        y2: Int,
    ) {
        rect[x1.toFloat(), y1.toFloat(), x2.toFloat()] = y2.toFloat()
        matrix.mapRect(rect)
        centerX = rect.centerX()
        centerY = rect.centerY()
        recalc()
    }

    fun updateScale(scale: Float) {
        this.scale = scale
    }

    fun contains(
        x: Int,
        y: Int,
    ): Boolean = rect.contains(x.toFloat(), y.toFloat())

    fun intersects(r: RectF?): Boolean = RectF.intersects(rect, r!!)

    fun getBounds(): RectF = rect

    open fun draw(
        canvas: Canvas,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        back =
            if (isOn()) {
                active_back
            } else {
                nomal_back
            }
    }

    // New multi-touch API
    fun addPointer(pointerId: Int) {
        pointIdSet.add(pointerId)
    }

    fun removePointer(pointerId: Int) {
        pointIdSet.remove(pointerId)
    }

    fun clearPointers() {
        pointIdSet.clear()
    }

    fun hasPointer(pointerId: Int): Boolean = pointIdSet.contains(pointerId)

    // Legacy API for backward compatibility
    fun on(index: Int) {
        addPointer(index)
    }

    open fun off() {
        clearPointers()
        pointidInternal = -1
    }

    fun isOn(index: Int): Boolean = hasPointer(index)

    fun isOn(): Boolean = pointIdSet.isNotEmpty()

    init {
        pointidInternal = -1
        rect = RectF()
        scale = 1.0f
    }
}

internal class DPadButton : PadButton() {
    override fun draw(
        canvas: Canvas,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        super.draw(canvas, nomal_back, active_back, front)
        if (isOn()) {
            canvas.drawRect(rect, pushPaint!!)
        } else {
            // canvas.drawRect(rect, pushPaint!!)
        }
    }
}

internal class StartButton : PadButton() {
    override fun draw(
        canvas: Canvas,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        super.draw(canvas, nomal_back, active_back, front)
        if (isOn()) {
            val newWidth = rect.width() * 1.5f
            val newHeight = rect.height() * 1.5f

            // 新しいRectFオブジェクトを作成
            val drect =
                RectF(
                    centerX - newWidth / 2,
                    centerY - newHeight / 2,
                    centerX + newWidth / 2,
                    centerY + newHeight / 2,
                )

            canvas.drawOval(drect, pushPaint)
        } else {
            // canvas.drawOval(rect, pushPaint)
        }
    }
}

internal class ActionButton(
    private val width: Int,
    private val text: String,
    private val textsize: Int,
) : PadButton() {
    override fun draw(
        canvas: Canvas,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        super.draw(canvas, nomal_back, active_back, front)
        if (isOn()) {
            canvas.drawCircle(rect.centerX(), rect.centerY(), width * scale * 1.5f, pushPaint)
        } else {
            // canvas.drawCircle(rect.centerX(), rect.centerY(), width * scale, pushPaint)
        }
        // front.setTextSize(textsize);
        // front.setTextAlign(Paint.Align.CENTER);
        // canvas.drawText(text, rect.centerX() , rect.centerY() , front);
    }
}

data class DpadState(
    var left: Boolean,
    var right: Boolean,
    var up: Boolean,
    var down: Boolean,
)

// Touch event state for managing vibration during multi-button press
data class TouchEventState(
    var shouldVibrate: Boolean = false,
    val newlyPressedButtons: MutableSet<Int> = mutableSetOf(),
)

internal class Dpad(
    private val width: Int,
    private val deadZone: Int,
) : PadButton() {
    val path = Path()
    val drect = RectF()

    override fun recalc() {
        val newWidth = rect.width() * 1.5f
        val newHeight = rect.height() * 1.5f

        // 新しいRectFオブジェクトを作成
        drect.set(
            centerX - newWidth / 2,
            centerY - newHeight / 2,
            centerX + newWidth / 2,
            centerY + newHeight / 2,
        )

        val outerRadius = Math.min(newWidth, newWidth) / 2f
        val innerRadius = (deadZone * scale)
        path.apply {
            reset()
            addCircle(centerX, centerY, outerRadius, Path.Direction.CW)
            addCircle(centerX, centerY, innerRadius, Path.Direction.CCW)
        }
    }

    init {
        recalc()
    }

    data class Point(
        val x: Double,
        val y: Double,
    )

    fun normalizeRadian(radian: Double): Double = ((radian % (2 * PI) + 2 * PI) % (2 * PI))

    fun angleBetweenPoints(
        point1: Point,
        point2: Point,
    ): Double {
        val deltaX = point2.x - point1.x
        val deltaY = point2.y - point1.y
        return normalizeRadian(atan2(deltaY, deltaX))
    }

    fun radiansToDegrees(radians: Double): Double = radians * (180 / PI)

    fun degreesToRadians(degrees: Double): Double = degrees * (PI / 180)

    fun distance(
        x1: Double,
        y1: Double,
        x2: Double,
        y2: Double,
    ): Double = sqrt((x2 - x1).pow(2) + (y2 - y1).pow(2))

    var ds = DpadState(false, false, false, false)

    fun getState(
        x: Int,
        y: Int,
    ): DpadState {
        if (distance(centerX.toDouble(), centerY.toDouble(), x.toDouble(), y.toDouble()) < deadZone * scale) {
            ds.up = false
            ds.right = false
            ds.down = false
            ds.left = false
            return ds
        }

        val pos = degreesToRadians(22.5)
        val rad = angleBetweenPoints(Point(centerX.toDouble(), centerY.toDouble()), Point(x.toDouble(), y.toDouble()))
        if (rad >= 0 && rad < pos) {
            ds.up = false
            ds.right = true
            ds.down = false
            ds.left = false
        } else if (rad >= pos && rad < pos + (pos * 2)) {
            ds.up = false
            ds.right = true
            ds.down = true
            ds.left = false
        } else if (rad >= pos + (pos * 2) && rad < pos + (pos * 4)) {
            ds.up = false
            ds.right = false
            ds.down = true
            ds.left = false
        } else if (rad >= pos + (pos * 4) && rad < pos + (pos * 6)) {
            ds.up = false
            ds.right = false
            ds.down = true
            ds.left = true
        } else if (rad >= pos + (pos * 6) && rad < pos + (pos * 8)) {
            ds.up = false
            ds.right = false
            ds.down = false
            ds.left = true
        } else if (rad >= pos + (pos * 8) && rad < pos + (pos * 10)) {
            ds.up = true
            ds.right = false
            ds.down = false
            ds.left = true
        } else if (rad >= pos + (pos * 10) && rad < pos + (pos * 12)) {
            ds.up = true
            ds.right = false
            ds.down = false
            ds.left = false
        } else if (rad >= pos + (pos * 12) && rad < pos + (pos * 14)) {
            ds.up = true
            ds.right = true
            ds.down = false
            ds.left = false
        } else if (rad >= pos + (pos * 14) && rad < (pos * 16)) {
            ds.up = false
            ds.right = true
            ds.down = false
            ds.left = false
        }
        return ds
    }

    private val transPaint =
        Paint().apply {
            style = Paint.Style.FILL // 塗りつぶしスタイルを設定
            color = 0xFF000000.toInt() // 色を設定（この例では緑）
        }

    override fun draw(
        canvas: Canvas,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        // canvas.drawRect(rect,pushPaint)
        var dstate = 0
        var startDir = 0.0f
        if (ds.left) {
            dstate = dstate or 0x01
        }
        if (ds.right) {
            dstate = dstate or 0x02
        }
        if (ds.up) {
            dstate = dstate or 0x04
        }
        if (ds.down) {
            dstate = dstate or 0x08
        }

        if (dstate == 0) return

        if (dstate == 0x02) {
            startDir = -22.5f
        }
        if (dstate == 0x0A) {
            startDir = -22.5f + 45.0f
        }
        if (dstate == 0x08) {
            startDir = -22.5f + (45.0f * 2)
        }
        if (dstate == 0x09) {
            startDir = -22.5f + (45.0f * 3)
        }
        if (dstate == 0x01) {
            startDir = -22.5f + (45.0f * 4)
        }
        if (dstate == 0x05) {
            startDir = -22.5f + (45.0f * 5)
        }
        if (dstate == 0x04) {
            startDir = -22.5f + (45.0f * 6)
        }
        if (dstate == 0x06) {
            startDir = -22.5f + (45.0f * 7)
        }

        canvas.save()
        canvas.clipPath(path)
        canvas.drawArc(drect, startDir, 45.0f, true, pushPaint)
        canvas.restore()
    }

    override fun off() {
        ds.right = false
        ds.left = false
        ds.down = false
        ds.up = false
        super.off()
    }
}

internal class AnalogPad(
    private val width: Int,
    private val text: String,
    private val textsize: Int,
) : PadButton() {
    private val paint = Paint()

    fun getXvalue(posx: Int): Int {
        var xv = (posx - rect.centerX()) / (width * scale / 2) * 128 + 128
        if (xv > 255) xv = 255f
        if (xv < 0) xv = 0f
        return xv.toInt()
    }

    fun getYvalue(posy: Int): Int {
        var yv = (posy - rect.centerY()) / (width * scale / 2) * 128 + 128
        if (yv > 255) yv = 255f
        if (yv < 0) yv = 0f
        return yv.toInt()
    }

    fun draw(
        canvas: Canvas,
        sx: Int,
        sy: Int,
        nomal_back: Paint?,
        active_back: Paint?,
        front: Paint?,
    ) {
        super.draw(canvas, nomal_back, active_back, front)
        // canvas.drawCircle(rect.centerX(), rect.centerY(), width * this.scale, back);
        // front.setTextSize(textsize);
        // front.setTextAlign(Paint.Align.CENTER);
        // canvas.drawText(text, rect.centerX() , rect.centerY() , front);
        // canvas.drawRect(rect,pushPaint)

        val dx = (sx - 128.0) / 128.0 * (width * scale / 2)
        val dy = (sy - 128.0) / 128.0 * (width * scale / 2)
        canvas.drawCircle(
            rect.centerX() + dx.toInt(),
            rect.centerY() + dy.toInt(),
            width * scale / 2,
            paint,
        )
    }

    init {
        paint.setARGB(0x80, 0x80, 0x80, 0x80)
    }
}

class DraggableBitmap(
    val bitmap: Bitmap,
    var x: Float,
    var y: Float,
    var centerX: Float,
    var centerY: Float,
    var scale: Float,
) {
    val width: Float
        get() {
            return bitmap.width.toFloat()
        }
    val height: Float
        get() {
            return bitmap.height.toFloat()
        }

    var matrix: Matrix = Matrix()

    fun updateMatrix(): Matrix {
        matrix.reset()

        matrix.postTranslate(-centerX, -centerY)
        matrix.postScale(scale, scale)
        matrix.postTranslate(centerX, centerY)
        matrix.postTranslate(x, y)

        return matrix
    }
}

class YabausePad :
    View,
    OnTouchListener {
    interface OnPadListener {
        fun onPad(event: PadEvent?): Boolean
    }

    private var isDragging = false
    private lateinit var buttons: Array<PadButton?>
    private var listener: OnPadListener? = null
    private var active: HashMap<Int, Int>? = null
    private var vibrator: Vibrator? = null

    // private DisplayMetrics metrics = null;
    var widthValue = 0
    var heightValue = 0
    var bitmapPadLeft: DraggableBitmap? = null
    var bitmapPadRight: DraggableBitmap? = null
    var bitmapPadTopLeft: DraggableBitmap? = null
    var bitmapPadTopRight: DraggableBitmap? = null
    var bitmapPadMiddle: DraggableBitmap? = null

    var bitmapPadLeftH: DraggableBitmap? = null
    var bitmapPadRightH: DraggableBitmap? = null
    var bitmapPadTopLeftH: DraggableBitmap? = null
    var bitmapPadTopRightH: DraggableBitmap? = null
    var bitmapPadMiddleH: DraggableBitmap? = null

    var isShow: Boolean = true

    private val mPaint = Paint()
    private val paint = Paint()
    private val apaint = Paint()
    private val tpaint = Paint()
    var scale = 1.0f
    var leftYPosition = 0.0f
    var topLeftYPosition = 0.0f
    var centerYPosition = 0.0f
    var rightYPosition = 0.0f
    var topRightYPosition = 0.0f
    var visualFeedback = true
    var forceFeedback = true
    var basewidth = 1920.0f
    var baseheight = 1080.0f
    private var wscale = 0f
    private var hscale = 0f
    var padTestestMode: Boolean = false
    var statusString: String? = null
        private set
    private var analogPad: AnalogPad? = null
    private var dynamicAnalog: DynamicAnalogStick? = null
    private lateinit var dpad: Dpad
    private var axiX = 128
    private var axiY = 128
    private var padMode = 0
    var analogMaxDistance: Float = 200f
        set(value) {
            field = value
            dynamicAnalog?.maxDistance = value
            invalidate()
        }
    var trans = 1.0f
    var hitsizeMultiplier = 1.0f // Default hitsize multiplier (1.0 = 15px base)
    var useClassicPadStyle = false // false = new style, true = classic style
    private var navigationBarHeight = 0

    // Debug: Store touch positions and hittest rectangles for visualization
    private val debugHittestRects = mutableListOf<RectF>()
    private val debugHittestFillPaint =
        Paint().apply {
            color = 0x4000FF00.toInt() // Semi-transparent green fill
            style = Paint.Style.FILL
        }
    private val debugHittestStrokePaint =
        Paint().apply {
            color = 0xFF00FF00.toInt() // Bright green stroke
            style = Paint.Style.STROKE
            strokeWidth = 8f
        }
    private val debugTouchPaint =
        Paint().apply {
            color = 0xFFFF0000.toInt() // Bright red
            style = Paint.Style.FILL
        }

    var bitmaps: MutableList<DraggableBitmap?>? = null
    private var draggingBitmap: DraggableBitmap? = null

    fun setPadMode(mode: Int) {
        padMode = mode
        invalidate()
    }

    fun updateWindowInsets(navigationBarHeight: Int) {
        this.navigationBarHeight = navigationBarHeight
        updateButtonPos()
        invalidate()
    }

    private fun findTouchedBitmap(
        x: Float,
        y: Float,
    ): DraggableBitmap? = bitmaps?.find { bitmap ->
        x >= bitmap!!.x &&
            x <= bitmap.x + bitmap.bitmap.width &&
            y >= bitmap.y &&
            y <= bitmap.y + bitmap.bitmap.height
    }

    private var lastX = 0f
    private var lastY = 0f

    fun onTouchDragging(
        v: View,
        event: MotionEvent,
    ): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.x
                lastY = event.y
                draggingBitmap = findTouchedBitmap(lastX, lastY)
            }

            MotionEvent.ACTION_MOVE -> {
                if (draggingBitmap == null) {
                    lastX = event.x
                    lastY = event.y
                    draggingBitmap = findTouchedBitmap(lastX, lastY)
                }
                draggingBitmap?.let {
                    val x = event.x
                    val y = event.y
                    val deltaX = x - lastX
                    val deltaY = y - lastY

                    it.x += deltaX
                    it.y += deltaY
                    it.updateMatrix()

                    lastX = x
                    lastY = y

                    invalidate()
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                isDragging = false
                draggingBitmap = null
                updateButtonPos()
            }
        }
        return true
    }

    // Reload only bitmap images while preserving positions
    fun reloadBitmapStyle() {
        if (!isShow) return

        val options =
            BitmapFactory.Options().apply {
                inScaled = false
            }

        // Select bitmap resources based on pad style setting
        val padLRes = if (useClassicPadStyle) R.drawable.pad_l else R.drawable.pad_l_new
        val padRRes = if (useClassicPadStyle) R.drawable.pad_r else R.drawable.pad_r_new
        val padMRes = if (useClassicPadStyle) R.drawable.pad_m else R.drawable.pad_m_new
        val padTopLRes = if (useClassicPadStyle) R.drawable.pad_top_l else R.drawable.pad_top_l_new
        val padTopRRes = if (useClassicPadStyle) R.drawable.pad_top_r else R.drawable.pad_top_r_new

        // Save current positions
        val leftX = bitmapPadLeft?.x ?: -1f
        val leftY = bitmapPadLeft?.y ?: -1f
        val rightX = bitmapPadRight?.x ?: -1f
        val rightY = bitmapPadRight?.y ?: -1f
        val topLeftX = bitmapPadTopLeft?.x ?: -1f
        val topLeftY = bitmapPadTopLeft?.y ?: -1f
        val topRightX = bitmapPadTopRight?.x ?: -1f
        val topRightY = bitmapPadTopRight?.y ?: -1f
        val middleX = bitmapPadMiddle?.x ?: -1f
        val middleY = bitmapPadMiddle?.y ?: -1f

        val leftXH = bitmapPadLeftH?.x ?: -1f
        val leftYH = bitmapPadLeftH?.y ?: -1f
        val rightXH = bitmapPadRightH?.x ?: -1f
        val rightYH = bitmapPadRightH?.y ?: -1f
        val topLeftXH = bitmapPadTopLeftH?.x ?: -1f
        val topLeftYH = bitmapPadTopLeftH?.y ?: -1f
        val topRightXH = bitmapPadTopRightH?.x ?: -1f
        val topRightYH = bitmapPadTopRightH?.y ?: -1f
        val middleXH = bitmapPadMiddleH?.x ?: -1f
        val middleYH = bitmapPadMiddleH?.y ?: -1f

        // Load new bitmaps
        val bpl = BitmapFactory.decodeResource(resources, padLRes, options)
        bitmapPadLeft = DraggableBitmap(bpl, leftX, leftY, -1F, -1F, 1.0f)
        bitmapPadLeftH = DraggableBitmap(bpl, leftXH, leftYH, -1F, -1F, 1.0f)

        val bpr = BitmapFactory.decodeResource(resources, padRRes, options)
        bitmapPadRight = DraggableBitmap(bpr, rightX, rightY, -1f, -1f, 1.0f)
        bitmapPadRightH = DraggableBitmap(bpr, rightXH, rightYH, -1f, -1f, 1.0f)

        val ptl = BitmapFactory.decodeResource(resources, padTopLRes, options)
        bitmapPadTopLeft = DraggableBitmap(ptl, topLeftX, topLeftY, ptl.width / 2F, ptl.height / 2F, 1.0f)
        bitmapPadTopLeftH = DraggableBitmap(ptl, topLeftXH, topLeftYH, ptl.width / 2F, ptl.height / 2F, 1.0f)

        val ptr = BitmapFactory.decodeResource(resources, padTopRRes, options)
        bitmapPadTopRight = DraggableBitmap(ptr, topRightX, topRightY, ptr.width / 2F, ptr.height / 2F, 1.0f)
        bitmapPadTopRightH = DraggableBitmap(ptr, topRightXH, topRightYH, ptr.width / 2F, ptr.height / 2F, 1.0f)

        val btn = BitmapFactory.decodeResource(resources, padMRes, options)
        bitmapPadMiddle = DraggableBitmap(btn, middleX, middleY, btn.width / 2F, btn.height / 2F, 1.0f)
        bitmapPadMiddleH = DraggableBitmap(btn, middleXH, middleYH, btn.width / 2F, btn.height / 2F, 1.0f)

        // Update bitmaps list based on current orientation
        if (widthValue > heightValue) {
            bitmaps =
                mutableListOf(
                    bitmapPadMiddleH,
                    bitmapPadTopLeftH,
                    bitmapPadTopRightH,
                    bitmapPadLeftH,
                    bitmapPadRightH,
                )
        } else {
            bitmaps =
                mutableListOf(
                    bitmapPadMiddle,
                    bitmapPadTopLeft,
                    bitmapPadTopRight,
                    bitmapPadLeft,
                    bitmapPadRight,
                )
        }
    }

    fun show(b: Boolean) {
        isShow = b
        if (b == false) {
            bitmapPadTopLeft = null
            bitmapPadTopRight = null
            bitmapPadMiddle = null
            bitmapPadLeft = null
            bitmapPadRight = null

            bitmapPadTopLeftH = null
            bitmapPadTopRightH = null
            bitmapPadMiddleH = null
            bitmapPadLeftH = null
            bitmapPadRightH = null

            bitmaps?.clear()
        } else {
            val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
            val options =
                BitmapFactory.Options().apply {
                    inScaled = false
                }

            // Select bitmap resources based on pad style setting
            val padLRes = if (useClassicPadStyle) R.drawable.pad_l else R.drawable.pad_l_new
            val padRRes = if (useClassicPadStyle) R.drawable.pad_r else R.drawable.pad_r_new
            val padMRes = if (useClassicPadStyle) R.drawable.pad_m else R.drawable.pad_m_new
            val padTopLRes = if (useClassicPadStyle) R.drawable.pad_top_l else R.drawable.pad_top_l_new
            val padTopRRes = if (useClassicPadStyle) R.drawable.pad_top_r else R.drawable.pad_top_r_new

            val bpl = BitmapFactory.decodeResource(resources, padLRes, options)
            bitmapPadLeft =
                DraggableBitmap(
                    bpl,
                    sharedPref.getFloat("bitmapPadLeft_x", -1f),
                    sharedPref.getFloat("bitmapPadLeft_y", -1f),
                    -1F,
                    -1F,
                    1.0f,
                )
            bitmapPadLeftH =
                DraggableBitmap(
                    bpl,
                    sharedPref.getFloat("bitmapPadLeft_x_h", -1f),
                    sharedPref.getFloat("bitmapPadLeft_y_h", -1f),
                    -1F,
                    -1F,
                    1.0f,
                )

            val bpr = BitmapFactory.decodeResource(resources, padRRes, options)
            bitmapPadRight =
                DraggableBitmap(
                    bpr,
                    sharedPref.getFloat("bitmapPadRight_x", -1f),
                    sharedPref.getFloat("bitmapPadRight_y", -1f),
                    -1f,
                    -1f,
                    1.0f,
                )
            bitmapPadRightH =
                DraggableBitmap(
                    bpr,
                    sharedPref.getFloat("bitmapPadRight_x_h", -1f),
                    sharedPref.getFloat("bitmapPadRight_y_h", -1f),
                    -1f,
                    -1f,
                    1.0f,
                )

            val ptl = BitmapFactory.decodeResource(resources, padTopLRes, options)
            bitmapPadTopLeft =
                DraggableBitmap(
                    ptl,
                    sharedPref.getFloat("bitmapPadTopLeft_x", -1f),
                    sharedPref.getFloat("bitmapPadTopLeft_y", -1f),
                    ptl.width / 2F,
                    ptl.height / 2F,
                    1.0f,
                )
            bitmapPadTopLeftH =
                DraggableBitmap(
                    ptl,
                    sharedPref.getFloat("bitmapPadTopLeft_x_h", -1f),
                    sharedPref.getFloat("bitmapPadTopLeft_y_h", -1f),
                    ptl.width / 2F,
                    ptl.height / 2F,
                    1.0f,
                )

            val ptr = BitmapFactory.decodeResource(resources, padTopRRes, options)
            bitmapPadTopRight =
                DraggableBitmap(
                    ptr,
                    sharedPref.getFloat("bitmapPadTopRight_x", -1f),
                    sharedPref.getFloat("bitmapPadTopRight_y", -1f),
                    ptr.width / 2F,
                    ptr.height / 2F,
                    1.0f,
                )
            bitmapPadTopRightH =
                DraggableBitmap(
                    ptr,
                    sharedPref.getFloat("bitmapPadTopRight_x_h", -1f),
                    sharedPref.getFloat("bitmapPadTopRight_y_h", -1f),
                    ptr.width / 2F,
                    ptr.height / 2F,
                    1.0f,
                )

            val btn = BitmapFactory.decodeResource(resources, padMRes, options)
            bitmapPadMiddle =
                DraggableBitmap(
                    btn,
                    sharedPref.getFloat("bitmapPadMiddle_x", -1f),
                    sharedPref.getFloat("bitmapPadMiddle_y", -1f),
                    btn.width / 2F,
                    btn.height / 2F,
                    1.0f,
                )
            bitmapPadMiddleH =
                DraggableBitmap(
                    btn,
                    sharedPref.getFloat("bitmapPadMiddle_x_h", -1f),
                    sharedPref.getFloat("bitmapPadMiddle_y_h", -1f),
                    btn.width / 2F,
                    btn.height / 2F,
                    1.0f,
                )

            bitmaps =
                mutableListOf(
                    bitmapPadMiddle,
                    bitmapPadTopLeft,
                    bitmapPadTopRight,
                    bitmapPadLeft,
                    bitmapPadRight,
                )
        }
        invalidate()
    }

    fun saveCurrentPositionState() {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        val editor = sharedPref.edit()

        if (bitmapPadLeft != null) {
            editor.putFloat("bitmapPadLeft_x", bitmapPadLeft!!.x)
            editor.putFloat("bitmapPadLeft_y", bitmapPadLeft!!.y)
        }
        if (bitmapPadRight != null) {
            editor.putFloat("bitmapPadRight_x", bitmapPadRight!!.x)
            editor.putFloat("bitmapPadRight_y", bitmapPadRight!!.y)
        }
        if (bitmapPadTopLeft != null) {
            editor.putFloat("bitmapPadTopLeft_x", bitmapPadTopLeft!!.x)
            editor.putFloat("bitmapPadTopLeft_y", bitmapPadTopLeft!!.y)
        }
        if (bitmapPadTopRight != null) {
            editor.putFloat("bitmapPadTopRight_x", bitmapPadTopRight!!.x)
            editor.putFloat("bitmapPadTopRight_y", bitmapPadTopRight!!.y)
        }
        if (bitmapPadMiddle != null) {
            editor.putFloat("bitmapPadMiddle_x", bitmapPadMiddle!!.x)
            editor.putFloat("bitmapPadMiddle_y", bitmapPadMiddle!!.y)
        }

        if (bitmapPadLeftH != null) {
            editor.putFloat("bitmapPadLeft_x_h", bitmapPadLeftH!!.x)
            editor.putFloat("bitmapPadLeft_y_h", bitmapPadLeftH!!.y)
        }
        if (bitmapPadRightH != null) {
            editor.putFloat("bitmapPadRight_x_h", bitmapPadRightH!!.x)
            editor.putFloat("bitmapPadRight_y_h", bitmapPadRightH!!.y)
        }
        if (bitmapPadTopLeftH != null) {
            editor.putFloat("bitmapPadTopLeft_x_h", bitmapPadTopLeftH!!.x)
            editor.putFloat("bitmapPadTopLeft_y_h", bitmapPadTopLeftH!!.y)
        }
        if (bitmapPadTopRightH != null) {
            editor.putFloat("bitmapPadTopRight_x_h", bitmapPadTopRightH!!.x)
            editor.putFloat("bitmapPadTopRight_y_h", bitmapPadTopRightH!!.y)
        }
        if (bitmapPadMiddleH != null) {
            editor.putFloat("bitmapPadMiddle_x_h", bitmapPadMiddleH!!.x)
            editor.putFloat("bitmapPadMiddle_y_h", bitmapPadMiddleH!!.y)
        }

        editor.commit()
    }

    constructor(context: Context?) : super(context) {
        init()
    }

    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs) {
        init()
    }

    constructor(context: Context?, attrs: AttributeSet?, defStyle: Int) : super(
        context,
        attrs,
        defStyle,
    ) {
        init()
    }

    fun setTestmode(test: Boolean) {
        padTestestMode = test
    }

    fun getPreferences() {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        scale = sharedPref.getFloat("pref_pad_scale", 0.75f)
        leftYPosition = sharedPref.getFloat("pref_pad_pos", 0.1f)
        centerYPosition = sharedPref.getFloat("pref_pad_center_pos", 0.1f)
        rightYPosition = sharedPref.getFloat("pref_pad_right_pos", 0.1f)
        topLeftYPosition = sharedPref.getFloat("pref_pad_top_left_pos", 1.2f)
        topRightYPosition = sharedPref.getFloat("pref_pad_top_right_pos", 1.2f)
        trans = sharedPref.getFloat("pref_pad_trans", 0.7f)
        hitsizeMultiplier = sharedPref.getFloat("pref_pad_hitsize", 1.0f)
        useClassicPadStyle = sharedPref.getBoolean("pref_classic_pad_style", false)
        visualFeedback = sharedPref.getBoolean("pref_visual_feedback", true)
        forceFeedback = sharedPref.getBoolean("pref_force_feedback", true)
        analogMaxDistance = sharedPref.getFloat("pref_analog_max_distance", 200f)
        dynamicAnalog?.maxDistance = analogMaxDistance
        dynamicAnalog?.updateScale(scale)

        bitmapPadLeft?.x = sharedPref.getFloat("bitmapPadLeft_x", -1F)
        bitmapPadLeft?.y = sharedPref.getFloat("bitmapPadLeft_y", -1F)
        bitmapPadRight?.x = sharedPref.getFloat("bitmapPadRight_x", -1F)
        bitmapPadRight?.y = sharedPref.getFloat("bitmapPadRight_y", -1F)
        bitmapPadTopRight?.x = sharedPref.getFloat("bitmapPadTopRight_x", -1F)
        bitmapPadTopRight?.y = sharedPref.getFloat("bitmapPadTopRight_y", -1F)
        bitmapPadTopLeft?.x = sharedPref.getFloat("bitmapPadTopLeft_x", -1F)
        bitmapPadTopLeft?.y = sharedPref.getFloat("bitmapPadTopLeft_y", -1F)
        bitmapPadMiddle?.x = sharedPref.getFloat("bitmapPadMiddle_x", -1F)
        bitmapPadMiddle?.y = sharedPref.getFloat("bitmapPadMiddle_y", -1F)

        bitmapPadLeftH?.x = sharedPref.getFloat("bitmapPadLeft_x_h", -1F)
        bitmapPadLeftH?.y = sharedPref.getFloat("bitmapPadLeft_y_h", -1F)
        bitmapPadRightH?.x = sharedPref.getFloat("bitmapPadRight_x_h", -1F)
        bitmapPadRightH?.y = sharedPref.getFloat("bitmapPadRight_y_h", -1F)
        bitmapPadTopRightH?.x = sharedPref.getFloat("bitmapPadTopRight_x_h", -1F)
        bitmapPadTopRightH?.y = sharedPref.getFloat("bitmapPadTopRight_y_h", -1F)
        bitmapPadTopLeftH?.x = sharedPref.getFloat("bitmapPadTopLeft_x_h", -1F)
        bitmapPadTopLeftH?.y = sharedPref.getFloat("bitmapPadTopLeft_y_h", -1F)
        bitmapPadMiddleH?.x = sharedPref.getFloat("bitmapPadMiddle_x_h", -1F)
        bitmapPadMiddleH?.y = sharedPref.getFloat("bitmapPadMiddle_y_h", -1F)
    }

    fun updateScale() {
        // setPadScale( widthValue, heightValue );
        val oldClassicPadStyle = useClassicPadStyle
        getPreferences()
        // Reload bitmaps if pad style changed
        if (oldClassicPadStyle != useClassicPadStyle) {
            reloadBitmapStyle()
        }
        requestLayout()
        this.invalidate()
    }

    private fun init() {
        setOnTouchListener(this)
        isLongClickable = true
        setOnLongClickListener {
            if (this.padTestestMode) {
                isDragging = true
                var btnindex = 4
                while (btnindex < PadEvent.BUTTON_LAST) {
                    if (buttons[btnindex]!!.isOn()) {
                        buttons[btnindex]!!.off()
                        invalidate()
                    }
                    btnindex++
                }
                analogPad!!.off()
                dpad.off()
                preDstate = 0
            }
            true
        }
        getPreferences()
        buttons = arrayOfNulls(PadEvent.BUTTON_LAST)
        vibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager =
                    context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                vibratorManager.defaultVibrator
            } else {
                @Suppress("DEPRECATION")
                context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            }
        if (vibrator == null) {
            Log.e("Vibration", "Vibrator service is not available")
        } else if (vibrator?.hasVibrator() != true) {
            Log.e("Vibration", "This device does not support vibration")
            vibrator = null
        }
        buttons[PadEvent.BUTTON_RIGHT_TRIGGER] = DPadButton()
        buttons[PadEvent.BUTTON_LEFT_TRIGGER] = DPadButton()
        buttons[PadEvent.BUTTON_START] = StartButton()
        buttons[PadEvent.BUTTON_A] = ActionButton(100, "", 40)
        buttons[PadEvent.BUTTON_B] = ActionButton(100, "", 40)
        buttons[PadEvent.BUTTON_C] = ActionButton(100, "", 40)
        buttons[PadEvent.BUTTON_X] = ActionButton(72, "", 25)
        buttons[PadEvent.BUTTON_Y] = ActionButton(72, "", 25)
        buttons[PadEvent.BUTTON_Z] = ActionButton(72, "", 25)
        analogPad = AnalogPad(256, "", 40)
        dynamicAnalog = DynamicAnalogStick()
        dpad = Dpad(256, 60)
        active = HashMap()
    }

    override fun onAttachedToWindow() {
        paint.setARGB(0xFF, 0, 0, 0xFF)
        apaint.setARGB(0xFF, 0xFF, 0x00, 0x00)
        tpaint.setARGB(0x80, 0xFF, 0xFF, 0xFF)
        // bitmapPadLeft = BitmapFactory.decodeResource(getResources(), R.drawable.pad_l);
        // bitmapPadRight= BitmapFactory.decodeResource(getResources(), R.drawable.pad_r);
        mPaint.isAntiAlias = true
        mPaint.isFilterBitmap = true
        mPaint.isDither = true
        super.onAttachedToWindow()
    }

    public override fun onDraw(canvas: Canvas) {
        if (!isShow) {
            return
        }

        mPaint.alpha = (255.0f * trans).toInt()
        if (bitmaps != null) {
            for ((index, item) in bitmaps!!.asReversed().withIndex()) {
                if (item != null) {
                    // In analog mode, skip drawing the D-PAD bitmap (padLeft is at index 3)
                    // bitmaps are reversed, so padLeft is at reversed index = size - 1 - 3 = 1
                    val originalIndex = bitmaps!!.size - 1 - index
                    if (padMode == PadManager.MODE_ANALOG && originalIndex == padLeft) {
                        continue // Skip D-PAD bitmap in analog mode
                    }
                    canvas.drawBitmap(item!!.bitmap, item.matrix, mPaint)
                } else {
                    return
                }
            }
        }

        canvas.setMatrix(null)
        if (!visualFeedback) return
        if (isDragging) return

        canvas.save()
        buttons[PadEvent.BUTTON_LEFT_TRIGGER]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_A]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_B]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_C]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_X]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_Y]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_Z]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_RIGHT_TRIGGER]?.draw(canvas, paint, apaint, tpaint)
        buttons[PadEvent.BUTTON_START]?.draw(canvas, paint, apaint, tpaint)

        if (padMode == PadManager.MODE_ANALOG || padTestestMode) {
            // Draw the dynamic analog stick (iOS-style)
            // In test mode, always show to allow visual adjustment of maxDistance
            dynamicAnalog?.draw(canvas)
        }
        if (padMode != PadManager.MODE_ANALOG) {
            dpad.draw(canvas, paint, apaint, tpaint)
        }

        // Debug: Draw hittest rectangles when in test mode
        if (padTestestMode) {
            for (rect in debugHittestRects) {
                // Draw the hittest area fill (semi-transparent green)
                canvas.drawRect(rect, debugHittestFillPaint)
                // Draw the hittest area outline (bright green)
                canvas.drawRect(rect, debugHittestStrokePaint)
                // Draw the center touch point (red)
                val centerX = rect.centerX()
                val centerY = rect.centerY()
                canvas.drawCircle(centerX, centerY, 15f, debugTouchPaint)
            }
        }
        canvas.restore()
    }

    fun setOnPadListener(listener: OnPadListener?) {
        this.listener = listener
    }

    var preDstate = 0

    fun viberate() {
        if (!forceFeedback) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            vibrator?.vibrate(
                VibrationEffect.createOneShot(
                    16,
                    VibrationEffect.DEFAULT_AMPLITUDE,
                ),
            )
        }
    }

    fun actionDpadState(dpadState: DpadState) {
        var dstate = 0
        if (!padTestestMode) {
            if (dpadState.left) {
                YabauseRunnable.press(PadEvent.BUTTON_LEFT, 0)
            } else {
                YabauseRunnable.release(PadEvent.BUTTON_LEFT, 0)
            }
            if (dpadState.right) {
                YabauseRunnable.press(PadEvent.BUTTON_RIGHT, 0)
            } else {
                YabauseRunnable.release(PadEvent.BUTTON_RIGHT, 0)
            }
            if (dpadState.up) {
                YabauseRunnable.press(PadEvent.BUTTON_UP, 0)
            } else {
                YabauseRunnable.release(PadEvent.BUTTON_UP, 0)
            }
            if (dpadState.down) {
                YabauseRunnable.press(PadEvent.BUTTON_DOWN, 0)
            } else {
                YabauseRunnable.release(PadEvent.BUTTON_DOWN, 0)
            }
        }

        if (dpadState.left) {
            dstate = dstate or 0x01
        }
        if (dpadState.right) {
            dstate = dstate or 0x02
        }
        if (dpadState.up) {
            dstate = dstate or 0x04
        }
        if (dpadState.down) {
            dstate = dstate or 0x08
        }

        if (preDstate != dstate) {
            if (dstate != 0) {
                viberate()
            }
            preDstate = dstate
        }
    }

    private fun updateDPad(
        hittest: RectF,
        posx: Int,
        posy: Int,
        pointerId: Int,
    ) {
        if (dpad.intersects(hittest)) {
            dpad.on(pointerId)
            invalidate()
            val dpadState = dpad.getState(posx, posy)
            actionDpadState(dpadState)
        } else if (dpad.isOn(pointerId)) {
            val dpadState = dpad.getState(posx, posy)
            invalidate()
            actionDpadState(dpadState)
        }
    }

    private fun releaseDPad(pointerId: Int) {
        if (dpad.isOn(pointerId)) {
            dpad.off()
            if (!padTestestMode) {
                YabauseRunnable.release(PadEvent.BUTTON_LEFT, 0)
                YabauseRunnable.release(PadEvent.BUTTON_RIGHT, 0)
                YabauseRunnable.release(PadEvent.BUTTON_UP, 0)
                YabauseRunnable.release(PadEvent.BUTTON_DOWN, 0)
                preDstate = 0
            }
            invalidate()
        }
    }

    private fun updatePad(
        hittest: RectF,
        posx: Int,
        posy: Int,
        pointerId: Int,
    ) {
        if (analogPad!!.intersects(hittest)) {
            analogPad!!.on(pointerId)
            axiX = analogPad!!.getXvalue(posx)
            axiY = analogPad!!.getYvalue(posy)
            invalidate()
            if (!padTestestMode) {
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_X, 0, axiX)
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_Y, 0, axiY)
            }
        } else if (analogPad!!.isOn(pointerId)) {
            axiX = analogPad!!.getXvalue(posx)
            axiY = analogPad!!.getYvalue(posy)
            invalidate()
            if (!padTestestMode) {
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_X, 0, axiX)
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_Y, 0, axiY)
            }
        }
    }

    private fun releasePad(pointerId: Int) {
        if (analogPad!!.isOn(pointerId)) {
            analogPad!!.off()
            axiX = 128
            axiY = 128
            if (!padTestestMode) {
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_X, 0, axiX)
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_Y, 0, axiY)
            }
            invalidate()
        }
    }

    // Dynamic analog stick methods for iOS-style touch handling
    private fun updateDynamicAnalog(
        posx: Int,
        posy: Int,
        pointerId: Int,
        action: Int,
    ) {
        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                // Check if touch is within the analog pad bounds
                val analogBounds = analogPad?.getBounds() ?: return
                if (analogBounds.contains(posx.toFloat(), posy.toFloat())) {
                    dynamicAnalog!!.onTouchBegin(posx.toFloat(), posy.toFloat(), pointerId)
                    viberate()
                    sendDynamicAnalogValues()
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (dynamicAnalog!!.isTrackingPointer(pointerId)) {
                    val shouldVibrate = dynamicAnalog!!.onTouchMove(posx.toFloat(), posy.toFloat())
                    if (shouldVibrate) {
                        viberate()
                    }
                    sendDynamicAnalogValues()
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                if (dynamicAnalog!!.isTrackingPointer(pointerId)) {
                    dynamicAnalog!!.onTouchEnd()
                    sendDynamicAnalogValues()
                }
            }
        }
        invalidate()
    }

    private fun sendDynamicAnalogValues() {
        axiX = dynamicAnalog!!.getXvalue()
        axiY = dynamicAnalog!!.getYvalue()
        if (!padTestestMode) {
            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_X, 0, axiX)
            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_Y, 0, axiY)
        }
    }

    private fun releaseDynamicAnalog(pointerId: Int) {
        if (dynamicAnalog!!.isTrackingPointer(pointerId)) {
            dynamicAnalog!!.onTouchEnd()
            axiX = 128
            axiY = 128
            if (!padTestestMode) {
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_X, 0, axiX)
                YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_Y, 0, axiY)
            }
            invalidate()
        }
    }

    // Multi-touch button press handler: registers pointer for ALL intersecting buttons
    private var shouldVibrateOnEvent = false

    private fun handleButtonPress(
        hittest: RectF,
        pointerId: Int,
    ) {
        var anyNewPress = false
        var btnindex = 4
        while (btnindex < PadEvent.BUTTON_LAST) {
            if (buttons[btnindex]!!.intersects(hittest)) {
                if (!buttons[btnindex]!!.hasPointer(pointerId)) {
                    buttons[btnindex]!!.addPointer(pointerId)
                    anyNewPress = true
                }
            }
            btnindex++
        }
        if (anyNewPress) {
            shouldVibrateOnEvent = true
            invalidate()
        }
    }

    // Multi-touch button release handler: removes pointer from ALL buttons
    private fun handleButtonRelease(pointerId: Int) {
        var btnindex = 4
        while (btnindex < PadEvent.BUTTON_LAST) {
            if (buttons[btnindex]!!.hasPointer(pointerId)) {
                buttons[btnindex]!!.removePointer(pointerId)
            }
            btnindex++
        }
        invalidate()
    }

    // Multi-touch button move handler: updates button states based on current touch position
    private fun handleButtonMove(
        hittest: RectF,
        pointerId: Int,
    ) {
        var anyNewPress = false
        var btnindex = 4
        while (btnindex < PadEvent.BUTTON_LAST) {
            val button = buttons[btnindex]!!
            val intersects = button.intersects(hittest)
            val hasPointer = button.hasPointer(pointerId)

            if (intersects && !hasPointer) {
                // Finger moved into button area
                button.addPointer(pointerId)
                anyNewPress = true
            } else if (!intersects && hasPointer) {
                // Finger moved out of button area
                button.removePointer(pointerId)
            }
            btnindex++
        }
        if (anyNewPress) {
            shouldVibrateOnEvent = true
        }
        invalidate()
    }

    override fun onTouch(
        v: View,
        event: MotionEvent,
    ): Boolean {
        if (isDragging) {
            return onTouchDragging(v, event)
        }

        val action = event.actionMasked
        val touchCount = event.pointerCount
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        val posx = event.getX(pointerIndex).toInt()
        val posy = event.getY(pointerIndex).toInt()
        val hitsize = 30.0f * scale * hitsizeMultiplier
        val hittest =
            RectF(
                (posx - hitsize).toFloat(),
                (posy - hitsize).toFloat(),
                (posx + hitsize).toFloat(),
                (posy + hitsize).toFloat(),
            )

        // Debug: Update hittest visualization for all active touch points
        if (padTestestMode) {
            debugHittestRects.clear()
            for (i in 0 until touchCount) {
                val x = event.getX(i)
                val y = event.getY(i)
                debugHittestRects.add(
                    RectF(
                        x - hitsize,
                        y - hitsize,
                        x + hitsize,
                        y + hitsize,
                    ),
                )
            }
        }

        when (action) {
            MotionEvent.ACTION_DOWN -> {
                shouldVibrateOnEvent = false
                handleButtonPress(hittest, pointerId)
                if (padMode == PadManager.MODE_ANALOG) {
                    updateDynamicAnalog(posx, posy, pointerId, action)
                } else {
                    updateDPad(hittest, posx, posy, pointerId)
                }
                if (shouldVibrateOnEvent) {
                    viberate()
                }
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                shouldVibrateOnEvent = false
                handleButtonPress(hittest, pointerId)
                if (padMode == PadManager.MODE_ANALOG) {
                    updateDynamicAnalog(posx, posy, pointerId, action)
                } else {
                    updateDPad(hittest, posx, posy, pointerId)
                }
                if (shouldVibrateOnEvent) {
                    viberate()
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                handleButtonRelease(pointerId)
                if (padMode == PadManager.MODE_ANALOG) {
                    releaseDynamicAnalog(pointerId)
                } else {
                    releaseDPad(pointerId)
                }
            }
            MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_UP -> {
                handleButtonRelease(pointerId)
                if (padMode == PadManager.MODE_ANALOG) {
                    releaseDynamicAnalog(pointerId)
                } else {
                    releaseDPad(pointerId)
                }
                // Debug: Clear hittest visualization when all fingers are lifted
                if (padTestestMode) {
                    debugHittestRects.clear()
                }
            }
            MotionEvent.ACTION_MOVE -> {
                shouldVibrateOnEvent = false
                var index = 0
                while (index < touchCount) {
                    val eventID2 = event.getPointerId(index)
                    val x2 = event.getX(index)
                    val y2 = event.getY(index)
                    val hittest2 =
                        RectF(
                            (x2 - hitsize),
                            (y2 - hitsize),
                            (x2 + hitsize),
                            (y2 + hitsize),
                        )
                    handleButtonMove(hittest2, eventID2)
                    if (padMode == PadManager.MODE_ANALOG) {
                        updateDynamicAnalog(x2.toInt(), y2.toInt(), eventID2, action)
                    } else {
                        updateDPad(hittest2, x2.toInt(), y2.toInt(), eventID2)
                    }
                    index++
                }
                if (shouldVibrateOnEvent) {
                    viberate()
                }
            }
        }
        if (!padTestestMode) {
            if (padMode == 0) {
                for (btnindex in 4 until PadEvent.BUTTON_LAST) {
                    if (buttons[btnindex]!!.isOn()) {
                        YabauseRunnable.press(btnindex, 0)
                    } else {
                        YabauseRunnable.release(btnindex, 0)
                    }
                }
            } else {
                for (btnindex in PadEvent.BUTTON_RIGHT_TRIGGER until PadEvent.BUTTON_LAST) {
                    if (buttons[btnindex]!!.isOn()) {
                        YabauseRunnable.press(btnindex, 0)
                        if (btnindex == PadEvent.BUTTON_RIGHT_TRIGGER) {
                            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_RTRIGGER, 0, 255)
                        }
                        if (btnindex == PadEvent.BUTTON_LEFT_TRIGGER) {
                            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_LTRIGGER, 0, 255)
                        }
                    } else {
                        YabauseRunnable.release(btnindex, 0)
                        if (btnindex == PadEvent.BUTTON_RIGHT_TRIGGER) {
                            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_RTRIGGER, 0, 0)
                        }
                        if (btnindex == PadEvent.BUTTON_LEFT_TRIGGER) {
                            YabauseRunnable.axisWithDebug(PadEvent.PERANALOG_AXIS_LTRIGGER, 0, 0)
                        }
                    }
                }
            }
        }
        if (padTestestMode) {
            statusString = ""
            statusString += "START:"
            if (buttons[PadEvent.BUTTON_START]!!.isOn()) statusString += "ON " else statusString += "OFF "

            statusString += "\nDpad: "
            if (dpad.ds.up) {
                if (dpad.ds.left) {
                    statusString += "\u2196"
                } else if (dpad.ds.right) {
                    statusString += "\u2197"
                } else {
                    statusString += "\u2191"
                }
            } else if (dpad.ds.down) {
                if (dpad.ds.left) {
                    statusString += "\u2199"
                } else if (dpad.ds.right) {
                    statusString += "\u2198"
                } else {
                    statusString += "\u2193"
                }
            } else {
                if (dpad.ds.left) {
                    statusString += "\u2190"
                } else if (dpad.ds.right) {
                    statusString += "\u2192"
                }
            }
            statusString += "\nA:"
            if (buttons[PadEvent.BUTTON_A]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "B:"
            if (buttons[PadEvent.BUTTON_B]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "C:"
            if (buttons[PadEvent.BUTTON_C]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "\nX:"
            if (buttons[PadEvent.BUTTON_X]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "Y:"
            if (buttons[PadEvent.BUTTON_Y]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "Z:"
            if (buttons[PadEvent.BUTTON_Z]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "\nLT:"
            if (buttons[PadEvent.BUTTON_LEFT_TRIGGER]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "RT:"
            if (buttons[PadEvent.BUTTON_RIGHT_TRIGGER]!!.isOn()) statusString += "ON " else statusString += "OFF "
            statusString += "\nAX:"
            if (analogPad!!.isOn()) statusString += "ON $axiX" else statusString += "OFF $axiX"
            statusString += "AY:"
            if (analogPad!!.isOn()) statusString += "ON $axiY" else statusString += "OFF $axiY"
        }
        if (listener != null) {
            listener!!.onPad(null)
        }
        return false
    }

    override fun onMeasure(
        widthMeasureSpec: Int,
        heightMeasureSpec: Int,
    ) {
        if (!isShow) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec)
            return
        }
        widthValue = MeasureSpec.getSize(widthMeasureSpec)
        heightValue = MeasureSpec.getSize(heightMeasureSpec)
        setPadScale(widthValue, heightValue)
    }

    val buttonCenterX = 347
    val buttonCenterY = 720
    val rectsize = 230

    fun updateButtonPos() {
        if (bitmaps == null || bitmaps!!.size <= padRight) return
        if (bitmaps!!.get(padLeft) == null) return
        if (bitmaps!!.get(padRight) == null) return
        if (bitmaps!!.get(padTopLeft) == null) return
        if (bitmaps!!.get(padTopRight) == null) return
        if (analogPad == null) return

        analogPad!!.updateRect(
            bitmaps!!.get(padLeft)!!.matrix,
            buttonCenterX - rectsize,
            buttonCenterY - rectsize,
            buttonCenterX + rectsize,
            buttonCenterY + rectsize,
        )
        analogPad!!.updateScale(scale)

        dpad.updateRect(
            bitmaps!!.get(padLeft)!!.matrix,
            buttonCenterX - rectsize,
            buttonCenterY - rectsize,
            buttonCenterX + rectsize,
            buttonCenterY + rectsize,
        )
        dpad.updateScale(scale)

        // Set default center for dynamic analog stick (same position as D-PAD center)
        dynamicAnalog?.let {
            val dpadBounds = dpad.getBounds()
            it.defaultCenter = android.graphics.PointF(dpadBounds.centerX(), dpadBounds.centerY())
            it.updateScale(scale)
        }

        // buttons[PadEvent.BUTTON_START].updateRect(matrix_left,510,1013,510+182,1013+57);
        buttons[PadEvent.BUTTON_START]!!
            .updateRect(bitmaps!!.get(padMiddle)!!.matrix, 0, 53, 185, 132)

        // Right Part
        buttons[PadEvent.BUTTON_A]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 59, 801, 59 + 213, 801 + 225)
        buttons[PadEvent.BUTTON_A]!!.updateScale(scale)
        buttons[PadEvent.BUTTON_B]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 268, 672, 268 + 229, 672 + 221)
        buttons[PadEvent.BUTTON_B]!!.updateScale(scale)
        buttons[PadEvent.BUTTON_C]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 507, 577, 507 + 224, 577 + 229)
        buttons[PadEvent.BUTTON_C]!!.updateScale(scale)
        buttons[PadEvent.BUTTON_X]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 15, 602, 15 + 149, 602 + 150)
        buttons[PadEvent.BUTTON_X]!!.updateScale(scale)
        buttons[PadEvent.BUTTON_Y]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 202, 481, 202 + 149, 481 + 148)
        buttons[PadEvent.BUTTON_Y]!!.updateScale(scale)
        buttons[PadEvent.BUTTON_Z]!!.updateRect(bitmaps!!.get(padRight)!!.matrix, 397, 409, 397 + 151, 409 + 152)
        buttons[PadEvent.BUTTON_Z]!!.updateScale(scale)

        buttons[PadEvent.BUTTON_LEFT_TRIGGER]!!.updateRect(
            bitmaps!!.get(padTopLeft)!!.matrix,
            57,
            48,
            57 + 379,
            48 + 100,
        )
        buttons[PadEvent.BUTTON_RIGHT_TRIGGER]!!.updateRect(
            bitmaps!!.get(padTopRight)!!.matrix,
            338,
            48,
            338 + 379,
            48 + 100,
        )

        // Update gesture exclusion rects for D-pad area (Android 10+)
        updateGestureExclusionRects()
    }

    private fun updateGestureExclusionRects() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val dpadRect = dpad.getBounds()
            val margin = (20 * resources.displayMetrics.density).toInt() // 20dp margin

            val exclusionRect =
                Rect(
                    0, // (dpadRect.left - margin).toInt().coerceAtLeast(0),
                    (dpadRect.top - margin).toInt().coerceAtLeast(0),
                    (dpadRect.right + margin).toInt().coerceAtMost(width),
                    (dpadRect.bottom + margin).toInt().coerceAtMost(height),
                )

            systemGestureExclusionRects = listOf(exclusionRect)
        }
    }

    val padMiddle = 0
    val padTopLeft = 1
    val padTopRight = 2
    val padLeft = 3
    val padRight = 4

    fun setPadScale(
        width: Int,
        height: Int,
    ) {
        var dens = resources.displayMetrics.density
        val dir = context.getResources().getConfiguration().orientation

        val leftPadPosX = -120F
        var leftPadPosY = 500F

        val rightPadPosX = -60F
        var rightPadPosY = 80F

        val triggerX = -40F
        var triggerY = 1080f - 250f

        // 横画面
        if (width > height) {
            leftPadPosY = 0F
            rightPadPosY = 0F
            triggerY = 0F
            bitmaps =
                mutableListOf(
                    bitmapPadMiddleH,
                    bitmapPadTopLeftH,
                    bitmapPadTopRightH,
                    bitmapPadLeftH,
                    bitmapPadRightH,
                )
        } else {
            bitmaps =
                mutableListOf(
                    bitmapPadMiddle,
                    bitmapPadTopLeft,
                    bitmapPadTopRight,
                    bitmapPadLeft,
                    bitmapPadRight,
                )
        }

        // midedle
        bitmaps?.get(padMiddle)?.apply {
            if (x == -1F && y == -1F) {
                x = (width / 2).toFloat() - (this.width / 2.0F)
                y = height - this.height - navigationBarHeight
            }
            centerX = this.width / 2.0F
            centerY = this.height / 2.0F
            scale = this@YabausePad.scale
            updateMatrix()
        }

        // Top left
        bitmaps?.get(padTopLeft)?.apply {
            if (x == -1F && y == -1F) {
                x = triggerX
                y = triggerY
            }
            centerX = this.width / 2.0F
            centerY = this.height / 2.0F
            scale = this@YabausePad.scale
            updateMatrix()
        }

        // Top right
        bitmaps?.get(padTopRight)?.apply {
            if (x == -1F && y == -1F) {
                x = width.toFloat() - (this.width.toFloat() + triggerX)
                y = triggerY
            }
            centerX = this.width / 2.0F
            centerY = this.height / 2.0F
            scale = this@YabausePad.scale
            updateMatrix()
        }

        // bitmapPadLeft
        bitmaps?.get(padLeft)?.apply {
            if (x == -1F && y == -1F) {
                x = leftPadPosX
                y = height - ((this.height.toFloat()) + leftPadPosY) - navigationBarHeight
            }
            centerX = buttonCenterX.toFloat()
            centerY = buttonCenterY.toFloat()
            scale = this@YabausePad.scale
            updateMatrix()
        }

        // bitmapPadRight
        bitmaps?.get(padRight)?.apply {
            if (x == -1F && y == -1F) {
                x = width.toFloat() - (this.width.toFloat() + rightPadPosX)
                y = height - (this.height.toFloat() + rightPadPosY) - navigationBarHeight
            }
            centerX = 315f
            centerY = 652f
            scale = this@YabausePad.scale
            updateMatrix()
        }
        updateButtonPos()
        setMeasuredDimension(width, height)
    }
}
