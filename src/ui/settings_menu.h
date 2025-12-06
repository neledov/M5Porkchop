// Settings menu system
#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>

enum class SettingType {
    TOGGLE,     // ON/OFF
    VALUE,      // Numeric value with min/max
    ACTION,     // Trigger action (like Save)
    TEXT        // Text input (SSID, password, etc.)
};

struct SettingItem {
    String label;
    SettingType type;
    int value;
    int minVal;
    int maxVal;
    int step;
    String suffix;  // %, ms, etc.
    String textValue;  // For TEXT type
};

class SettingsMenu {
public:
    static void init();
    static void update();
    static void draw(M5Canvas& canvas);
    
    static void show();
    static void hide();
    static bool isActive() { return active; }
    static bool shouldExit() { return exitRequested; }
    static void clearExit() { exitRequested = false; }
    
private:
    static std::vector<SettingItem> items;
    static uint8_t selectedIndex;
    static uint8_t scrollOffset;
    static bool active;
    static bool exitRequested;
    static bool keyWasPressed;
    static bool editing;  // Currently adjusting a value
    static bool textEditing;  // Currently editing text
    static String textBuffer;  // Buffer for text input
    static uint8_t cursorPos;  // Cursor position in text
    
    static const uint8_t VISIBLE_ITEMS = 6;  // Fits without nav instructions
    
    static void handleInput();
    static void handleTextInput();
    static void loadFromConfig();
    static void saveToConfig();
};
