#include "InitializationScreen.h"

InitializationScreen::InitializationScreen(Arduino_GFX* graphics, TouchPanel* touch)
    : gfx(graphics), touchPanel(touch), progressValue(0), previousProgress(0),
      screenInitialized(false), currentStep(INIT_SYSTEM), 
      animatedProgressAngle(0), targetProgressAngle(0),
      lastAnimationUpdateTime(0) {
}

void InitializationScreen::setProgress(int progress) {
    previousProgress = progressValue;
    progressValue = constrain(progress, 0, 100);
    
    // Calculate new target angle based on progress
    float prevAngle = targetProgressAngle;
    targetProgressAngle = map(progressValue, 0, 100, 0, 360);
    
    // If this is a significant progress change, update animation start time
    if (abs(targetProgressAngle - prevAngle) > 1) {
        animationStartTime = millis();
        startProgressAngle = animatedProgressAngle;
    }
    
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
        animatedProgressAngle = 0;
        startProgressAngle = 0;
        targetProgressAngle = 0;
        animationStartTime = millis();
        
        screenInitialized = true;
        
        // Draw initial step info
        drawStepInfo();
    } else {
        // Update animation
        unsigned long currentMillis = millis();
        if (currentMillis - lastAnimationUpdateTime >= 16) { // ~60fps update rate
            updateAnimation();
            lastAnimationUpdateTime = currentMillis;
            
            // Redraw the progress arc with the current animated angle
            drawProgressArc(centerX, centerY, radius);
            
            // Update progress percentage text
            drawProgressText(centerX, centerY, radius);
        }
    }
}

void InitializationScreen::updateAnimation() {
    unsigned long currentMillis = millis();
    
    // Calculate animation progress (0.0 to 1.0)
    float progress = constrain((float)(currentMillis - animationStartTime) / ANIMATION_DURATION, 0.0, 1.0);
    
    // Use easing function for smoother animation (ease-out cubic)
    float easedProgress = 1.0 - pow(1.0 - progress, 3);
    
    // Interpolate between start and target angles
    animatedProgressAngle = startProgressAngle + (targetProgressAngle - startProgressAngle) * easedProgress;
}

void InitializationScreen::drawBackgroundArc(int centerX, int centerY, int radius) {
    // Thicker arc (30px)
    int arcThickness = 30;
    int arcRadius = radius - (arcThickness / 2);
    
    // Define the step size for smoother arc
    const float angleStep = 0.5;
    
    // Draw background arc (dim blue) using triangles
    for (float angle = 0; angle < 360; angle += angleStep) {
        float nextAngle = angle + angleStep;
        
        // Convert angles to radians
        float rad1 = angle * PI / 180.0;
        float rad2 = nextAngle * PI / 180.0;
        
        // Calculate points for arc segment
        int x1Inner = centerX + (arcRadius - arcThickness/2) * cos(rad1);
        int y1Inner = centerY + (arcRadius - arcThickness/2) * sin(rad1);
        int x2Inner = centerX + (arcRadius - arcThickness/2) * cos(rad2);
        int y2Inner = centerY + (arcRadius - arcThickness/2) * sin(rad2);
        
        int x1Outer = centerX + (arcRadius + arcThickness/2) * cos(rad1);
        int y1Outer = centerY + (arcRadius + arcThickness/2) * sin(rad1);
        int x2Outer = centerX + (arcRadius + arcThickness/2) * cos(rad2);
        int y2Outer = centerY + (arcRadius + arcThickness/2) * sin(rad2);
        
        // Draw arc segment (filled quadrilateral using two triangles)
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, gfx->color565(10, 10, 40));
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, gfx->color565(10, 10, 40));
    }
}

void InitializationScreen::drawProgressArc(int centerX, int centerY, int radius) {
    // Thicker arc (30px)
    int arcThickness = 30;
    int arcRadius = radius - (arcThickness / 2);
    
    // Use a smaller angle step for smoother arcs
    const float angleStep = 0.5;
    
    // Red color for progress arc
    uint16_t progressColor = RED;
    
    // Draw progress arc using triangles
    for (float angle = 0; angle < animatedProgressAngle; angle += angleStep) {
        float nextAngle = min(angle + angleStep, animatedProgressAngle);
        
        // Convert angles to radians
        float rad1 = angle * PI / 180.0;
        float rad2 = nextAngle * PI / 180.0;
        
        // Calculate points for arc segment
        int x1Inner = centerX + (arcRadius - arcThickness/2) * cos(rad1);
        int y1Inner = centerY + (arcRadius - arcThickness/2) * sin(rad1);
        int x2Inner = centerX + (arcRadius - arcThickness/2) * cos(rad2);
        int y2Inner = centerY + (arcRadius - arcThickness/2) * sin(rad2);
        
        int x1Outer = centerX + (arcRadius + arcThickness/2) * cos(rad1);
        int y1Outer = centerY + (arcRadius + arcThickness/2) * sin(rad1);
        int x2Outer = centerX + (arcRadius + arcThickness/2) * cos(rad2);
        int y2Outer = centerY + (arcRadius + arcThickness/2) * sin(rad2);
        
        // Draw arc segment (filled quadrilateral using two triangles)
        gfx->fillTriangle(x1Inner, y1Inner, x1Outer, y1Outer, x2Outer, y2Outer, progressColor);
        gfx->fillTriangle(x1Inner, y1Inner, x2Inner, y2Inner, x2Outer, y2Outer, progressColor);
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
    
    // Set text size for QNOB
    gfx->setTextSize(5);
    
    // Calculate text dimensions for proper centering
    int16_t qx1, qy1, qnobx1, qnoby1;
    uint16_t qw, qh, qnobw, qnobh;
    
    // Get bounds of "Q" and "QNOB" to calculate total width
    gfx->getTextBounds("Q", 0, 0, &qx1, &qy1, &qw, &qh);
    gfx->getTextBounds("QNOB", 0, 0, &qnobx1, &qnoby1, &qnobw, &qnobh);
    
    // Calculate starting position to center "QNOB"
    int startX = centerX - (qnobw / 2);
    int textY = centerY - (qh / 2); // Vertically center
    
    // First draw the "Q" in red
    gfx->setTextColor(RED);
    gfx->setCursor(startX, textY);
    gfx->print("Q");
    
    // Then draw "NOB" in white right after Q
    gfx->setTextColor(WHITE);
    gfx->setCursor(startX + qw, textY);
    gfx->print("NOB");
    
    // Add "initializing" in smaller text below
    gfx->setTextSize(2);
    String initText = "initializing...";
    int16_t ix1, iy1;
    uint16_t iw, ih;
    gfx->getTextBounds(initText, 0, 0, &ix1, &iy1, &iw, &ih);
    gfx->setCursor(centerX - (iw / 2), textY + qh + 10);
    gfx->print(initText);
    
    // Show current initialization step as status text
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
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);  // Always white
    
    // Calculate exact width of status text for proper centering
    int16_t sx1, sy1;
    uint16_t sw, sh;
    gfx->getTextBounds(statusText, 0, 0, &sx1, &sy1, &sw, &sh);
    
    // Position status text below the "initializing" text
    // Account for the QNOB text size and initializing text
    int statusY = textY + qh + 10 + ih + 10;
    gfx->setCursor(centerX - (sw / 2), statusY);
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
    animatedProgressAngle = 0;
    targetProgressAngle = 0;
    startProgressAngle = 0;
    animationStartTime = millis();
}