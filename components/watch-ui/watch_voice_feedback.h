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
    if (!code) return "Error de conexión";
    if (!strcmp(code,"AI_DISABLED")) return "AI desactivada";
    if (!strcmp(code,"AI_SIGN_IN")) return "Inicia sesión en el móvil";
    if (!strcmp(code,"AI_TEST_MODE")) return "Prueba completada · AI desactivada";
    if (!strcmp(code,"AI_TIMEOUT")) return "Tiempo de espera agotado";
    if (!strcmp(code,"AI_LIMIT")) return "Límite de consultas alcanzado";
    if (!strcmp(code,"AI_SERVICE")) return "Servicio AI no disponible";
    if (!strcmp(code,"AI_CANCELLED")) return "Consulta cancelada";
    return "Error de conexión";
}
inline const char *hint(const char *message) {
    if (!message) return "Mantén pulsado para intentarlo de nuevo.";
    if (strstr(message,"Prueba completada")) return "Audio verificado. No se ha consultado AI.";
    if (strstr(message,"AI desactivada")) return "Puedes activarla desde AI Assistant en el móvil.";
    if (strstr(message,"sesión")) return "Abre AI Assistant en el móvil e inicia sesión.";
    if (strstr(message,"desconectado")) return "Acerca el móvil y comprueba la app companion.";
    if (strstr(message,"demasiado corta")) return "Mantén pulsado un poco más antes de soltar.";
    if (strstr(message,"micrófono") || strstr(message,"Micrófono") || strstr(message,"audio")) return "Suelta el botón y vuelve a grabar.";
    return "Comprueba el móvil y vuelve a intentarlo.";
}
}
