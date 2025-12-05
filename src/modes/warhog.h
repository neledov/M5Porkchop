// Warhog Mode - Wardriving with GPS
#pragma once

#include <Arduino.h>
#include <vector>
#include <esp_wifi.h>
#include "../gps/gps.h"

struct WardrivingEntry {
    uint8_t bssid[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    double latitude;
    double longitude;
    double altitude;
    uint32_t timestamp;
    bool saved;
};

class WarhogMode {
public:
    static void init();
    static void start();
    static void stop();
    static void update();
    static bool isRunning() { return running; }
    
    // Scan control
    static void triggerScan();
    static bool isScanComplete();
    
    // Data access
    static const std::vector<WardrivingEntry>& getEntries() { return entries; }
    static size_t getEntryCount() { return entries.size(); }
    static size_t getNewCount() { return newCount; }
    
    // Export
    static bool exportCSV(const char* path);
    static bool exportKismet(const char* path);
    static bool exportWigle(const char* path);
    
    // GPS
    static bool hasGPSFix();
    static GPSData getGPSData();
    
    // Statistics
    static uint32_t getTotalNetworks() { return totalNetworks; }
    static uint32_t getOpenNetworks() { return openNetworks; }
    static uint32_t getWEPNetworks() { return wepNetworks; }
    static uint32_t getWPANetworks() { return wpaNetworks; }
    
private:
    static bool running;
    static bool scanComplete;
    static uint32_t lastScanTime;
    static uint32_t scanInterval;
    
    static std::vector<WardrivingEntry> entries;
    static size_t newCount;
    
    // Statistics
    static uint32_t totalNetworks;
    static uint32_t openNetworks;
    static uint32_t wepNetworks;
    static uint32_t wpaNetworks;
    
    static void performScan();
    static void processScanResults();
    static int findEntry(const uint8_t* bssid);
    static String authModeToString(wifi_auth_mode_t mode);
    static String generateFilename(const char* ext);
};
