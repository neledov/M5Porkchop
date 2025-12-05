// Configuration management implementation

#include "config.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

// Static member initialization
GPSConfig Config::gpsConfig;
MLConfig Config::mlConfig;
WiFiConfig Config::wifiConfig;
PersonalityConfig Config::personalityConfig;
bool Config::initialized = false;

bool Config::init() {
    // M5Cardputer handles SD initialization via M5.begin()
    // SD is on the built-in SD card slot (GPIO 12 for CS)
    
    if (!SD.begin(GPIO_NUM_12, SPI, 25000000)) {
        Serial.println("[CONFIG] SD card init failed, using defaults");
        createDefaultConfig();
        createDefaultPersonality();
        initialized = true;
        return true;
    }
    
    Serial.println("[CONFIG] SD card mounted");
    
    // Create directories if needed
    if (!SD.exists("/handshakes")) SD.mkdir("/handshakes");
    if (!SD.exists("/mldata")) SD.mkdir("/mldata");
    if (!SD.exists("/models")) SD.mkdir("/models");
    if (!SD.exists("/logs")) SD.mkdir("/logs");
    if (!SD.exists("/wardriving")) SD.mkdir("/wardriving");
    
    // Load or create config
    if (!SD.exists(CONFIG_FILE)) {
        Serial.println("[CONFIG] Creating default config");
        createDefaultConfig();
        save();
    }
    
    if (!SD.exists(PERSONALITY_FILE)) {
        Serial.println("[CONFIG] Creating default personality");
        createDefaultPersonality();
    }
    
    if (!load()) {
        Serial.println("[CONFIG] Failed to load config, using defaults");
        createDefaultConfig();
    }
    
    if (!loadPersonality()) {
        Serial.println("[CONFIG] Failed to load personality, using defaults");
        createDefaultPersonality();
    }
    
    initialized = true;
    return true;
}

bool Config::load() {
    File file = SD.open(CONFIG_FILE, FILE_READ);
    if (!file) {
        Serial.println("[CONFIG] Cannot open config file");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    
    if (err) {
        Serial.printf("[CONFIG] JSON parse error: %s\n", err.c_str());
        return false;
    }
    
    // GPS config
    if (doc["gps"].is<JsonObject>()) {
        gpsConfig.enabled = doc["gps"]["enabled"] | true;
        gpsConfig.rxPin = doc["gps"]["rxPin"] | 1;
        gpsConfig.txPin = doc["gps"]["txPin"] | 2;
        gpsConfig.baudRate = doc["gps"]["baudRate"] | 9600;
        gpsConfig.updateInterval = doc["gps"]["updateInterval"] | 5;
        gpsConfig.sleepTimeMs = doc["gps"]["sleepTimeMs"] | 5000;
        gpsConfig.powerSave = doc["gps"]["powerSave"] | true;
    }
    
    // ML config
    if (doc["ml"].is<JsonObject>()) {
        mlConfig.enabled = doc["ml"]["enabled"] | true;
        mlConfig.modelPath = doc["ml"]["modelPath"] | "/models/porkchop_model.bin";
        mlConfig.confidenceThreshold = doc["ml"]["confidenceThreshold"] | 0.7f;
        mlConfig.rogueApThreshold = doc["ml"]["rogueApThreshold"] | 0.8f;
        mlConfig.vulnScorerThreshold = doc["ml"]["vulnScorerThreshold"] | 0.6f;
        mlConfig.autoUpdate = doc["ml"]["autoUpdate"] | false;
        mlConfig.updateUrl = doc["ml"]["updateUrl"] | "";
    }
    
    // WiFi config
    if (doc["wifi"].is<JsonObject>()) {
        wifiConfig.channelHopInterval = doc["wifi"]["channelHopInterval"] | 500;
        wifiConfig.scanDuration = doc["wifi"]["scanDuration"] | 2000;
        wifiConfig.maxNetworks = doc["wifi"]["maxNetworks"] | 50;
        wifiConfig.enableDeauth = doc["wifi"]["enableDeauth"] | false;
        wifiConfig.otaSSID = doc["wifi"]["otaSSID"] | "";
        wifiConfig.otaPassword = doc["wifi"]["otaPassword"] | "";
        wifiConfig.autoConnect = doc["wifi"]["autoConnect"] | false;
    }
    
    Serial.println("[CONFIG] Loaded successfully");
    return true;
}

bool Config::loadPersonality() {
    File file = SD.open(PERSONALITY_FILE, FILE_READ);
    if (!file) {
        return false;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    
    if (err) {
        return false;
    }
    
    const char* name = doc["name"] | "Porkchop";
    strncpy(personalityConfig.name, name, sizeof(personalityConfig.name) - 1);
    personalityConfig.name[sizeof(personalityConfig.name) - 1] = '\0';
    
    personalityConfig.mood = doc["mood"] | 50;
    personalityConfig.experience = doc["experience"] | 0;
    personalityConfig.curiosity = doc["curiosity"] | 0.7f;
    personalityConfig.aggression = doc["aggression"] | 0.3f;
    personalityConfig.patience = doc["patience"] | 0.5f;
    personalityConfig.soundEnabled = doc["soundEnabled"] | true;
    
    Serial.printf("[CONFIG] Personality: %s (mood: %d)\n", 
                  personalityConfig.name, 
                  personalityConfig.mood);
    return true;
}

bool Config::save() {
    JsonDocument doc;
    
    // GPS config
    doc["gps"]["enabled"] = gpsConfig.enabled;
    doc["gps"]["rxPin"] = gpsConfig.rxPin;
    doc["gps"]["txPin"] = gpsConfig.txPin;
    doc["gps"]["baudRate"] = gpsConfig.baudRate;
    doc["gps"]["updateInterval"] = gpsConfig.updateInterval;
    doc["gps"]["sleepTimeMs"] = gpsConfig.sleepTimeMs;
    doc["gps"]["powerSave"] = gpsConfig.powerSave;
    
    // ML config
    doc["ml"]["enabled"] = mlConfig.enabled;
    doc["ml"]["modelPath"] = mlConfig.modelPath;
    doc["ml"]["confidenceThreshold"] = mlConfig.confidenceThreshold;
    doc["ml"]["rogueApThreshold"] = mlConfig.rogueApThreshold;
    doc["ml"]["vulnScorerThreshold"] = mlConfig.vulnScorerThreshold;
    doc["ml"]["autoUpdate"] = mlConfig.autoUpdate;
    doc["ml"]["updateUrl"] = mlConfig.updateUrl;
    
    // WiFi config
    doc["wifi"]["channelHopInterval"] = wifiConfig.channelHopInterval;
    doc["wifi"]["scanDuration"] = wifiConfig.scanDuration;
    doc["wifi"]["maxNetworks"] = wifiConfig.maxNetworks;
    doc["wifi"]["enableDeauth"] = wifiConfig.enableDeauth;
    doc["wifi"]["otaSSID"] = wifiConfig.otaSSID;
    doc["wifi"]["otaPassword"] = wifiConfig.otaPassword;
    doc["wifi"]["autoConnect"] = wifiConfig.autoConnect;
    
    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        return false;
    }
    
    serializeJsonPretty(doc, file);
    file.close();
    return true;
}

bool Config::createDefaultConfig() {
    gpsConfig = GPSConfig();
    mlConfig = MLConfig();
    wifiConfig = WiFiConfig();
    return true;
}

bool Config::createDefaultPersonality() {
    strcpy(personalityConfig.name, "Porkchop");
    personalityConfig.mood = 50;
    personalityConfig.experience = 0;
    personalityConfig.curiosity = 0.7f;
    personalityConfig.aggression = 0.3f;
    personalityConfig.patience = 0.5f;
    personalityConfig.soundEnabled = true;
    return true;
}

void Config::setGPS(const GPSConfig& cfg) {
    gpsConfig = cfg;
    save();
}

void Config::setML(const MLConfig& cfg) {
    mlConfig = cfg;
    save();
}

void Config::setWiFi(const WiFiConfig& cfg) {
    wifiConfig = cfg;
    save();
}

void Config::setPersonality(const PersonalityConfig& cfg) {
    personalityConfig = cfg;
    
    // Save personality to separate file
    JsonDocument doc;
    doc["name"] = cfg.name;
    doc["mood"] = cfg.mood;
    doc["experience"] = cfg.experience;
    doc["curiosity"] = cfg.curiosity;
    doc["aggression"] = cfg.aggression;
    doc["patience"] = cfg.patience;
    doc["soundEnabled"] = cfg.soundEnabled;
    
    File file = SD.open(PERSONALITY_FILE, FILE_WRITE);
    if (file) {
        serializeJsonPretty(doc, file);
        file.close();
    }
}
