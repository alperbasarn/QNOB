#ifndef INITIALIZATION_SCREEN_H
#define INITIALIZATION_SCREEN_H

#include <Arduino_GFX_Library.h>
#include "TouchPanel.h"

class InitializationScreen {
private:
    enum InitStep {
        INIT_SYSTEM,
        INIT_DISPLAY,
        INIT_CONTROLLERS,
        INIT_WIFI,
        INIT_MQTT,
        INIT_COMPLETE
    };

    Arduino_GFX* gfx;
    TouchPanel* touchPanel;
    int progressValue;       // Current progress value (0-100)
    int previousProgress;    // Previous progress value for incremental updates
    bool screenInitialized;
    InitStep currentStep;    // Current initialization step
    
    // Network status indicators
    bool wifiConnected = false;
    int wifiStrength = 0;    // 0-3 for signal strength
    bool mqttConnected = false;
    
    // Animation properties
    float animatedProgressAngle;  // Current animated angle (smoothly animated)
    float targetProgressAngle;    // Target angle based on progress
    float startProgressAngle;     // Starting angle for animation
    unsigned long animationStartTime;  // When current animation started
    unsigned long lastAnimationUpdateTime;  // Last time animation was updated
    const unsigned long ANIMATION_DURATION = 500;  // Animation duration in milliseconds
    
    void updateCurrentStep();
    void updateAnimation();
    void drawBackgroundArc(int centerX, int centerY, int radius);
    void drawProgressArc(int centerX, int centerY, int radius);
    void drawProgressText(int centerX, int centerY, int radius);
    void drawStepInfo();

public:
    InitializationScreen(Arduino_GFX* graphics, TouchPanel* touch);
    
    void updateScreen();
    void setProgress(int progress);  // Set progress value (0-100)
    void setWiFiStatus(bool connected, int strength);
    void setMQTTStatus(bool isConnected);
    void reset();
};

#endif // INITIALIZATION_SCREEN_H