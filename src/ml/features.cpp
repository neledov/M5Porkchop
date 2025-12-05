// ML Feature Extraction implementation

#include "features.h"
#include <string.h>

// Static members
float FeatureExtractor::featureMeans[FEATURE_VECTOR_SIZE] = {0};
float FeatureExtractor::featureStds[FEATURE_VECTOR_SIZE] = {1};  // Default to 1 to avoid div/0
bool FeatureExtractor::normParamsLoaded = false;

void FeatureExtractor::init() {
    // Reset normalization params
    for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
        featureMeans[i] = 0.0f;
        featureStds[i] = 1.0f;
    }
    normParamsLoaded = false;
    
    Serial.println("[ML] Feature extractor initialized");
}

WiFiFeatures FeatureExtractor::extractFromScan(const wifi_ap_record_t* ap) {
    WiFiFeatures f = {0};
    
    f.rssi = ap->rssi;
    f.noise = -95;  // Typical noise floor for ESP32
    f.snr = (float)(f.rssi - f.noise);
    
    f.channel = ap->primary;
    f.secondaryChannel = ap->second;
    
    // Parse authmode
    switch (ap->authmode) {
        case WIFI_AUTH_OPEN:
            f.hasWPA = false;
            f.hasWPA2 = false;
            f.hasWPA3 = false;
            break;
        case WIFI_AUTH_WPA_PSK:
            f.hasWPA = true;
            break;
        case WIFI_AUTH_WPA2_PSK:
            f.hasWPA2 = true;
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            f.hasWPA = true;
            f.hasWPA2 = true;
            break;
        case WIFI_AUTH_WPA3_PSK:
            f.hasWPA3 = true;
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            f.hasWPA2 = true;
            f.hasWPA3 = true;
            break;
        default:
            break;
    }
    
    // Check if hidden SSID
    f.isHidden = (ap->ssid[0] == 0);
    
    // 802.11n/ac capabilities
    f.htCapabilities = (ap->phy_11n) ? 1 : 0;
    // Note: VHT would need frame parsing
    
    return f;
}

WiFiFeatures FeatureExtractor::extractFromBeacon(const uint8_t* frame, uint16_t len, int8_t rssi) {
    WiFiFeatures f = {0};
    
    if (len < 36) return f;  // Minimum beacon frame size
    
    // Frame control is first 2 bytes, skip to fixed params at offset 24
    // Fixed params: timestamp(8) + beacon_interval(2) + capability(2)
    
    f.rssi = rssi;
    f.noise = -95;
    f.snr = (float)(f.rssi - f.noise);
    
    f.beaconInterval = parseBeaconInterval(frame, len);
    f.capability = parseCapability(frame, len);
    
    // Parse capability bits
    f.isHidden = !(f.capability & 0x0001);  // ESS bit
    f.hasWPS = false;  // Determined from IEs
    
    // Parse Information Elements
    parseIEs(frame, len, f);
    
    return f;
}

ProbeFeatures FeatureExtractor::extractFromProbe(const uint8_t* frame, uint16_t len, int8_t rssi) {
    ProbeFeatures p = {0};
    
    if (len < 24) return p;  // Minimum frame size
    
    // Source MAC is at offset 10
    memcpy(p.macPrefix, frame + 10, 3);
    
    // Check if randomized MAC (bit 1 of first byte set = locally administered)
    p.randomMAC = isRandomMAC(frame + 10);
    
    p.avgRSSI = rssi;
    p.probeCount = 1;
    p.lastSeen = millis();
    
    return p;
}

void FeatureExtractor::toFeatureVector(const WiFiFeatures& features, float* output) {
    // Fill feature vector - order matters for model!
    output[0] = (float)features.rssi;
    output[1] = (float)features.noise;
    output[2] = features.snr;
    output[3] = (float)features.channel;
    output[4] = (float)features.secondaryChannel;
    output[5] = (float)features.beaconInterval;
    output[6] = (float)(features.capability & 0xFF);
    output[7] = (float)((features.capability >> 8) & 0xFF);
    output[8] = features.hasWPS ? 1.0f : 0.0f;
    output[9] = features.hasWPA ? 1.0f : 0.0f;
    output[10] = features.hasWPA2 ? 1.0f : 0.0f;
    output[11] = features.hasWPA3 ? 1.0f : 0.0f;
    output[12] = features.isHidden ? 1.0f : 0.0f;
    output[13] = (float)features.responseTime;
    output[14] = (float)features.beaconCount;
    output[15] = features.beaconJitter;
    output[16] = features.respondsToProbe ? 1.0f : 0.0f;
    output[17] = (float)features.probeResponseTime;
    output[18] = (float)features.vendorIECount;
    output[19] = (float)features.supportedRates;
    output[20] = (float)features.htCapabilities;
    output[21] = (float)features.vhtCapabilities;
    output[22] = features.anomalyScore;
    // Pad remaining with zeros
    for (int i = 23; i < FEATURE_VECTOR_SIZE; i++) {
        output[i] = 0.0f;
    }
    
    // Apply normalization if available
    if (normParamsLoaded) {
        for (int i = 0; i < FEATURE_VECTOR_SIZE; i++) {
            output[i] = normalize(output[i], featureMeans[i], featureStds[i]);
        }
    }
}

void FeatureExtractor::probeToFeatureVector(const ProbeFeatures& features, float* output) {
    memset(output, 0, FEATURE_VECTOR_SIZE * sizeof(float));
    
    output[0] = (float)features.macPrefix[0];
    output[1] = (float)features.macPrefix[1];
    output[2] = (float)features.macPrefix[2];
    output[3] = (float)features.probeCount;
    output[4] = (float)features.uniqueSSIDCount;
    output[5] = features.randomMAC ? 1.0f : 0.0f;
    output[6] = (float)features.avgRSSI;
}

std::vector<float> FeatureExtractor::extractBatchFeatures(const std::vector<WiFiFeatures>& networks) {
    std::vector<float> batch;
    batch.reserve(networks.size() * FEATURE_VECTOR_SIZE);
    
    float vec[FEATURE_VECTOR_SIZE];
    for (const auto& net : networks) {
        toFeatureVector(net, vec);
        batch.insert(batch.end(), vec, vec + FEATURE_VECTOR_SIZE);
    }
    
    return batch;
}

void FeatureExtractor::setNormalizationParams(const float* means, const float* stds) {
    memcpy(featureMeans, means, FEATURE_VECTOR_SIZE * sizeof(float));
    memcpy(featureStds, stds, FEATURE_VECTOR_SIZE * sizeof(float));
    normParamsLoaded = true;
    
    Serial.println("[ML] Normalization parameters loaded");
}

uint16_t FeatureExtractor::parseBeaconInterval(const uint8_t* frame, uint16_t len) {
    if (len < 34) return 100;  // Default beacon interval
    // Beacon interval at offset 32 (after 24 byte header + 8 byte timestamp)
    return frame[32] | (frame[33] << 8);
}

uint16_t FeatureExtractor::parseCapability(const uint8_t* frame, uint16_t len) {
    if (len < 36) return 0;
    // Capability at offset 34
    return frame[34] | (frame[35] << 8);
}

void FeatureExtractor::parseIEs(const uint8_t* frame, uint16_t len, WiFiFeatures& features) {
    // IEs start at offset 36 (after fixed params)
    uint16_t offset = 36;
    
    while (offset + 2 < len) {
        uint8_t id = frame[offset];
        uint8_t ieLen = frame[offset + 1];
        
        if (offset + 2 + ieLen > len) break;
        
        switch (id) {
            case 0:  // SSID
                // Check for hidden SSID
                if (ieLen == 0 || frame[offset + 2] == 0) {
                    features.isHidden = true;
                }
                break;
                
            case 1:  // Supported Rates
                features.supportedRates = ieLen;
                break;
                
            case 45:  // HT Capabilities
                features.htCapabilities = 1;
                break;
                
            case 191:  // VHT Capabilities
                features.vhtCapabilities = 1;
                break;
                
            case 221:  // Vendor Specific
                features.vendorIECount++;
                // Check for WPS
                if (ieLen >= 4) {
                    if (frame[offset+2] == 0x00 && frame[offset+3] == 0x50 &&
                        frame[offset+4] == 0xF2 && frame[offset+5] == 0x04) {
                        features.hasWPS = true;
                    }
                }
                break;
        }
        
        offset += 2 + ieLen;
    }
}

bool FeatureExtractor::isRandomMAC(const uint8_t* mac) {
    // Locally administered bit (bit 1 of first octet)
    return (mac[0] & 0x02) != 0;
}

float FeatureExtractor::normalize(float value, float mean, float std) {
    if (std < 0.001f) return 0.0f;
    return (value - mean) / std;
}
