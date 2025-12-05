// ML Feature Extraction for WiFi analysis
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <vector>

// Feature vector size for Edge Impulse model
#define FEATURE_VECTOR_SIZE 32

struct WiFiFeatures {
    // Signal characteristics
    int8_t rssi;
    int8_t noise;
    float snr;
    
    // Channel info
    uint8_t channel;
    uint8_t secondaryChannel;
    
    // Beacon analysis
    uint16_t beaconInterval;
    uint16_t capability;
    bool hasWPS;
    bool hasWPA;
    bool hasWPA2;
    bool hasWPA3;
    bool isHidden;
    
    // Timing features
    uint32_t responseTime;
    uint16_t beaconCount;
    float beaconJitter;
    
    // Probe response analysis
    bool respondsToProbe;
    uint16_t probeResponseTime;
    
    // IEs (Information Elements)
    uint8_t vendorIECount;
    uint8_t supportedRates;
    uint8_t htCapabilities;
    uint8_t vhtCapabilities;
    
    // Derived
    float anomalyScore;
};

struct ProbeFeatures {
    uint8_t macPrefix[3];
    uint8_t probeCount;
    uint8_t uniqueSSIDCount;
    bool randomMAC;
    int8_t avgRSSI;
    uint32_t lastSeen;
};

class FeatureExtractor {
public:
    static void init();
    
    // Extract features from raw WiFi scan
    static WiFiFeatures extractFromScan(const wifi_ap_record_t* ap);
    static WiFiFeatures extractFromBeacon(const uint8_t* frame, uint16_t len, int8_t rssi);
    
    // Extract probe request features
    static ProbeFeatures extractFromProbe(const uint8_t* frame, uint16_t len, int8_t rssi);
    
    // Convert to feature vector for ML
    static void toFeatureVector(const WiFiFeatures& features, float* output);
    static void probeToFeatureVector(const ProbeFeatures& features, float* output);
    
    // Batch feature extraction
    static std::vector<float> extractBatchFeatures(const std::vector<WiFiFeatures>& networks);
    
    // Normalization (must be called after model training)
    static void setNormalizationParams(const float* means, const float* stds);
    
private:
    static float featureMeans[FEATURE_VECTOR_SIZE];
    static float featureStds[FEATURE_VECTOR_SIZE];
    static bool normParamsLoaded;
    
    static uint16_t parseBeaconInterval(const uint8_t* frame, uint16_t len);
    static uint16_t parseCapability(const uint8_t* frame, uint16_t len);
    static void parseIEs(const uint8_t* frame, uint16_t len, WiFiFeatures& features);
    static bool isRandomMAC(const uint8_t* mac);
    static float normalize(float value, float mean, float std);
};
