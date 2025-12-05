// Display management implementation

#include "display.h"
#include <M5Cardputer.h>
#include "../core/porkchop.h"
#include "../core/config.h"
#include "../piglet/mood.h"
#include "../piglet/avatar.h"

// Static member initialization
M5Canvas Display::topBar(&M5.Display);
M5Canvas Display::mainCanvas(&M5.Display);
M5Canvas Display::bottomBar(&M5.Display);
bool Display::gpsStatus = false;
bool Display::wifiStatus = false;
bool Display::mlStatus = false;

extern Porkchop porkchop;

void Display::init() {
    M5.Display.setRotation(1);
    M5.Display.fillScreen(COLOR_BG);
    M5.Display.setTextColor(COLOR_FG);
    
    // Create canvas sprites
    topBar.createSprite(DISPLAY_W, TOP_BAR_H);
    mainCanvas.createSprite(DISPLAY_W, MAIN_H);
    bottomBar.createSprite(DISPLAY_W, BOTTOM_BAR_H);
    
    topBar.setTextSize(1);
    mainCanvas.setTextSize(1);
    bottomBar.setTextSize(1);
    
    Serial.println("[DISPLAY] Initialized");
}

void Display::update() {
    drawTopBar();
    
    // Draw main content based on mode
    mainCanvas.fillSprite(COLOR_BG);
    mainCanvas.setTextColor(COLOR_FG);
    
    PorkchopMode mode = porkchop.getMode();
    
    switch (mode) {
        case PorkchopMode::IDLE:
        case PorkchopMode::OINK_MODE:
        case PorkchopMode::WARHOG_MODE:
            // Draw piglet avatar and mood
            Avatar::draw(mainCanvas);
            Mood::draw(mainCanvas);
            break;
            
        case PorkchopMode::MENU:
            // Menu handled by menu.cpp
            break;
            
        case PorkchopMode::SETTINGS:
            // Settings handled by settings screen
            break;
    }
    
    drawBottomBar();
    pushAll();
}

void Display::clear() {
    topBar.fillSprite(COLOR_BG);
    mainCanvas.fillSprite(COLOR_BG);
    bottomBar.fillSprite(COLOR_BG);
    pushAll();
}

void Display::pushAll() {
    M5.Display.startWrite();
    topBar.pushSprite(0, 0);
    mainCanvas.pushSprite(0, TOP_BAR_H);
    bottomBar.pushSprite(0, DISPLAY_H - BOTTOM_BAR_H);
    M5.Display.endWrite();
}

void Display::drawTopBar() {
    topBar.fillSprite(COLOR_BG);
    topBar.setTextColor(COLOR_FG);
    topBar.setTextDatum(top_left);
    
    // Left side: hostname and mode
    String hostname = Config::personality().name;
    topBar.drawString(hostname + ">", 2, 2);
    
    // Mode indicator
    PorkchopMode mode = porkchop.getMode();
    String modeStr;
    uint16_t modeColor = COLOR_FG;
    
    switch (mode) {
        case PorkchopMode::IDLE:
            modeStr = "IDLE";
            break;
        case PorkchopMode::OINK_MODE:
            modeStr = "OINK";
            modeColor = COLOR_ACCENT;
            break;
        case PorkchopMode::WARHOG_MODE:
            modeStr = "WARHOG";
            modeColor = COLOR_DANGER;
            break;
        case PorkchopMode::MENU:
            modeStr = "MENU";
            break;
        case PorkchopMode::SETTINGS:
            modeStr = "CONFIG";
            break;
    }
    
    topBar.setTextColor(modeColor);
    topBar.setTextDatum(top_center);
    topBar.drawString(modeStr, DISPLAY_W / 2, 2);
    
    // Right side: status icons
    topBar.setTextDatum(top_right);
    topBar.setTextColor(COLOR_FG);
    
    String status = "";
    status += gpsStatus ? "G" : "-";
    status += wifiStatus ? "W" : "-";
    status += mlStatus ? "M" : "-";
    
    topBar.drawString(status, DISPLAY_W - 2, 2);
    
    // Draw separator line
    topBar.drawLine(0, TOP_BAR_H - 1, DISPLAY_W, TOP_BAR_H - 1, COLOR_FG);
}

void Display::drawBottomBar() {
    bottomBar.fillSprite(COLOR_BG);
    bottomBar.setTextColor(COLOR_FG);
    
    // Draw separator line
    bottomBar.drawLine(0, 0, DISPLAY_W, 0, COLOR_FG);
    
    // Left: stats
    uint16_t hsCount = porkchop.getHandshakeCount();
    uint16_t netCount = porkchop.getNetworkCount();
    
    bottomBar.setTextDatum(top_left);
    String stats = "HS:" + String(hsCount) + " NET:" + String(netCount);
    bottomBar.drawString(stats, 2, 3);
    
    // Right: uptime
    bottomBar.setTextDatum(top_right);
    uint32_t uptime = porkchop.getUptime();
    uint16_t mins = uptime / 60;
    uint16_t secs = uptime % 60;
    String uptimeStr = String(mins) + ":" + (secs < 10 ? "0" : "") + String(secs);
    bottomBar.drawString(uptimeStr, DISPLAY_W - 2, 3);
}

void Display::showInfoBox(const String& title, const String& line1, 
                          const String& line2, bool blocking) {
    mainCanvas.fillSprite(COLOR_BG);
    mainCanvas.setTextColor(COLOR_FG);
    
    // Draw border
    mainCanvas.drawRect(10, 5, DISPLAY_W - 20, MAIN_H - 10, COLOR_FG);
    
    // Title
    mainCanvas.setTextDatum(top_center);
    mainCanvas.setTextSize(2);
    mainCanvas.drawString(title, DISPLAY_W / 2, 15);
    
    // Content
    mainCanvas.setTextSize(1);
    mainCanvas.drawString(line1, DISPLAY_W / 2, 45);
    if (line2.length() > 0) {
        mainCanvas.drawString(line2, DISPLAY_W / 2, 60);
    }
    
    if (blocking) {
        mainCanvas.drawString("[ENTER to continue]", DISPLAY_W / 2, MAIN_H - 20);
    }
    
    pushAll();
    
    if (blocking) {
        while (true) {
            M5.update();
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                while (M5Cardputer.Keyboard.isPressed()) {
                    M5.update();
                    M5Cardputer.update();
                    delay(10);
                }
                break;
            }
            delay(10);
        }
    }
}

bool Display::showConfirmBox(const String& title, const String& message) {
    mainCanvas.fillSprite(COLOR_BG);
    mainCanvas.setTextColor(COLOR_FG);
    
    mainCanvas.drawRect(10, 5, DISPLAY_W - 20, MAIN_H - 10, COLOR_FG);
    
    mainCanvas.setTextDatum(top_center);
    mainCanvas.setTextSize(2);
    mainCanvas.drawString(title, DISPLAY_W / 2, 15);
    
    mainCanvas.setTextSize(1);
    mainCanvas.drawString(message, DISPLAY_W / 2, 45);
    mainCanvas.drawString("[Y]es / [N]o", DISPLAY_W / 2, MAIN_H - 20);
    
    pushAll();
    
    while (true) {
        M5.update();
        M5Cardputer.update();
        
        if (M5Cardputer.Keyboard.isChange()) {
            auto keys = M5Cardputer.Keyboard.keysState();
            for (auto c : keys.word) {
                if (c == 'y' || c == 'Y') return true;
                if (c == 'n' || c == 'N') return false;
            }
        }
        delay(10);
    }
}

void Display::showProgress(const String& title, uint8_t percent) {
    mainCanvas.fillSprite(COLOR_BG);
    mainCanvas.setTextColor(COLOR_FG);
    
    mainCanvas.setTextDatum(top_center);
    mainCanvas.setTextSize(2);
    mainCanvas.drawString(title, DISPLAY_W / 2, 20);
    
    // Progress bar
    int barW = DISPLAY_W - 40;
    int barH = 15;
    int barX = 20;
    int barY = MAIN_H / 2;
    
    mainCanvas.drawRect(barX, barY, barW, barH, COLOR_FG);
    int fillW = (barW - 2) * percent / 100;
    mainCanvas.fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_ACCENT);
    
    // Percentage text
    mainCanvas.setTextSize(1);
    mainCanvas.drawString(String(percent) + "%", DISPLAY_W / 2, barY + barH + 10);
    
    pushAll();
}

void Display::setGPSStatus(bool hasFix) {
    gpsStatus = hasFix;
}

void Display::setWiFiStatus(bool connected) {
    wifiStatus = connected;
}

void Display::setMLStatus(bool active) {
    mlStatus = active;
}
