#include "InfoScreen.h"
#include "esp_log.h"
#include <cmath>
#include <algorithm>

static const char* TAG = "InfoScreen";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

InfoScreen::InfoScreen(LGFX* graphics, TouchPanel* touch)
    : gfx(graphics), touchPanel(touch),
      screenInitialized(false), pageBackRequested(false),
      currentDate("--/--"), currentTime("--:--"), currentDayOfWeek("---"),
      colonVisible(true), lastColonToggleTime(0), colonX(0), colonY(0),
      indoorTemp(18.0f), outdoorTemp(0.0f),
      lastIndoorTemp(0.0f), lastOutdoorTemp(0.0f),
      indoorTempChanged(false), outdoorTempChanged(false),
      dateTimeChanged(false), networkStatusChanged(false),
      wifiConnected(false), internetConnected(false), mqttConnected(false), wifiStrength(0),
      lastActivityTime(0), lastUpdateTime(0), inactivityTimeoutReached(false) {

    // Initialize animation state
    float effectiveStart = std::fmod(ARC_START_ANGLE + 180.0f, 360.0f);
    animatedIndoorAngle = effectiveStart;
    animatedOutdoorAngle = effectiveStart;
    targetIndoorAngle = effectiveStart;
    targetOutdoorAngle = effectiveStart;
    startIndoorAngle = effectiveStart;
    startOutdoorAngle = effectiveStart;
    arcAnimationStartTime = millis();

    lastColonToggleTime = millis();
}

void InfoScreen::update() {
    int64_t currentMillis = millis();

    // Fetch data from callbacks if available
    if (dateTimeCallback) {
        std::string newDate, newTime, newDayOfWeek;
        dateTimeCallback(newDate, newTime, newDayOfWeek);

        if (newDate != currentDate || newTime != currentTime || newDayOfWeek != currentDayOfWeek) {
            currentDate = newDate;
            currentTime = newTime;
            currentDayOfWeek = newDayOfWeek;
            formatDate();

            if (formattedDate != lastFormattedDate || currentTime != lastFormattedTime) {
                lastFormattedDate = formattedDate;
                lastFormattedTime = currentTime;
                dateTimeChanged = true;
            }
        }
    }

    // Check for colon blink
    if (currentMillis - lastColonToggleTime >= COLON_BLINK_INTERVAL) {
        colonVisible = !colonVisible;
        lastColonToggleTime = currentMillis;

        if (colonX > 0) {
            drawColonOnly();
        } else {
            dateTimeChanged = true;
        }
    }

    // Reset change flags
    indoorTempChanged = false;
    outdoorTempChanged = false;
    networkStatusChanged = false;

    // Update network status
    updateNetworkStatus();

    // Update outdoor temperature if callback available
    if (outdoorTempCallback && (currentMillis - lastUpdateTime >= UPDATE_INTERVAL)) {
        float newOutdoorTemp = outdoorTempCallback();
        if (std::abs(newOutdoorTemp - outdoorTemp) > 0.5f) {
            outdoorTemp = newOutdoorTemp;
            outdoorTempChanged = true;
        }
        lastUpdateTime = currentMillis;
    }

    // Full screen redraw on initialization
    if (!screenInitialized) {
        gfx->fillScreen(TFT_BLACK);
        drawDateTime();
        drawWiFiStatus();
        drawTemperatureArcs();
        screenInitialized = true;
    } else {
        if (indoorTempChanged || outdoorTempChanged) {
            drawTemperatureArcs();
        }
        if (dateTimeChanged) {
            drawDateTime();
            dateTimeChanged = false;
        }
        if (networkStatusChanged) {
            drawWiFiStatus();
        }
    }

    // Check for touch events
    if (touchPanel && touchPanel->getHasNewPress()) {
        resetLastActivityTime();
        pageBackRequested = true;
        screenInitialized = false;
        ESP_LOGI(TAG, "Touch detected, setting pageBackRequested");
    }
}

void InfoScreen::formatDate() {
    // Parse YYYY/MM/DD format
    size_t firstSlash = currentDate.find('/');
    size_t secondSlash = currentDate.find('/', firstSlash + 1);

    if (firstSlash != std::string::npos && secondSlash != std::string::npos) {
        std::string dayStr = currentDate.substr(secondSlash + 1);

        // Remove leading zeros from day
        while (dayStr.length() > 1 && dayStr[0] == '0') {
            dayStr = dayStr.substr(1);
        }

        // Get month name
        int monthNum = std::stoi(currentDate.substr(firstSlash + 1, secondSlash - firstSlash - 1));
        std::string monthName = getMonthName(monthNum);

        formattedDate = dayStr + " " + monthName;
    } else {
        formattedDate = currentDate;
    }
}

std::string InfoScreen::getMonthName(int month) {
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    if (month >= 1 && month <= 12) {
        return months[month - 1];
    }
    return "---";
}

void InfoScreen::drawColonOnly() {
    // Scale for display size
    int scale = gfx->width() / 80;  // ~3 for 240px
    int colonWidth = 10 * scale / 3;
    int colonHeight = 6 * scale;

    gfx->fillRect(colonX, colonY, colonWidth, colonHeight, TFT_BLACK);

    if (colonVisible) {
        gfx->setTextColor(TFT_WHITE);
        gfx->setTextSize(scale);
        gfx->setCursor(colonX, colonY);
        gfx->print(":");
    }
}

void InfoScreen::drawDateTime() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    int scale = gfx->width() / 80;

    int clearRadius = 55 * gfx->width() / 240;
    gfx->fillCircle(centerX, centerY, clearRadius, TFT_BLACK);

    gfx->setTextColor(TFT_WHITE);
    gfx->setTextSize(scale);

    // Find colon position
    size_t colonPos = currentTime.find(':');
    std::string hours = "";
    std::string minutes = "";

    if (colonPos != std::string::npos) {
        hours = currentTime.substr(0, colonPos);
        minutes = currentTime.substr(colonPos + 1);
    } else {
        hours = currentTime;
    }

    // Calculate positioning
    int charWidth = 6 * scale;
    int timeWidth = (hours.length() + minutes.length() + 1) * charWidth;
    int timeX = centerX - timeWidth / 2;
    int timeY = centerY - 30 * gfx->height() / 240;

    // Draw hours
    gfx->setCursor(timeX, timeY);
    gfx->print(hours.c_str());

    // Save colon position
    colonX = timeX + hours.length() * charWidth;
    colonY = timeY;

    // Draw colon if visible
    if (colonVisible) {
        gfx->setCursor(colonX, colonY);
        gfx->print(":");
    }

    // Draw minutes
    gfx->setCursor(colonX + charWidth, timeY);
    gfx->print(minutes.c_str());

    // Draw day of week
    int smallScale = scale * 2 / 3;
    gfx->setTextSize(smallScale);
    int dayWidth = currentDayOfWeek.length() * 6 * smallScale;
    gfx->setCursor(centerX - dayWidth / 2, centerY);
    gfx->print(currentDayOfWeek.c_str());

    // Draw date
    int dateWidth = formattedDate.length() * 6 * smallScale;
    gfx->setCursor(centerX - dateWidth / 2, centerY + 25 * gfx->height() / 240);
    gfx->print(formattedDate.c_str());
}

void InfoScreen::drawTemperatureArcs() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    int spacing = 5 * gfx->width() / 240;
    int arcThickness = ARC_THICKNESS * gfx->width() / 240;

    int indoorOuterRadius = std::min(gfx->width(), gfx->height()) / 2;
    int indoorInnerRadius = indoorOuterRadius - arcThickness;
    int outdoorOuterRadius = indoorInnerRadius - spacing;
    int outdoorInnerRadius = outdoorOuterRadius - arcThickness;

    float effectiveStart = std::fmod(ARC_START_ANGLE + 180.0f, 360.0f);
    float effectiveEnd = std::fmod(ARC_END_ANGLE + 180.0f, 360.0f);
    if (effectiveEnd < effectiveStart) {
        effectiveEnd += 360.0f;
    }
    float arcSpan = effectiveEnd - effectiveStart;

    // Compute target fill angles
    float normalizedIndoor = constrain((indoorTemp - 0.0f) / 40.0f, 0.0f, 1.0f);
    float newTargetIndoor = effectiveStart + normalizedIndoor * arcSpan;
    float normalizedOutdoor = constrain((outdoorTemp - (-20.0f)) / 70.0f, 0.0f, 1.0f);
    float newTargetOutdoor = effectiveStart + normalizedOutdoor * arcSpan;

    int64_t currentMillis = millis();

    if (!screenInitialized || indoorTempChanged || outdoorTempChanged) {
        if (!screenInitialized) {
            animatedIndoorAngle = newTargetIndoor;
            animatedOutdoorAngle = newTargetOutdoor;
        } else {
            startIndoorAngle = effectiveStart;
            startOutdoorAngle = effectiveStart;
            arcAnimationStartTime = currentMillis;
        }
        targetIndoorAngle = newTargetIndoor;
        targetOutdoorAngle = newTargetOutdoor;
    }

    float progress = constrain(static_cast<float>(currentMillis - arcAnimationStartTime) / ANIMATION_DURATION, 0.0f, 1.0f);
    animatedIndoorAngle = startIndoorAngle + (targetIndoorAngle - startIndoorAngle) * progress;
    animatedOutdoorAngle = startOutdoorAngle + (targetOutdoorAngle - startOutdoorAngle) * progress;

    uint16_t indoorColor = getTemperatureColor(indoorTemp, true);
    uint16_t outdoorColor = getTemperatureColor(outdoorTemp, false);
    uint16_t bgColor = gfx->color565(30, 30, 30);

    int capRadius = arcThickness / 2;

    // Draw start caps
    float startRad = effectiveStart * M_PI / 180.0f;
    int indoorStartX = centerX - indoorOuterRadius * std::cos(startRad) + capRadius * std::cos(startRad);
    int indoorStartY = centerY - indoorOuterRadius * std::sin(startRad) + capRadius * std::sin(startRad);
    int outdoorStartX = centerX - outdoorOuterRadius * std::cos(startRad) + capRadius * std::cos(startRad);
    int outdoorStartY = centerY - outdoorOuterRadius * std::sin(startRad) + capRadius * std::sin(startRad);

    gfx->fillCircle(indoorStartX, indoorStartY, capRadius, indoorColor);
    gfx->fillCircle(outdoorStartX, outdoorStartY, capRadius, outdoorColor);

    const float angleStep = 0.5f;

    for (float angle = effectiveStart; angle < effectiveEnd; angle += angleStep) {
        float nextAngle = std::min(angle + angleStep, effectiveEnd);
        float rad1 = angle * M_PI / 180.0f;
        float rad2 = nextAngle * M_PI / 180.0f;

        // Indoor arc
        int x1Inner = centerX - indoorInnerRadius * std::cos(rad1);
        int y1Inner = centerY - indoorInnerRadius * std::sin(rad1);
        int x2Inner = centerX - indoorInnerRadius * std::cos(rad2);
        int y2Inner = centerY - indoorInnerRadius * std::sin(rad2);
        int x1Outer = centerX - indoorOuterRadius * std::cos(rad1);
        int y1Outer = centerY - indoorOuterRadius * std::sin(rad1);
        int x2Outer = centerX - indoorOuterRadius * std::cos(rad2);
        int y2Outer = centerY - indoorOuterRadius * std::sin(rad2);

        uint16_t indoorPixelColor = (angle < animatedIndoorAngle) ? indoorColor : bgColor;
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, indoorPixelColor);
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, indoorPixelColor);

        // Outdoor arc
        x1Inner = centerX - outdoorInnerRadius * std::cos(rad1);
        y1Inner = centerY - outdoorInnerRadius * std::sin(rad1);
        x2Inner = centerX - outdoorInnerRadius * std::cos(rad2);
        y2Inner = centerY - outdoorInnerRadius * std::sin(rad2);
        x1Outer = centerX - outdoorOuterRadius * std::cos(rad1);
        y1Outer = centerY - outdoorOuterRadius * std::sin(rad1);
        x2Outer = centerX - outdoorOuterRadius * std::cos(rad2);
        y2Outer = centerY - outdoorOuterRadius * std::sin(rad2);

        uint16_t outdoorPixelColor = (angle < animatedOutdoorAngle) ? outdoorColor : bgColor;
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, outdoorPixelColor);
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, outdoorPixelColor);
    }

    // Draw end caps
    float indoorEndRad = animatedIndoorAngle * M_PI / 180.0f;
    float outdoorEndRad = animatedOutdoorAngle * M_PI / 180.0f;

    int indoorEndX = centerX - (indoorInnerRadius + indoorOuterRadius) / 2 * std::cos(indoorEndRad);
    int indoorEndY = centerY - (indoorInnerRadius + indoorOuterRadius) / 2 * std::sin(indoorEndRad);
    int outdoorEndX = centerX - (outdoorInnerRadius + outdoorOuterRadius) / 2 * std::cos(outdoorEndRad);
    int outdoorEndY = centerY - (outdoorInnerRadius + outdoorOuterRadius) / 2 * std::sin(outdoorEndRad);

    gfx->fillCircle(indoorEndX, indoorEndY, capRadius, indoorColor);
    gfx->fillCircle(outdoorEndX, outdoorEndY, capRadius, outdoorColor);

    // Temperature labels
    int textScale = gfx->width() / 80;
    gfx->setTextColor(TFT_WHITE);
    gfx->setTextSize(textScale);

    float midAngle = (effectiveStart + effectiveEnd) / 2.0f;
    float midRad = midAngle * M_PI / 180.0f;
    int textX = centerX;

    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%d", static_cast<int>(std::round(indoorTemp)));
    int textY = centerY - (indoorInnerRadius + arcThickness / 2) * std::sin(midRad);
    int textWidth = strlen(tempStr) * 6 * textScale;
    gfx->setCursor(textX - textWidth / 2, textY - 6 * textScale);
    gfx->print(tempStr);

    snprintf(tempStr, sizeof(tempStr), "%d", static_cast<int>(std::round(outdoorTemp)));
    textY = centerY - (outdoorInnerRadius + arcThickness / 2) * std::sin(midRad);
    textWidth = strlen(tempStr) * 6 * textScale;
    gfx->setCursor(textX - textWidth / 2, textY - 6 * textScale);
    gfx->print(tempStr);
}

uint16_t InfoScreen::getTemperatureColor(float temperature, bool isIndoor) {
    uint8_t r, g, b;

    if (isIndoor) {
        if (temperature <= 15.0f) {
            float t = temperature / 15.0f;
            r = 150 * t; g = 150 * t; b = 255;
        } else if (temperature <= 18.0f) {
            r = 0; g = 0; b = 255;
        } else if (temperature <= 23.0f) {
            float t = constrain((temperature - 19.0f) / 2.0f, 0.0f, 1.0f);
            r = 0; g = 150 + 105 * t; b = 0;
        } else if (temperature <= 26.0f) {
            float t = (temperature - 24.0f) / 4.0f;
            r = 255; g = 255 - 140 * t; b = 0;
        } else {
            float t = constrain((temperature - 26.0f) / 10.0f, 0.0f, 1.0f);
            r = 255; g = 115 * (1 - t); b = 0;
        }
    } else {
        if (temperature <= 5.0f) {
            float t = constrain((temperature + 5.0f) / 10.0f, 0.0f, 1.0f);
            r = 150 * t; g = 150 * t; b = 255;
        } else if (temperature <= 15.0f) {
            r = 0; g = 0; b = 255;
        } else if (temperature <= 24.0f) {
            float t = (temperature - 16.0f) / 8.0f;
            r = 0; g = 150 + 105 * t; b = 0;
        } else if (temperature <= 30.0f) {
            float t = (temperature - 25.0f) / 5.0f;
            r = 255; g = 255 - 140 * t; b = 0;
        } else {
            float t = constrain((temperature - 30.0f) / 10.0f, 0.0f, 1.0f);
            r = 255; g = 115 * (1 - t); b = 0;
        }
    }

    return gfx->color565(r, g, b);
}

void InfoScreen::drawWiFiStatus() {
    int iconSize = 15 * gfx->width() / 240;
    int iconX = gfx->width() / 2;
    int iconY = gfx->height() - 20 * gfx->height() / 240;

    gfx->fillRect(iconX - iconSize - 5, iconY - iconSize - 5,
                  (iconSize + 5) * 2, (iconSize + 5) * 2, TFT_BLACK);
    drawWiFiIcon(iconX, iconY, iconSize);
}

void InfoScreen::drawWiFiIcon(int x, int y, int size) {
    uint16_t wifiColor;
    if (!wifiConnected) {
        wifiColor = gfx->color565(150, 150, 150);
    } else if (internetConnected) {
        wifiColor = gfx->color565(0, 200, 0);
    } else {
        wifiColor = gfx->color565(150, 150, 150);
    }

    // Draw WiFi arcs
    for (int i = 1; i <= 3; i++) {
        int radius = (size / 3) * i;
        uint16_t arcColor = (wifiConnected && wifiStrength >= i) ? wifiColor : gfx->color565(60, 60, 60);
        gfx->drawArc(x, y, radius, radius - 1, 225, 315, arcColor);
    }

    gfx->fillCircle(x, y, 2 * size / 15, wifiColor);

    if (!wifiConnected) {
        gfx->drawLine(x - size / 2, y - size / 2, x + size / 2, y + size / 2, TFT_RED);
        gfx->drawLine(x - size / 2 + 1, y - size / 2, x + size / 2 + 1, y + size / 2, TFT_RED);
    }

    if (mqttConnected) {
        int tickSize = size / 5;
        gfx->fillTriangle(
            x, y + size / 2 + 5,
            x - tickSize, y + size / 2 + 5 + tickSize,
            x + tickSize, y + size / 2 + 5 + tickSize,
            gfx->color565(0, 255, 0));
    }
}

void InfoScreen::updateNetworkStatus() {
    if (networkStatusCallback) {
        bool newWifi, newInternet, newMqtt;
        int newStrength;
        networkStatusCallback(newWifi, newInternet, newMqtt, newStrength);

        if (newWifi != wifiConnected || newInternet != internetConnected ||
            newMqtt != mqttConnected || newStrength != wifiStrength) {
            wifiConnected = newWifi;
            internetConnected = newInternet;
            mqttConnected = newMqtt;
            wifiStrength = newStrength;
            networkStatusChanged = true;
        }
    }
}

void InfoScreen::updateDateTime(const std::string& date, const std::string& time, const std::string& dayOfWeek) {
    if (date != currentDate || time != currentTime) {
        currentDate = date;
        currentTime = time;
        if (!dayOfWeek.empty()) {
            currentDayOfWeek = dayOfWeek;
        }
        formatDate();

        if (formattedDate != lastFormattedDate || currentTime != lastFormattedTime) {
            lastFormattedDate = formattedDate;
            lastFormattedTime = currentTime;
            dateTimeChanged = true;
            drawDateTime();
        }
    }
}

void InfoScreen::updateIndoorTemperature(float temperature) {
    if (std::abs(temperature - indoorTemp) > 0.5f) {
        indoorTemp = temperature;
        indoorTempChanged = true;
        drawTemperatureArcs();
    }
}

void InfoScreen::updateOutdoorTemperature(float temperature) {
    if (std::abs(temperature - outdoorTemp) > 0.5f) {
        outdoorTemp = temperature;
        outdoorTempChanged = true;
        drawTemperatureArcs();
    }
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

void InfoScreen::resetScreen() {
    ESP_LOGI(TAG, "resetScreen called - forcing redraw");
    screenInitialized = false;
}
