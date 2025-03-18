#include "InfoScreen.h"
#include <math.h>

InfoScreen::InfoScreen(Arduino_GFX* graphics, TouchPanel* touch, WiFiTCPClient* client)
  : gfx(graphics), touchPanel(touch), tcpClient(client),
    screenInitialized(false), pageBackRequested(false),
    currentDate("--/--"), currentTime("--:--"), lastDisplayedTime(""),
    lastUpdateTime(0), lastActivityTime(0), inactivityTimeoutReached(false) {
  
  // Initialize animation state.
  // We use the final target angles immediately at startup.
  float effectiveStart = fmod(ARC_START_ANGLE + 180.0, 360.0);
  animatedIndoorAngle = effectiveStart;
  animatedOutdoorAngle = effectiveStart;
  targetIndoorAngle = effectiveStart;
  targetOutdoorAngle = effectiveStart;
  startIndoorAngle = effectiveStart;
  startOutdoorAngle = effectiveStart;
  arcAnimationStartTime = millis();
}

void InfoScreen::setTCPClient(WiFiTCPClient* client) {
  tcpClient = client;
}

void InfoScreen::update() {
  unsigned long currentMillis = millis();

  // Reset change flags
  indoorTempChanged = false;
  outdoorTempChanged = false;
  dateTimeChanged = false;
  networkStatusChanged = false;

  // Update network status
  updateNetworkStatus();

  // Check for inactivity timeout
  if (currentMillis - lastActivityTime >= INACTIVITY_TIMEOUT) {
    inactivityTimeoutReached = true;
    Serial.println("InfoScreen: Inactivity timeout reached");
  }

  // Update time and temperature data if internet is available and it's time to update
  if (tcpClient && tcpClient->isInternetAvailable() && (currentMillis - lastUpdateTime >= UPDATE_INTERVAL)) {

    // Fetch and update time
    String newDate = tcpClient->getCurrentDate();
    String newTime = tcpClient->getCurrentTime();
    String newDayOfWeek = tcpClient->getDayOfWeek();  // Get day of week from client

    // Check if time, date, or day of week has changed
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

    // Fetch outdoor temperature
    String newWeatherTemp = tcpClient->getWeatherTemperature();

    // Parse temperature value
    float newOutdoorTemp = 0.0;
    int tempEnd = newWeatherTemp.indexOf('°');
    if (tempEnd > 0) {
      newOutdoorTemp = newWeatherTemp.substring(0, tempEnd).toFloat();
    }

    // Check if outdoor temperature has changed
    if (abs(newOutdoorTemp - outdoorTemp) > 0.5) {
      outdoorTemp = newOutdoorTemp;
      outdoorTempChanged = true;
    }

    lastUpdateTime = currentMillis;
  }

  // Full screen redraw on initialization
  if (!screenInitialized) {
    gfx->fillScreen(BLACK);
    // On first startup, bypass animation (draw final arc lengths)
    drawDateTime();
    drawWiFiStatus();
    drawTemperatureArcs();
    screenInitialized = true;
  } else {
    // Update only those elements that changed
    if (indoorTempChanged || outdoorTempChanged) {
      drawTemperatureArcs();
    }
    if (dateTimeChanged) {
      drawDateTime();
    }
    if (networkStatusChanged) {
      drawWiFiStatus();
    }
  }

  // Check for touch events
  if (touchPanel && touchPanel->getHasNewPress()) {
    resetLastActivityTime();
    pageBackRequested = true;
    screenInitialized = false;  // Force a full redraw
    Serial.println("InfoScreen: Touch detected, setting pageBackRequested");
  }
}

void InfoScreen::formatDate() {
  // Parse YYYY/MM/DD format
  int firstSlash = currentDate.indexOf('/');
  int secondSlash = currentDate.indexOf('/', firstSlash + 1);

  if (firstSlash > 0 && secondSlash > 0) {
    String yearStr = currentDate.substring(0, firstSlash);
    String monthStr = currentDate.substring(firstSlash + 1, secondSlash);
    String dayStr = currentDate.substring(secondSlash + 1);

    // Remove leading zeros from day
    while (dayStr.length() > 1 && dayStr[0] == '0') {
      dayStr = dayStr.substring(1);
    }

    // Get month name
    int monthNum = monthStr.toInt();
    String monthName = getMonthName(monthNum);

    // Format as "17 MONTH" - e.g., "17 MAR" (day of week will be displayed separately)
    formattedDate = dayStr + " " + monthName;
  } else {
    formattedDate = currentDate;  // Fallback if date format is unexpected
  }
}

String InfoScreen::getMonthName(int month) {
  const char* months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

  if (month >= 1 && month <= 12) {
    return months[month - 1];
  }
  return "---";
}

// Replace the drawDateTime method with this more efficient version
void InfoScreen::drawDateTime() {
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;

  // Calculate required radius for our text content
  // This should be enough to contain all three lines of text without overlapping the arcs
  int clearRadius = 55;  // Adjust based on your testing
  
  // Clear a circular area in the center instead of a rectangle
  // This avoids drawing over the temperature arcs
  gfx->fillCircle(centerX, centerY, clearRadius, BLACK);

  // Draw time at the top, large size
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);

  int timeWidth = currentTime.length() * 18;  // Approximate width for size 3 text
  gfx->setCursor(centerX - timeWidth / 2, centerY - 30);
  gfx->print(currentTime);

  // Draw day of week in the middle
  gfx->setTextSize(2);
  int dayWidth = currentDayOfWeek.length() * 12;  // Approximate width for size 2 text
  gfx->setCursor(centerX - dayWidth / 2, centerY);
  gfx->print(currentDayOfWeek);

  // Draw date at the bottom
  gfx->setTextSize(2);
  int dateWidth = formattedDate.length() * 12;  // Approximate width for size 2 text
  gfx->setCursor(centerX - dateWidth / 2, centerY + 25);
  gfx->print(formattedDate);
}

void InfoScreen::drawTemperatureArcs() {
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int spacing = 5;  // Increased spacing between arcs

  // Calculate radii for indoor and outdoor arcs
  int indoorOuterRadius = min(gfx->width(), gfx->height()) / 2;
  int indoorInnerRadius = indoorOuterRadius - ARC_THICKNESS;
  int outdoorOuterRadius = indoorInnerRadius - spacing;
  int outdoorInnerRadius = outdoorOuterRadius - ARC_THICKNESS;

  // Compute effective angles by rotating by 180° (consistent for all draws)
  float effectiveStart = fmod(ARC_START_ANGLE + 180.0, 360.0);
  float effectiveEnd = fmod(ARC_END_ANGLE + 180.0, 360.0);
  if (effectiveEnd < effectiveStart) {
    effectiveEnd += 360.0;
  }
  float arcSpan = effectiveEnd - effectiveStart;

  // Compute target fill angles based on temperature values
  float normalizedIndoor = constrain((indoorTemp - 0) / 40.0, 0.0, 1.0);
  float newTargetIndoor = effectiveStart + normalizedIndoor * arcSpan;
  float normalizedOutdoor = constrain((outdoorTemp - (-20)) / 70.0, 0.0, 1.0);
  float newTargetOutdoor = effectiveStart + normalizedOutdoor * arcSpan;

  unsigned long currentMillis = millis();

  // On first startup or if any temperature changes, reset animation
  if (!screenInitialized || indoorTempChanged || outdoorTempChanged) {
    // For initial display, we can bypass animation
    if (!screenInitialized) {
      animatedIndoorAngle = newTargetIndoor;
      animatedOutdoorAngle = newTargetOutdoor;
    } else {
      // Not initial display, so we need to animate
      // Both animations start from minimum and grow together
      startIndoorAngle = effectiveStart;
      startOutdoorAngle = effectiveStart;
      // Reset animation time
      arcAnimationStartTime = currentMillis;
    }

    // Update target angles regardless
    targetIndoorAngle = newTargetIndoor;
    targetOutdoorAngle = newTargetOutdoor;
  }

  // Calculate animation progress
  float progress = min(1.0f, (currentMillis - arcAnimationStartTime) / (float)ANIMATION_DURATION);

  // Apply the same progress to both arcs - this ensures synchronized growth
  animatedIndoorAngle = startIndoorAngle + (targetIndoorAngle - startIndoorAngle) * progress;
  animatedOutdoorAngle = startOutdoorAngle + (targetOutdoorAngle - startOutdoorAngle) * progress;

  // Get colors for arcs
  uint16_t indoorColor = getTemperatureColor(indoorTemp);
  uint16_t outdoorColor = getTemperatureColor(outdoorTemp);
  uint16_t bgColor = gfx->color565(30, 30, 30);

  // Use a smaller angle step for smoother arcs
  const float angleStep = 0.5;

  // Draw both arcs using filled triangles to avoid the lines to center
  for (float angle = effectiveStart; angle < effectiveEnd; angle += angleStep) {
    float nextAngle = min(angle + angleStep, effectiveEnd);

    // Convert angles to radians
    float rad1 = angle * PI / 180.0;
    float rad2 = nextAngle * PI / 180.0;

    // Calculate points for indoor arc segment
    int x1Inner = centerX + indoorInnerRadius * cos(rad1);
    int y1Inner = centerY - indoorInnerRadius * sin(rad1);
    int x2Inner = centerX + indoorInnerRadius * cos(rad2);
    int y2Inner = centerY - indoorInnerRadius * sin(rad2);

    int x1Outer = centerX + indoorOuterRadius * cos(rad1);
    int y1Outer = centerY - indoorOuterRadius * sin(rad1);
    int x2Outer = centerX + indoorOuterRadius * cos(rad2);
    int y2Outer = centerY - indoorOuterRadius * sin(rad2);

    // Draw indoor arc segment (filled quadrilateral)
    uint16_t indoorPixelColor = (angle < animatedIndoorAngle) ? indoorColor : bgColor;
    gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, indoorPixelColor);
    gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, indoorPixelColor);

    // Calculate points for outdoor arc segment
    x1Inner = centerX + outdoorInnerRadius * cos(rad1);
    y1Inner = centerY - outdoorInnerRadius * sin(rad1);
    x2Inner = centerX + outdoorInnerRadius * cos(rad2);
    y2Inner = centerY - outdoorInnerRadius * sin(rad2);

    x1Outer = centerX + outdoorOuterRadius * cos(rad1);
    y1Outer = centerY - outdoorOuterRadius * sin(rad1);
    x2Outer = centerX + outdoorOuterRadius * cos(rad2);
    y2Outer = centerY - outdoorOuterRadius * sin(rad2);

    // Draw outdoor arc segment (filled quadrilateral)
    uint16_t outdoorPixelColor = (angle < animatedOutdoorAngle) ? outdoorColor : bgColor;
    gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, outdoorPixelColor);
    gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, outdoorPixelColor);
  }

  // Display temperature labels with DOUBLED text size
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);  // Doubled from 1 to 2

  // Indoor temperature label
  float midAngle = (effectiveStart + effectiveEnd) / 2.0;
  float midRad = midAngle * PI / 180.0;
  int textX = centerX;
  // Adjust Y position to account for larger text
  int textY = centerY - (indoorInnerRadius + ARC_THICKNESS / 2) * sin(midRad);
  String indoorText = String(int(round(indoorTemp)));
  int textWidth = indoorText.length() * 12;  // Doubled width for size 2 text
  gfx->setCursor(textX - textWidth / 2, textY - 12);  // Adjusted Y offset for larger text
  gfx->print(indoorText);

  // Outdoor temperature label
  textY = centerY - (outdoorInnerRadius + ARC_THICKNESS / 2) * sin(midRad);
  String outdoorText = String(int(round(outdoorTemp)));
  textWidth = outdoorText.length() * 12;  // Doubled width for size 2 text
  gfx->setCursor(textX - textWidth / 2, textY - 12);  // Adjusted Y offset for larger text
  gfx->print(outdoorText);
}
/*
// Also update the drawArc method to handle larger text
void InfoScreen::drawArc(int x, int y, int radius, int thickness,
                         float startAngle, float endAngle,
                         float value, float minValue, float maxValue,
                         uint16_t startColor, uint16_t endColor,
                         bool showTemp, const String& label,
                         float fillAngle) {
  uint16_t arcColor = getTemperatureColor(value);
  uint16_t bgColor = gfx->color565(30, 30, 30);

  // Outer loop: iterate over the angular range to fill the arc rotationally.
  for (float angle = startAngle; angle <= endAngle; angle += 0.2) {
    uint16_t colorToUse = (angle <= fillAngle) ? arcColor : bgColor;
    float rad = angle * PI / 180.0;
    // Inner loop: iterate over the radial thickness.
    for (int r = radius - thickness / 2; r <= radius + thickness / 2; r++) {
      int x1 = x + r * cos(rad);
      int y1 = y - r * sin(rad);
      gfx->drawPixel(x1, y1, colorToUse);
    }
  }

  // For the label: force it to be centered horizontally (X center)
  // and use a fixed angular midpoint based on the full arc for Y position.
  float midAngle = (startAngle + endAngle) / 2.0;
  float midRad = midAngle * PI / 180.0;
  int textX = x;                         // Always center on X axis.
  int textY = y - radius * sin(midRad);  // Y based on the arc's nominal radius.
  String tempText = label + " " + String(int(round(value))) + "\xB0C";
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);  // Doubled from 1 to 2
  int textWidth = tempText.length() * 12;  // Doubled width for size 2 text
  gfx->setCursor(textX - textWidth / 2, textY - 6);  // Adjusted Y offset for larger text
  gfx->print(tempText);
}
*/
uint16_t InfoScreen::getTemperatureColor(float temperature) {
  // Map temperature to a blue-yellow-red color spectrum.
  float normalizedTemp;
  if (temperature <= 0) {
    normalizedTemp = 0;
  } else if (temperature >= 40) {
    normalizedTemp = 1.0;
  } else {
    normalizedTemp = temperature / 40.0;
  }

  uint8_t r, g, b;
  if (normalizedTemp < 0.5) {
    float t = normalizedTemp * 2;
    r = 255 * t;
    g = 255 * t;
    b = 255 * (1 - t);
  } else {
    float t = (normalizedTemp - 0.5) * 2;
    r = 255;
    g = 255 * (1 - t);
    b = 0;
  }
  return gfx->color565(r, g, b);
}

void InfoScreen::drawWiFiStatus() {
  int iconSize = 15;
  int iconX = gfx->width() / 2;
  int iconY = gfx->height() - 20;

  // Clear the area to avoid artifacts.
  gfx->fillRect(iconX - iconSize - 5, iconY - iconSize - 5,
                (iconSize + 5) * 2, (iconSize + 5) * 2, BLACK);
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

  // Draw WiFi arcs using the native drawArc (for the icon).
  for (int i = 1; i <= 3; i++) {
    int radius = (size / 3) * i;
    if (wifiConnected && wifiStrength >= i) {
      gfx->drawArc(x, y, radius, radius - 1, 225, 315, wifiColor);
    } else {
      gfx->drawArc(x, y, radius, radius - 1, 225, 315, gfx->color565(60, 60, 60));
    }
  }
  gfx->fillCircle(x, y, 2, wifiColor);
  if (!wifiConnected) {
    gfx->drawLine(x - size / 2, y - size / 2, x + size / 2, y + size / 2, gfx->color565(255, 0, 0));
    gfx->drawLine(x - size / 2 + 1, y - size / 2, x + size / 2 + 1, y + size / 2, gfx->color565(255, 0, 0));
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
  if (tcpClient) {
    bool newWifiConnected = tcpClient->isWiFiConnected();
    bool newInternetConnected = tcpClient->isInternetAvailable();
    bool newMqttConnected = tcpClient->isMQTTConnected();
    int newWifiStrength = tcpClient->getWiFiSignalStrength();

    if (newWifiConnected != wifiConnected || newInternetConnected != internetConnected ||
        newMqttConnected != mqttConnected || newWifiStrength != wifiStrength) {

      wifiConnected = newWifiConnected;
      internetConnected = newInternetConnected;
      mqttConnected = newMqttConnected;
      wifiStrength = newWifiStrength;

      networkStatusChanged = true;
    }
  }
}

void InfoScreen::updateDateTime(const String& date, const String& time) {
  if (date != currentDate || time != currentTime) {
    currentDate = date;
    currentTime = time;
    
    // Update day of week whenever date changes
    if (tcpClient) {
      currentDayOfWeek = tcpClient->getDayOfWeek();
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
  if (abs(temperature - indoorTemp) > 0.5) {
    indoorTemp = temperature;
    indoorTempChanged = true;
    drawTemperatureArcs();
  }
}

void InfoScreen::updateOutdoorTemperature(const String& temperature) {
  float newTemp = 0.0;
  int tempEnd = temperature.indexOf('°');
  if (tempEnd > 0) {
    newTemp = temperature.substring(0, tempEnd).toFloat();
  }
  if (abs(newTemp - outdoorTemp) > 0.5) {
    outdoorTemp = newTemp;
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
  Serial.println("InfoScreen::resetScreen called - forcing redraw");
  screenInitialized = false;
  if (tcpClient) {
    wifiConnected = tcpClient->isWiFiConnected();
    internetConnected = tcpClient->isInternetAvailable();
    mqttConnected = tcpClient->isMQTTConnected();
    wifiStrength = tcpClient->getWiFiSignalStrength();
  }
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