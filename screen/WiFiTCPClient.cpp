#include "WiFiTCPClient.h"

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

#include "WiFiTCPClient.h"

// Replace the initialize method with this improved version
void WiFiTCPClient::initialize() {
  Serial.println("Initializing WiFi/TCP/MQTT components...");
  
  // First initialize WiFiHandler (starts AP mode)
  wifiHandler->initialize();
  
  // Load saved configuration
  loadConfiguration();
  
  // Try to connect to WiFi (only first call will actually attempt connection)
  bool wifiConnected = connectToWifi();
  
  // Start TCP server on the appropriate port
  tcpHandler->startTCPServer(AP_SERVER_PORT);
  if (wifiHandler->isWiFiConnected()) {
    Serial.println("TCP Server started in WiFi mode on port " + String(AP_SERVER_PORT));
  } else {
    Serial.println("TCP Server started in AP mode on port " + String(AP_SERVER_PORT));
  }
  
  // ALWAYS initialize MQTT configuration, regardless of internet availability
  mqttHandler->initializeMQTT(false); // Default to sound configuration
  Serial.println("MQTT configuration loaded");
  
  // Complete initialization without MQTT connection attempt
  Serial.println("WiFi/TCP components initialized - MQTT connection will be attempted shortly");
  
  // Delay to allow system to stabilize
  delay(1000);
  
  // Now attempt MQTT connection in a separate task after initialization
  xTaskCreate(
    [](void* pvParameters) {
      WiFiTCPClient* client = (WiFiTCPClient*)pvParameters;
      
      // Allow some time for the system to stabilize
      vTaskDelay(3000 / portTICK_PERIOD_MS);
      
      // Try to connect to MQTT server if internet is available
      if (client->isInternetAvailable()) {
        Serial.println("Internet available, attempting MQTT connection after initialization");
        bool mqttConnected = client->connectToMQTTServer();
        if (mqttConnected) {
          Serial.println("✅ MQTT connected successfully");
        } else {
          Serial.println("⚠️ MQTT connection failed, will retry during update cycle");
        }
      }
      
      vTaskDelete(NULL);  // Delete the task when done
    },
    "MQTT_Connect", 4096, this, 1, NULL);
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
  bool result = wifiHandler->startAPMode();
  
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
          if (currentMillis - lastMQTTAttempt > 5000) { // 5 second interval
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

// Add this method to your existing WiFiHandler.cpp file

int WiFiHandler::getWiFiSignalStrength() {
    if (!wifiConnected) {
        return 0; // No signal if not connected
    }
    
    // Get RSSI (signal strength in dBm)
    int rssi = WiFi.RSSI();
    
    // Optional debug output
    // Serial.print("WiFi RSSI: ");
    // Serial.println(rssi);
    
    // Convert RSSI to a 0-3 scale for display
    // Typical RSSI values: -30 (very strong) to -90 (very weak)
    if (rssi > -55) {
        return 3; // Strong signal
    } else if (rssi > -70) {
        return 2; // Medium signal
    } else if (rssi > -85) {
        return 1; // Weak signal
    } else {
        return 0; // Very weak signal
    }
}