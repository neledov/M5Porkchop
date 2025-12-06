// Oink Mode - Deauth and Packet Sniffing
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <vector>
#include <FS.h>
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

struct EAPOLFrame {
    uint8_t data[512];
    uint16_t len;
    uint8_t messageNum;  // 1-4
    uint32_t timestamp;
};

struct CapturedHandshake {
    uint8_t bssid[6];
    uint8_t station[6];
    char ssid[33];
    EAPOLFrame frames[4];  // M1, M2, M3, M4
    uint8_t capturedMask;  // Bits 0-3 for M1-M4
    uint32_t firstSeen;
    uint32_t lastSeen;
    bool saved;  // Already saved to SD
    
    bool hasM1() const { return capturedMask & 0x01; }
    bool hasM2() const { return capturedMask & 0x02; }
    bool hasM3() const { return capturedMask & 0x04; }
    bool hasM4() const { return capturedMask & 0x08; }
    bool isComplete() const { return hasM1() && hasM2(); }  // M1+M2 is enough for crack
    bool isFull() const { return (capturedMask & 0x0F) == 0x0F; }
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
    static uint16_t getCompleteHandshakeCount();
    static bool saveHandshakePCAP(const CapturedHandshake& hs, const char* path);
    static bool saveAllHandshakes();
    static void autoSaveCheck();
    
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
    
    // Beacon frame storage (for PCAP)
    static uint8_t* beaconFrame;
    static uint16_t beaconFrameLen;
    static bool beaconCaptured;
    
    // Promiscuous mode callback (IRAM for ISR performance)
    static void IRAM_ATTR promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    
    static void processBeacon(const uint8_t* payload, uint16_t len, int8_t rssi);
    static void processDataFrame(const uint8_t* payload, uint16_t len, int8_t rssi);
    static void processEAPOL(const uint8_t* payload, uint16_t len, const uint8_t* srcMac, const uint8_t* dstMac);
    
    static void sendDeauthFrame(const uint8_t* bssid, const uint8_t* station, uint8_t reason);
    static void hopChannel();
    
    static int findNetwork(const uint8_t* bssid);
    static int findOrCreateHandshake(const uint8_t* bssid, const uint8_t* station);
    static void writePCAPHeader(fs::File& f);
    static void writePCAPPacket(fs::File& f, const uint8_t* data, uint16_t len, uint32_t ts);
};
