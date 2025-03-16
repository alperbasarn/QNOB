#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>  // Add time.h for NTP functions
#include "EEPROMManager.h"

// Configuration values
#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD "knobsetup"  // Password for AP mode
#define AP_IP_ADDRESS IPAddress(192, 168, 4, 1)
#define AP_SERVER_PORT 23  // Default telnet port

// Internet check configuration
#define INTERNET_CHECK_HOST "8.8.8.8" // Google DNS for internet connectivity check
#define INTERNET_CHECK_INTERVAL 60000 // Check internet connectivity once per minute
#define AP_CHECK_INTERVAL 30000       // Check AP status every 30 seconds
#define WIFI_SCAN_INTERVAL 60000      // Scan for networks once per minute

// Time and weather configuration
#define NTP_SERVER "pool.ntp.org"
#define WEATHER_API_URL "http://api.openweathermap.org/data/2.5/weather?q=Rotterdam,nl&units=metric&appid=YOUR_API_KEY"
#define TIME_UPDATE_INTERVAL 600000   // Update time every 10 minutes
#define WEATHER_UPDATE_INTERVAL 900000 // Update weather every 15 minutes

class WiFiHandler {
private:
  EEPROMManager* eepromManager;        // Reference to EEPROM manager
  
  bool wifiConnected = false;          // WiFi connection status
  bool apModeActive = false;           // AP mode status
  bool internetAvailable = false;      // Internet connectivity status
  unsigned long lastInternetCheck = 0; // Time of last internet connectivity check
  unsigned long lastAPCheckTime = 0;   // Time of last AP status check
  bool autoStartAP = true;             // Flag to auto-start AP mode when WiFi connection fails
  int wifiChannel = 1;                 // WiFi channel (must be the same for AP and STA modes)
  
  // Time and weather data
  String currentDate = "2025/03/14";   // Current date (default to today's date)
  String currentTime = "12:00";        // Current time (default value)
  String weatherTemp = "10.5°C";       // Weather temperature (default value)
  unsigned long lastDateTimeUpdate = 0;// Time of last date/time update
  unsigned long lastWeatherUpdate = 0; // Time of last weather update
  WiFiUDP ntpUDP;                      // UDP instance for NTP
  const int timeZone = 1;              // Netherlands is UTC+1 (UTC+2 with DST)

public:
  WiFiHandler(EEPROMManager* eepromMgr);
  ~WiFiHandler();

  // Initialization method - call this before using
  void initialize();                   // Initialize WiFi and start AP mode

  // WiFi connection methods
  bool connectToWifi();                // Connect to WiFi using stored credentials
  bool startAPMode();                  // Start Access Point mode for configuration
  bool forceStartAPMode();             // Start AP mode with multiple retries
  void restartAPWithChannel();         // Restart AP mode using the current WiFi channel
  void disconnect();                   // Disconnect from WiFi
  
  // Status methods
  bool isWiFiConnected() const;        // Check if connected to WiFi
  bool isAPModeActive() const;         // Check if AP mode is active
  bool isAPActive();                   // Check if AP is really active by scanning
  int getWiFiSignalStrength();         // Get WiFi signal strength (0-3)
  
  // Configuration methods
  void setAutoStartAP(bool enable);    // Enable/disable auto-starting AP mode

  // Internet connectivity methods
  bool checkInternetConnectivity();    // Check if internet is available
  bool isInternetAvailable() const;    // Get internet availability status
  
  // OTA update methods
  void beginOTA(const char* hostname); // Initialize OTA updates
  void handleOTA();                    // Handle OTA update process
  
  // Time and weather methods
  bool updateDateTime();               // Update date and time from NTP server
  bool updateWeather();                // Update weather data
  String getCurrentDate() const;       // Get current date
  String getCurrentTime() const;       // Get current time
  String getWeatherTemperature() const;// Get weather temperature
  
  // Main update method - call this regularly
  void update();                       // Periodic update (checks internet, handles OTA)
};

#endif // WIFI_HANDLER_H