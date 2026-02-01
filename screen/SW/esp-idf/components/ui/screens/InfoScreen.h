#pragma once

#include "display/LGFX_Config.hpp"
#include "touch_panel/TouchPanel.h"
#include "esp_timer.h"
#include <cstdint>
#include <string>
#include <functional>

// Callback types for external data sources
using DateTimeCallback = std::function<void(std::string& date, std::string& time, std::string& dayOfWeek)>;
using TemperatureCallback = std::function<float()>;
using NetworkStatusCallback = std::function<void(bool& wifi, bool& internet, bool& mqtt, int& strength)>;

class InfoScreen {
private:
    LGFX* gfx;
    TouchPanel* touchPanel;

    bool screenInitialized;
    bool pageBackRequested;

    // Date and time
    std::string currentDate;
    std::string currentTime;
    std::string lastDisplayedTime;
    std::string formattedDate;
    std::string currentDayOfWeek;

    // For blinking colon
    bool colonVisible;
    int64_t lastColonToggleTime;
    static constexpr int64_t COLON_BLINK_INTERVAL = 1000;
    int colonX;
    int colonY;

    // Temperature data
    float indoorTemp;
    float outdoorTemp;

    // Previous values to detect changes
    float lastIndoorTemp;
    float lastOutdoorTemp;
    std::string lastFormattedDate;
    std::string lastFormattedTime;

    // Flags for redrawing specific elements
    bool indoorTempChanged;
    bool outdoorTempChanged;
    bool dateTimeChanged;
    bool networkStatusChanged;

    // Network status
    bool wifiConnected;
    bool internetConnected;
    bool mqttConnected;
    int wifiStrength;

    // Timing for auto mode switching
    int64_t lastActivityTime;
    int64_t lastUpdateTime;
    bool inactivityTimeoutReached;

    // Arc configuration
    static constexpr int ARC_THICKNESS = 25;
    static constexpr float ARC_START_ANGLE = 120.0f;
    static constexpr float ARC_LENGTH = 300.0f;
    static constexpr float ARC_END_ANGLE = ARC_START_ANGLE + ARC_LENGTH;

    static constexpr int64_t UPDATE_INTERVAL = 5000;
    static constexpr int64_t INACTIVITY_TIMEOUT = 60000;

    // Animation state for arcs
    float animatedIndoorAngle;
    float animatedOutdoorAngle;
    float targetIndoorAngle;
    float targetOutdoorAngle;
    float startIndoorAngle;
    float startOutdoorAngle;
    int64_t arcAnimationStartTime;
    static constexpr int64_t ANIMATION_DURATION = 1000;

    // Callbacks for external data
    DateTimeCallback dateTimeCallback;
    TemperatureCallback outdoorTempCallback;
    NetworkStatusCallback networkStatusCallback;

    // Screen rendering
    void drawDateTime();
    void drawColonOnly();
    void drawTemperatureArcs();
    void drawWiFiStatus();

    // Drawing helpers
    void drawWiFiIcon(int x, int y, int size);
    uint16_t getTemperatureColor(float temperature, bool isIndoor);
    std::string getMonthName(int month);
    void formatDate();
    void updateNetworkStatus();

    // Helper functions
    int64_t millis() const { return esp_timer_get_time() / 1000; }

    template<typename T>
    static T constrain(T value, T minVal, T maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

public:
    InfoScreen(LGFX* graphics, TouchPanel* touch);

    // Main update method
    void update();

    // Data update methods
    void updateDateTime(const std::string& date, const std::string& time, const std::string& dayOfWeek = "");
    void updateIndoorTemperature(float temperature);
    void updateOutdoorTemperature(float temperature);

    // Callbacks for external data sources
    void setDateTimeCallback(DateTimeCallback callback) { dateTimeCallback = callback; }
    void setOutdoorTempCallback(TemperatureCallback callback) { outdoorTempCallback = callback; }
    void setNetworkStatusCallback(NetworkStatusCallback callback) { networkStatusCallback = callback; }

    // Activity tracking
    void resetLastActivityTime();
    bool isInactivityTimeoutReached();

    // Page navigation
    bool isPageBackRequested();
    void resetPageBackRequest();
    void resetScreen();
};
