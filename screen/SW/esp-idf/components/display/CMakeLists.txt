# Display Component - Display controller and screen management
idf_component_register(
    SRCS "DisplayController.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver hal touch_panel LovyanGFX
)
