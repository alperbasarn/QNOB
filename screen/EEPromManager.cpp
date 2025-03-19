#include "EEPROMManager.h"

EEPROMManager::EEPROMManager() 
  : lastConnectedNetworkIndex(-1) {
  // Initialize default values
  soundTCPServerIP = "";
  soundTCPServerPort = 12345;
  lightTCPServerIP = "";
  lightTCPServerPort = 12345;
  soundMQTTServerURL = "";
  soundMQTTServerPort = 8883;
  soundMQTTUsername = "";
  soundMQTTPassword = "";
  lightMQTTServerURL = "";
  lightMQTTServerPort = 8883;
  lightMQTTUsername = "";
  lightMQTTPassword = "";
  deviceName = "ESP32Device"; // Set a default device name
  
  for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    wifiCredentials[i].ssid = "";
    wifiCredentials[i].password = "";
    wifiCredentials[i].remember = false;
  }
}

void EEPROMManager::begin() {
  // Initialize the EEPROM with the specified size.
  EEPROM.begin(EEPROM_SIZE);
  loadAllConfigurations();
}

void EEPROMManager::writeStringToEEPROM(int address, const String &str, int maxLength) {
  int len = str.length();
  if (len > maxLength - 1) {
    len = maxLength - 1;  // Reserve space for null terminator.
  }
  for (int i = 0; i < len; i++) {
    EEPROM.write(address + i, str[i]);
  }
  EEPROM.write(address + len, 0);  // Write null terminator.
  // Fill the remaining bytes with 0.
  for (int i = len + 1; i < maxLength; i++) {
    EEPROM.write(address + i, 0);
  }
}

String EEPROMManager::readStringFromEEPROM(int address, int maxLength) {
  String s = "";
  for (int i = 0; i < maxLength; i++) {
    char c = EEPROM.read(address + i);
    if (c == 0) break;  // End of string.
    s += c;
  }
  return s;
}

void EEPROMManager::writeIntToEEPROM(int address, int value) {
  // Using EEPROM.put is more convenient, as it writes the int as multiple bytes.
  EEPROM.put(address, value);
}

int EEPROMManager::readIntFromEEPROM(int address, int defaultValue) {
  int value = defaultValue;
  EEPROM.get(address, value);
  return value;
}

void EEPROMManager::saveWiFiCredentials() {
  // Save each of the WiFi credentials into a fixed block.
  for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    int baseAddress = WIFI_CREDENTIALS_START + i * CREDENTIAL_BLOCK_SIZE;
    int ssidAddr = baseAddress;                       // First MAX_SSID_LENGTH bytes for SSID.
    int passAddr = baseAddress + MAX_SSID_LENGTH;       // Next MAX_PASS_LENGTH bytes for password.
    int rememberAddr = baseAddress + MAX_SSID_LENGTH + MAX_PASS_LENGTH; // 1 byte for remember flag.

    writeStringToEEPROM(ssidAddr, wifiCredentials[i].ssid, MAX_SSID_LENGTH);
    yield();
    writeStringToEEPROM(passAddr, wifiCredentials[i].password, MAX_PASS_LENGTH);
    yield();
    EEPROM.write(rememberAddr, wifiCredentials[i].remember ? 1 : 0);
    yield();
  }
  // Write the last connected network index.
  writeIntToEEPROM(LAST_CONNECTED_ADDRESS, lastConnectedNetworkIndex);
  EEPROM.commit();
}

void EEPROMManager::loadWiFiCredentials() {
  // Load each WiFi credential from its fixed block.
  for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    int baseAddress = WIFI_CREDENTIALS_START + i * CREDENTIAL_BLOCK_SIZE;
    int ssidAddr = baseAddress;
    int passAddr = baseAddress + MAX_SSID_LENGTH;
    int rememberAddr = baseAddress + MAX_SSID_LENGTH + MAX_PASS_LENGTH;

    wifiCredentials[i].ssid = readStringFromEEPROM(ssidAddr, MAX_SSID_LENGTH);
    yield();
    wifiCredentials[i].password = readStringFromEEPROM(passAddr, MAX_PASS_LENGTH);
    yield();
    wifiCredentials[i].remember = (EEPROM.read(rememberAddr) == 1);
    yield();
  }
  lastConnectedNetworkIndex = readIntFromEEPROM(LAST_CONNECTED_ADDRESS, -1);
}

void EEPROMManager::clearEntireEEPROM() {
  // Overwrite every byte in the EEPROM with 0.
  for (int addr = 0; addr < EEPROM_SIZE; addr++) {
    EEPROM.write(addr, 0);
  }
  EEPROM.commit();
  Serial.println("✅ Entire EEPROM cleared.");
  
  // Reset default values
  soundTCPServerIP = "";
  soundTCPServerPort = 12345;
  lightTCPServerIP = "";
  lightTCPServerPort = 12345;
  soundMQTTServerURL = "";
  soundMQTTServerPort = 8883;
  soundMQTTUsername = "";
  soundMQTTPassword = "";
  lightMQTTServerURL = "";
  lightMQTTServerPort = 8883;
  lightMQTTUsername = "";
  lightMQTTPassword = "";
  deviceName = "ESP32Device";
  
  for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    wifiCredentials[i].ssid = "";
    wifiCredentials[i].password = "";
    wifiCredentials[i].remember = false;
  }
  lastConnectedNetworkIndex = -1;
}

// New configuration methods
void EEPROMManager::saveSoundTCPServer(const String& ip, int port) {
  writeStringToEEPROM(SOUND_TCP_SERVER_IP_ADDRESS, ip, MAX_IP_LENGTH);
  writeIntToEEPROM(SOUND_TCP_SERVER_PORT_ADDRESS, port);
  soundTCPServerIP = ip;
  soundTCPServerPort = port;
  EEPROM.commit();
  Serial.println("Sound TCP Server configuration saved.");
}

void EEPROMManager::saveLightTCPServer(const String& ip, int port) {
  writeStringToEEPROM(LIGHT_TCP_SERVER_IP_ADDRESS, ip, MAX_IP_LENGTH);
  writeIntToEEPROM(LIGHT_TCP_SERVER_PORT_ADDRESS, port);
  lightTCPServerIP = ip;
  lightTCPServerPort = port;
  EEPROM.commit();
  Serial.println("Light TCP Server configuration saved.");
}

void EEPROMManager::saveSoundMQTTServer(const String& url, int port, const String& username, const String& password) {
  writeStringToEEPROM(SOUND_MQTT_SERVER_URL_ADDRESS, url, MAX_URL_LENGTH);
  writeIntToEEPROM(SOUND_MQTT_SERVER_PORT_ADDRESS, port);
  writeStringToEEPROM(SOUND_MQTT_SERVER_USERNAME_ADDRESS, username, MAX_USERNAME_LENGTH);
  writeStringToEEPROM(SOUND_MQTT_SERVER_PASSWORD_ADDRESS, password, MAX_PASS_LENGTH);
  
  soundMQTTServerURL = url;
  soundMQTTServerPort = port;
  soundMQTTUsername = username;
  soundMQTTPassword = password;
  
  EEPROM.commit();
  Serial.println("Sound MQTT Server configuration saved.");
}

void EEPROMManager::saveLightMQTTServer(const String& url, int port, const String& username, const String& password) {
  writeStringToEEPROM(LIGHT_MQTT_SERVER_URL_ADDRESS, url, MAX_URL_LENGTH);
  writeIntToEEPROM(LIGHT_MQTT_SERVER_PORT_ADDRESS, port);
  writeStringToEEPROM(LIGHT_MQTT_SERVER_USERNAME_ADDRESS, username, MAX_USERNAME_LENGTH);
  writeStringToEEPROM(LIGHT_MQTT_SERVER_PASSWORD_ADDRESS, password, MAX_PASS_LENGTH);
  
  lightMQTTServerURL = url;
  lightMQTTServerPort = port;
  lightMQTTUsername = username;
  lightMQTTPassword = password;
  
  EEPROM.commit();
  Serial.println("Light MQTT Server configuration saved.");
}

void EEPROMManager::saveDeviceName(const String& name) {
  writeStringToEEPROM(DEVICE_NAME_ADDRESS, name, MAX_DEVICE_NAME_LENGTH);
  deviceName = name;
  EEPROM.commit();
  Serial.println("Device name saved: " + name);
}

void EEPROMManager::loadAllConfigurations() {
  // Load WiFi credentials
  loadWiFiCredentials();
  
  // Load TCP Server configurations
  soundTCPServerIP = readStringFromEEPROM(SOUND_TCP_SERVER_IP_ADDRESS, MAX_IP_LENGTH);
  soundTCPServerPort = readIntFromEEPROM(SOUND_TCP_SERVER_PORT_ADDRESS, 12345);
  lightTCPServerIP = readStringFromEEPROM(LIGHT_TCP_SERVER_IP_ADDRESS, MAX_IP_LENGTH);
  lightTCPServerPort = readIntFromEEPROM(LIGHT_TCP_SERVER_PORT_ADDRESS, 12345);
  
  // Load MQTT Server configurations
  soundMQTTServerURL = readStringFromEEPROM(SOUND_MQTT_SERVER_URL_ADDRESS, MAX_URL_LENGTH);
  soundMQTTServerPort = readIntFromEEPROM(SOUND_MQTT_SERVER_PORT_ADDRESS, 8883);
  soundMQTTUsername = readStringFromEEPROM(SOUND_MQTT_SERVER_USERNAME_ADDRESS, MAX_USERNAME_LENGTH);
  soundMQTTPassword = readStringFromEEPROM(SOUND_MQTT_SERVER_PASSWORD_ADDRESS, MAX_PASS_LENGTH);
  
  lightMQTTServerURL = readStringFromEEPROM(LIGHT_MQTT_SERVER_URL_ADDRESS, MAX_URL_LENGTH);
  lightMQTTServerPort = readIntFromEEPROM(LIGHT_MQTT_SERVER_PORT_ADDRESS, 8883);
  lightMQTTUsername = readStringFromEEPROM(LIGHT_MQTT_SERVER_USERNAME_ADDRESS, MAX_USERNAME_LENGTH);
  lightMQTTPassword = readStringFromEEPROM(LIGHT_MQTT_SERVER_PASSWORD_ADDRESS, MAX_PASS_LENGTH);
  
  // Load device name
  deviceName = readStringFromEEPROM(DEVICE_NAME_ADDRESS, MAX_DEVICE_NAME_LENGTH);
  if (deviceName.isEmpty()) {
    deviceName = "ESP32Device"; // Default name if not set
    
    // Save the default name to EEPROM for next time
    writeStringToEEPROM(DEVICE_NAME_ADDRESS, deviceName, MAX_DEVICE_NAME_LENGTH);
    EEPROM.commit();
  }
  
  Serial.println("All configurations loaded.");
}

String EEPROMManager::getConfigurationInfo() {
  String info = "=== CONFIGURATION INFO ===\n";
  
  // Device name
  info += "Device Name: " + deviceName + "\n\n";
  
  // WiFi Networks
  info += "WiFi Networks:\n";
  for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    info += "  Slot " + String(i) + ": ";
    if (wifiCredentials[i].ssid.isEmpty()) {
      info += "Not Set\n";
    } else {
      info += "SSID: " + wifiCredentials[i].ssid + "\n";
    }
  }
  info += "Last Used: " + String(lastConnectedNetworkIndex) + "\n\n";
  
  // Sound TCP Server
  info += "Sound TCP Server:\n";
  info += "  IP: " + (soundTCPServerIP.isEmpty() ? "Not Set" : soundTCPServerIP) + "\n";
  info += "  Port: " + String(soundTCPServerPort) + "\n\n";
  
  // Light TCP Server
  info += "Light TCP Server:\n";
  info += "  IP: " + (lightTCPServerIP.isEmpty() ? "Not Set" : lightTCPServerIP) + "\n";
  info += "  Port: " + String(lightTCPServerPort) + "\n\n";
  
  // Sound MQTT Server
  info += "Sound MQTT Server:\n";
  info += "  URL: " + (soundMQTTServerURL.isEmpty() ? "Not Set" : soundMQTTServerURL) + "\n";
  info += "  Port: " + String(soundMQTTServerPort) + "\n";
  info += "  Username: " + (soundMQTTUsername.isEmpty() ? "Not Set" : soundMQTTUsername) + "\n\n";
  
  // Light MQTT Server
  info += "Light MQTT Server:\n";
  info += "  URL: " + (lightMQTTServerURL.isEmpty() ? "Not Set" : lightMQTTServerURL) + "\n";
  info += "  Port: " + String(lightMQTTServerPort) + "\n";
  info += "  Username: " + (lightMQTTUsername.isEmpty() ? "Not Set" : lightMQTTUsername) + "\n";
  
  return info;
}