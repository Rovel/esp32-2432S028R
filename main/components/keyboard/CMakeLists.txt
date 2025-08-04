# Keyboard Component CMakeLists.txt

set(COMPONENT_SRCS "keyboard.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

# Register the component
register_component()

# Add component dependencies
target_link_libraries(${COMPONENT_LIB}
    INTERFACE
    lvgl::lvgl
    idf::esp_log
)
