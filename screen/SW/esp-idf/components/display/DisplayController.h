#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include "LGFX_Config.hpp"
// #include "SoundController.h"
// #include "ModeController.h"
// #include "LightController.h"
// #include "TouchPanel.h"
// #include "WiFiTCPClient.h"
// #include "Common.h"
// #include "KnobController.h"
// #include "InitializationScreen.h"
// #include "InfoScreen.h"
// #include "AnimationHelper.h"

// Forward declarations
class SoundController;
class ModeController;
class LightController;
class TouchPanel;
class WiFiTCPClient;
class KnobController;
class InitializationScreen;
class InfoScreen;
class AnimationHelper;

enum Mode {
  INITIALIZATION,
  INFO,
  CALIBRATE_ORIENTATION,
  SOUND,
  LIGHT,
  HOME,
  SLEEP
};

class DisplayController {
private:
  LGFX* gfx;
  TouchPanel* touchPanel;
  WiFiTCPClient* tcpClient;
  bool displayIsOn;
  bool screenPressed;
  bool screenReleased;
  bool modeControllerInitialized = false;
  bool screenCalibrationInitialized = false;
  
  Mode currentMode;
  unsigned long lastActivityTime;
  
  SoundController* soundController;
  ModeController* modeController;
  LightController* lightController;
  KnobController* knobController;
  
  // Screen controllers
  InitializationScreen* initializationScreen;
  InfoScreen* infoScreen;
  
  // Transition with animation when changing modes
  void transitionToMode(Mode newMode, int transitionType = 0);
  
  // Activity tracking and auto-switching
  void checkActivityAndAutoSwitch();
  void resetActivityTime();
  

public:
  DisplayController(LGFX* graphics, int sda, int scl, int rst, int irq, WiFiTCPClient* tcpClient);
  void initScreen();
  void update();
  void drawCalibrationMark();
  void setSoundLevel(int level);
  void incrementSetpoint();
  void decrementSetpoint();
  void setSetpoint(int setpoint);
  void setMode(Mode mode);
  void turnDisplayOff();
  void turnDisplayOn();
  void setDeepSleepEnabled(bool enabled);
  bool getPressed();
  bool isPressed();
  bool isPageBackRequested();
  bool getKnobHasNewMessage();
  bool getHasNewEvent();
  void registerKnobController(KnobController* knob);
  void setDisplayOff();
  
  // Methods for initialization and info screens
  void updateInitProgress(int progress);
  void setTCPClient(WiFiTCPClient* client);
  
  ~DisplayController();
};

#endif