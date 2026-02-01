# MQTT Manager Component
idf_component_register(
    SRCS "MQTTManager.cpp"
    INCLUDE_DIRS "."
    REQUIRES mqtt wifi_manager storage
)
