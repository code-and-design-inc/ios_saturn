package org.uoyabause.android

import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.GLES20
import android.os.Build

/**
 * Lightweight device information for bug reports. The GPU renderer is queried once
 * (best-effort) via a throwaway EGL PBuffer context and cached; failures yield "".
 */
object DeviceInfo {
    fun model(): String = "${Build.MANUFACTURER} ${Build.MODEL}".trim()

    fun osVersion(): String = "Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})"

    @Volatile
    private var cachedGpu: String? = null

    /**
     * GL_VENDOR + GL_RENDERER (e.g. "Qualcomm Adreno (TM) 650"), or "" if it cannot
     * be determined. Result is cached after the first successful query. Safe to call
     * from a background thread; it creates and tears down its own EGL context and does
     * not touch any context current on other threads.
     */
    fun gpuRenderer(): String {
        cachedGpu?.let { return it }
        val result = queryGlRenderer()
        cachedGpu = result
        return result
    }

    private fun queryGlRenderer(): String {
        var display = EGL14.EGL_NO_DISPLAY
        var context = EGL14.EGL_NO_CONTEXT
        var surface = EGL14.EGL_NO_SURFACE
        try {
            display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
            if (display == EGL14.EGL_NO_DISPLAY) return ""
            val version = IntArray(2)
            if (!EGL14.eglInitialize(display, version, 0, version, 1)) return ""

            val configAttribs =
                intArrayOf(
                    EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                    EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                    EGL14.EGL_NONE,
                )
            val configs = arrayOfNulls<EGLConfig>(1)
            val numConfig = IntArray(1)
            if (!EGL14.eglChooseConfig(display, configAttribs, 0, configs, 0, 1, numConfig, 0) ||
                numConfig[0] == 0 || configs[0] == null
            ) {
                return ""
            }

            val contextAttribs = intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE)
            context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT, contextAttribs, 0)
            if (context == EGL14.EGL_NO_CONTEXT) return ""

            val surfaceAttribs = intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE)
            surface = EGL14.eglCreatePbufferSurface(display, configs[0], surfaceAttribs, 0)
            if (surface == EGL14.EGL_NO_SURFACE) return ""

            if (!EGL14.eglMakeCurrent(display, surface, surface, context)) return ""

            val vendor = GLES20.glGetString(GLES20.GL_VENDOR) ?: ""
            val renderer = GLES20.glGetString(GLES20.GL_RENDERER) ?: ""
            return listOf(vendor, renderer).filter { it.isNotBlank() }.joinToString(" ").trim()
        } catch (e: Exception) {
            return ""
        } finally {
            if (display != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(
                    display,
                    EGL14.EGL_NO_SURFACE,
                    EGL14.EGL_NO_SURFACE,
                    EGL14.EGL_NO_CONTEXT,
                )
                if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, surface)
                if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display, context)
                EGL14.eglTerminate(display)
            }
        }
    }
}
