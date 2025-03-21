#include "Client.h"
#include "WiFiTCPClient.h"
#include "WeatherHandler.h"  // Include the complete WeatherHandler class definition

// Add a static flag to prevent double connection attempts
static bool wifiConnectionAttempted = false;

WiFiTCPClient::WiFiTCPClient(EEPROMManager* eepromMgr, unsigned long mqttInterval)
  : eepromManager(eepromMgr), intervalMs(mqttInterval) {
  wifiHandler = new WiFiHandler(eepromManager);
  tcpHandler = new TCPHandler(wifiHandler);
  mqttHandler = new MQTTHandler(wifiHandler, eepromManager);
  wifiConnectionAttempted = false;
}

WiFiTCPClient::~WiFiTCPClient() {
  // Delete in reverse order of creation
  delete mqttHandler;
  delete tcpHandler;
  delete wifiHandler;
}

// Update initialize method to apply static IP configuration from EEPROM
void WiFiTCPClient::initialize() {
  Serial.println("Initializing WiFi/TCP/MQTT components...");

  // First initialize WiFiHandler (starts AP mode)
  wifiHandler->initialize();

  // Apply static IP configuration from EEPROM
  applyEEPROMStaticIPConfig();
  
  // Load saved configuration
  loadConfiguration();
  
  // Start TCP server on the appropriate port
  tcpHandler->startTCPServer(AP_SERVER_PORT);
  
  if (wifiHandler->isWiFiConnected()) {
    Serial.println("TCP Server started in WiFi mode on port " + String(AP_SERVER_PORT));
  } else {
    Serial.println("TCP Server started in AP mode on port " + String(AP_SERVER_PORT));
  }

  // ALWAYS initialize MQTT configuration, regardless of internet availability
  mqttHandler->initializeMQTT(false);  // Default to sound configuration
  Serial.println("MQTT configuration loaded");
  
  // Complete initialization without MQTT connection attempt
  Serial.println("WiFi/TCP components initialized - MQTT connection will be attempted shortly");
  
  // Delay to allow system to stabilize
  delay(10);
  
  // Try to connect to MQTT server if internet is available
  if (wifiHandler->isInternetAvailable()) {
    Serial.println("Internet available, getting time and weather info from web.");
    wifiHandler->updateDateTime();
    wifiHandler->updateWeather();
    //mqttHandler->connectToMQTTServer();
  }
}

void WiFiTCPClient::loadConfiguration() {
  if (!eepromManager) {
    Serial.println("Error: EEPROMManager is null");
    return;
  }

  // Load TCP server configurations from EEPROM manager
  String soundServerIP = eepromManager->soundTCPServerIP;
  uint16_t soundServerPort = eepromManager->soundTCPServerPort;
  String lightServerIP = eepromManager->lightTCPServerIP;
  uint16_t lightServerPort = eepromManager->lightTCPServerPort;

  // Ensure default values if empty
  if (soundServerPort <= 0) soundServerPort = 12345;
  if (lightServerPort <= 0) lightServerPort = 12345;

  // Set the server information in the TCP handler
  tcpHandler->setServerInfo(soundServerIP, soundServerPort, lightServerIP, lightServerPort);

  Serial.println("Configuration loaded from EEPROM");
}

// Apply static IP configuration from EEPROM
void WiFiTCPClient::applyEEPROMStaticIPConfig() {
  if (!eepromManager || !tcpHandler) {
    Serial.println("Error: EEPROMManager or TCPHandler is null");
    return;
  }
  
  // Apply the static IP configuration from EEPROM to TCPHandler
  tcpHandler->applyEEPROMConfig(
    eepromManager->staticIPEnabled,
    eepromManager->staticIP,
    eepromManager->staticGateway,
    eepromManager->staticSubnet,
    eepromManager->staticDNS1,
    eepromManager->staticDNS2
  );
  
  Serial.println("Static IP configuration applied from EEPROM");
}

// Static IP configuration methods
void WiFiTCPClient::setStaticIP(bool enable, const IPAddress& ip, const IPAddress& gateway,
                               const IPAddress& subnet, const IPAddress& dns1, const IPAddress& dns2) {
  if (tcpHandler) {
    tcpHandler->setStaticIP(enable, ip, gateway, subnet, dns1, dns2);
  }
}

void WiFiTCPClient::setStaticIPFromStrings(bool enable, const String& ip, const String& gateway,
                                          const String& subnet, const String& dns1, const String& dns2) {
  if (tcpHandler) {
    tcpHandler->setStaticIPFromStrings(enable, ip, gateway, subnet, dns1, dns2);
  }
}

bool WiFiTCPClient::isUsingStaticIP() const {
  return tcpHandler ? tcpHandler->isUsingStaticIP() : false;
}

IPAddress WiFiTCPClient::getServerIP() const {
  return tcpHandler ? tcpHandler->getServerIP() : IPAddress();
}

String WiFiTCPClient::getStaticIPConfig() const {
  return tcpHandler ? tcpHandler->getStaticIPConfig() : "TCP Handler not available";
}

// WiFi methods - delegate to WiFiHandler
bool WiFiTCPClient::connectToWifi() {
  // Only attempt to connect once per boot cycle to prevent duplicate connections
  if (wifiConnectionAttempted) {
    Serial.println("WiFi connection already attempted this boot cycle, skipping");
    return isWiFiConnected();
  }

  wifiConnectionAttempted = true;
  Serial.println("Making first WiFi connection attempt since boot");
  // Connect to WiFi - AP mode will keep running even if WiFi connects
  return wifiHandler->connectToWifi();
}

bool WiFiTCPClient::startAPMode() {
  bool result = wifiHandler->startAPSTAMode();

  // If AP mode was successfully started, start the TCP server
  if (result) {
    tcpHandler->startTCPServer(AP_SERVER_PORT);
  }

  return result;
}

bool WiFiTCPClient::isWiFiConnected() const {
  return wifiHandler ? wifiHandler->isWiFiConnected() : false;
}

bool WiFiTCPClient::isAPModeActive() const {
  return wifiHandler ? wifiHandler->isAPModeActive() : false;
}

bool WiFiTCPClient::isInternetAvailable() const {
  return wifiHandler ? wifiHandler->isInternetAvailable() : false;
}

int WiFiTCPClient::getWiFiSignalStrength() {
  if (wifiHandler) {
    return wifiHandler->getWiFiSignalStrength();
  }
  return 0;
}

void WiFiTCPClient::beginOTA(const char* hostname) {
  if (wifiHandler) {
    wifiHandler->beginOTA(hostname);
  }
}

// TCP methods - delegate to TCPHandler
void WiFiTCPClient::sendTCPResponse(const String& message) {
  if (tcpHandler) {
    tcpHandler->sendTCPResponse(message);
  }
}

bool WiFiTCPClient::getHasNewMessage() {
  return tcpHandler ? tcpHandler->getHasNewMessage() : false;
}

String WiFiTCPClient::getMessage() {
  return tcpHandler ? tcpHandler->getMessage() : "";
}

bool WiFiTCPClient::connectToSoundServer() {
  return tcpHandler ? tcpHandler->connectToSoundServer() : false;
}

bool WiFiTCPClient::connectToLightServer() {
  return tcpHandler ? tcpHandler->connectToLightServer() : false;
}

void WiFiTCPClient::sendData(const String& data) {
  if (tcpHandler) {
    tcpHandler->sendData(data);
  }
}

String WiFiTCPClient::receiveData() {
  return tcpHandler ? tcpHandler->receiveData() : "";
}

void WiFiTCPClient::disconnect() {
  if (tcpHandler) {
    tcpHandler->disconnect();
  }
}

bool WiFiTCPClient::isServerConnected() {
  return tcpHandler ? tcpHandler->isServerConnected() : false;
}

// MQTT methods - delegate to MQTTHandler
void WiFiTCPClient::initializeMQTT() {
  // By default, initialize with sound configuration
  if (mqttHandler) {
    mqttHandler->initializeMQTT(false);
  }
}

bool WiFiTCPClient::connectToMQTTServer() {
  return mqttHandler ? mqttHandler->connectToMQTTServer() : false;
}

void WiFiTCPClient::sendMQTTMessage(const char* topic, const String& message) {
  if (mqttHandler) {
    mqttHandler->sendMQTTMessage(topic, message);
  }
}

bool WiFiTCPClient::hasSoundMQTTConfigured() {
  return mqttHandler ? mqttHandler->hasSoundMQTTConfigured() : false;
}

bool WiFiTCPClient::hasLightMQTTConfigured() {
  return mqttHandler ? mqttHandler->hasLightMQTTConfigured() : false;
}

bool WiFiTCPClient::isMQTTConnected() {
  return mqttHandler ? mqttHandler->isMQTTConnected() : false;
}

// Time and weather methods - delegate to WiFiHandler
String WiFiTCPClient::getCurrentDate() {
  if (wifiHandler) {
    String date = wifiHandler->getCurrentDate();
    // Verify we got a valid date
    if (date.length() > 5) {
      return date;
    }
  }
  return "----/--/--";
}

String WiFiTCPClient::getDayOfWeek() {
  if (wifiHandler) {
    String dayOfWeek = wifiHandler->getDayOfWeek();
    if (dayOfWeek.length() == 3) {
      return dayOfWeek;
    }
  }
  return "---";
}

String WiFiTCPClient::getCurrentTime() {
  if (wifiHandler) {
    String time = wifiHandler->getCurrentTime();
    // Verify we got a valid time
    if (time.length() == 5) {
      return time;
    }
  }
  return "--:--";
}

String WiFiTCPClient::getWeatherTemperature() {
  if (wifiHandler) {
    String temp = wifiHandler->getWeatherTemperature();
    // Verify we got a valid temperature
    if (temp.length() > 2 && temp != "10.5°C") {
      return temp;
    }
  }
  return "--°C";
}

// Main update method - coordinate all updates
void WiFiTCPClient::update() {
  // Only check one handler per update cycle to distribute load
  static uint8_t handlerIndex = 0;

  switch (handlerIndex) {
    case 0:
      // WiFi handler update - optimized to be less frequent
      if (wifiHandler) {
        wifiHandler->update();
      }
      break;

    case 1:
      // TCP handling is most critical for responsiveness
      if (tcpHandler) {
        tcpHandler->update();
      }
      break;

    case 2:
      // Only update MQTT if we have internet - avoid unnecessary processing
      if (wifiHandler && tcpHandler && mqttHandler && wifiHandler->isInternetAvailable()) {
        if (mqttHandler->isMQTTConnected()) {
          mqttHandler->update();
        } else {
          // Check connection less frequently
          static unsigned long lastMQTTAttempt = 0;
          unsigned long currentMillis = millis();
          if (currentMillis - lastMQTTAttempt > 5000) {  // 5 second interval
            mqttHandler->connectToMQTTServer();
            lastMQTTAttempt = currentMillis;
          }
        }
      }
      break;
  }

  // Move to next handler for next cycle
  handlerIndex = (handlerIndex + 1) % 3;
}

int WiFiTCPClient::getWeatherHumidity() {
  if (wifiHandler) {
    InternetHandler* internetHandler = wifiHandler->getInternetHandler();
    if (internetHandler) {
      WeatherHandler* weatherHandler = internetHandler->getWeatherHandler();
      if (weatherHandler) {
        return weatherHandler->getHumidity();
      }
    }
  }
  return 0;
}

float WiFiTCPClient::getWeatherWindSpeed() {
  if (wifiHandler) {
    InternetHandler* internetHandler = wifiHandler->getInternetHandler();
    if (internetHandler) {
      WeatherHandler* weatherHandler = internetHandler->getWeatherHandler();
      if (weatherHandler) {
        return weatherHandler->getWindSpeed();
      }
    }
  }
  return 0.0;
}

String WiFiTCPClient::getWeatherCondition() {
  if (wifiHandler) {
    InternetHandler* internetHandler = wifiHandler->getInternetHandler();
    if (internetHandler) {
      WeatherHandler* weatherHandler = internetHandler->getWeatherHandler();
      if (weatherHandler) {
        return weatherHandler->getCondition();
      }
    }
  }
  return "Unknown";
}