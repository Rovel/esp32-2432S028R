#include "wifi.h"
#include "../home/home.h"
#include "components/keyboard/keyboard.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

static const char *TAG = "wifi";

// WiFi screen globals
lv_obj_t *wifi_screen = NULL;
lv_obj_t *wifi_list = NULL;
lv_obj_t *wifi_status_label = NULL;
lv_obj_t *wifi_enable_switch = NULL;
lv_obj_t *scan_btn = NULL;

// WiFi state
static bool wifi_initialized = false;
static bool wifi_enabled = false;
static bool scanning = false;
static bool ui_update_needed = false;

// WiFi scan results
static wifi_ap_record_t ap_records[20];
static uint16_t ap_count = 0;

// LVGL timer for UI updates
static lv_timer_t *ui_update_timer = NULL;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void update_wifi_status(void);
static void update_network_list(void);
static void wifi_activity_event_handler(lv_event_t *e);
static void switch_event_handler(lv_event_t *e);
static void scan_btn_event_handler(lv_event_t *e);
static void back_btn_event_handler(lv_event_t *e);
static void network_item_event_handler(lv_event_t *e);
static void ui_update_timer_callback(lv_timer_t *timer);

// WiFi initialization
esp_err_t wifi_init(void) {
    if (wifi_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting WiFi initialization...");
    
    // Add a delay to ensure system is stable
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Netif initialized");

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Event loop created");

    // Create WiFi station interface
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create default WiFi station");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WiFi station interface created");

    // Add another delay before WiFi initialization
    vTaskDelay(pdMS_TO_TICKS(200));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi driver initialized");

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi event handler registered");

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi mode set to station");

    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized successfully");
    return ESP_OK;
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WiFi scan completed");
                scanning = false;
                ap_count = sizeof(ap_records) / sizeof(ap_records[0]);
                esp_wifi_scan_get_ap_records(&ap_count, ap_records);
                ui_update_needed = true;
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                wifi_enabled = true;
                ui_update_needed = true;
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "WiFi station stopped");
                wifi_enabled = false;
                scanning = false;
                ap_count = 0;
                ui_update_needed = true;
                break;
            default:
                break;
        }
    }
}

// UI update timer callback - runs in LVGL context
static void ui_update_timer_callback(lv_timer_t *timer) {
    if (ui_update_needed) {
        ui_update_needed = false;
        update_wifi_status();
        update_network_list();
        ESP_LOGD(TAG, "UI updated from timer callback");
    }
}

// Update WiFi status display
static void update_wifi_status(void) {
    if (!wifi_status_label) return;

    char status_text[100];
    if (!wifi_initialized) {
        snprintf(status_text, sizeof(status_text), "WiFi: Not initialized");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x888888), 0);
    } else if (!wifi_enabled) {
        snprintf(status_text, sizeof(status_text), "WiFi: Disabled");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6666), 0);
    } else if (scanning) {
        snprintf(status_text, sizeof(status_text), "WiFi: Scanning...");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFFF66), 0);
    } else {
        snprintf(status_text, sizeof(status_text), "WiFi: Ready (%d networks)", ap_count);
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x66FF66), 0);
    }
    
    lv_label_set_text(wifi_status_label, status_text);
}

// Update network list
static void update_network_list(void) {
    if (!wifi_list) return;

    // Clear existing list items
    lv_obj_clean(wifi_list);

    if (!wifi_enabled || ap_count == 0) {
        if (!wifi_enabled) {
            lv_obj_t *no_wifi_item = lv_list_add_text(wifi_list, "WiFi is disabled");
            lv_obj_set_style_text_color(no_wifi_item, lv_color_hex(0x888888), 0);
        } else {
            lv_obj_t *no_networks_item = lv_list_add_text(wifi_list, "No networks found");
            lv_obj_set_style_text_color(no_networks_item, lv_color_hex(0x888888), 0);
        }
        return;
    }

    // Add network items
    for (int i = 0; i < ap_count; i++) {
        char network_text[64];
        char *security = "";
        
        if (ap_records[i].authmode != WIFI_AUTH_OPEN) {
            security = " 🔒";
        }
        
        snprintf(network_text, sizeof(network_text), "%s%s", 
                (char*)ap_records[i].ssid, security);
        
        lv_obj_t *network_item = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, network_text);
        lv_obj_set_style_text_color(network_item, lv_color_white(), 0);
        lv_obj_add_event_cb(network_item, network_item_event_handler, LV_EVENT_CLICKED, &ap_records[i]);
        
        // Set signal strength color
        if (ap_records[i].rssi > -50) {
            lv_obj_set_style_bg_color(network_item, lv_color_hex(0x006600), LV_STATE_DEFAULT);
        } else if (ap_records[i].rssi > -70) {
            lv_obj_set_style_bg_color(network_item, lv_color_hex(0x666600), LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(network_item, lv_color_hex(0x664400), LV_STATE_DEFAULT);
        }
    }
}

// Network item click handler
static void network_item_event_handler(lv_event_t *e) {
    wifi_ap_record_t *ap_record = (wifi_ap_record_t *)lv_event_get_user_data(e);
    if (!ap_record) return;

    ESP_LOGI(TAG, "Selected network: %s (RSSI: %d, Auth: %d)", 
             ap_record->ssid, ap_record->rssi, ap_record->authmode);
    
    // TODO: Implement connection dialog with password input using keyboard component
    // For now, just log the selection
}

// WiFi enable/disable switch handler
static void switch_event_handler(lv_event_t *e) {
    bool enabled = lv_obj_has_state(wifi_enable_switch, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "Switch event: WiFi %s", enabled ? "enabled" : "disabled");
    
    if (enabled) {
        // Initialize WiFi when user tries to enable it
        if (!wifi_initialized) {
            ESP_LOGI(TAG, "Initializing WiFi...");
            if (wifi_init() != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize WiFi");
                // Revert switch state
                lv_obj_clear_state(wifi_enable_switch, LV_STATE_CHECKED);
                return;
            }
        }
        wifi_enable();
    } else {
        wifi_disable();
    }
    
    // Force immediate UI update
    ui_update_needed = true;
}

// Scan button event handler
static void scan_btn_event_handler(lv_event_t *e) {
    ESP_LOGI(TAG, "Scan button clicked - WiFi enabled: %s, scanning: %s", 
             wifi_enabled ? "yes" : "no", scanning ? "yes" : "no");
    
    if (wifi_enabled && !scanning) {
        wifi_start_scan();
    } else if (!wifi_enabled) {
        ESP_LOGW(TAG, "Cannot scan - WiFi not enabled");
    } else if (scanning) {
        ESP_LOGW(TAG, "Cannot scan - scan already in progress");
    }
}

// Back button event handler
static void back_btn_event_handler(lv_event_t *e) {
    ESP_LOGI(TAG, "Back button clicked, returning to home");
    switch_to_home();
}

// WiFi activity event handler
static void wifi_activity_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // Reset home timer on any user activity
    switch(code) {
        case LV_EVENT_PRESSED:
        case LV_EVENT_CLICKED:
        case LV_EVENT_LONG_PRESSED:
        case LV_EVENT_KEY:
            home_reset_idle_timer();
            break;
        default:
            break;
    }
}

// Create WiFi settings screen
void create_wifi_screen(void) {
    ESP_LOGI(TAG, "Creating WiFi settings screen");
    
    // Don't initialize WiFi here - do it lazily when needed
    
    // Create a new screen for WiFi settings
    wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(0x1C1C1C), 0);
    
    // Add activity event handler
    lv_obj_add_event_cb(wifi_screen, wifi_activity_event_handler, 
                       LV_EVENT_PRESSED | LV_EVENT_CLICKED | LV_EVENT_LONG_PRESSED, NULL);
    
    // Title
    lv_obj_t *title = lv_label_create(wifi_screen);
    lv_label_set_text(title, "WiFi Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Back button
    lv_obj_t *back_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x666666), 0);
    lv_obj_add_event_cb(back_btn, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    
    // WiFi enable/disable switch
    lv_obj_t *switch_container = lv_obj_create(wifi_screen);
    lv_obj_set_size(switch_container, lv_pct(90), 50);
    lv_obj_align(switch_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(switch_container, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_border_width(switch_container, 1, 0);
    lv_obj_set_style_border_color(switch_container, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(switch_container, 5, 0);
    
    lv_obj_t *switch_label = lv_label_create(switch_container);
    lv_label_set_text(switch_label, "Enable WiFi");
    lv_obj_set_style_text_color(switch_label, lv_color_white(), 0);
    lv_obj_align(switch_label, LV_ALIGN_LEFT_MID, 10, 0);
    
    wifi_enable_switch = lv_switch_create(switch_container);
    lv_obj_align(wifi_enable_switch, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(wifi_enable_switch, switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // WiFi status label
    wifi_status_label = lv_label_create(wifi_screen);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 110);
    
    // Scan button
    scan_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(scan_btn, 100, 35);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -10, 110);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x0066CC), 0);
    lv_obj_add_event_cb(scan_btn, scan_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_center(scan_label);
    
    // Network list
    wifi_list = lv_list_create(wifi_screen);
    lv_obj_set_size(wifi_list, lv_pct(90), 130);
    lv_obj_align(wifi_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_border_color(wifi_list, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(wifi_list, 1, 0);
    lv_obj_set_style_radius(wifi_list, 5, 0);
    
    // Update initial status
    update_wifi_status();
    update_network_list();
    
    // Create UI update timer
    ui_update_timer = lv_timer_create(ui_update_timer_callback, 250, NULL);
    if (!ui_update_timer) {
        ESP_LOGE(TAG, "Failed to create UI update timer");
    } else {
        ESP_LOGI(TAG, "UI update timer created");
    }
    
    ESP_LOGI(TAG, "WiFi settings screen created successfully");
}

// WiFi control functions
void wifi_enable(void) {
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return;
    }
    
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi enabled");
    }
}

void wifi_disable(void) {
    if (!wifi_initialized) {
        return;
    }
    
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi disabled");
    }
}

bool wifi_is_enabled(void) {
    return wifi_enabled;
}

void wifi_start_scan(void) {
    if (!wifi_enabled) {
        ESP_LOGW(TAG, "WiFi not enabled, cannot scan");
        return;
    }
    
    if (scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return;
    }
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret == ESP_OK) {
        scanning = true;
        ESP_LOGI(TAG, "WiFi scan started");
        ui_update_needed = true;  // Trigger UI update
    } else {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
    }
}

void wifi_stop_scan(void) {
    if (scanning) {
        esp_wifi_scan_stop();
        scanning = false;
        ESP_LOGI(TAG, "WiFi scan stopped");
    }
}

// Switch to WiFi screen
void switch_to_wifi(void) {
    ESP_LOGI(TAG, "Switching to WiFi settings screen");
    if (wifi_screen) {
        lv_screen_load(wifi_screen);
        home_reset_idle_timer(); // Reset the home idle timer
        ESP_LOGI(TAG, "WiFi settings screen loaded");
    } else {
        ESP_LOGW(TAG, "WiFi screen not created yet");
    }
}

// Get WiFi screen object
lv_obj_t* get_wifi_screen(void) {
    return wifi_screen;
}
