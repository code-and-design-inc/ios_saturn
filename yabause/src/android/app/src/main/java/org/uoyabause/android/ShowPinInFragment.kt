package org.uoyabause.android

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser
import com.google.android.gms.auth.api.signin.GoogleSignIn
import com.google.android.gms.auth.api.signin.GoogleSignInOptions
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.devmiyax.yabasanshiro.R
import org.json.JSONObject

class ShowPinInFragment : DialogFragment() {
    private var isRequestInProgress = false
    private var timeoutJob: Job? = null
    lateinit var rootView: View
    private var pinNumber: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val user = FirebaseAuth.getInstance().currentUser
        if (user == null) {
            return
        } else {
            getIpdToken(user)
        }
    }

    val timerDuration = 30

    fun getIpdToken(user: FirebaseUser) {
        val activity = activity ?: return
        val gso = GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
            .requestIdToken(getString(org.devmiyax.yabasanshiro.R.string.default_web_client_id))
            .requestEmail()
            .build()
        val client = GoogleSignIn.getClient(activity, gso)
        client.silentSignIn().addOnSuccessListener { account ->
            val googleIdToken = account.idToken
            if (googleIdToken != null) {
                fetchPinin(googleIdToken)
            } else {
                Log.e(javaClass.name, "Google ID token is null from silentSignIn")
            }
        }.addOnFailureListener { e ->
            Log.e(javaClass.name, "silentSignIn failed: ${e.message}")
        }
    }

    private fun fetchPinin(token: String) {
        if (isRequestInProgress) return
        isRequestInProgress = true
        pinNumber = null

        lifecycleScope.launch {
            try {
                val result = getPinin(token)
                val tv = rootView.findViewById<TextView>(R.id.pinin)
                pinNumber = result
                tv.text = result

                val message = rootView.findViewById<TextView>(R.id.message)
                message.text = getString(R.string.enter_pinin)

                // Start timeout after successful PIN retrieval
                startTimeout()
            } catch (e: Exception) {
                val tv = rootView.findViewById<TextView>(R.id.pinin)
                tv.text = "Error:${e.localizedMessage}"
                pinNumber = null
            } finally {
                isRequestInProgress = false
            }
        }
    }

    private fun startTimeout() {
        timeoutJob?.cancel()
        timeoutJob =
            lifecycleScope.launch {
                delay(60_000L)
                Log.d(javaClass.name, "&&&& on timer")
                dismiss()
            }
    }

    private suspend fun removePinin(pin: String): Result<Unit> =
        withContext(Dispatchers.IO) {
            try {
                val url = requireActivity().getString(R.string.url_getTokenAndDelete)
                val key = requireActivity().getString(R.string.key_getTokenAndDelete)
                if (key == "") {
                    return@withContext Result.failure(Exception("NO!"))
                }
                val client = OkHttpClient()
                val mime = "application/json; charset=utf-8".toMediaTypeOrNull()
                val requestBody = "{ \"key\":\"$pin\" }".toRequestBody(mime)
                val request: Request =
                    Request
                        .Builder()
                        .url(url)
                        .post(requestBody)
                        .addHeader("x-api-key", key)
                        .addHeader("Content-Type", "application/json")
                        .build()
                val response = client.newCall(request).execute()
                if (response.isSuccessful) {
                    Result.success(Unit)
                } else {
                    Result.failure(Exception(response.message))
                }
            } catch (e: Exception) {
                Result.failure(e)
            }
        }

    private suspend fun getPinin(token: String): String =
        withContext(Dispatchers.IO) {
            val url = requireActivity().getString(R.string.url_getLoginPinIn)
            val key = requireActivity().getString(R.string.key_getLoginPinIn)
            if (key == "") {
                throw Exception("NO!")
            }
            val client = OkHttpClient()
            val mime = "application/json; charset=utf-8".toMediaTypeOrNull()
            val requestBody = "{ \"token\":\"$token\" }".toRequestBody(mime)
            val request: Request =
                Request
                    .Builder()
                    .url(url)
                    .post(requestBody)
                    .addHeader("x-api-key", key)
                    .addHeader("Content-Type", "application/json")
                    .build()
            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val jsonData: String = response.body!!.string()
                Log.d(javaClass.name, jsonData)
                val jObject = JSONObject(jsonData)
                jObject.getString("pinin")
            } else {
                Log.d(javaClass.name, response.message)
                throw Exception(response.message)
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        rootView = inflater.inflate(R.layout.fragment_show_pin_in, container, false)
        val cancel = rootView.findViewById<Button>(R.id.cancel)
        cancel.setOnClickListener {
            if (pinNumber != null) {
                lifecycleScope.launch {
                    removePinin(pinNumber!!)
                    pinNumber = null
                    dismiss()
                }
            } else {
                dismiss()
            }
        }
        return rootView
    }

    override fun onDestroy() {
        timeoutJob?.cancel()
        if (pinNumber != null) {
            lifecycleScope.launch {
                removePinin(pinNumber!!)
            }
        }
        super.onDestroy()
    }

    override fun onResume() {
        super.onResume()
        val window = dialog?.window ?: return
        val params = window.attributes
        params.width = 600
        params.height = 600
        window.attributes = params
    }

    companion object {
        @JvmStatic
        fun newInstance() = ShowPinInFragment()
    }
}
