#pragma once
#include <stdint.h>
#include <string.h>

// Shared presentation policy. Percent is the phone-confirmed prefix, never
// queued radio bytes or elapsed time; incomplete transfers cannot show 100%.
namespace watch_voice_feedback {
inline uint8_t percent(uint32_t confirmed, uint32_t total) {
    if (!total) return 0;
    return confirmed >= total ? 100 : static_cast<uint8_t>(uint64_t(confirmed)*100/total);
}
inline const char *message(const char *code) {
    if (!code) return "Connection error";
    if (!strcmp(code,"AI_DISABLED")) return "AI disabled";
    if (!strcmp(code,"AI_SIGN_IN")) return "Sign in on your phone";
    if (!strcmp(code,"AI_TEST_MODE")) return "Test complete · AI disabled";
    if (!strcmp(code,"AI_TIMEOUT")) return "Request timed out";
    if (!strcmp(code,"AI_LIMIT")) return "Request limit reached";
    if (!strcmp(code,"AI_SERVICE")) return "AI service unavailable";
    if (!strcmp(code,"AI_CANCELLED")) return "Request cancelled";
    return "Connection error";
}
inline const char *hint(const char *message) {
    if (!message) return "Hold to try again.";
    if (strstr(message,"Test complete")) return "Audio verified. No AI request was sent.";
    if (strstr(message,"AI disabled")) return "Enable AI Assistant on your phone.";
    if (strstr(message,"Sign in")) return "Open AI Assistant on your phone and sign in.";
    if (strstr(message,"disconnected")) return "Bring your phone closer and check the companion app.";
    if (strstr(message,"too short")) return "Hold a little longer before releasing.";
    if (strstr(message,"microphone") || strstr(message,"Microphone") || strstr(message,"audio") || strstr(message,"Audio")) return "Release the button and record again.";
    return "Check your phone and try again.";
}
}
