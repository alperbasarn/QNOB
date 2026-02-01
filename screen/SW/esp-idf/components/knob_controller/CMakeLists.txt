# KnobController Component - UART Communication with External Knob
idf_component_register(
    SRCS "KnobController.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver hal
)
