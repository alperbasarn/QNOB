#ifndef INTERNET_HANDLER_H
#define INTERNET_HANDLER_H

#include <Arduino.h>

// Forward declarations to resolve circular dependencies
class WiFiHandler;
class TimeHandler;
class WeatherHandler;

class InternetHandler {
private:
  WiFiHandler* wifiHandler;             // Pointer to parent WiFiHandler
  TimeHandler* timeHandler;             // Time functionality handler
  WeatherHandler* weatherHandler;       // Weather functionality handler
  bool internetAvailable = false;       // Internet connectivity status
  unsigned long lastInternetCheck = 0;  // Time of last internet connectivity check
  unsigned long lastTimeCheck = 0;      // Time of last internet connectivity check
  unsigned long lastWeatherCheck = 0;   // Time of last internet connectivity check
  // Configuration values
  static const unsigned long INTERNET_CHECK_INTERVAL = 30000;  // 30 seconds
  static const unsigned long WEATHER_CHECK_INTERVAL = 30000;    // 300 seconds

public:
  InternetHandler(WiFiHandler* wifiHandler);
  ~InternetHandler();

  // Internet connectivity methods
  bool checkInternetConnectivity();
  bool isInternetAvailable() const {
    return internetAvailable;
  }

  // Getter methods for delegated handlers
  TimeHandler* getTimeHandler() const {
    return timeHandler;
  }
  WeatherHandler* getWeatherHandler() const {
    return weatherHandler;
  }

  // Additional delegation methods to avoid direct access to sub-handlers
  bool updateDateTime();  // Delegate to TimeHandler
  bool updateWeather();   // Delegate to WeatherHandler

  // Forwarding methods to maintain compatibility with WiFiHandler
  String getCurrentDate() const;
  String getCurrentTime() const;
  String getDayOfWeek() const;
  String getWeatherTemperature() const;

  // Main update method - call this regularly
  void update();
};

#endif  // INTERNET_HANDLER_H