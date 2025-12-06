// WiFi File Server for SD card access
#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

class FileServer {
public:
    static void init();
    static bool start(const char* ssid, const char* password);
    static void stop();
    static void update();
    
    static bool isRunning() { return running; }
    static bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    static String getIP() { return WiFi.localIP().toString(); }
    static const char* getStatus() { return statusMessage; }
    
private:
    static WebServer* server;
    static bool running;
    static char statusMessage[64];
    
    // HTTP handlers
    static void handleRoot();
    static void handleFileList();
    static void handleDownload();
    static void handleUpload();
    static void handleUploadProcess();
    static void handleDelete();
    static void handleNotFound();
    
    // HTML template
    static const char* getHTML();
};
