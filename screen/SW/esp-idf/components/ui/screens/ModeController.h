#pragma once

#include "display/LGFX_Config.hpp"
#include "touch_panel/TouchPanel.h"
#include <cstdint>

class ModeController {
public:
    enum Mode {
        MODE_LIGHT = 0,
        MODE_TEMPERATURE = 1,
        MODE_SOUND = 2,
        MODE_COUNT = 3
    };

private:
    LGFX* gfx;
    TouchPanel* touchPanel;
    Mode currentMode;
    Mode lastMode;
    bool active;

    void drawThermometer();
    void drawBulb();
    void drawSpeaker();
    void drawModeLogo();

public:
    ModeController(LGFX* graphics, TouchPanel* touch);

    void updateScreen();
    void nextMode();
    void previousMode();
    void setActive(bool isActive);
    void setMode(Mode mode);

    Mode getCurrentMode() const { return currentMode; }
    bool isSoundModeActive() const { return currentMode == MODE_SOUND; }
    bool isLightModeActive() const { return currentMode == MODE_LIGHT; }
    bool isTemperatureModeActive() const { return currentMode == MODE_TEMPERATURE; }
};
