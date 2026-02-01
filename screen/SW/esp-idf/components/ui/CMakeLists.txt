# UI Component - Graphics primitives, UI elements, and screens
idf_component_register(
    SRCS "Arc.cpp"
         "Buttons.cpp"
         "screens/InitializationScreen.cpp"
         "screens/InfoScreen.cpp"
         "screens/ModeController.cpp"
    INCLUDE_DIRS "." "screens"
    REQUIRES display touch_panel hal
)
