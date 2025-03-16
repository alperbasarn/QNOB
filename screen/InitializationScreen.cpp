#include "InitializationScreen.h"

InitializationScreen::InitializationScreen(Arduino_GFX* graphics, TouchPanel* touch)
    : gfx(graphics), touchPanel(touch), progressValue(0), previousProgress(0),
      screenInitialized(false), currentStep(INIT_SYSTEM) {
}

void InitializationScreen::setProgress(int progress) {
    previousProgress = progressValue;
    progressValue = constrain(progress, 0, 100);
    updateCurrentStep();
    updateScreen();
}

void InitializationScreen::updateCurrentStep() {
    // Update the current step based on progress value
    InitStep previousStep = currentStep;
    
    if (progressValue < 20) {
        currentStep = INIT_SYSTEM;
    } else if (progressValue < 40) {
        currentStep = INIT_DISPLAY;
    } else if (progressValue < 60) {
        currentStep = INIT_CONTROLLERS;
    } else if (progressValue < 80) {
        currentStep = INIT_WIFI;
    } else if (progressValue < 90) {
        currentStep = INIT_MQTT;
    } else {
        currentStep = INIT_COMPLETE;
    }
    
    // If step changed, force a redraw of the center area
    if (previousStep != currentStep) {
        drawStepInfo();
    }
}

void InitializationScreen::setMQTTStatus(bool isConnected) {
    mqttConnected = isConnected;
    // Only redraw if we're in the MQTT initialization step
    if (currentStep == INIT_MQTT || currentStep == INIT_COMPLETE) {
        drawStepInfo();
    }
}

void InitializationScreen::updateScreen() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    int radius = min(centerX, centerY) - 2; // Use almost full screen
    
    if (!screenInitialized) {
        // Clear screen with a dark background
        gfx->fillScreen(BLACK);
        
        // Draw a simple circular outline
        gfx->drawCircle(centerX, centerY, radius, gfx->color565(40, 40, 80));
        
        // Draw background arc (dim blue) - continuous around the edge
        drawBackgroundArc(centerX, centerY, radius);
        
        // Initialize to 0% progress
        previousProgress = 0;
        drawProgressArc(centerX, centerY, radius, 0, 0);
        
        screenInitialized = true;
        
        // Draw initial step info
        drawStepInfo();
    } else {
        // Only draw the new progress segment (from previousProgress to progressValue)
        drawProgressArc(centerX, centerY, radius, previousProgress, progressValue);
        
        // Update progress percentage text
        drawProgressText(centerX, centerY, radius);
    }
}

void InitializationScreen::drawBackgroundArc(int centerX, int centerY, int radius) {
    // Thicker arc (12px)
    int arcThickness = 12;
    int arcRadius = radius - (arcThickness / 2);
    
    // Draw background arc (dim blue)
    for (int angle = 0; angle < 360; angle += 1) {
        float rad = angle * PI / 180.0;
        int x = centerX + arcRadius * cos(rad);
        int y = centerY + arcRadius * sin(rad);
        gfx->fillCircle(x, y, arcThickness/2, gfx->color565(10, 10, 40));
    }
}

// Replace the drawProgressArc method in InitializationScreen.cpp with this version

void InitializationScreen::drawProgressArc(int centerX, int centerY, int radius, int startProgress, int endProgress) {
    // Thicker arc (12px)
    int arcThickness = 12;
    int arcRadius = radius - (arcThickness / 2);
    
    // Calculate start and end angles for the arc segment
    int startAngle = map(startProgress, 0, 100, 0, 360);
    int endAngle = map(endProgress, 0, 100, 0, 360);
    
    // Draw only the new segment of the progress arc
    for (int angle = startAngle; angle <= endAngle; angle += 1) {
        float rad = angle * PI / 180.0;
        int x = centerX + arcRadius * cos(rad);
        int y = centerY + arcRadius * sin(rad);
        
        // Create a RED to GREEN gradient based on progress
        uint8_t red = map(angle, 0, 360, 255, 0);    // RED decreases as progress increases
        uint8_t green = map(angle, 0, 360, 0, 255);  // GREEN increases as progress increases
        
        gfx->fillCircle(x, y, arcThickness/2, gfx->color565(red, green, 0));
    }
}

void InitializationScreen::drawProgressText(int centerX, int centerY, int radius) {
    // Clear the area for percentage text
    gfx->fillRect(centerX - 20, centerY - radius * 0.5 - 10, 40, 20, BLACK);
    
    // Show percentage in small text at the top
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);
    String percentText = String(progressValue) + "%";
    int textWidth = percentText.length() * 6;
    gfx->setCursor(centerX - textWidth/2, centerY - radius * 0.5);
    gfx->print(percentText);
}

void InitializationScreen::drawStepInfo() {
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    int radius = min(centerX, centerY) - 2;
    
    // Clear the center area for step text and info
    gfx->fillCircle(centerX, centerY, radius * 0.6, BLACK);
    
    // Show current initialization step in the center
    gfx->setTextSize(2);
    String stepText;
    uint16_t stepColor;
    
    switch(currentStep) {
        case INIT_SYSTEM:
            stepText = "System";
            stepColor = WHITE;
            break;
        case INIT_DISPLAY:
            stepText = "Display";
            stepColor = WHITE;
            break;
        case INIT_CONTROLLERS:
            stepText = "Controllers";
            stepColor = WHITE;
            break;
        case INIT_WIFI:
            stepText = "WiFi";
            stepColor = wifiConnected ? gfx->color565(100, 255, 100) : gfx->color565(100, 200, 255);
            break;
        case INIT_MQTT:
            stepText = "MQTT";
            stepColor = mqttConnected ? gfx->color565(100, 255, 100) : gfx->color565(255, 100, 100);
            break;
        case INIT_COMPLETE:
            stepText = "Complete";
            stepColor = gfx->color565(100, 255, 100);
            break;
    }
    
    // Display the step text (larger and centered)
    int textWidth = stepText.length() * 12; // Width for text size 2
    gfx->setCursor(centerX - textWidth/2, centerY - 10);
    gfx->setTextColor(stepColor);
    gfx->print(stepText);
    
    // Display status text below
    gfx->setTextSize(1);
    String statusText = "";
    
    switch(currentStep) {
        case INIT_WIFI:
            statusText = wifiConnected ? "Connected" : "Connecting...";
            break;
        case INIT_MQTT:
            statusText = mqttConnected ? "Connected" : "Connecting...";
            break;
        case INIT_SYSTEM:
            statusText = "Starting...";
            break;
        case INIT_DISPLAY:
            statusText = "Configuring...";
            break;
        case INIT_CONTROLLERS:
            statusText = "Initializing...";
            break;
        case INIT_COMPLETE:
            statusText = "Ready";
            break;
    }
    
    // Display status text
    textWidth = statusText.length() * 6; // Width for text size 1
    gfx->setCursor(centerX - textWidth/2, centerY + 15);
    gfx->setTextColor(WHITE);
    gfx->print(statusText);
    
    // Draw progress text at the top
    drawProgressText(centerX, centerY, radius);
}

void InitializationScreen::setWiFiStatus(bool connected, int strength) {
    bool statusChanged = (wifiConnected != connected);
    
    wifiConnected = connected;
    wifiStrength = constrain(strength, 0, 3);
    
    // Redraw if we're in the WiFi initialization step or if status changed
    if (statusChanged || currentStep == INIT_WIFI || currentStep == INIT_MQTT || currentStep == INIT_COMPLETE) {
        drawStepInfo();
    }
}

void InitializationScreen::reset() {
    progressValue = 0;
    previousProgress = 0;
    currentStep = INIT_SYSTEM;
    wifiConnected = false;
    wifiStrength = 0;
    mqttConnected = false;
    screenInitialized = false;
}