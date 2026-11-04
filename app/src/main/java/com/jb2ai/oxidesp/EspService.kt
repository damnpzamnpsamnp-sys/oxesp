package com.jb2ai.oxidesp

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.view.Gravity
import android.view.WindowManager
import android.util.Log

class EspService : Service() {

    companion object {
        private const val TAG = "OxEsp"
        private const val CH = "oxesp"
        private const val GAME_PKG = "com.oxidesurvivalisland"
        private const val TICK_MS = 8L
    }

    private lateinit var wm: WindowManager
    private var view: EspOverlayView? = null
    private val handler = Handler(Looper.getMainLooper())

    private val loop = object : Runnable {
        override fun run() {
            val v = view ?: return
            try {
                v.boxes = NativeBridge.tick()
                v.invalidate()
            } catch (t: Throwable) {
                Log.e(TAG, "tick fail", t)
            }
            handler.postDelayed(this, TICK_MS)
        }
    }

    override fun onBind(i: Intent?): IBinder? = null

    override fun onStartCommand(i: Intent?, f: Int, id: Int): Int {
        startForegroundCompat()
        wm = getSystemService(WINDOW_SERVICE) as WindowManager

        if (view == null) {
            val metrics = resources.displayMetrics
            val sw = metrics.widthPixels
            val sh = metrics.heightPixels

            if (!NativeBridge.init(GAME_PKG, sw, sh)) {
                Log.e(TAG, "native init failed — игра не запущена или нет root")
                stopSelf()
                return START_NOT_STICKY
            }

            val lp = WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                        or WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                PixelFormat.TRANSLUCENT
            ).apply {
                gravity = Gravity.TOP or Gravity.START
                x = 0; y = 0
            }

            val v = EspOverlayView(this).apply {
                setBackgroundColor(0x00000000)
            }
            wm.addView(v, lp)
            view = v
            handler.post(loop)
        }
        return START_STICKY
    }

    override fun onDestroy() {
        handler.removeCallbacks(loop)
        view?.let { runCatching { wm.removeView(it) } }
        view = null
        runCatching { NativeBridge.shutdown() }
        super.onDestroy()
    }

    private fun startForegroundCompat() {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26 && nm.getNotificationChannel(CH) == null) {
            nm.createNotificationChannel(NotificationChannel(CH, "oxesp", NotificationManager.IMPORTANCE_LOW))
        }
        val notif = Notification.Builder(this, CH)
            .setContentTitle("oxide esp")
            .setContentText("overlay активен")
            .setSmallIcon(android.R.drawable.ic_menu_view)
            .build()
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(0x101, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(0x101, notif)
        }
    }
}
