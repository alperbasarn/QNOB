#ifndef INFO_SCREEN_H
#define INFO_SCREEN_H

#include <Arduino_GFX_Library.h>
#include "TouchPanel.h"
#include "WiFiTCPClient.h"

class InfoScreen {
private:
    Arduino_GFX* gfx;
    TouchPanel* touchPanel;
    WiFiTCPClient* tcpClient;
    
    bool screenInitialized;
    bool pageBackRequested;
    
    String currentDate;
    String currentTime;
    String lastDisplayedTime; // To track time changes
    String weatherTemp;
    float roomTemp;
    
    // Network status
    bool wifiConnected = false;
    bool internetConnected = false;
    bool mqttConnected = false;
    int wifiStrength = 0; // 0-3 for signal strength
    
    // Timing for auto mode switching
    unsigned long lastActivityTime;
    unsigned long lastUpdateTime;
    bool inactivityTimeoutReached;
    
    const unsigned long UPDATE_INTERVAL = 60000;    // Update data every minute
    const unsigned long INACTIVITY_TIMEOUT = 60000; // 1 minute timeout for sleep
    
    void drawClock();
    void drawWeatherAndTemp();
    void drawWiFiStatus();
    
    // Icon drawing helpers
    void drawWiFiIcon(int x, int y, int size);
    void drawClockIcon(int x, int y, int size);
    void drawWeatherIcon(int x, int y, int size);
    void drawThermometerIcon(int x, int y, int size);

public:
    InfoScreen(Arduino_GFX* graphics, TouchPanel* touch, WiFiTCPClient* client);
    
    void updateScreen();
    void setTCPClient(WiFiTCPClient* client);
    void updateDateTime(const String& date, const String& time);
    void updateWeather(const String& temperature);
    void updateRoomTemperature(float temperature);
    void updateNetworkStatus();
    
    void resetLastActivityTime();
    bool isInactivityTimeoutReached();
    
    bool isPageBackRequested();
    void resetPageBackRequest();
    void resetScreen(); // Added method to reset screen initialization flag
    
    // Debug method to help diagnose touch issues
    void printTouchDebugInfo();
};

#endif // INFO_SCREEN_H