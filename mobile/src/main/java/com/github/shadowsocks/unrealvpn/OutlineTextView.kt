package com.github.shadowsocks.unrealvpn

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import android.util.TypedValue
import android.widget.TextView
import androidx.appcompat.widget.AppCompatTextView
import com.github.shadowsocks.R

class OutlineTextView : AppCompatTextView {
    private var mAdditionalPadding = 0

    private val defaultStrokeWidth = 0F
    private var isDrawing: Boolean = false

    private var strokeColor: Int = 0
    private var strokeWidth: Float = 0.toFloat()

    constructor(context: Context) : super(context) {
        initResources(context, null)
    }

    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs) {
        initResources(context, attrs)
    }

    private fun initResources(context: Context, attrs: AttributeSet?) {
        includeFontPadding = false

        if (attrs != null) {
            val a = context.obtainStyledAttributes(attrs, R.styleable.outlineAttrs)
            strokeColor = a.getColor(
                R.styleable.outlineAttrs_outlineColor,
                currentTextColor
            )
            strokeWidth = a.getFloat(
                R.styleable.outlineAttrs_outlineWidth,
                defaultStrokeWidth
            )

            a.recycle()
        } else {
            strokeColor = currentTextColor
            strokeWidth = defaultStrokeWidth
        }
        setStrokeWidth(strokeWidth)
    }


    /**
     *  give value in sp
     */
    private fun setStrokeWidth(width: Float) {
        strokeWidth = width.toPx(context)
    }

    override fun invalidate() {
        if (isDrawing) return
        super.invalidate()
    }


    override fun onDraw(canvas: Canvas) {
        val yOff = -mAdditionalPadding / 6
        canvas.translate(0f, yOff.toFloat())

        if (strokeWidth > 0) {
            isDrawing = true

            val p = paint
            val currentTextColor = currentTextColor

            p.style = Paint.Style.STROKE
            p.strokeWidth = strokeWidth
            setTextColor(strokeColor)
            super.onDraw(canvas)

            setTextColor(currentTextColor)
            p.style = Paint.Style.FILL
            super.onDraw(canvas)

            isDrawing = false
        } else {
            super.onDraw(canvas)
        }
    }

    private fun Float.toPx(context: Context) =
        (this * context.resources.displayMetrics.scaledDensity + 0.5F)


    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        getAdditionalPadding()
        val mode = MeasureSpec.getMode(heightMeasureSpec)
        if (mode != MeasureSpec.EXACTLY) {
            val measureHeight = measureHeight(getText().toString(), widthMeasureSpec)
            var height = measureHeight - mAdditionalPadding
            height += paddingTop + paddingBottom
            super.onMeasure(
                widthMeasureSpec,
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
            )
        } else {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        }
    }

    private fun measureHeight(text: String, widthMeasureSpec: Int): Int {
        val textSize = textSize
        val textView = TextView(context)
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize)
        textView.text = text
        textView.measure(widthMeasureSpec, 0)
        return textView.measuredHeight
    }

    private fun getAdditionalPadding() {
        val textSize = textSize
        val textView = TextView(context)
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize)
        textView.setLines(1)
        textView.measure(0, 0)
        val measuredHeight = textView.measuredHeight
        if (measuredHeight - textSize > 0) {
            mAdditionalPadding = (measuredHeight - textSize).toInt()
        }
    }
}
