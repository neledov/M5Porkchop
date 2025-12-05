// Oink Mode - Deauth and Packet Sniffing
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <vector>
#include "../ml/features.h"

struct DetectedNetwork {
    uint8_t bssid[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    WiFiFeatures features;
    uint32_t lastSeen;
    uint16_t beaconCount;
    bool isTarget;
};

struct CapturedHandshake {
    uint8_t bssid[6];
    uint8_t station[6];
    char ssid[33];
    uint8_t eapolData[512];
    uint16_t eapolLen;
    uint8_t messageNum;  // 1-4
    uint32_t timestamp;
    bool complete;
};

class OinkMode {
public:
    static void init();
    static void start();
    static void stop();
    static void update();
    static bool isRunning() { return running; }
    
    // Scanning
    static void startScan();
    static void stopScan();
    static const std::vector<DetectedNetwork>& getNetworks() { return networks; }
    
    // Target selection
    static void selectTarget(int index);
    static void clearTarget();
    static DetectedNetwork* getTarget();
    
    // Deauth (educational use only)
    static void startDeauth();
    static void stopDeauth();
    static bool isDeauthing() { return deauthing; }
    
    // Handshake capture
    static const std::vector<CapturedHandshake>& getHandshakes() { return handshakes; }
    static bool saveHandshakes(const char* path);
    
    // Channel hopping
    static void setChannel(uint8_t ch);
    static uint8_t getChannel() { return currentChannel; }
    static void enableChannelHop(bool enable);
    
    // Statistics
    static uint32_t getPacketCount() { return packetCount; }
    static uint32_t getDeauthCount() { return deauthCount; }
    
private:
    static bool running;
    static bool scanning;
    static bool deauthing;
    static bool channelHopping;
    static uint8_t currentChannel;
    static uint32_t lastHopTime;
    static uint32_t lastScanTime;
    
    static std::vector<DetectedNetwork> networks;
    static std::vector<CapturedHandshake> handshakes;
    static int targetIndex;
    static uint32_t packetCount;
    static uint32_t deauthCount;
    
    // Promiscuous mode callback
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    
    static void processBeacon(const uint8_t* payload, uint16_t len, int8_t rssi);
    static void processDataFrame(const uint8_t* payload, uint16_t len, int8_t rssi);
    static void processEAPOL(const uint8_t* payload, uint16_t len, const uint8_t* srcMac, const uint8_t* dstMac);
    
    static void sendDeauthFrame(const uint8_t* bssid, const uint8_t* station, uint8_t reason);
    static void hopChannel();
    
    static int findNetwork(const uint8_t* bssid);
    static void updateNetwork(int index, const wifi_ap_record_t* ap);
};
