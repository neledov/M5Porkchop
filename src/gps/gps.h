// GPS AT6668 Module Interface
#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

struct GPSData {
    double latitude;
    double longitude;
    double altitude;
    float speed;
    float course;
    uint8_t satellites;
    uint16_t hdop;
    uint32_t date;
    uint32_t time;
    bool valid;
    bool fix;
    uint32_t age;  // Age of last fix in ms
};

class GPS {
public:
    static void init(uint8_t rxPin, uint8_t txPin, uint32_t baud = 9600);
    static void update();
    static void sleep();
    static void wake();
    
    static bool hasFix();
    static GPSData getData();
    static String getLocationString();
    static String getTimeString();
    
    // Power management
    static void setPowerMode(bool active);
    static bool isActive();
    
    // Statistics
    static uint32_t getFixCount() { return fixCount; }
    static uint32_t getLastFixTime() { return lastFixTime; }
    
private:
    static TinyGPSPlus gps;
    static HardwareSerial* serial;
    static bool active;
    static GPSData currentData;
    static uint32_t fixCount;
    static uint32_t lastFixTime;
    static uint32_t lastUpdateTime;
    
    static void processSerial();
    static void updateData();
};
