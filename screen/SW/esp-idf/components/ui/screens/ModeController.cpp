#include "ModeController.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ModeController::ModeController(LGFX* graphics, TouchPanel* touch)
    : gfx(graphics), touchPanel(touch),
      currentMode(MODE_SOUND), lastMode(static_cast<Mode>(-1)), active(false) {
}

void ModeController::updateScreen() {
    if (!active) {
        return;
    }

    if (currentMode != lastMode) {
        lastMode = currentMode;
        drawModeLogo();
    }
}

void ModeController::nextMode() {
    currentMode = static_cast<Mode>((currentMode + 1) % MODE_COUNT);
    drawModeLogo();
}

void ModeController::previousMode() {
    currentMode = static_cast<Mode>((currentMode - 1 + MODE_COUNT) % MODE_COUNT);
    drawModeLogo();
}

void ModeController::setActive(bool isActive) {
    active = isActive;
    if (active) {
        drawModeLogo();
    }
}

void ModeController::setMode(Mode mode) {
    if (mode >= 0 && mode < MODE_COUNT) {
        currentMode = mode;
        if (active) {
            drawModeLogo();
        }
    }
}

void ModeController::drawModeLogo() {
    gfx->fillScreen(TFT_BLACK);

    switch (currentMode) {
        case MODE_LIGHT:
            drawBulb();
            break;
        case MODE_TEMPERATURE:
            drawThermometer();
            break;
        case MODE_SOUND:
            drawSpeaker();
            break;
        default:
            break;
    }
}

void ModeController::drawThermometer() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;

    // Scale for display size (reference: 240px)
    float scale = static_cast<float>(gfx->width()) / 240.0f;

    // Draw a simple thermometer
    int bulbRadius = static_cast<int>(15 * scale);
    int stemWidth = static_cast<int>(10 * scale);
    int stemHeight = static_cast<int>(70 * scale);
    int bulbOffsetY = static_cast<int>(30 * scale);
    int stemOffsetY = static_cast<int>(40 * scale);

    gfx->fillCircle(centerX, centerY + bulbOffsetY, bulbRadius, TFT_RED);
    gfx->fillRect(centerX - stemWidth / 2, centerY - stemOffsetY, stemWidth, stemHeight, TFT_RED);

    // Draw outline
    gfx->drawCircle(centerX, centerY + bulbOffsetY, bulbRadius + 2, TFT_WHITE);
    gfx->drawRect(centerX - stemWidth / 2 - 2, centerY - stemOffsetY, stemWidth + 4, stemHeight, TFT_WHITE);

    // Draw temperature ticks
    for (int i = 0; i < 5; i++) {
        int y = centerY - static_cast<int>(30 * scale) + i * static_cast<int>(10 * scale);
        gfx->drawLine(centerX - static_cast<int>(12 * scale), y,
                      centerX - stemWidth / 2 - 2, y, TFT_WHITE);
    }

    // Draw title at bottom
    int textScale = std::max(1, static_cast<int>(2 * scale));
    gfx->setTextSize(textScale);
    gfx->setTextColor(TFT_WHITE);

    const char* title = "TEMPERATURE";
    int textWidth = gfx->textWidth(title);
    gfx->setCursor(centerX - textWidth / 2, centerY + static_cast<int>(60 * scale));
    gfx->print(title);
}

void ModeController::drawBulb() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;

    float scale = static_cast<float>(gfx->width()) / 240.0f;

    // Draw a simple light bulb as a circle with rays
    int bulbRadius = static_cast<int>(25 * scale);
    uint16_t yellow = gfx->color565(255, 255, 0);
    uint16_t gray = gfx->color565(180, 180, 180);

    gfx->fillCircle(centerX, centerY, bulbRadius, yellow);

    // Draw bulb base
    int baseWidth = static_cast<int>(14 * scale);
    int baseHeight = static_cast<int>(15 * scale);
    gfx->fillRect(centerX - baseWidth / 2, centerY + bulbRadius, baseWidth, baseHeight, gray);

    // Add short rays around the bulb
    int rayInner = static_cast<int>(30 * scale);
    int rayOuter = static_cast<int>(40 * scale);

    for (int i = 0; i < 360; i += 45) {
        float rad = i * M_PI / 180.0f;
        int x1 = centerX + rayInner * std::cos(rad);
        int y1 = centerY + rayInner * std::sin(rad);
        int x2 = centerX + rayOuter * std::cos(rad);
        int y2 = centerY + rayOuter * std::sin(rad);
        gfx->drawLine(x1, y1, x2, y2, yellow);
    }

    // Draw title at bottom
    int textScale = std::max(1, static_cast<int>(2 * scale));
    gfx->setTextSize(textScale);
    gfx->setTextColor(TFT_WHITE);

    const char* title = "LIGHT";
    int textWidth = gfx->textWidth(title);
    gfx->setCursor(centerX - textWidth / 2, centerY + static_cast<int>(60 * scale));
    gfx->print(title);
}

void ModeController::drawSpeaker() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;

    float scale = static_cast<float>(gfx->width()) / 240.0f;
    uint16_t lightGray = gfx->color565(192, 192, 192);
    uint16_t darkGray = gfx->color565(160, 160, 160);

    // Speaker body
    int bodyWidth = static_cast<int>(20 * scale);
    int bodyHeight = static_cast<int>(40 * scale);
    gfx->fillRect(centerX - static_cast<int>(25 * scale), centerY - bodyHeight / 2,
                  bodyWidth, bodyHeight, lightGray);

    // Speaker cone
    int coneLeft = centerX - static_cast<int>(5 * scale);
    int coneRight = centerX + static_cast<int>(20 * scale);
    int coneTop = centerY - static_cast<int>(30 * scale);
    int coneBottom = centerY + static_cast<int>(30 * scale);

    gfx->fillTriangle(coneLeft, coneTop, coneLeft, coneBottom, coneRight, centerY, darkGray);

    // Speaker outline
    gfx->drawRect(centerX - static_cast<int>(25 * scale), centerY - bodyHeight / 2,
                  bodyWidth, bodyHeight, TFT_WHITE);
    gfx->drawTriangle(coneLeft, coneTop, coneLeft, coneBottom, coneRight, centerY, TFT_WHITE);

    // Sound waves using arcs
    int waveX = centerX + static_cast<int>(25 * scale);
    gfx->drawArc(waveX, centerY, static_cast<int>(10 * scale), static_cast<int>(5 * scale), 270, 90, TFT_WHITE);
    gfx->drawArc(waveX + static_cast<int>(10 * scale), centerY,
                 static_cast<int>(15 * scale), static_cast<int>(5 * scale), 270, 90, TFT_WHITE);

    // Draw title at bottom
    int textScale = std::max(1, static_cast<int>(2 * scale));
    gfx->setTextSize(textScale);
    gfx->setTextColor(TFT_WHITE);

    const char* title = "SOUND";
    int textWidth = gfx->textWidth(title);
    gfx->setCursor(centerX - textWidth / 2, centerY + static_cast<int>(60 * scale));
    gfx->print(title);
}
