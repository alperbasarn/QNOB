#include "InfoScreen.h"

InfoScreen::InfoScreen(Arduino_GFX* graphics, TouchPanel* touch, WiFiTCPClient* client)
    : gfx(graphics), touchPanel(touch), tcpClient(client), 
      screenInitialized(false), pageBackRequested(false),
      currentDate("--/--"), currentTime("--:--"), lastDisplayedTime(""),
      weatherTemp("--°"), roomTemp(20.0),
      lastUpdateTime(0), lastActivityTime(0), inactivityTimeoutReached(false) {
}

void InfoScreen::setTCPClient(WiFiTCPClient* client) {
    tcpClient = client;
}

void InfoScreen::updateScreen() {
    unsigned long currentMillis = millis();
    
    // Always update network status first - this doesn't redraw the screen
    updateNetworkStatus();
    
    // Initialize screen if needed or if we're coming from sleep mode
    if (!screenInitialized) {
        Serial.println("InfoScreen: Initializing screen");
        gfx->fillScreen(BLACK);
        screenInitialized = true;
        lastDisplayedTime = ""; // Force redraw of all elements
        
        // Draw all screen elements
        Serial.println("InfoScreen: Drawing clock - Time: " + currentTime + ", Date: " + currentDate);
        drawClock();
        
        Serial.println("InfoScreen: Drawing weather & temp - Weather: " + weatherTemp + ", Temp: " + String(roomTemp));
        drawWeatherAndTemp();
        
        Serial.println("InfoScreen: Drawing WiFi status - Connected: " + String(wifiConnected) + 
                       ", Internet: " + String(internetConnected) + 
                       ", MQTT: " + String(mqttConnected) + 
                       ", Strength: " + String(wifiStrength));
        drawWiFiStatus();
    }
    
    // If internet is available, try to update time and weather
    if (tcpClient && tcpClient->isInternetAvailable() && 
        (currentMillis - lastUpdateTime >= UPDATE_INTERVAL)) {
        lastUpdateTime = currentMillis;
    }
    
    // Only redraw time, date and weather if they have changed
    if (currentTime != lastDisplayedTime) {
        Serial.println("InfoScreen: Time changed, redrawing central content - Old: " + 
                       lastDisplayedTime + ", New: " + currentTime);
        
        // Clear the central area (not the whole screen, to avoid flicker)
        int centerX = gfx->width() / 2;
        int centerY = gfx->height() / 2;
        gfx->fillRect(centerX - 80, centerY - 40, 160, 100, BLACK);
        
        // Redraw central elements
        drawClock();
        drawWeatherAndTemp();
        lastDisplayedTime = currentTime;
    }
    
    // Check for touch events
    if (touchPanel) {
        if (touchPanel->getHasNewPress()) {
            // Reset activity timer
            resetLastActivityTime();
            pageBackRequested = true;
            Serial.println("InfoScreen: Touch detected, setting pageBackRequested");
        }
    }
    
    // Check for inactivity timeout
    if (currentMillis - lastActivityTime >= INACTIVITY_TIMEOUT) {
        inactivityTimeoutReached = true;
        Serial.println("InfoScreen: Inactivity timeout reached");
    }
}

void InfoScreen::drawClock() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    
    // Draw time in center, large size
    gfx->setTextColor(WHITE);
    gfx->setTextSize(3);
    
    // Calculate width to center the time
    int timeWidth = currentTime.length() * 18; // Approx width for size 3 text
    gfx->setCursor(centerX - timeWidth/2, centerY - 30);
    gfx->print(currentTime);
    
    // Draw date below time, smaller size
    gfx->setTextSize(1);
    int dateWidth = currentDate.length() * 6; // Approx width for size 1 text
    gfx->setCursor(centerX - dateWidth/2, centerY + 5);
    gfx->print(currentDate);
}

void InfoScreen::drawWeatherAndTemp() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    
    // Draw weather temperature
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    int weatherWidth = weatherTemp.length() * 12; // Approx width for size 2 text
    gfx->setCursor(centerX - weatherWidth/2, centerY + 25);
    gfx->print(weatherTemp);
    
    // Draw room temperature - using alternative degree representation
    String roomTempStr = String(roomTemp, 1) + "°C"; // Using Unicode degree symbol
    int roomTempWidth = roomTempStr.length() * 12; // Approx width for size 2 text
    gfx->setCursor(centerX - roomTempWidth/2, centerY + 50);
    gfx->print(roomTempStr);
    
    // Draw small weather icon
    drawWeatherIcon(centerX - weatherWidth/2 - 20, centerY + 25, 10);
    
    // Draw small thermometer icon
    drawThermometerIcon(centerX - roomTempWidth/2 - 20, centerY + 50, 10);
}

void InfoScreen::drawWiFiStatus() {
    // Draw WiFi status at the top of the screen
    int iconSize = 15;
    int iconX = gfx->width() / 2;
    int iconY = 20;
    
    // Clear the area first to avoid artifacts
    gfx->fillRect(iconX - iconSize, iconY - iconSize, iconSize * 2, iconSize * 2, BLACK);
    
    drawWiFiIcon(iconX, iconY, iconSize);
}

void InfoScreen::drawWiFiIcon(int x, int y, int size) {
    // Base color for WiFi icon
    uint16_t wifiColor;
    
    if (!wifiConnected) {
        // Grey with slash if not connected
        wifiColor = gfx->color565(150, 150, 150);
    } else if (internetConnected) {
        // Green if internet is available
        wifiColor = gfx->color565(0, 200, 0);
    } else {
        // Grey if connected but no internet
        wifiColor = gfx->color565(150, 150, 150);
    }
    
    // Draw WiFi arcs based on signal strength
    for (int i = 1; i <= 3; i++) {
        int radius = (size / 3) * i;
        
        if (wifiConnected && wifiStrength >= i) {
            // Full strength for this arc
            gfx->drawArc(x, y, radius, radius-1, 225, 315, wifiColor);
        } else {
            // Lower strength or disconnected
            gfx->drawArc(x, y, radius, radius-1, 225, 315, gfx->color565(60, 60, 60));
        }
    }
    
    // Draw center dot
    gfx->fillCircle(x, y, 2, wifiColor);
    
    // Draw slash if disconnected
    if (!wifiConnected) {
        gfx->drawLine(x - size/2, y - size/2, x + size/2, y + size/2, gfx->color565(255, 0, 0));
        gfx->drawLine(x - size/2 + 1, y - size/2, x + size/2 + 1, y + size/2, gfx->color565(255, 0, 0));
    }
    
    // Draw small tick mark at bottom if MQTT is connected
    if (mqttConnected) {
        int tickSize = size / 5;
        gfx->fillTriangle(
            x, y + size/2 + 5,
            x - tickSize, y + size/2 + 5 + tickSize,
            x + tickSize, y + size/2 + 5 + tickSize,
            gfx->color565(0, 255, 0)
        );
    }
}

void InfoScreen::drawClockIcon(int x, int y, int size) {
    // Draw clock circle
    gfx->drawCircle(x, y, size, WHITE);
    
    // Draw clock hands
    gfx->drawLine(x, y, x, y - size * 0.7, WHITE);      // Hour hand
    gfx->drawLine(x, y, x + size * 0.6, y + size * 0.3, WHITE); // Minute hand
    
    // Draw center dot
    gfx->fillCircle(x, y, 2, WHITE);
}

void InfoScreen::drawWeatherIcon(int x, int y, int size) {
    // Draw sun
    gfx->fillCircle(x, y, size * 0.7, gfx->color565(255, 200, 0));
    
    // Draw small cloud
    int cloudY = y + size * 0.3;
    gfx->fillCircle(x - size * 0.3, cloudY, size * 0.4, gfx->color565(200, 200, 200));
    gfx->fillCircle(x + size * 0.3, cloudY, size * 0.5, gfx->color565(220, 220, 220));
    gfx->fillRect(x - size * 0.3, cloudY, size * 0.6, size * 0.4, gfx->color565(210, 210, 210));
}

void InfoScreen::drawThermometerIcon(int x, int y, int size) {
    // Draw thermometer bulb
    gfx->fillCircle(x, y + size * 0.5, size * 0.5, gfx->color565(255, 0, 0));
    
    // Draw thermometer stem
    int stemWidth = size * 0.3;
    int stemHeight = size * 1.0;
    gfx->fillRect(x - stemWidth/2, y - stemHeight/2, stemWidth, stemHeight, BLACK);
    gfx->drawRect(x - stemWidth/2, y - stemHeight/2, stemWidth, stemHeight, WHITE);
    
    // Fill thermometer with "mercury"
    int level = map(roomTemp, 10, 30, 2, stemHeight-2);
    gfx->fillRect(x - stemWidth/2 + 1, y + stemHeight/2 - level, stemWidth - 2, level, gfx->color565(255, 0, 0));
}

void InfoScreen::updateNetworkStatus() {
    if (tcpClient) {
        bool newWifiConnected = tcpClient->isWiFiConnected();
        bool newInternetConnected = tcpClient->isInternetAvailable();
        bool newMqttConnected = tcpClient->isMQTTConnected();
        int newWifiStrength = tcpClient->getWiFiSignalStrength();
        
        // Only trigger redraw if status has changed
        if (newWifiConnected != wifiConnected || 
            newInternetConnected != internetConnected ||
            newMqttConnected != mqttConnected ||
            newWifiStrength != wifiStrength) {
            
            // Log status changes
            if (newWifiConnected != wifiConnected) {
                Serial.println("WiFi status changed: " + String(newWifiConnected ? "Connected" : "Disconnected"));
            }
            if (newInternetConnected != internetConnected) {
                Serial.println("Internet status changed: " + String(newInternetConnected ? "Available" : "Unavailable"));
            }
            if (newMqttConnected != mqttConnected) {
                Serial.println("MQTT status changed: " + String(newMqttConnected ? "Connected" : "Disconnected"));
            }
            if (newWifiStrength != wifiStrength) {
                Serial.println("WiFi strength changed from " + String(wifiStrength) + " to " + String(newWifiStrength));
            }
            
            wifiConnected = newWifiConnected;
            internetConnected = newInternetConnected;
            mqttConnected = newMqttConnected;
            wifiStrength = newWifiStrength;
            
            // Redraw WiFi status immediately
            drawWiFiStatus();
        }
    }
}

void InfoScreen::updateDateTime(const String& date, const String& time) {
    String newDate;
    
    // Format date without year (assuming input is YYYY/MM/DD)
    int slashPos = date.indexOf('/', 5); // Find second slash after year
    if (slashPos > 0) {
        newDate = date.substring(slashPos + 1) + "/" + date.substring(5, slashPos);
    } else {
        newDate = date;
    }
    
    if (newDate != currentDate || time != currentTime) {
        currentDate = newDate;
        currentTime = time;
    }
}

void InfoScreen::updateWeather(const String& temperature) {
    weatherTemp = temperature;
}

void InfoScreen::updateRoomTemperature(float temperature) {
    roomTemp = temperature;
}

void InfoScreen::resetLastActivityTime() {
    lastActivityTime = millis();
    inactivityTimeoutReached = false;
}

bool InfoScreen::isInactivityTimeoutReached() {
    return inactivityTimeoutReached;
}

bool InfoScreen::isPageBackRequested() {
    return pageBackRequested;
}

void InfoScreen::resetPageBackRequest() {
    pageBackRequested = false;
}

void InfoScreen::printTouchDebugInfo() {
    if (touchPanel) {
        Serial.println("-----Touch Debug Info-----");
        Serial.print("Touch detected: ");
        Serial.println(touchPanel->isPressed() ? "YES" : "NO");
        Serial.print("New press: ");
        Serial.println(touchPanel->getHasNewPress() ? "YES" : "NO");
        Serial.print("X position: ");
        Serial.println(touchPanel->getTouchX());
        Serial.print("Y position: ");
        Serial.println(touchPanel->getTouchY());
        Serial.println("------------------------");
    }
}

void InfoScreen::resetScreen() {
    Serial.println("InfoScreen::resetScreen called - forcing redraw");
    screenInitialized = false;
    lastDisplayedTime = ""; // Force redraw of time
    
    // Ensure we have the latest network status
    if (tcpClient) {
        wifiConnected = tcpClient->isWiFiConnected();
        internetConnected = tcpClient->isInternetAvailable();
        mqttConnected = tcpClient->isMQTTConnected();
        wifiStrength = tcpClient->getWiFiSignalStrength();
    }
}