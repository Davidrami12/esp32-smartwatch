package dev.esp32.smartwatch.ai

import android.app.Activity
import android.os.Bundle
import android.text.InputType
import android.view.WindowManager
import android.widget.*
import dev.esp32.smartwatch.BuildConfig

/** Non-exported configuration. Password/tokens never persisted; refresh is memory-only. */
class VoiceAiSettingsActivity : Activity() {
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
        layout.addView(TextView(this).apply { text = "AI Assistant\nFirebase project: ${VoiceAiConfig.PROJECT}\nTokens refresh in memory. Re-login after process restart. Signing in and verifying backend access automatically enables paid AI for new hold-to-talk questions. Requests may incur charges. Switch it off below or sign out to stop new requests." })
        val url = field("HTTPS backend URL",VoiceAiConfig.BACKEND).apply { isEnabled = false }
        val key = field("Firebase public Web API key",BuildConfig.FIREBASE_WEB_API_KEY.ifBlank { preferences.getString("key","") ?: "" })
        val email = field("Firebase account email")
        val password = field("Password",password=true)
        val status = TextView(this).apply { text = if (VoiceAiClient.configured()) "Signed in" else "Not configured / signed out" }; layout.addView(status)
        val paid = CheckBox(this).apply { text = "Allow paid AI requests after hold-to-talk"; isChecked = VoiceAiClient.paidRequestsEnabled
            setOnCheckedChangeListener { _, checked -> VoiceAiClient.enablePaidRequests(checked); if (checked && !VoiceAiClient.configured()) { isChecked = false; status.text = "Sign in first" } }
        }; layout.addView(paid)
        if (BuildConfig.VOICE_TRANSFER_ONLY) {
            paid.isChecked = false; paid.isEnabled = false
            layout.addView(TextView(this).apply { text = "TRANSFER TEST MODE: audio CRC is verified, then discarded. No voice request reaches Cloud Run/OpenAI." })
        }
        layout.addView(Button(this).apply { text = "Sign in & enable paid AI"; setOnClickListener {
            val target = url.text.toString().trim(); val publicKey = key.text.toString().trim()
            isEnabled = false; status.text = "Signing in…"
            paid.isChecked = false
            VoiceAiClient.signIn(target,publicKey,email.text.toString(),password.text.toString()) { ok, message -> runOnUiThread {
                password.text.clear(); isEnabled = true
                status.text = message
                paid.isChecked = VoiceAiClient.paidRequestsEnabled
                if (ok) preferences.edit().putString("url",target).putString("key",publicKey).apply()
            } }
        } })
        layout.addView(Button(this).apply { text = "Sign out"; setOnClickListener { VoiceAiClient.signOut(); paid.isChecked = false; password.text.clear(); status.text = "Signed out" } })
        setContentView(ScrollView(this).apply { addView(layout) })
    }
}
