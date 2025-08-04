
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Keyboard component configuration
 */
typedef struct {
    lv_obj_t *parent;              /**< Parent object where keyboard will be created */
    lv_obj_t *target_textarea;     /**< Target textarea to receive keyboard input */
    lv_coord_t width;              /**< Keyboard width (0 = parent width) */
    lv_coord_t height;             /**< Keyboard height (0 = auto) */
    bool show_special_keys;        /**< Show special keys like symbols */
    bool show_numbers;             /**< Show number row */
    void (*on_ready_cb)(lv_obj_t *keyboard, const char *text);  /**< Callback when ready/enter is pressed */
    void (*on_cancel_cb)(lv_obj_t *keyboard);                   /**< Callback when cancel is pressed */
} keyboard_config_t;

/**
 * @brief Keyboard mode enumeration
 */
typedef enum {
    KEYBOARD_MODE_TEXT,           /**< Text mode (letters) */
    KEYBOARD_MODE_NUMBERS,        /**< Numbers mode */
    KEYBOARD_MODE_SPECIAL,        /**< Special characters mode */
} keyboard_mode_t;

/**
 * @brief Create a keyboard component
 * 
 * @param config Keyboard configuration
 * @return lv_obj_t* Pointer to the created keyboard object, NULL on error
 */
lv_obj_t *keyboard_create(const keyboard_config_t *config);

/**
 * @brief Set keyboard mode
 * 
 * @param keyboard Keyboard object
 * @param mode Keyboard mode to set
 */
void keyboard_set_mode(lv_obj_t *keyboard, keyboard_mode_t mode);

/**
 * @brief Get current keyboard mode
 * 
 * @param keyboard Keyboard object
 * @return keyboard_mode_t Current keyboard mode
 */
keyboard_mode_t keyboard_get_mode(lv_obj_t *keyboard);

/**
 * @brief Set target textarea for keyboard input
 * 
 * @param keyboard Keyboard object
 * @param textarea Target textarea object
 */
void keyboard_set_textarea(lv_obj_t *keyboard, lv_obj_t *textarea);

/**
 * @brief Get target textarea
 * 
 * @param keyboard Keyboard object
 * @return lv_obj_t* Target textarea object
 */
lv_obj_t *keyboard_get_textarea(lv_obj_t *keyboard);

/**
 * @brief Show/hide the keyboard
 * 
 * @param keyboard Keyboard object
 * @param show true to show, false to hide
 */
void keyboard_set_visible(lv_obj_t *keyboard, bool show);

/**
 * @brief Check if keyboard is visible
 * 
 * @param keyboard Keyboard object
 * @return true if visible, false if hidden
 */
bool keyboard_is_visible(lv_obj_t *keyboard);

/**
 * @brief Clear the target textarea
 * 
 * @param keyboard Keyboard object
 */
void keyboard_clear_text(lv_obj_t *keyboard);

/**
 * @brief Get text from target textarea
 * 
 * @param keyboard Keyboard object
 * @return const char* Text from textarea, NULL if no textarea set
 */
const char *keyboard_get_text(lv_obj_t *keyboard);

/**
 * @brief Set text in target textarea
 * 
 * @param keyboard Keyboard object
 * @param text Text to set
 */
void keyboard_set_text(lv_obj_t *keyboard, const char *text);

/**
 * @brief Delete keyboard component
 * 
 * @param keyboard Keyboard object to delete
 */
void keyboard_delete(lv_obj_t *keyboard);

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_H */
