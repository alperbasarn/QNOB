#include "InitializationScreen.h"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "InitScreen";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

InitializationScreen::InitializationScreen(LGFX* graphics)
    : gfx(graphics), progressValue(0), previousProgress(0),
      screenInitialized(false), qnobTextDrawn(false),
      wifiConnected(false), wifiStrength(0), mqttConnected(false),
      animatedProgressAngle(0), targetProgressAngle(0),
      startProgressAngle(0), animationStartTime(0),
      lastAnimationUpdateTime(0) {
}

void InitializationScreen::setProgress(int progress) {
    previousProgress = progressValue;
    progressValue = constrain(progress, 0, 100);
    ESP_LOGI(TAG, "Progress updated: %d", progress);

    // Calculate new target angle based on progress
    float prevAngle = targetProgressAngle;
    targetProgressAngle = mapValue(static_cast<float>(progressValue), 0, 100, 0, 360);

    // If this is a significant progress change, update animation start time
    if (std::abs(targetProgressAngle - prevAngle) > 1) {
        animationStartTime = millis();
        startProgressAngle = animatedProgressAngle;
    }

    updateScreen();
}

void InitializationScreen::setMQTTStatus(bool isConnected) {
    mqttConnected = isConnected;
}

void InitializationScreen::updateScreen() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    int radius = std::min(centerX, centerY) - 2;  // Use almost full screen

    if (!screenInitialized) {
        // Clear screen with a dark background
        gfx->fillScreen(TFT_BLACK);

        // Draw a simple circular outline
        gfx->drawCircle(centerX, centerY, radius, gfx->color565(40, 40, 80));

        // Draw background arc (dim blue) - continuous around the edge
        drawBackgroundArc(centerX, centerY, radius);

        // Initialize to 0% progress
        animatedProgressAngle = 0;
        startProgressAngle = 0;
        targetProgressAngle = 0;
        animationStartTime = millis();

        screenInitialized = true;

        // Draw QNOB text once at the beginning
        drawQNOBText(centerX, centerY, radius);
    } else {
        // Update animation
        int64_t currentMillis = millis();
        if (currentMillis - lastAnimationUpdateTime >= 16) {  // ~60fps update rate
            updateAnimation();
            lastAnimationUpdateTime = currentMillis;

            // Redraw the progress arc with the current animated angle
            drawProgressArc(centerX, centerY, radius);
        }
    }
}

void InitializationScreen::updateAnimation() {
    int64_t currentMillis = millis();

    // Calculate animation progress (0.0 to 1.0)
    float progress = constrain(static_cast<float>(currentMillis - animationStartTime) / ANIMATION_DURATION, 0.0f, 1.0f);

    // Use easing function for smoother animation (ease-out cubic)
    float easedProgress = 1.0f - std::pow(1.0f - progress, 3);

    // Interpolate between start and target angles
    animatedProgressAngle = startProgressAngle + (targetProgressAngle - startProgressAngle) * easedProgress;
}

void InitializationScreen::drawBackgroundArc(int centerX, int centerY, int radius) {
    // Thicker arc (30px) - scale for larger displays
    int arcThickness = 30 * gfx->width() / 240;  // Scale based on 240px reference
    int arcRadius = radius - (arcThickness / 2);

    // Define the step size for smoother arc
    const float angleStep = 0.5f;

    // Draw background arc (dim blue) using triangles
    for (float angle = 0; angle < 360; angle += angleStep) {
        float nextAngle = angle + angleStep;

        // Convert angles to radians
        float rad1 = angle * M_PI / 180.0f;
        float rad2 = nextAngle * M_PI / 180.0f;

        // Calculate points for arc segment
        int x1Inner = centerX + (arcRadius - arcThickness / 2) * std::cos(rad1);
        int y1Inner = centerY + (arcRadius - arcThickness / 2) * std::sin(rad1);
        int x2Inner = centerX + (arcRadius - arcThickness / 2) * std::cos(rad2);
        int y2Inner = centerY + (arcRadius - arcThickness / 2) * std::sin(rad2);

        int x1Outer = centerX + (arcRadius + arcThickness / 2) * std::cos(rad1);
        int y1Outer = centerY + (arcRadius + arcThickness / 2) * std::sin(rad1);
        int x2Outer = centerX + (arcRadius + arcThickness / 2) * std::cos(rad2);
        int y2Outer = centerY + (arcRadius + arcThickness / 2) * std::sin(rad2);

        // Draw arc segment (filled quadrilateral using two triangles)
        uint16_t bgColor = gfx->color565(10, 10, 40);
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, bgColor);
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, bgColor);
    }
}

void InitializationScreen::drawProgressArc(int centerX, int centerY, int radius) {
    // Thicker arc (30px) - scale for larger displays
    int arcThickness = 30 * gfx->width() / 240;
    int arcRadius = radius - (arcThickness / 2);

    // Use a smaller angle step for smoother arcs
    const float angleStep = 0.5f;

    // Red color for progress arc
    uint16_t progressColor = TFT_RED;

    // Draw progress arc using triangles
    for (float angle = 0; angle < animatedProgressAngle; angle += angleStep) {
        float nextAngle = std::min(angle + angleStep, animatedProgressAngle);

        // Convert angles to radians
        float rad1 = angle * M_PI / 180.0f;
        float rad2 = nextAngle * M_PI / 180.0f;

        // Calculate points for arc segment
        int x1Inner = centerX + (arcRadius - arcThickness / 2) * std::cos(rad1);
        int y1Inner = centerY + (arcRadius - arcThickness / 2) * std::sin(rad1);
        int x2Inner = centerX + (arcRadius - arcThickness / 2) * std::cos(rad2);
        int y2Inner = centerY + (arcRadius - arcThickness / 2) * std::sin(rad2);

        int x1Outer = centerX + (arcRadius + arcThickness / 2) * std::cos(rad1);
        int y1Outer = centerY + (arcRadius + arcThickness / 2) * std::sin(rad1);
        int x2Outer = centerX + (arcRadius + arcThickness / 2) * std::cos(rad2);
        int y2Outer = centerY + (arcRadius + arcThickness / 2) * std::sin(rad2);

        // Draw arc segment (filled quadrilateral using two triangles)
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, progressColor);
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, progressColor);
    }
}

void InitializationScreen::drawQNOBText(int centerX, int centerY, int radius) {
    // Clear the center area for QNOB text
    gfx->fillCircle(centerX, centerY, radius * 0.6f, TFT_BLACK);

    // Scale text size based on display size
    int textScale = gfx->width() / 48;  // ~5 for 240px, ~10 for 466px
    gfx->setTextSize(textScale);

    // Calculate text dimensions for proper centering
    // For LovyanGFX, we use textWidth instead of getTextBounds
    int qWidth = gfx->textWidth("Q");
    int qnobWidth = gfx->textWidth("QNOB");
    int textHeight = gfx->fontHeight();

    // Calculate starting position to center "QNOB"
    int startX = centerX - (qnobWidth / 2);
    int textY = centerY - (textHeight / 2);

    // First draw the "Q" in red
    gfx->setTextColor(TFT_RED);
    gfx->setCursor(startX, textY);
    gfx->print("Q");

    // Then draw "NOB" in white right after Q
    gfx->setTextColor(TFT_WHITE);
    gfx->setCursor(startX + qWidth, textY);
    gfx->print("NOB");

    qnobTextDrawn = true;
}

void InitializationScreen::setWiFiStatus(bool connected, int strength) {
    wifiConnected = connected;
    wifiStrength = constrain(strength, 0, 3);
}

void InitializationScreen::reset() {
    progressValue = 0;
    previousProgress = 0;
    wifiConnected = false;
    wifiStrength = 0;
    mqttConnected = false;
    screenInitialized = false;
    qnobTextDrawn = false;
    animatedProgressAngle = 0;
    targetProgressAngle = 0;
    startProgressAngle = 0;
    animationStartTime = millis();
}
