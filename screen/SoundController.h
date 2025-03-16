#ifndef SOUND_CONTROLLER_H
#define SOUND_CONTROLLER_H

#include <Arduino_GFX_Library.h>
#include "TouchPanel.h"
#include "WiFiTCPClient.h"

class SoundController {
private:
  Arduino_GFX* gfx;
  TouchPanel* touchPanel;
  WiFiTCPClient* tcpClient;  // Pointer to the TCP client
  int setpoint;              // Current volume level (0-100)
  int lastSetpoint;          // Last drawn volume level
  int lastDrawnSetpoint;     // Last drawn volume in visualization
  float volume;              // Actual volume (0.0 - 1.0)
  bool colorChange;          // Indicates if the color has changed
  bool initialized;          // Indicates if static elements have been drawn
  bool pageBackRequested;    // Flag for page-back request
  int lastSentSetpoint;      // Last setpoint sent to server
  unsigned long lastSentTime;  // Time of last server update
  
  // Missing variables that were causing errors
  unsigned long lastSetpointChangeTime;  // Time when the setpoint was last changed
  unsigned long lastSetpointSentTime;    // Time when the setpoint was last sent
  const int barCount = 20;               // Number of volume bars to display
  
  // Methods with correct names matching the implementation
  void drawStaticVolumeElements();        // Draw static sound visualization
  void drawSetPoint();                   // Draw the current volume level
  void checkTouchInput();                // Check for touch input
  void sendSetpointToServer();           // Send volume setpoint to server
  void requestInitialSetpointFromServer(); // Request initial setpoint from server
  void drawBackButton();                 // Draw back button
  void drawVolumeBars(int volume);       // Draw volume bars
  
  // Helper methods for drawing
  void drawUpDownArrow(int x, int y, bool up);
  uint16_t adjustBrightness(uint16_t color, float brightness);
  uint16_t interpolateColor(uint16_t color1, uint16_t color2, float fraction);
  
public:
  SoundController(Arduino_GFX* graphics, TouchPanel* touch, WiFiTCPClient* client);
  void updateScreen();
  void updateSetpoint(int setpoint);
  void incrementSetpoint();
  void decrementSetpoint();
  bool isPageBackRequested() const;
  void resetPageBackRequest();
  void setTCPClient(WiFiTCPClient* client);
};

#endif // SOUND_CONTROLLER_H