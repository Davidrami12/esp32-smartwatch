import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}
val aiLocal = Properties().apply {
    val file = rootProject.file("ai.local.properties")
    if (file.exists()) file.inputStream().use { load(it) }
}
val firebaseWebKey = aiLocal.getProperty("firebase.webApiKey", "").trim()
require(firebaseWebKey.isEmpty() || firebaseWebKey.matches(Regex("AIza[A-Za-z0-9_-]{35}"))) {
    "ai.local.properties must contain a Firebase public Web API key, never an OpenAI key"
}
android {
    namespace = "dev.esp32.smartwatch"
    compileSdk = 36
    defaultConfig {
        applicationId = "dev.esp32.smartwatch"
        minSdk = 33
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        buildConfigField("String", "FIREBASE_WEB_API_KEY", "\"$firebaseWebKey\"")
        // Unpaid transport validation: -PvoiceTransferOnly=true blocks voice HTTP.
        buildConfigField("boolean", "VOICE_TRANSFER_ONLY", providers.gradleProperty("voiceTransferOnly").getOrElse("false").also {
            require(it == "true" || it == "false") { "voiceTransferOnly must be true or false" }
        })
        // Offline A/B builds: -PvoiceNotify=false keeps the legacy pull transport.
        buildConfigField("boolean", "VOICE_NOTIFY_ENABLED", providers.gradleProperty("voiceNotify").getOrElse("false").also {
            require(it == "true" || it == "false") { "voiceNotify must be true or false" }
        })
    }
    buildFeatures { compose = true; buildConfig = true }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }
dependencies {
    implementation("androidx.health.connect:connect-client:1.1.0")
    implementation(platform("androidx.compose:compose-bom:2025.12.01"))
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.activity:activity-compose:1.12.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.10.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.10.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20250517")
}
