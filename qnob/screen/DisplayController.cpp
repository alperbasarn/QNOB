#include "DisplayController.h"

DisplayController::DisplayController(Arduino_GFX* graphics, int sda, int scl, int rst, int irq, WiFiTCPClient* tcpClient)
  : gfx(graphics), tcpClient(tcpClient), displayIsOn(true), soundController(nullptr),
    modeController(nullptr), lightController(nullptr), knobController(nullptr),
    initializationScreen(nullptr), infoScreen(nullptr), currentMode(INITIALIZATION),
    lastActivityTime(0), deepSleepEnabled(true), screenPressed(false), screenReleased(false) {

  // Initialize touch panel first
  touchPanel = new TouchPanel(sda, scl, rst, irq);

  // Create all controllers
  soundController = new SoundController(gfx, touchPanel, tcpClient);
  modeController = new ModeController(gfx, touchPanel, tcpClient);
  initializationScreen = new InitializationScreen(gfx, touchPanel);
  infoScreen = new InfoScreen(gfx, touchPanel, tcpClient);

  // Light controller is initialized later in registerKnobController
}

void DisplayController::initScreen() {
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->invertDisplay(true);
  gfx->fillScreen(BLACK);
  pinMode(DF_GFX_BL, OUTPUT);
  digitalWrite(DF_GFX_BL, HIGH);

  // Start with initialization screen
  setMode(INITIALIZATION);
  touchPanel->initialize();

  // Initialize activity timestamp
  resetActivityTime();
}

void DisplayController::registerKnobController(KnobController* knob) {
  knobController = knob;
  // Now that we have the knob controller, we can initialize the light controller
  lightController = new LightController(gfx, touchPanel, tcpClient, knobController);
}

void DisplayController::setTCPClient(WiFiTCPClient* client) {
  tcpClient = client;

  // Update client reference in all controllers that need it
  if (soundController) soundController->setTCPClient(client);
  if (modeController) modeController->setTCPClient(client);
  if (lightController) lightController->setTCPClient(client);
  if (infoScreen) infoScreen->setTCPClient(client);
}

void DisplayController::updateInitProgress(int progress) {
  if (initializationScreen && currentMode == INITIALIZATION) {
    // Update network status indicators in init screen
    if (tcpClient) {
      bool wifiConnected = tcpClient->isWiFiConnected();
      bool mqttConnected = tcpClient->isMQTTConnected();

      initializationScreen->setWiFiStatus(wifiConnected, wifiConnected ? 3 : 0);
      initializationScreen->setMQTTStatus(mqttConnected);
    }

    initializationScreen->setProgress(progress);
  }
}

bool DisplayController::getHasNewEvent() {
  if (isPressed() || (knobController && knobController->getHasNewMessage())) {
    resetActivityTime();
    return true;
  }
  return false;
}

void DisplayController::resetActivityTime() {
  lastActivityTime = millis();

  // Also reset the infoScreen activity timer if it's active
  if (infoScreen && currentMode == INFO) {
    infoScreen->resetLastActivityTime();
  }

  // Wake up display if it was in sleep mode
  if (!displayIsOn) {
    turnDisplayOn();
  }
}

void DisplayController::drawCalibrationMark() {
  if (!screenCalibrationInitialized) {
    if (knobController) {
      knobController->sendCommand("disable");
    }
    int screenWidth = gfx->width();
    int screenHeight = gfx->height();
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;

    // Set parameters for the calibration marks
    int radius = min(screenWidth, screenHeight) / 3;  // distance from center where marks start
    int markLength = 20;                              // length of each calibration mark
    int markThickness = 3;                            // thickness of the mark lines

    gfx->fillScreen(BLACK);

    // Draw calibration marks every 45 degrees (0°, 45°, 90°, ..., 315°)
    for (int angle = 0; angle < 360; angle += 15) {
      float rad = angle * PI / 180.0;
      // Calculate starting point on the circle
      int startX = centerX + radius * cos(rad);
      int startY = centerY + radius * sin(rad);
      // Calculate ending point (mark extends outward by markLength)
      int endX = centerX + (radius + markLength) * cos(rad);
      int endY = centerY + (radius + markLength) * sin(rad);

      // Draw the main line for the mark
      gfx->drawLine(startX, startY, endX, endY, RED);

      // Optionally, draw additional parallel lines to achieve the desired thickness
      for (int t = 1; t < markThickness; t++) {
        // Offset perpendicular to the line direction
        int offsetX = -t * sin(rad);
        int offsetY = t * cos(rad);
        gfx->drawLine(startX + offsetX, startY + offsetY, endX + offsetX, endY + offsetY, RED);
        gfx->drawLine(startX - offsetX, startY - offsetY, endX - offsetX, endY - offsetY, RED);
      }
    }

    screenCalibrationInitialized = true;
  } else {
    if (touchPanel->getHasNewHoldRelease()) {
      screenCalibrationInitialized = false;
      if (knobController) {
        knobController->sendCommand("zeroPos");
      }
      setMode(HOME);
    }
  }
}

bool DisplayController::getKnobHasNewMessage() {
  return knobController && knobController->getHasNewMessage();
}

void DisplayController::checkActivityAndAutoSwitch() {
  unsigned long currentMillis = millis();

  // Don't auto-switch during initialization, calibration, or when already in INFO or SLEEP mode
  if (currentMode == INITIALIZATION || currentMode == CALIBRATE_ORIENTATION || 
      currentMode == INFO || currentMode == SLEEP) {
    return;
  }

  // If inactive for 10 seconds from any mode, switch to info mode
  if (currentMillis - lastActivityTime >= 10000) {
    setMode(INFO);
    if (infoScreen) {
      infoScreen->resetLastActivityTime();
    }
    return;
  }
}

void DisplayController::goToSleep() {
  // Use FADE transition to sleep
  setMode(SLEEP);
  turnDisplayOff();
}

void DisplayController::turnDisplayOff() {
  if (displayIsOn) {
    digitalWrite(DF_GFX_BL, LOW);
    displayIsOn = false;
    Serial.println("Display turned off");
  }
}

void DisplayController::update() {
  if (knobController) {
    knobController->update();
  }
  touchPanel->handleTouchPanel();
  
  // If we're in sleep mode and detect activity, wake up to INFO mode
  if (currentMode == SLEEP) {
    if (isPressed() || (knobController && knobController->getHasNewMessage())) {
      Serial.println("Activity detected while in SLEEP mode, waking up to INFO mode");
      turnDisplayOn();
      return; // Skip the rest of the update for this cycle
    }
  } else {
    // Check for activity and handle auto-switching between modes
    checkActivityAndAutoSwitch();
  }

  // Process the current mode
  switch (currentMode) {
    case INITIALIZATION:
      if (initializationScreen) {
        initializationScreen->updateScreen();
      }
      break;
      
    case INFO:
      if (infoScreen) {
        // Debug print the data being passed to info screen
        if (tcpClient) {
          String date = tcpClient->getCurrentDate();
          String time = tcpClient->getCurrentTime();
          String weather = tcpClient->getWeatherTemperature();
          Serial.println("InfoScreen Update - Date: " + date + ", Time: " + time + ", Weather: " + weather);
          
          infoScreen->updateDateTime(date, time);
          infoScreen->updateWeather(weather);
          infoScreen->updateRoomTemperature(20.0); // Mock temperature value
        }
        
        infoScreen->updateScreen();
        
        // Check if back button was pressed
        if (infoScreen->isPageBackRequested()) {
          setMode(HOME);
          infoScreen->resetPageBackRequest();
          modeController->setActive(true);
          delay(200); // Debounce
        }
        
        // Check if sleep timeout reached
        if (infoScreen->isInactivityTimeoutReached() && deepSleepEnabled) {
          goToSleep();
        }
      }
      break;
    
    case CALIBRATE_ORIENTATION:
      drawCalibrationMark();
      break;
      
    case SOUND:
      if (soundController) {
        soundController->updateScreen();
        if (soundController->isPageBackRequested()) {
          setMode(HOME);
          Serial.println("Returning to HOME mode.");
          tcpClient->disconnect();
          modeController->setActive(true);
        }
      }
      break;
      
    case LIGHT:
      if (lightController) {
        lightController->updateScreen();
        if (lightController->isPageBackRequested()) {
          setMode(HOME);
          Serial.println("Returning to HOME mode.");
          tcpClient->disconnect();
          modeController->setActive(true);
        }
      }
      break;
      
    case HOME:
      if (modeController) {
        if (!modeControllerInitialized) {
          modeController->setActive(true);
          modeControllerInitialized = true;
        }

        modeController->updateScreen();
        if (touchPanel->getHasNewPress()) {
          screenPressed = true;
          screenReleased = false;
        }
        if (screenPressed) {
          if (touchPanel->getHasNewRelease()) {
            screenPressed = false;
            screenReleased = true;
          }
          if (screenReleased) {
            screenReleased = false;
            modeControllerInitialized = false;
            if (modeController->isSoundModeActive()) {
              setMode(SOUND);
              Serial.println("Switching to SOUND mode.");
              modeController->setActive(false);
              if (tcpClient) {
                if (tcpClient->hasSoundMQTTConfigured()) {
                  tcpClient->initializeMQTT();
                  tcpClient->connectToMQTTServer();
                } else {
                  Serial.println("Sound MQTT not configured, skipping connection");
                }
              }
              if (soundController) {
                soundController->resetPageBackRequest();
              }
              delay(200);
            } else if (modeController->isLightModeActive()) {
              Serial.println("Switching to LIGHT mode.");
              setMode(LIGHT);
              modeController->setActive(false);
              if (tcpClient) {
                if (tcpClient->hasLightMQTTConfigured()) {
                  tcpClient->initializeMQTT();
                  tcpClient->connectToMQTTServer();
                } else {
                  Serial.println("Light MQTT not configured, skipping connection");
                }
              }
              if (lightController) {
                lightController->resetPageBackRequest();
              }
              delay(200);
            }
          }
        }
      }
      break;
      
    case SLEEP:
      // In sleep mode, just wait for activity detection (handled at the beginning of update)
      break;
      
    default:
      break;
  }
}

// Replace the turnDisplayOn method
void DisplayController::turnDisplayOn() {
  if (!displayIsOn) {
    digitalWrite(DF_GFX_BL, HIGH);
    displayIsOn = true;
    Serial.println("Display turned on");
    
    // Reset activity time
    resetActivityTime();
    
    // Return to info mode if coming from sleep
    if (currentMode == SLEEP) {
      // Force a proper redraw of the info screen
      if (infoScreen) {
        infoScreen->resetScreen();
      }
      // Always go to INFO mode when waking from sleep, never HOME
      setMode(INFO);
      Serial.println("Switched to INFO mode after waking from sleep");
    }
  }
}

void DisplayController::setSoundLevel(int level) {
  resetActivityTime();
  if (soundController) {
    soundController->updateSetpoint(level);
  }
}

void DisplayController::incrementSetpoint() {
  resetActivityTime();

  if (currentMode == SOUND && soundController) {
    soundController->incrementSetpoint();
  } else if (currentMode == HOME && modeController) {
    // For home screen, use WIPE_RIGHT animation as requested
    AnimationHelper::performTransition(gfx, AnimationHelper::WIPE_RIGHT, 200);
    modeController->nextMode();
  } else if (currentMode == LIGHT && lightController) {
    lightController->incrementSetpoint();
  }
}

void DisplayController::decrementSetpoint() {
  resetActivityTime();

  if (currentMode == SOUND && soundController) {
    soundController->decrementSetpoint();
  } else if (currentMode == HOME && modeController) {
    // For home screen, use WIPE_LEFT animation as requested
    AnimationHelper::performTransition(gfx, AnimationHelper::WIPE_LEFT, 200);
    modeController->previousMode();
  } else if (currentMode == LIGHT && lightController) {
    lightController->decrementSetpoint();
  }
}

void DisplayController::setSetpoint(int setpoint) {
  resetActivityTime();

  if (currentMode == SOUND && soundController) {
    soundController->updateSetpoint(setpoint);
  } else if (currentMode == HOME && modeController) {
    // No action needed
  } else if (currentMode == LIGHT && lightController) {
    lightController->setSetpoint(setpoint);
  }
}

void DisplayController::transitionToMode(Mode newMode, AnimationHelper::TransitionType transitionType) {
  // Skip transition during initialization or if new mode is the same as current
  if (currentMode == INITIALIZATION || currentMode == newMode) {
    currentMode = newMode;
    return;
  }

  // Perform transition animation
  AnimationHelper::performTransition(gfx, transitionType);

  // Set the new mode
  currentMode = newMode;

  // Initialize the new screen based on mode
  switch (newMode) {
    case INFO:
      if (infoScreen) {
        infoScreen->resetPageBackRequest();
        infoScreen->resetLastActivityTime();
      }
      break;

    case HOME:
      if (modeController) {
        modeController->setActive(true);
        modeControllerInitialized = true;
      }
      break;

    case CALIBRATE_ORIENTATION:
      screenCalibrationInitialized = false;
      break;

    case SOUND:
      if (soundController) {
        soundController->resetPageBackRequest();
      }
      break;

    case LIGHT:
      if (lightController) {
        lightController->resetPageBackRequest();
      }
      break;

    case SLEEP:
      // Nothing specific to initialize for sleep mode
      break;

    default:
      break;
  }
}

// Updated setMode to use the required animations based on the mode transition
void DisplayController::setMode(Mode mode) {
  AnimationHelper::TransitionType transition;

  // Choose different transition effects based on the mode change
  if (currentMode == HOME && (mode == SOUND || mode == LIGHT)) {
    // When switching to control screen from home, use ZOOM_IN
    transition = AnimationHelper::ZOOM_IN;
  } else if ((currentMode == SOUND || currentMode == LIGHT) && mode == HOME) {
    // When going back to home from control screen, use ZOOM_OUT
    transition = AnimationHelper::ZOOM_OUT;
  } else if (currentMode == HOME && mode == INFO) {
    // From home to info, use WIPE_UP
    transition = AnimationHelper::WIPE_UP;
  } else if (currentMode == INFO && mode == HOME) {
    // From info to home, use WIPE_DOWN
    transition = AnimationHelper::WIPE_DOWN;
  } else if (mode == SLEEP) {
    // When going to sleep, use FADE
    transition = AnimationHelper::FADE;
  } else if (currentMode == SLEEP) {
    // When waking from sleep, use FADE
    transition = AnimationHelper::FADE;
  } else if (mode == INITIALIZATION) {
    // No animation for initialization screen
    transition = AnimationHelper::NONE;
  } else {
    // Default transition
    transition = AnimationHelper::FADE;
  }

  // Perform the transition
  transitionToMode(mode, transition);

  // Log the mode change
  String modeNames[] = { "SOUND", "LIGHT", "TEMPERATURE", "HOME", "CALIBRATE_ORIENTATION", "INITIALIZATION", "INFO", "SLEEP" };
  Serial.println("Switching to " + modeNames[mode] + " mode.");

  // Additional mode-specific actions
  if (mode == INITIALIZATION && initializationScreen) {
    initializationScreen->reset();
  } else if (mode == CALIBRATE_ORIENTATION) {
    delay(500);
  }

  // Reset activity time on mode change
  resetActivityTime();
}

void DisplayController::setDeepSleepEnabled(bool enabled) {
  deepSleepEnabled = enabled;
}

bool DisplayController::getPressed() {
  return touchPanel->isPressed();
}

bool DisplayController::isPressed() {
  return touchPanel->isPressed();
}

bool DisplayController::isPageBackRequested() {
  return (soundController && soundController->isPageBackRequested()) || 
         (lightController && lightController->isPageBackRequested()) || 
         (infoScreen && infoScreen->isPageBackRequested());
}

DisplayController::~DisplayController() {
  delete soundController;
  delete modeController;
  delete lightController;
  delete touchPanel;
  delete initializationScreen;
  delete infoScreen;
  // Don't delete knobController here as it's managed externally
}