# Keyboard Component

A reusable LVGL keyboard component for ESP32 projects.

## Features

- **Multiple Modes**: Text, Numbers, and Special characters
- **Customizable**: Configurable size, callbacks, and features
- **Easy Integration**: Simple API for quick implementation
- **Memory Efficient**: Proper memory management and cleanup
- **Event Handling**: Ready/Enter and Cancel callbacks

## Usage

### Basic Example

```c
#include "components/keyboard/keyboard.h"

// Create a textarea for input
lv_obj_t *textarea = lv_textarea_create(parent);
lv_textarea_set_placeholder_text(textarea, "Enter text...");

// Configure keyboard
keyboard_config_t kb_config = {
    .parent = parent,
    .target_textarea = textarea,
    .width = LV_PCT(100),
    .height = 200,
    .show_special_keys = true,
    .show_numbers = true,
    .on_ready_cb = on_keyboard_ready,
    .on_cancel_cb = on_keyboard_cancel
};

// Create keyboard
lv_obj_t *keyboard = keyboard_create(&kb_config);
```

### Callback Functions

```c
void on_keyboard_ready(lv_obj_t *keyboard, const char *text) {
    printf("User entered: %s\n", text);
    // Hide keyboard or process input
    keyboard_set_visible(keyboard, false);
}

void on_keyboard_cancel(lv_obj_t *keyboard) {
    printf("User cancelled input\n");
    // Hide keyboard
    keyboard_set_visible(keyboard, false);
}
```

## API Reference

### Configuration Structure

```c
typedef struct {
    lv_obj_t *parent;              // Parent object where keyboard will be created
    lv_obj_t *target_textarea;     // Target textarea to receive keyboard input
    lv_coord_t width;              // Keyboard width (0 = parent width)
    lv_coord_t height;             // Keyboard height (0 = auto)
    bool show_special_keys;        // Show special keys like symbols
    bool show_numbers;             // Show number row
    void (*on_ready_cb)(lv_obj_t *keyboard, const char *text);  // Ready callback
    void (*on_cancel_cb)(lv_obj_t *keyboard);                   // Cancel callback
} keyboard_config_t;
```

### Functions

- `keyboard_create()` - Create keyboard component
- `keyboard_set_mode()` - Change keyboard mode (TEXT/NUMBERS/SPECIAL)
- `keyboard_get_mode()` - Get current keyboard mode
- `keyboard_set_textarea()` - Set target textarea
- `keyboard_get_textarea()` - Get target textarea
- `keyboard_set_visible()` - Show/hide keyboard
- `keyboard_is_visible()` - Check visibility
- `keyboard_clear_text()` - Clear textarea content
- `keyboard_get_text()` - Get textarea content
- `keyboard_set_text()` - Set textarea content
- `keyboard_delete()` - Delete keyboard component

## Integration

To use this component in your project:

1. Include the header file:
   ```c
   #include "components/keyboard/keyboard.h"
   ```

2. Make sure the component directory is in your CMakeLists.txt include path

3. Create and configure the keyboard as shown in the examples above

## Notes

- The keyboard automatically handles LVGL keyboard events
- Memory is properly managed - use `keyboard_delete()` when done
- The component is thread-safe when used with LVGL port locking
- Supports all standard LVGL keyboard features and styling
