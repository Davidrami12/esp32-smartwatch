#include "watch_display_start.h"
#include "bsp/esp-bsp.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
namespace { bool display_starting = false, startup_locked = false; }

// Port init launches the refresh task before the BSP has finished display/event/
// input registration. Hold its recursive lock across the entire remaining BSP
// startup, not just individual API calls. No managed dependency is modified.
extern "C" esp_err_t __real_lvgl_port_init(const lvgl_port_cfg_t *config);
extern "C" esp_err_t __wrap_lvgl_port_init(const lvgl_port_cfg_t *config)
{
    const esp_err_t result = __real_lvgl_port_init(config);
    if (display_starting && result == ESP_OK) startup_locked = lvgl_port_lock(0);
    return result;
}
// This AMOLED uses a QSPI SH8601 panel, not an RGB panel. The bundled BSP calls
// add_disp_rgb, whose RGB callback registration casts the SPI panel as RGB and
// can overwrite adjacent heap metadata. Use the supported SPI/IO-backed path.
extern "C" lv_display_t *__wrap_lvgl_port_add_disp_rgb(const lvgl_port_display_cfg_t *config,
                                                       const lvgl_port_display_rgb_cfg_t *)
{
    return lvgl_port_add_disp(config);
}
lv_display_t *watch_display_start()
{
    display_starting = true;
    lv_display_t *display = bsp_display_start();
    display_starting = false;
    if (startup_locked) { lvgl_port_unlock(); startup_locked = false; }
    return display;
}
