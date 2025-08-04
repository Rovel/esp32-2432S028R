idf_component_register(SRCS demo.c 
                           drivers/lcd.c 
                           drivers/touch.c
                           frontend/keypad/keypad.c
                           frontend/calibration/calibration.c
                           frontend/home/home.c
                           frontend/passkey/passkey_change.c
                           frontend/wifi/wifi.c
                           components/keyboard/keyboard.c
                           components/keyboard/keyboard_example.c
                    INCLUDE_DIRS "." "components"
                    REQUIRES esp_lcd driver nvs_flash esp_timer esp_wifi
                    )
