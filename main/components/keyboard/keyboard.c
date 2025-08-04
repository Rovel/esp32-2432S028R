#include "keyboard.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "keyboard";

/**
 * @brief Internal keyboard data structure
 */
typedef struct {
    lv_obj_t *textarea;
    keyboard_mode_t mode;
    bool show_special_keys;
    bool show_numbers;
    void (*on_ready_cb)(lv_obj_t *keyboard, const char *text);
    void (*on_cancel_cb)(lv_obj_t *keyboard);
    lv_obj_t *kb;  // LVGL keyboard widget
} keyboard_data_t;

/**
 * @brief Keyboard event handler
 */
static void keyboard_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *keyboard = lv_event_get_target(e);
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    
    if (!data) {
        ESP_LOGE(TAG, "No keyboard data found");
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        // Handle special keys
        uint32_t btn_id = lv_keyboard_get_selected_btn(data->kb);
        const char *btn_text = lv_keyboard_get_btn_text(data->kb, btn_id);
        
        if (btn_text != NULL) {
            if (strcmp(btn_text, LV_SYMBOL_OK) == 0 || strcmp(btn_text, "Enter") == 0) {
                // Ready/Enter button pressed
                if (data->on_ready_cb && data->textarea) {
                    const char *text = lv_textarea_get_text(data->textarea);
                    data->on_ready_cb(keyboard, text);
                }
            } else if (strcmp(btn_text, LV_SYMBOL_CLOSE) == 0 || strcmp(btn_text, "Cancel") == 0) {
                // Cancel button pressed
                if (data->on_cancel_cb) {
                    data->on_cancel_cb(keyboard);
                }
            } else if (strcmp(btn_text, "123") == 0) {
                // Switch to numbers mode
                keyboard_set_mode(keyboard, KEYBOARD_MODE_NUMBERS);
            } else if (strcmp(btn_text, "ABC") == 0) {
                // Switch to text mode
                keyboard_set_mode(keyboard, KEYBOARD_MODE_TEXT);
            } else if (strcmp(btn_text, "#$%") == 0 || strcmp(btn_text, "Sym") == 0) {
                // Switch to special characters mode
                keyboard_set_mode(keyboard, KEYBOARD_MODE_SPECIAL);
            }
        }
    }
}

/**
 * @brief Update keyboard layout based on mode
 */
static void update_keyboard_layout(lv_obj_t *keyboard)
{
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data || !data->kb) return;

    switch (data->mode) {
        case KEYBOARD_MODE_TEXT:
            lv_keyboard_set_mode(data->kb, LV_KEYBOARD_MODE_TEXT_LOWER);
            break;
        case KEYBOARD_MODE_NUMBERS:
            lv_keyboard_set_mode(data->kb, LV_KEYBOARD_MODE_NUMBER);
            break;
        case KEYBOARD_MODE_SPECIAL:
            lv_keyboard_set_mode(data->kb, LV_KEYBOARD_MODE_SPECIAL);
            break;
    }
}

lv_obj_t *keyboard_create(const keyboard_config_t *config)
{
    if (!config || !config->parent) {
        ESP_LOGE(TAG, "Invalid keyboard configuration");
        return NULL;
    }

    // Create container for the keyboard
    lv_obj_t *keyboard = lv_obj_create(config->parent);
    if (!keyboard) {
        ESP_LOGE(TAG, "Failed to create keyboard container");
        return NULL;
    }

    // Allocate keyboard data
    keyboard_data_t *data = lv_malloc(sizeof(keyboard_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate keyboard data");
        lv_obj_del(keyboard);
        return NULL;
    }

    // Initialize keyboard data
    memset(data, 0, sizeof(keyboard_data_t));
    data->textarea = config->target_textarea;
    data->mode = KEYBOARD_MODE_TEXT;
    data->show_special_keys = config->show_special_keys;
    data->show_numbers = config->show_numbers;
    data->on_ready_cb = config->on_ready_cb;
    data->on_cancel_cb = config->on_cancel_cb;

    // Set keyboard container properties
    lv_obj_set_size(keyboard, 
        config->width > 0 ? config->width : LV_PCT(100), 
        config->height > 0 ? config->height : LV_SIZE_CONTENT);
    
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(keyboard, 0, 0);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_TRANSP, 0);

    // Create LVGL keyboard widget
    data->kb = lv_keyboard_create(keyboard);
    if (!data->kb) {
        ESP_LOGE(TAG, "Failed to create LVGL keyboard widget");
        lv_free(data);
        lv_obj_del(keyboard);
        return NULL;
    }

    lv_obj_set_size(data->kb, LV_PCT(100), LV_PCT(100));
    lv_obj_align(data->kb, LV_ALIGN_CENTER, 0, 0);

    // Set target textarea if provided
    if (data->textarea) {
        lv_keyboard_set_textarea(data->kb, data->textarea);
    }

    // Set user data and event handler
    lv_obj_set_user_data(keyboard, data);
    lv_obj_add_event_cb(data->kb, keyboard_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Update layout
    update_keyboard_layout(keyboard);

    ESP_LOGI(TAG, "Keyboard component created successfully");
    return keyboard;
}

void keyboard_set_mode(lv_obj_t *keyboard, keyboard_mode_t mode)
{
    if (!keyboard) return;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data) return;

    data->mode = mode;
    update_keyboard_layout(keyboard);
    
    ESP_LOGD(TAG, "Keyboard mode changed to %d", mode);
}

keyboard_mode_t keyboard_get_mode(lv_obj_t *keyboard)
{
    if (!keyboard) return KEYBOARD_MODE_TEXT;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data) return KEYBOARD_MODE_TEXT;

    return data->mode;
}

void keyboard_set_textarea(lv_obj_t *keyboard, lv_obj_t *textarea)
{
    if (!keyboard) return;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data || !data->kb) return;

    data->textarea = textarea;
    lv_keyboard_set_textarea(data->kb, textarea);
    
    ESP_LOGD(TAG, "Keyboard textarea set");
}

lv_obj_t *keyboard_get_textarea(lv_obj_t *keyboard)
{
    if (!keyboard) return NULL;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data) return NULL;

    return data->textarea;
}

void keyboard_set_visible(lv_obj_t *keyboard, bool show)
{
    if (!keyboard) return;
    
    if (show) {
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    ESP_LOGD(TAG, "Keyboard visibility set to %s", show ? "visible" : "hidden");
}

bool keyboard_is_visible(lv_obj_t *keyboard)
{
    if (!keyboard) return false;
    
    return !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void keyboard_clear_text(lv_obj_t *keyboard)
{
    if (!keyboard) return;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data || !data->textarea) return;

    lv_textarea_set_text(data->textarea, "");
    ESP_LOGD(TAG, "Keyboard textarea cleared");
}

const char *keyboard_get_text(lv_obj_t *keyboard)
{
    if (!keyboard) return NULL;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data || !data->textarea) return NULL;

    return lv_textarea_get_text(data->textarea);
}

void keyboard_set_text(lv_obj_t *keyboard, const char *text)
{
    if (!keyboard || !text) return;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (!data || !data->textarea) return;

    lv_textarea_set_text(data->textarea, text);
    ESP_LOGD(TAG, "Keyboard textarea text set");
}

void keyboard_delete(lv_obj_t *keyboard)
{
    if (!keyboard) return;
    
    keyboard_data_t *data = (keyboard_data_t *)lv_obj_get_user_data(keyboard);
    if (data) {
        lv_free(data);
    }
    
    lv_obj_del(keyboard);
    ESP_LOGD(TAG, "Keyboard component deleted");
}
