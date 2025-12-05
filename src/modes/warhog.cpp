// Warhog Mode implementation

#include "warhog.h"
#include "../core/config.h"
#include "../ui/display.h"
#include "../piglet/mood.h"
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>

// Static members
bool WarhogMode::running = false;
bool WarhogMode::scanComplete = false;
uint32_t WarhogMode::lastScanTime = 0;
uint32_t WarhogMode::scanInterval = 5000;
std::vector<WardrivingEntry> WarhogMode::entries;
size_t WarhogMode::newCount = 0;
uint32_t WarhogMode::totalNetworks = 0;
uint32_t WarhogMode::openNetworks = 0;
uint32_t WarhogMode::wepNetworks = 0;
uint32_t WarhogMode::wpaNetworks = 0;

void WarhogMode::init() {
    entries.clear();
    newCount = 0;
    totalNetworks = 0;
    openNetworks = 0;
    wepNetworks = 0;
    wpaNetworks = 0;
    
    scanInterval = Config::gps().updateInterval * 1000;
    
    Serial.println("[WARHOG] Initialized");
}

void WarhogMode::start() {
    if (running) return;
    
    Serial.println("[WARHOG] Starting...");
    
    // Initialize WiFi in STA mode for scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Wake up GPS
    GPS::wake();
    
    running = true;
    scanComplete = false;
    lastScanTime = 0;  // Trigger immediate scan
    newCount = 0;
    
    Display::setWiFiStatus(true);
    Serial.println("[WARHOG] Running");
}

void WarhogMode::stop() {
    if (!running) return;
    
    Serial.println("[WARHOG] Stopping...");
    
    running = false;
    
    // Put GPS to sleep if power management enabled
    if (Config::gps().powerSave) {
        GPS::sleep();
    }
    
    Display::setWiFiStatus(false);
    
    Serial.println("[WARHOG] Stopped");
}

void WarhogMode::update() {
    if (!running) return;
    
    uint32_t now = millis();
    
    // Periodic scanning
    if (now - lastScanTime >= scanInterval) {
        performScan();
        lastScanTime = now;
    }
    
    // Process results when scan complete
    if (scanComplete) {
        processScanResults();
        scanComplete = false;
    }
}

void WarhogMode::triggerScan() {
    performScan();
}

bool WarhogMode::isScanComplete() {
    return scanComplete;
}

void WarhogMode::performScan() {
    Serial.println("[WARHOG] Starting WiFi scan...");
    
    // Start async scan
    int result = WiFi.scanNetworks(true, true);  // async=true, hidden=true
    
    if (result == WIFI_SCAN_RUNNING) {
        // Will check completion in update()
    } else if (result == WIFI_SCAN_FAILED) {
        Serial.println("[WARHOG] Scan failed");
    }
}

void WarhogMode::processScanResults() {
    int n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_RUNNING) {
        return;  // Still scanning
    }
    
    if (n == WIFI_SCAN_FAILED || n < 0) {
        Serial.println("[WARHOG] Scan failed");
        WiFi.scanDelete();
        return;
    }
    
    // Get current GPS data
    GPSData gps = GPS::getData();
    bool hasGPS = GPS::hasFix();
    
    Serial.printf("[WARHOG] Found %d networks (GPS: %s)\n", 
                 n, hasGPS ? "yes" : "no");
    
    size_t previousCount = entries.size();
    
    for (int i = 0; i < n; i++) {
        uint8_t* bssid = WiFi.BSSID(i);
        
        int idx = findEntry(bssid);
        
        if (idx < 0) {
            // New network
            WardrivingEntry entry = {0};
            memcpy(entry.bssid, bssid, 6);
            strncpy(entry.ssid, WiFi.SSID(i).c_str(), 32);
            entry.rssi = WiFi.RSSI(i);
            entry.channel = WiFi.channel(i);
            entry.authmode = WiFi.encryptionType(i);
            entry.timestamp = millis();
            entry.saved = false;
            
            if (hasGPS) {
                entry.latitude = gps.latitude;
                entry.longitude = gps.longitude;
                entry.altitude = gps.altitude;
            }
            
            entries.push_back(entry);
            totalNetworks++;
            
            // Track auth types
            switch (entry.authmode) {
                case WIFI_AUTH_OPEN:
                    openNetworks++;
                    break;
                case WIFI_AUTH_WEP:
                    wepNetworks++;
                    break;
                default:
                    wpaNetworks++;
                    break;
            }
            
            Serial.printf("[WARHOG] New: %s (ch%d, %s)\n",
                         entry.ssid, entry.channel, 
                         authModeToString(entry.authmode).c_str());
        } else {
            // Update existing - maybe update GPS if we have better fix
            if (hasGPS && (entries[idx].latitude == 0 || 
                          WiFi.RSSI(i) > entries[idx].rssi)) {
                entries[idx].latitude = gps.latitude;
                entries[idx].longitude = gps.longitude;
                entries[idx].altitude = gps.altitude;
                entries[idx].rssi = WiFi.RSSI(i);
            }
        }
    }
    
    newCount = entries.size() - previousCount;
    
    if (newCount > 0) {
        Mood::onNewNetwork();
    }
    
    WiFi.scanDelete();
    scanComplete = true;
}

bool WarhogMode::hasGPSFix() {
    return GPS::hasFix();
}

GPSData WarhogMode::getGPSData() {
    return GPS::getData();
}

bool WarhogMode::exportCSV(const char* path) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[WARHOG] Failed to open %s\n", path);
        return false;
    }
    
    // CSV header
    f.println("BSSID,SSID,RSSI,Channel,AuthMode,Latitude,Longitude,Altitude,Timestamp");
    
    for (const auto& e : entries) {
        f.printf("%02X:%02X:%02X:%02X:%02X:%02X,",
                e.bssid[0], e.bssid[1], e.bssid[2],
                e.bssid[3], e.bssid[4], e.bssid[5]);
        
        // Escape SSID for CSV
        f.print("\"");
        for (int i = 0; i < 32 && e.ssid[i]; i++) {
            if (e.ssid[i] == '"') f.print("\"\"");
            else f.print(e.ssid[i]);
        }
        f.print("\",");
        
        f.printf("%d,%d,%s,%.6f,%.6f,%.1f,%lu\n",
                e.rssi, e.channel, authModeToString(e.authmode).c_str(),
                e.latitude, e.longitude, e.altitude, e.timestamp);
    }
    
    f.close();
    Serial.printf("[WARHOG] Exported %d entries to %s\n", entries.size(), path);
    return true;
}

bool WarhogMode::exportWigle(const char* path) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        return false;
    }
    
    // Wigle CSV format
    f.println("WigleWifi-1.4,appRelease=porkchop,model=M5Cardputer,release=1.0.0,device=ESP32-S3,display=,board=,brand=M5Stack");
    f.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
    
    for (const auto& e : entries) {
        f.printf("%02X:%02X:%02X:%02X:%02X:%02X,",
                e.bssid[0], e.bssid[1], e.bssid[2],
                e.bssid[3], e.bssid[4], e.bssid[5]);
        
        f.print("\"");
        for (int i = 0; i < 32 && e.ssid[i]; i++) {
            if (e.ssid[i] == '"') f.print("\"\"");
            else f.print(e.ssid[i]);
        }
        f.print("\",");
        
        f.printf("%s,", authModeToString(e.authmode).c_str());
        
        // Timestamp - would need RTC for proper date
        f.printf("2024-01-01 00:00:00,");
        
        f.printf("%d,%d,%.6f,%.6f,%.1f,10.0,WIFI\n",
                e.channel, e.rssi,
                e.latitude, e.longitude, e.altitude);
    }
    
    f.close();
    Serial.printf("[WARHOG] Wigle export: %d entries to %s\n", entries.size(), path);
    return true;
}

bool WarhogMode::exportKismet(const char* path) {
    // Kismet NetXML format - simplified
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        return false;
    }
    
    f.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    f.println("<detection-run kismet-version=\"porkchop\">");
    
    for (const auto& e : entries) {
        f.println("<wireless-network>");
        f.printf("<BSSID>%02X:%02X:%02X:%02X:%02X:%02X</BSSID>\n",
                e.bssid[0], e.bssid[1], e.bssid[2],
                e.bssid[3], e.bssid[4], e.bssid[5]);
        f.printf("<SSID>%s</SSID>\n", e.ssid);
        f.printf("<channel>%d</channel>\n", e.channel);
        f.printf("<encryption>%s</encryption>\n", authModeToString(e.authmode).c_str());
        f.println("<gps-info>");
        f.printf("<lat>%.6f</lat>\n", e.latitude);
        f.printf("<lon>%.6f</lon>\n", e.longitude);
        f.printf("<alt>%.1f</alt>\n", e.altitude);
        f.println("</gps-info>");
        f.println("</wireless-network>");
    }
    
    f.println("</detection-run>");
    f.close();
    
    return true;
}

int WarhogMode::findEntry(const uint8_t* bssid) {
    for (size_t i = 0; i < entries.size(); i++) {
        if (memcmp(entries[i].bssid, bssid, 6) == 0) {
            return i;
        }
    }
    return -1;
}

String WarhogMode::authModeToString(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK: return "WAPI";
        default: return "UNKNOWN";
    }
}

String WarhogMode::generateFilename(const char* ext) {
    // Generate filename with date/time if GPS has time
    char buf[32];
    GPSData gps = GPS::getData();
    
    if (gps.date > 0) {
        snprintf(buf, sizeof(buf), "/warhog_%06lu.%s", gps.date, ext);
    } else {
        snprintf(buf, sizeof(buf), "/warhog_%lu.%s", millis(), ext);
    }
    
    return String(buf);
}
