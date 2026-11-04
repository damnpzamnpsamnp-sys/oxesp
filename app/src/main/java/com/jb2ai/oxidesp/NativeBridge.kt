package com.jb2ai.oxidesp

object NativeBridge {
    init { System.loadLibrary("oxesp") }

    external fun init(pkg: String, screenW: Int, screenH: Int): Boolean
    external fun tick(): FloatArray
    external fun shutdown()
}
