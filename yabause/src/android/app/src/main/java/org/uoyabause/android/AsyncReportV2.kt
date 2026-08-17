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

import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.preference.PreferenceManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Authenticator
import okhttp3.Credentials
import okhttp3.Headers.Companion.headersOf
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.MultipartBody
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody
import okhttp3.RequestBody.Companion.asRequestBody
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import okio.Buffer
import org.devmiyax.yabasanshiro.R
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Created by shinya on 2017/10/07.
 */
class AsyncReportV2(
    private val mainActivity: Yabause,
    private val screenshotFilename: String,
    private val reportFilename: String,
    private val reportContents: Yabause.ReportContents,
    private val gameInfo: JSONObject,
) {
    private var client: OkHttpClient? = null

    private fun responseCount(httpResponse: Response): Int {
        var response = httpResponse
        var result = 1
        while (response.priorResponse.also { response = it!! } != null) {
            result++
        }
        return result
    }

    // ----------------------------------------------------------------------------------------------
    fun getgameid(
        baseurl: String,
        productId: String,
    ): Long {
        var id: Long = -1
        try {
            var encodedProductId = productId
            encodedProductId = encodedProductId.replace(" ", "%20")
            encodedProductId = encodedProductId.replace(".", "%2E")
            encodedProductId = encodedProductId.replace("-", "%2D")
            var url = "$baseurl/games/$encodedProductId"
            var request = Request.Builder().url(url).build()
            var response = client!!.newCall(request).execute()
            if (response.isSuccessful) {
                val rootObject = JSONObject(response.body!!.string())
                if (rootObject.getBoolean("result") == true) {
                    id = rootObject.getLong("id")
                }
            }

            // if failed create new data
            if (id == -1L) {
                url = "$baseurl/games/"
                val mimeType = "application/json; charset=utf-8".toMediaTypeOrNull()
                val requestBody = gameInfo.toString().toRequestBody(mimeType)
                request = Request
                    .Builder()
                    .url(url)
                    .post(requestBody)
                    .build()
                response = client!!.newCall(request).execute()
                if (response.isSuccessful) {
                    val rootObject = JSONObject(response.body!!.string())
                    if (rootObject.getBoolean("result") == true) {
                        id = rootObject.getLong("id")
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(LOG_TAG, "message", e)
            return -1
        }
        return id
    }

    suspend fun report(
        uri: String,
        productId: String,
    ): Int {
        val deviceId = Settings.Secure.getString(mainActivity.contentResolver, Settings.Secure.ANDROID_ID)
        withContext(Dispatchers.Main) {
            mainActivity.showDialog()
        }
        val id = getgameid(uri, productId)
        if (id == -1L) {
            withContext(Dispatchers.Main) {
                mainActivity.reportStatus = Yabause.REPORT_STATE_FAIL_CONNECTION
//                mainActivity.reportStatus = 400
                mainActivity.dismissDialog()
            }
            return -1
        }
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(mainActivity)
        val cputype = sharedPref.getString("pref_cpu", "2")
        val gputype = sharedPref.getString("pref_video", "1")
        val reportJson = JSONObject()
        val sendJson = JSONObject()
        try {
            reportJson.put("rating", reportContents.rating)
            if (reportContents.message != null) {
                reportJson.put("message", reportContents.message)
            }
            reportJson.put("emulator_version", YabauseApplication.getVersionName())
            reportJson.put("device", Build.MODEL)
            reportJson.put("user_id", 1)
            reportJson.put("device_id", deviceId)
            reportJson.put("game_id", id)
            reportJson.put("cpu_type", cputype)
            reportJson.put("video_type", gputype)
            sendJson.put("report", reportJson)
        } catch (e: Exception) {
            Log.e(LOG_TAG, "message", e)
            withContext(Dispatchers.Main) {
                mainActivity.reportStatus = Yabause.REPORT_STATE_FAIL_CONNECTION
                mainActivity.dismissDialog()
            }
            return -1
        }
        val mimeType = "application/json; charset=utf-8".toMediaTypeOrNull()
        val boundary = System.currentTimeMillis().toString()

        // Use the imgur image upload API as documented at https://api.imgur.com/endpoints/image
        val requestBody: RequestBody =
            MultipartBody
                .Builder(boundary)
                .setType(MultipartBody.FORM)
                .addPart(
                    headersOf("Content-Disposition", "form-data; name=\"report\""),
                    reportJson.toString().toRequestBody(mimeType),
                ).addFormDataPart(
                    "screenshot",
                    screenshotFilename,
                    File(screenshotFilename).asRequestBody(MEDIA_TYPE_PNG),
                ).addFormDataPart(
                    "zip",
                    reportFilename,
                    File(reportFilename).asRequestBody(MEDIA_TYPE_ZIP),
                ).build()
        val buffer = Buffer()
        val contentLength: String
        contentLength =
            try {
                requestBody.writeTo(buffer)
                Log.d(LOG_TAG, buffer.toString())
                buffer.size.toString()
            } catch (e: IOException) {
                e.printStackTrace()
                withContext(Dispatchers.Main) {
                    mainActivity.reportStatus = Yabause.REPORT_STATE_FAIL_CONNECTION
                    mainActivity.dismissDialog()
                }
                return -1
            } finally {
                buffer.close()
            }
        try {
            val request =
                Request
                    .Builder()
                    .addHeader("Content-Length", contentLength)
                    .url(uri + "v2/reports/")
                    .post(requestBody)
                    .build()
            val response = client!!.newCall(request).execute()
            if (!response.isSuccessful) {
                throw IOException("Unexpected code $response")
            } else {
                val rootObject = JSONObject(response.body!!.string())
                if (rootObject.getBoolean("result") == true) {
                    mainActivity.reportStatus = Yabause.REPORT_STATE_SUCCESS
                } else {
                    val returnCode = rootObject.getInt("code")
                    mainActivity.reportStatus = returnCode
                }
            }
            var tmp = File(screenshotFilename)
            tmp.delete()
            tmp = File(reportFilename)
            tmp.delete()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "message", e)
            withContext(Dispatchers.Main) {
                mainActivity.reportStatus = Yabause.REPORT_STATE_FAIL_CONNECTION
                mainActivity.dismissDialog()
            }
            return -1
        }
        withContext(Dispatchers.Main) {
            mainActivity.dismissDialog()
        }
        return 0
    }

    companion object {
        private const val LOG_TAG = "AsyncReportv2"
        private const val IMGUR_CLIENT_ID = "..."
        private val MEDIA_TYPE_PNG = "image/png".toMediaTypeOrNull()
        private val MEDIA_TYPE_ZIP = "application/x-zip-compressed".toMediaTypeOrNull()
    }

    // コンストラクター
    init {
        client =
            OkHttpClient
                .Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(10, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .authenticator(
                    Authenticator { _, response ->
                        if (responseCount(response) >= 3) {
                            return@Authenticator null // If we've failed 3 times, give up. - in real life, never give up!!
                        }
                        val credential =
                            Credentials.basic(
                                mainActivity.getString(R.string.basic_user),
                                mainActivity.getString(R.string.basic_password),
                            )
                        response.request
                            .newBuilder()
                            .header("Authorization", credential)
                            .build()
                    },
                ).build()
    }
}
