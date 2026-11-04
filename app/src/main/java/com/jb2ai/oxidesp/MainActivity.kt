package com.jb2ai.oxidesp

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.button.MaterialButton
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : AppCompatActivity() {

    private val overlayReq = 0x10

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 96, 48, 48)
        }

        val label = TextView(this).apply {
            text = "oxide esp — overlay режим\nвыдай разрешение «Поверх других приложений», потом старт"
        }
        root.addView(label)

        val permBtn = MaterialButton(this).apply {
            text = "разрешение overlay"
            setOnClickListener {
                if (!Settings.canDrawOverlays(this@MainActivity)) {
                    startActivityForResult(
                        Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                            Uri.parse("package:$packageName")), overlayReq)
                }
            }
        }
        root.addView(permBtn)

        val startBtn = MaterialButton(this).apply {
            text = "старт"
            setOnClickListener {
                if (!Settings.canDrawOverlays(this@MainActivity)) {
                    label.text = "нет разрешения overlay"
                    return@setOnClickListener
                }
                if (Build.VERSION.SDK_INT >= 33) {
                    requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 1)
                }
                startForegroundService(Intent(this@MainActivity, EspService::class.java))
            }
        }
        root.addView(startBtn)

        val stopBtn = MaterialButton(this).apply {
            text = "стоп"
            setOnClickListener {
                stopService(Intent(this@MainActivity, EspService::class.java))
            }
        }
        root.addView(stopBtn)

        setContentView(root)
    }
}
