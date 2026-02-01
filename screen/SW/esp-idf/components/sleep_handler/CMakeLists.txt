# Sleep Handler Component - Power management and deep sleep
idf_component_register(
    SRCS "SleepHandler.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver hal esp_hw_support
)
