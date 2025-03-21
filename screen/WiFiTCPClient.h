#ifndef WIFI_TCP_CLIENT_H
#define WIFI_TCP_CLIENT_H

#include "WiFiHandler.h"
#include "TCPHandler.h"
#include "MQTTHandler.h"
#include "EEPROMManager.h"

class WiFiTCPClient {
private:
  EEPROMManager* eepromManager;  // EEPROM manager - owned externally
  WiFiHandler* wifiHandler;      // WiFi connection handler
  TCPHandler* tcpHandler;        // TCP communication handler
  MQTTHandler* mqttHandler;      // MQTT communication handler

  unsigned long intervalMs;      // Interval for periodic operations

public:
  WiFiTCPClient(EEPROMManager* eepromMgr, unsigned long mqttInterval = 5000);
  ~WiFiTCPClient();
  int getWeatherHumidity();
  float getWeatherWindSpeed();
  String getWeatherCondition();
  
  // Complete initialization method for all network components
  void initialize();

  // Methods delegated to handlers - maintaining the original interface

  // WiFi methods
  bool connectToWifi();                 // Connect to Wi-Fi using stored credentials
  bool startAPMode();                   // Start AP mode for configuration
  bool isWiFiConnected() const;         // Check Wi-Fi connection status
  bool isAPModeActive() const;          // Check if AP mode is active
  bool isInternetAvailable() const;     // Check if internet is available
  void beginOTA(const char* hostname);  // Initialize OTA
  int getWiFiSignalStrength();          // Get WiFi signal strength (0-3)
  
  // Static IP Configuration (new)
  void setStaticIP(bool enable, const IPAddress& ip, const IPAddress& gateway,
                  const IPAddress& subnet, const IPAddress& dns1 = IPAddress(8, 8, 8, 8),
                  const IPAddress& dns2 = IPAddress(8, 8, 4, 4));
  void setStaticIPFromStrings(bool enable, const String& ip, const String& gateway,
                             const String& subnet, const String& dns1 = "8.8.8.8",
                             const String& dns2 = "8.8.4.4");
  bool isUsingStaticIP() const;         // Check if static IP is enabled
  IPAddress getServerIP() const;        // Get current server IP address
  String getStaticIPConfig() const;     // Get static IP configuration as string
  void applyEEPROMStaticIPConfig();     // Apply static IP config from EEPROM

  // TCP methods
  void sendTCPResponse(const String& message);  // Send response to TCP client
  bool getHasNewMessage();                      // Check if there's a new TCP message
  String getMessage();                          // Get the received TCP message
  bool connectToSoundServer();                  // Connect to the sound server
  bool connectToLightServer();                  // Connect to the light server
  void sendData(const String& data);            // Send data to the server
  String receiveData();                         // Receive data from the server
  void disconnect();                            // Disconnect from the server
  bool isServerConnected();                     // Check server connection status

  // MQTT methods
  void initializeMQTT();                                           // Initialize MQTT with stored credentials
  bool connectToMQTTServer();                                      // Connect to MQTT broker
  void sendMQTTMessage(const char* topic, const String& message);  // Send MQTT message
  bool hasSoundMQTTConfigured();                                   // Check if Sound MQTT is configured
  bool hasLightMQTTConfigured();                                   // Check if Light MQTT is configured
  bool isMQTTConnected();                                          // Check if MQTT is connected

  // Time and weather methods
  String getCurrentDate();  // Get current date
  String getCurrentTime();  // Get current time
  String getDayOfWeek();    // Get day of week
  String getWeatherTemperature();  // Get weather temperature

  // Load configuration and update
  void loadConfiguration();  // Load configuration from EEPROM
  void update();             // Run periodic updates on all handlers
};

#endif  // WIFI_TCP_CLIENT_H