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

// Menu builder variants for different item types
typedef enum {
    WIFI_MENU_ITEM_BUILDER_VARIANT_1,
    WIFI_MENU_ITEM_BUILDER_VARIANT_2
} wifi_menu_builder_variant_t;

// WiFi screen globals
lv_obj_t *wifi_screen = NULL;
lv_obj_t *wifi_menu = NULL;
lv_obj_t *root_page = NULL;

// Menu pages
lv_obj_t *general_page = NULL;
lv_obj_t *networks_page = NULL;
lv_obj_t *settings_page = NULL;
lv_obj_t *mode_settings_page = NULL;

// WiFi state
static bool wifi_initialized = false;
static bool wifi_enabled = false;
static bool scanning = false;
static bool ui_update_needed = false;
static uint32_t wifi_start_time = 0;
static wifi_mode_t current_wifi_mode = WIFI_MODE_STA;

// WiFi scan results
static wifi_ap_record_t ap_records[20];
static uint16_t ap_count = 0;

// UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *enable_switch = NULL;
static lv_obj_t *networks_list = NULL;
static lv_obj_t *scan_btn = NULL;
static lv_obj_t *mode_dropdown = NULL;

// LVGL timer for UI updates
static lv_timer_t *ui_update_timer = NULL;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void update_status_display(void);
static void update_network_list(void);
static void wifi_activity_event_handler(lv_event_t *e);
static void ui_update_timer_callback(lv_timer_t *timer);
static void back_btn_event_handler(lv_event_t *e);
static void enable_switch_event_handler(lv_event_t *e);
static void scan_btn_event_handler(lv_event_t *e);
static void network_item_event_handler(lv_event_t *e);
static void mode_dropdown_event_handler(lv_event_t *e);

// Menu creation helpers
static lv_obj_t *create_text_item(lv_obj_t *parent, const char *icon, const char *txt, wifi_menu_builder_variant_t variant);
static lv_obj_t *create_switch_item(lv_obj_t *parent, const char *icon, const char *txt, bool checked);
static lv_obj_t *create_button_item(lv_obj_t *parent, const char *icon, const char *txt);
static lv_obj_t *create_dropdown_item(lv_obj_t *parent, const char *icon, const char *txt, const char *options);

// Page creation functions
static void create_general_page(void);
static void create_networks_page(void);
static void create_settings_page(void);
static void create_mode_settings_page(void);
static void create_root_page(void);

// WiFi initialization
esp_err_t wifi_init(void) {
    if (wifi_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting WiFi initialization...");
    
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create default WiFi station");
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_mode(current_wifi_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }

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
                wifi_start_time = xTaskGetTickCount();
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

// UI update timer callback
static void ui_update_timer_callback(lv_timer_t *timer) {
    if (ui_update_needed) {
        ui_update_needed = false;
        update_status_display();
        update_network_list();
    }
}

// Helper function to create text items
static lv_obj_t *create_text_item(lv_obj_t *parent, const char *icon, const char *txt, wifi_menu_builder_variant_t variant) {
    lv_obj_t *obj = lv_menu_cont_create(parent);

    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    if (icon) {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
    }

    if (txt) {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    if (variant == WIFI_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt) {
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_swap(img, label);
    }

    return obj;
}

// Helper function to create switch items
static lv_obj_t *create_switch_item(lv_obj_t *parent, const char *icon, const char *txt, bool checked) {
    lv_obj_t *obj = create_text_item(parent, icon, txt, WIFI_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sw = lv_switch_create(obj);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    return obj;
}

// Helper function to create button items
static lv_obj_t *create_button_item(lv_obj_t *parent, const char *icon, const char *txt) {
    return create_text_item(parent, icon, txt, WIFI_MENU_ITEM_BUILDER_VARIANT_1);
}

// Helper function to create dropdown items
static lv_obj_t *create_dropdown_item(lv_obj_t *parent, const char *icon, const char *txt, const char *options) {
    lv_obj_t *obj = create_text_item(parent, icon, txt, WIFI_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *dropdown = lv_dropdown_create(obj);
    lv_dropdown_set_options(dropdown, options);
    lv_obj_set_flex_grow(dropdown, 1);

    if (icon == NULL) {
        lv_obj_add_flag(dropdown, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }

    return obj;
}

// Update status display
static void update_status_display(void) {
    if (!status_label) return;

    char status_text[100];
    if (!wifi_initialized) {
        snprintf(status_text, sizeof(status_text), "Not initialized");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x888888), 0);
    } else if (!wifi_enabled) {
        snprintf(status_text, sizeof(status_text), "Disabled");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF6666), 0);
    } else if (scanning) {
        snprintf(status_text, sizeof(status_text), "Scanning...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFF66), 0);
    } else {
        snprintf(status_text, sizeof(status_text), "Ready (%d networks)", ap_count);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x66FF66), 0);
    }
    
    lv_label_set_text(status_label, status_text);

    // Update enable switch state
    if (enable_switch) {
        if (wifi_enabled) {
            lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(enable_switch, LV_STATE_CHECKED);
        }
    }
}

// Update network list
static void update_network_list(void) {
    if (!networks_list) return;

    // Clear existing list items
    lv_obj_clean(networks_list);

    if (!wifi_enabled || ap_count == 0) {
        if (!wifi_enabled) {
            lv_obj_t *no_wifi_item = lv_list_add_text(networks_list, "WiFi is disabled");
            lv_obj_set_style_text_color(no_wifi_item, lv_color_hex(0x888888), 0);
        } else {
            lv_obj_t *no_networks_item = lv_list_add_text(networks_list, "No networks found - Click scan");
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
        
        lv_obj_t *network_item = lv_list_add_btn(networks_list, LV_SYMBOL_WIFI, network_text);
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

// Event handlers
static void wifi_activity_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
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

static void back_btn_event_handler(lv_event_t *e) {
    ESP_LOGI(TAG, "Back button clicked, returning to home");
    switch_to_home();
}

static void enable_switch_event_handler(lv_event_t *e) {
    bool enabled = lv_obj_has_state(enable_switch, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "WiFi enable switch: %s", enabled ? "enabled" : "disabled");
    
    if (enabled) {
        if (!wifi_initialized) {
            ESP_LOGI(TAG, "Initializing WiFi...");
            if (wifi_init() != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize WiFi");
                lv_obj_clear_state(enable_switch, LV_STATE_CHECKED);
                return;
            }
        }
        wifi_enable();
    } else {
        wifi_disable();
    }
    
    ui_update_needed = true;
}

static void scan_btn_event_handler(lv_event_t *e) {
    ESP_LOGI(TAG, "Scan button clicked");
    
    if (wifi_enabled && !scanning) {
        wifi_start_scan();
    } else if (!wifi_enabled) {
        ESP_LOGW(TAG, "Cannot scan - WiFi not enabled");
    } else if (scanning) {
        ESP_LOGW(TAG, "Cannot scan - scan already in progress");
    }
}

static void network_item_event_handler(lv_event_t *e) {
    wifi_ap_record_t *ap_record = (wifi_ap_record_t *)lv_event_get_user_data(e);
    if (!ap_record) return;

    ESP_LOGI(TAG, "Selected network: %s (RSSI: %d, Auth: %d)", 
             ap_record->ssid, ap_record->rssi, ap_record->authmode);
    
    // TODO: Implement connection dialog with password input
}

static void mode_dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = lv_event_get_target_obj(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        
        wifi_mode_t new_mode;
        switch (selected) {
            case 0: new_mode = WIFI_MODE_STA; break;
            case 1: new_mode = WIFI_MODE_AP; break;
            case 2: new_mode = WIFI_MODE_APSTA; break;
            default: new_mode = WIFI_MODE_STA; break;
        }
        
        ESP_LOGI(TAG, "WiFi mode changed to: %d", new_mode);
        wifi_set_mode(new_mode);
    }
}

// Create WiFi settings screen with complex menu structure
void create_wifi_screen(void) {
    ESP_LOGI(TAG, "Creating WiFi settings screen with menu structure");
    
    // Create main screen
    wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(0x1C1C1C), 0);
    
    // Add activity event handler
    lv_obj_add_event_cb(wifi_screen, wifi_activity_event_handler, 
                       LV_EVENT_PRESSED | LV_EVENT_CLICKED | LV_EVENT_LONG_PRESSED, NULL);
    
    // Create menu
    wifi_menu = lv_menu_create(wifi_screen);
    lv_obj_set_size(wifi_menu, lv_pct(100), lv_pct(100));
    
    // Customize menu colors
    lv_color_t bg_color = lv_obj_get_style_bg_color(wifi_menu, LV_PART_MAIN);
    if (lv_color_brightness(bg_color) > 127) {
        lv_obj_set_style_bg_color(wifi_menu, lv_color_darken(bg_color, 10), 0);
    } else {
        lv_obj_set_style_bg_color(wifi_menu, lv_color_darken(bg_color, 50), 0);
    }
    
    // Enable root back button
    lv_menu_set_mode_root_back_button(wifi_menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);
    lv_obj_add_event_cb(wifi_menu, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Create sub-pages
    create_general_page();
    create_networks_page();
    create_settings_page();
    
    // Create root page
    create_root_page();
    
    // Set sidebar and initial page
    lv_menu_set_sidebar_page(wifi_menu, root_page);
    lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(wifi_menu), 0), 0), 
                      LV_EVENT_CLICKED, NULL);
    
    // Create UI update timer
    ui_update_timer = lv_timer_create(ui_update_timer_callback, 250, NULL);
    if (!ui_update_timer) {
        ESP_LOGE(TAG, "Failed to create UI update timer");
    }
    
    ESP_LOGI(TAG, "WiFi settings screen created successfully");
}

// Create General page (status, enable, log info)
static void create_general_page(void) {
    general_page = lv_menu_page_create(wifi_menu, NULL);
    lv_obj_set_style_pad_hor(general_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(wifi_menu), LV_PART_MAIN), 0);
    
    // Status section
    lv_menu_separator_create(general_page);
    lv_obj_t *section = lv_menu_section_create(general_page);
    
    // WiFi status display
    lv_obj_t *status_cont = create_text_item(section, LV_SYMBOL_WIFI, "Status:", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    status_label = lv_label_create(status_cont);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_flex_grow(status_label, 1);
    
    // Enable/disable switch
    lv_obj_t *enable_cont = create_switch_item(section, LV_SYMBOL_POWER, "Enable WiFi", false);
    enable_switch = lv_obj_get_child(enable_cont, lv_obj_get_child_count(enable_cont) - 1);
    lv_obj_add_event_cb(enable_switch, enable_switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Log info section
    lv_menu_separator_create(general_page);
    section = lv_menu_section_create(general_page);
    
    create_text_item(section, LV_SYMBOL_LIST, "Initialization: OK", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_text_item(section, LV_SYMBOL_LIST, "Driver: ESP32", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_text_item(section, LV_SYMBOL_LIST, "Version: 1.0", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
}

// Create Networks page (scan button and result lists)
static void create_networks_page(void) {
    networks_page = lv_menu_page_create(wifi_menu, NULL);
    lv_obj_set_style_pad_hor(networks_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(wifi_menu), LV_PART_MAIN), 0);
    
    // Scan controls section
    lv_menu_separator_create(networks_page);
    lv_obj_t *section = lv_menu_section_create(networks_page);
    
    // Scan button
    lv_obj_t *scan_cont = create_button_item(section, LV_SYMBOL_REFRESH, "Scan for networks");
    scan_btn = scan_cont;
    lv_obj_add_event_cb(scan_btn, scan_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Network list
    lv_menu_separator_create(networks_page);
    
    networks_list = lv_list_create(networks_page);
    lv_obj_set_size(networks_list, lv_pct(100), 200);
    lv_obj_set_style_bg_color(networks_list, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_border_color(networks_list, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(networks_list, 1, 0);
    lv_obj_set_style_radius(networks_list, 5, 0);
    
    // Add initial message
    lv_obj_t *initial_msg = lv_list_add_text(networks_list, "Enable WiFi and click scan to see networks");
    lv_obj_set_style_text_color(initial_msg, lv_color_hex(0x888888), 0);
}

// Create Settings page (change mode and other configurations)
static void create_settings_page(void) {
    settings_page = lv_menu_page_create(wifi_menu, NULL);
    lv_obj_set_style_pad_hor(settings_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(wifi_menu), LV_PART_MAIN), 0);
    
    // Mode settings section
    lv_menu_separator_create(settings_page);
    lv_obj_t *section = lv_menu_section_create(settings_page);
    
    // WiFi mode selection
    lv_obj_t *mode_cont = create_dropdown_item(section, LV_SYMBOL_SETTINGS, "WiFi Mode", 
                                               "Station\nAccess Point\nStation + AP");
    mode_dropdown = lv_obj_get_child(mode_cont, lv_obj_get_child_count(mode_cont) - 1);
    lv_obj_add_event_cb(mode_dropdown, mode_dropdown_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Link to mode-specific settings
    lv_obj_t *mode_settings_cont = create_text_item(section, LV_SYMBOL_EDIT, "Mode Settings", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(wifi_menu, mode_settings_cont, mode_settings_page);
    
    // Other settings section
    lv_menu_separator_create(settings_page);
    section = lv_menu_section_create(settings_page);
    
    create_switch_item(section, LV_SYMBOL_EYE_CLOSE, "Show hidden networks", false);
    create_switch_item(section, LV_SYMBOL_CHARGE, "Auto reconnect", true);
    create_text_item(section, LV_SYMBOL_HOME, "Reset to defaults", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    
    // Create mode-specific settings page
    create_mode_settings_page();
}

// Create mode-specific settings page
static void create_mode_settings_page(void) {
    mode_settings_page = lv_menu_page_create(wifi_menu, "Mode Settings");
    lv_obj_set_style_pad_hor(mode_settings_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(wifi_menu), LV_PART_MAIN), 0);
    
    // Station mode settings
    lv_menu_separator_create(mode_settings_page);
    lv_obj_t *section = lv_menu_section_create(mode_settings_page);
    
    create_text_item(section, NULL, "Station Mode Settings", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_switch_item(section, LV_SYMBOL_POWER, "DHCP Client", true);
    create_text_item(section, LV_SYMBOL_EDIT, "Static IP Config", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    
    // Access Point mode settings
    lv_menu_separator_create(mode_settings_page);
    section = lv_menu_section_create(mode_settings_page);
    
    create_text_item(section, NULL, "Access Point Settings", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_text_item(section, LV_SYMBOL_EDIT, "AP Name (SSID)", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_text_item(section, LV_SYMBOL_EDIT, "AP Password", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    create_dropdown_item(section, LV_SYMBOL_SETTINGS, "Security", "Open\nWPA2\nWPA3");
}

// Create root page (main menu)
static void create_root_page(void) {
    root_page = lv_menu_page_create(wifi_menu, "WiFi Settings");
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(wifi_menu), LV_PART_MAIN), 0);
    
    // Main sections
    lv_obj_t *section = lv_menu_section_create(root_page);
    
    lv_obj_t *general_cont = create_text_item(section, LV_SYMBOL_SETTINGS, "General", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(wifi_menu, general_cont, general_page);
    
    lv_obj_t *networks_cont = create_text_item(section, LV_SYMBOL_WIFI, "Networks", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(wifi_menu, networks_cont, networks_page);
    
    lv_obj_t *settings_cont = create_text_item(section, LV_SYMBOL_EDIT, "Settings", WIFI_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(wifi_menu, settings_cont, settings_page);
}

// WiFi control functions
void wifi_enable(void) {
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Starting WiFi...");
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi start command sent successfully");
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

wifi_mode_t wifi_get_mode(void) {
    return current_wifi_mode;
}

esp_err_t wifi_set_mode(wifi_mode_t mode) {
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_wifi_set_mode(mode);
    if (ret == ESP_OK) {
        current_wifi_mode = mode;
        ESP_LOGI(TAG, "WiFi mode set to: %d", mode);
    } else {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
    }
    
    return ret;
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
    
    // Check if enough time has passed since WiFi started
    uint32_t time_since_start = (xTaskGetTickCount() - wifi_start_time) * portTICK_PERIOD_MS;
    if (time_since_start < 3000) {
        ESP_LOGW(TAG, "WiFi started only %lu ms ago, waiting before scan", time_since_start);
        return;
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 200,
        .scan_time.active.max = 500
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret == ESP_OK) {
        scanning = true;
        ESP_LOGI(TAG, "WiFi scan started successfully");
        ui_update_needed = true;
    } else {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        scanning = false;
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
        home_reset_idle_timer();
        ESP_LOGI(TAG, "WiFi settings screen loaded");
    } else {
        ESP_LOGW(TAG, "WiFi screen not created yet");
    }
}

// Get WiFi screen object
lv_obj_t* get_wifi_screen(void) {
    return wifi_screen;
}
