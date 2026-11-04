package com.jb2ai.oxidesp

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.View

class EspOverlayView(ctx: Context) : View(ctx) {

    var boxes: FloatArray = FloatArray(0)

    private val boxPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        color = Color.RED
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.YELLOW
        textSize = 34f
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val b = boxes
        var i = 0
        while (i + 4 < b.size) {
            val x1 = b[i]
            val y1 = b[i + 1]
            val x2 = b[i + 2]
            val y2 = b[i + 3]
            val d  = b[i + 4].toInt()
            canvas.drawRect(x1, y1, x2, y2, boxPaint)
            canvas.drawText("${d} m", x1, y1 - 8f, textPaint)
            i += 5
        }
    }
}
