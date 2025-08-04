#ifndef KEYBOARD_EXAMPLE_H
#define KEYBOARD_EXAMPLE_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a simple keyboard example
 * 
 * This function creates a complete example showing how to use the keyboard component.
 * It includes a textarea, buttons to show/hide the keyboard, and demonstrates
 * the callback functionality.
 * 
 * @param parent Parent object where the example will be created
 * @return lv_obj_t* Container with the keyboard example, NULL on error
 */
lv_obj_t *keyboard_example_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_EXAMPLE_H */
