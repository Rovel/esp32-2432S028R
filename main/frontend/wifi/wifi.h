#ifndef WIFI_H
#define WIFI_H

#include <lvgl.h>
#include <esp_wifi.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi screen management functions
 */

/**
 * @brief Initialize WiFi functionality
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_init(void);

/**
 * @brief Create the WiFi settings screen
 */
void create_wifi_screen(void);

/**
 * @brief Switch to WiFi settings screen
 */
void switch_to_wifi(void);

/**
 * @brief Get WiFi screen object (for external access)
 * @return lv_obj_t* WiFi screen object
 */
lv_obj_t* get_wifi_screen(void);

/**
 * @brief Start WiFi scan for available networks
 */
void wifi_start_scan(void);

/**
 * @brief Stop WiFi scanning
 */
void wifi_stop_scan(void);

/**
 * @brief Enable WiFi station mode
 */
void wifi_enable(void);

/**
 * @brief Disable WiFi
 */
void wifi_disable(void);

/**
 * @brief Check if WiFi is enabled
 * @return true if enabled, false otherwise
 */
bool wifi_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_H */
