package dev.esp32.smartwatch.ai

import android.app.Activity
import android.os.Bundle
import android.text.InputType
import android.view.WindowManager
import android.widget.*
import dev.esp32.smartwatch.BuildConfig

/** Non-exported configuration. Password is never persisted; refresh uses Android Keystore. */
class VoiceAiSettingsActivity : Activity() {
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())
    private var refreshUi: (() -> Unit)? = null
    private val refresh = object : Runnable {
        override fun run() { refreshUi?.invoke(); handler.postDelayed(this,500) }
    }
    override fun onResume() { super.onResume(); VoiceAiClient.ensureAutomaticSignIn(); handler.post(refresh) }
    override fun onPause() { handler.removeCallbacks(refresh); super.onPause() }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        val layout = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(24,40,24,24) }
        val preferences = getSharedPreferences("voice_ai",MODE_PRIVATE)
        fun field(hint: String, initial: String = "", password: Boolean = false) = EditText(this).apply {
            this.hint = hint; setText(initial); isSaveEnabled = false
            inputType = if (password) InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD else InputType.TYPE_CLASS_TEXT
            layout.addView(this)
        }
        layout.addView(TextView(this).apply { text = "AI Assistant\nFirebase project: ${VoiceAiConfig.PROJECT}\nSign in once. Your renewable session is encrypted using Android Keystore and restored automatically. Passwords are never saved. Paid hold-to-talk AI is ON by default and may incur charges. Switching it off is remembered across restarts and reconnections. Sign out removes the saved session." })
        val url = field("HTTPS backend URL",VoiceAiConfig.BACKEND).apply { isEnabled = false }
        val key = field("Firebase public Web API key",BuildConfig.FIREBASE_WEB_API_KEY.ifBlank { preferences.getString("key","") ?: "" })
        val email = field("Firebase account email")
        val password = field("Password",password=true)
        val status = TextView(this); layout.addView(status)
        var updatingUi = false
        val paid = CheckBox(this).apply { text = "Enable paid AI automatically after hold-to-talk"; isChecked = VoiceAiClient.paidRequestsRequested
            setOnCheckedChangeListener { _, checked -> if (!updatingUi) VoiceAiClient.enablePaidRequests(checked) }
        }; layout.addView(paid)
        refreshUi = {
            updatingUi = true
            paid.isChecked = !BuildConfig.VOICE_TRANSFER_ONLY && VoiceAiClient.paidRequestsRequested
            updatingUi = false
            status.text = "${VoiceAiClient.sessionStatus}\nPaid AI: ${if (VoiceAiClient.paidRequestsEnabled) "ON" else "OFF"}"
        }
        if (BuildConfig.VOICE_TRANSFER_ONLY) {
            updatingUi = true; paid.isChecked = false; updatingUi = false; paid.isEnabled = false
            layout.addView(TextView(this).apply { text = "TRANSFER TEST MODE: audio CRC is verified, then discarded. No voice request reaches Cloud Run/OpenAI." })
        }
        layout.addView(Button(this).apply { text = "Sign in / save automatic session"; setOnClickListener {
            val target = url.text.toString().trim(); val publicKey = key.text.toString().trim()
            isEnabled = false; status.text = "Signing in…"
            VoiceAiClient.signIn(target,publicKey,email.text.toString(),password.text.toString()) { ok, message -> runOnUiThread {
                password.text.clear(); isEnabled = true
                status.text = message
                refreshUi?.invoke()
                if (ok) preferences.edit().putString("url",target).putString("key",publicKey).apply()
            } }
        } })
        layout.addView(Button(this).apply { text = "Sign out / remove saved session"; setOnClickListener { VoiceAiClient.signOut(); password.text.clear(); refreshUi?.invoke() } })
        setContentView(ScrollView(this).apply { addView(layout) })
    }
}
