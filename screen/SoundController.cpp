#include "SoundController.h"

SoundController::SoundController(Arduino_GFX* graphics, TouchPanel* touch, WiFiTCPClient* client)
  : gfx(graphics), touchPanel(touch), tcpClient(client), setpoint(50), lastSetpoint(-1), 
    lastSentSetpoint(-1), lastSetpointChangeTime(0), lastSetpointSentTime(0), 
    initialized(false), pageBackRequested(false), volume(0.5), lastDrawnSetpoint(-1), 
    colorChange(false), lastSentTime(0) {}

void SoundController::updateSetpoint(int level) {
  setpoint = level;
  lastSetpointChangeTime = millis();  // Update the last change timestamp
}

void SoundController::updateScreen() {
  checkTouchInput();

  if (!initialized) {
    requestInitialSetpointFromServer();
    drawStaticVolumeElements();
    initialized = true;
    Serial.println("Static volume elements drawn.");
  }

  if (setpoint != lastSetpoint) {
    drawSetPoint();
  }

  // Check if the setpoint needs to be sent to the server
  unsigned long currentMillis = millis();
  if ((setpoint != lastSentSetpoint) && (currentMillis - lastSetpointSentTime >= 25)) {
    sendSetpointToServer();
    lastSetpointSentTime = currentMillis;  // Record the time of the last sent setpoint
    lastSentSetpoint = setpoint;           // Update the last sent setpoint
  }
}

void SoundController::incrementSetpoint() {
  if (setpoint < 100) {
    setpoint++;
    lastSetpointChangeTime = millis();  // Update the last change timestamp
  }
}

void SoundController::decrementSetpoint() {
  if (setpoint > 0) {
    setpoint--;
    lastSetpointChangeTime = millis();  // Update the last change timestamp
  }
}

void SoundController::sendSetpointToServer() {
  // Use MQTT to send the setpoint
  if (tcpClient) {
    // Check if MQTT is configured before sending
    if (tcpClient->hasSoundMQTTConfigured()) {
      tcpClient->sendMQTTMessage("esp32/sound/setpoint", String(setpoint));
      Serial.println("Sound setpoint sent via MQTT: " + String(setpoint));
    } else {
      Serial.println("Sound MQTT not configured, skipping setpoint send");
    }
  } else {
    Serial.println("Error: TCP client is not available.");
  }
}

void SoundController::requestInitialSetpointFromServer() {
  // Use MQTT to request the initial setpoint
  if (tcpClient) {
    // Check if MQTT is configured before requesting
    if (tcpClient->hasSoundMQTTConfigured()) {
      tcpClient->sendMQTTMessage("esp32/sound/get_setpoint", "request");
      Serial.println("Sent get_setpoint request via MQTT.");
    } else {
      Serial.println("Sound MQTT not configured, using default setpoint");
    }
    
    // For now, we'll just use the default setpoint
    setpoint = 50;
    drawSetPoint();
  } else {
    Serial.println("Error: TCP client is not available for initial setpoint request.");
  }
}

void SoundController::drawStaticVolumeElements() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 140);
  int16_t x1, y1;
  uint16_t w, h;
  String demoString = "SOUND";
  gfx->getTextBounds(demoString, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor((240 - w) / 2, (150 - h) / 2);
  //gfx->print(demoString);
  //gfx->setTextSize(2);
  drawBackButton();
}

void SoundController::drawSetPoint() {
  //gfx->fillRect(75, 90, 90, 60, BLACK);
  gfx->fillCircle(120, 120, 55, BLACK);

  gfx->setTextSize(5);
  gfx->setTextColor(WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  String setpointStr = String(setpoint);
  gfx->getTextBounds(setpointStr, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor((240 - w) / 2, (240 - h) / 2);
  gfx->print(setpointStr);
  drawVolumeBars(setpoint);
  lastSetpoint = setpoint;
}

void SoundController::drawUpDownArrow(int x, int y, bool up) {
  if (up) {
    gfx->fillTriangle(x - 10, y + 10, x + 10, y + 10, x, y - 20, WHITE);
  } else {
    gfx->fillTriangle(x - 10, y - 10, x + 10, y - 10, x, y + 20, WHITE);
  }
}

uint16_t SoundController::adjustBrightness(uint16_t color, float brightness) {
  uint8_t r = ((color >> 11) & 0x1F) * brightness;
  uint8_t g = ((color >> 5) & 0x3F) * brightness;
  uint8_t b = (color & 0x1F) * brightness;
  return gfx->color565(r << 3, g << 2, b << 3);
}

uint16_t SoundController::interpolateColor(uint16_t color1, uint16_t color2, float fraction) {
  uint8_t r1 = (color1 >> 11) & 0x1F;
  uint8_t g1 = (color1 >> 5) & 0x3F;
  uint8_t b1 = color1 & 0x1F;

  uint8_t r2 = (color2 >> 11) & 0x1F;
  uint8_t g2 = (color2 >> 5) & 0x3F;
  uint8_t b2 = color2 & 0x1F;

  uint8_t r = r1 + fraction * (r2 - r1);
  uint8_t g = g1 + fraction * (g2 - g1);
  uint8_t b = b1 + fraction * (b2 - b1);

  return gfx->color565(r << 3, g << 2, b << 3);
}

void SoundController::drawVolumeBars(int volume) {
  int centerX = 120;
  int centerY = 120;
  int outerRadius = 120;
  int innerRadius = 90;
  float gap = 0;

  float angleStep = (300.0 / barCount) - gap;

  int fullBars = volume / (100 / barCount);
  float partialBarBrightness = (volume % (100 / barCount)) / static_cast<float>(100 / barCount);

  uint16_t startColor = gfx->color565(0, 255, 0);  // Green
  uint16_t endColor = gfx->color565(255, 0, 0);    // Red

  for (int i = 0; i < barCount; i++) {
    float angle1 = (120 + i * (angleStep + gap)) * PI / 180.0;
    float angle2 = (120 + (i + 1) * angleStep + i * gap) * PI / 180.0;

    int x1 = centerX + innerRadius * cos(angle1);
    int y1 = centerY + innerRadius * sin(angle1);
    int x2 = centerX + outerRadius * cos(angle1);
    int y2 = centerY + outerRadius * sin(angle1);
    int x3 = centerX + outerRadius * cos(angle2);
    int y3 = centerY + outerRadius * sin(angle2);
    int x4 = centerX + innerRadius * cos(angle2);
    int y4 = centerY + innerRadius * sin(angle2);

    float fraction = (float)i / (barCount - 1);
    uint16_t color = interpolateColor(startColor, endColor, fraction);

    if (i == fullBars) {
      color = adjustBrightness(color, partialBarBrightness);
    } else if (i > fullBars) {
      color = BLACK;
    }

    gfx->fillTriangle(x1, y1, x2, y2, x3, y3, color);
    gfx->fillTriangle(x1, y1, x3, y3, x4, y4, color);
  }
}

void SoundController::drawBackButton() {
  int centerX = 120;
  int centerY = 210;
  int width = 40;
  int height = 20;
  gfx->fillEllipse(centerX, centerY, width, height, gfx->color565(128, 128, 128));  // Gray back button
  // Draw the arrow inside the back button
  gfx->fillTriangle(centerX - 5, centerY, centerX + 5, centerY - 10, centerX + 5, centerY + 10, WHITE);  // White arrow
}

void SoundController::checkTouchInput() {
  if (touchPanel->isHold()) {
    if (!pageBackRequested) {    // Check if back button press is new
      pageBackRequested = true;  // Set flag if back button is pressed
      //screenInitialized=false;
    }
  }
}

bool SoundController::isPageBackRequested() const {
  return pageBackRequested;
}

void SoundController::setTCPClient(WiFiTCPClient* client) {
  tcpClient = client;
}

void SoundController::resetPageBackRequest() {
  pageBackRequested = false;
  initialized = false;
}