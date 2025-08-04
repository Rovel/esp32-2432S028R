#include "keyboard.h"
#include <esp_log.h>
#include <lvgl.h>

static const char *TAG = "keyboard_example";

/**
 * @brief Example callback when user presses Ready/Enter
 */
static void example_on_ready(lv_obj_t *keyboard, const char *text)
{
    ESP_LOGI(TAG, "User entered text: '%s'", text ? text : "");
    
    // Hide the keyboard after getting input
    keyboard_set_visible(keyboard, false);
    
    // You can process the text here
    // For example: validate input, save to NVS, etc.
}

/**
 * @brief Example callback when user presses Cancel
 */
static void example_on_cancel(lv_obj_t *keyboard)
{
    ESP_LOGI(TAG, "User cancelled input");
    
    // Hide the keyboard
    keyboard_set_visible(keyboard, false);
    
    // Clear the input if desired
    keyboard_clear_text(keyboard);
}

/**
 * @brief Button event handler for show keyboard button
 */
static void btn_show_event_handler(lv_event_t *e)
{
    lv_obj_t *keyboard = (lv_obj_t *)lv_event_get_user_data(e);
    keyboard_set_visible(keyboard, true);
}

/**
 * @brief Button event handler for hide keyboard button
 */
static void btn_hide_event_handler(lv_event_t *e)
{
    lv_obj_t *keyboard = (lv_obj_t *)lv_event_get_user_data(e);
    keyboard_set_visible(keyboard, false);
}

/**
 * @brief Textarea event handler to show keyboard when clicked
 */
static void textarea_event_handler(lv_event_t *e)
{
    lv_obj_t *keyboard = (lv_obj_t *)lv_event_get_user_data(e);
    keyboard_set_visible(keyboard, true);
}

/**
 * @brief Create a simple keyboard example
 * 
 * @param parent Parent object where the example will be created
 * @return lv_obj_t* Container with the keyboard example
 */
lv_obj_t *keyboard_example_create(lv_obj_t *parent)
{
    // Create container for the example
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create a label
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, "Keyboard Component Example");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create textarea for input
    lv_obj_t *textarea = lv_textarea_create(container);
    lv_obj_set_size(textarea, LV_PCT(80), 50);
    lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_placeholder_text(textarea, "Touch here to type...");
    lv_textarea_set_one_line(textarea, true);
    
    // Create show keyboard button
    lv_obj_t *btn_show = lv_btn_create(container);
    lv_obj_set_size(btn_show, 120, 40);
    lv_obj_align(btn_show, LV_ALIGN_TOP_MID, -70, 120);
    
    lv_obj_t *btn_show_label = lv_label_create(btn_show);
    lv_label_set_text(btn_show_label, "Show KB");
    lv_obj_center(btn_show_label);
    
    // Create hide keyboard button
    lv_obj_t *btn_hide = lv_btn_create(container);
    lv_obj_set_size(btn_hide, 120, 40);
    lv_obj_align(btn_hide, LV_ALIGN_TOP_MID, 70, 120);
    
    lv_obj_t *btn_hide_label = lv_label_create(btn_hide);
    lv_label_set_text(btn_hide_label, "Hide KB");
    lv_obj_center(btn_hide_label);
    
    // Configure and create keyboard
    keyboard_config_t kb_config = {
        .parent = container,
        .target_textarea = textarea,
        .width = LV_PCT(100),
        .height = 200,
        .show_special_keys = true,
        .show_numbers = true,
        .on_ready_cb = example_on_ready,
        .on_cancel_cb = example_on_cancel
    };
    
    lv_obj_t *keyboard = keyboard_create(&kb_config);
    if (!keyboard) {
        ESP_LOGE(TAG, "Failed to create keyboard");
        return container;
    }
    
    // Position keyboard at bottom
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Initially hide the keyboard
    keyboard_set_visible(keyboard, false);
    
    // Button event handlers
    lv_obj_add_event_cb(btn_show, btn_show_event_handler, LV_EVENT_CLICKED, keyboard);
    lv_obj_add_event_cb(btn_hide, btn_hide_event_handler, LV_EVENT_CLICKED, keyboard);
    
    // Textarea click event to show keyboard
    lv_obj_add_event_cb(textarea, textarea_event_handler, LV_EVENT_CLICKED, keyboard);
    
    ESP_LOGI(TAG, "Keyboard example created successfully");
    return container;
}
