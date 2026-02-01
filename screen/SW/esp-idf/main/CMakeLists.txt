# Main Application
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        hal
        display
        touch_panel
        knob_controller
        storage
        LovyanGFX
        driver
        nvs_flash
)
