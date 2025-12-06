// Settings menu implementation

#include "settings_menu.h"
#include "display.h"
#include "../core/config.h"
#include <M5Cardputer.h>

// Static member initialization
std::vector<SettingItem> SettingsMenu::items;
uint8_t SettingsMenu::selectedIndex = 0;
uint8_t SettingsMenu::scrollOffset = 0;
bool SettingsMenu::active = false;
bool SettingsMenu::exitRequested = false;
bool SettingsMenu::keyWasPressed = false;
bool SettingsMenu::editing = false;
bool SettingsMenu::textEditing = false;
String SettingsMenu::textBuffer = "";
uint8_t SettingsMenu::cursorPos = 0;

void SettingsMenu::init() {
    loadFromConfig();
}

void SettingsMenu::loadFromConfig() {
    items.clear();
    
    // WiFi SSID for file transfer
    items.push_back({
        "WiFi SSID",
        SettingType::TEXT,
        0, 0, 0, 0, "",
        Config::wifi().otaSSID
    });
    
    // WiFi Password
    items.push_back({
        "WiFi Pass",
        SettingType::TEXT,
        0, 0, 0, 0, "",
        Config::wifi().otaPassword
    });
    
    // Sound toggle
    items.push_back({
        "Sound",
        SettingType::TOGGLE,
        Config::personality().soundEnabled ? 1 : 0,
        0, 1, 1, "", ""
    });
    
    // Brightness (0-100%)
    items.push_back({
        "Brightness",
        SettingType::VALUE,
        (int)Config::personality().brightness,
        10, 100, 10, "%", ""
    });
    
    // Channel hop interval
    items.push_back({
        "CH Hop",
        SettingType::VALUE,
        (int)Config::wifi().channelHopInterval,
        100, 2000, 100, "ms", ""
    });
    
    // Scan duration
    items.push_back({
        "Scan Time",
        SettingType::VALUE,
        (int)Config::wifi().scanDuration,
        500, 5000, 500, "ms", ""
    });
    
    // Enable deauth
    items.push_back({
        "Deauth",
        SettingType::TOGGLE,
        Config::wifi().enableDeauth ? 1 : 0,
        0, 1, 1, "", ""
    });
    
    // GPS enabled
    items.push_back({
        "GPS",
        SettingType::TOGGLE,
        Config::gps().enabled ? 1 : 0,
        0, 1, 1, "", ""
    });
    
    // GPS power save
    items.push_back({
        "GPS PwrSave",
        SettingType::TOGGLE,
        Config::gps().powerSave ? 1 : 0,
        0, 1, 1, "", ""
    });
    
    // Timezone offset (UTC-12 to UTC+14)
    items.push_back({
        "Timezone",
        SettingType::VALUE,
        (int)Config::gps().timezoneOffset,
        -12, 14, 1, "h", ""
    });
    
    // Save & Exit action
    items.push_back({
        "< Save & Exit >",
        SettingType::ACTION,
        0, 0, 0, 0, "", ""
    });
}

void SettingsMenu::saveToConfig() {
    // WiFi settings - SSID and Password from TEXT items
    auto& w = Config::wifi();
    w.otaSSID = items[0].textValue;
    w.otaPassword = items[1].textValue;
    w.channelHopInterval = items[4].value;
    w.scanDuration = items[5].value;
    w.enableDeauth = items[6].value == 1;
    Config::setWiFi(w);
    
    // Sound and Brightness
    auto& p = Config::personality();
    p.soundEnabled = items[2].value == 1;
    p.brightness = items[3].value;
    Config::setPersonality(p);
    
    // Apply brightness to display
    M5.Display.setBrightness(items[3].value * 255 / 100);
    
    // GPS settings
    auto& g = Config::gps();
    g.enabled = items[7].value == 1;
    g.powerSave = items[8].value == 1;
    g.timezoneOffset = items[9].value;
    Config::setGPS(g);
    
    // Save to file
    Config::save();
}

void SettingsMenu::show() {
    active = true;
    exitRequested = false;
    selectedIndex = 0;
    scrollOffset = 0;
    editing = false;
    textEditing = false;
    textBuffer = "";
    cursorPos = 0;
    keyWasPressed = true;  // Ignore the Enter that selected us from menu
    loadFromConfig();
}

void SettingsMenu::hide() {
    active = false;
    editing = false;
}

void SettingsMenu::update() {
    if (!active) return;
    handleInput();
}

void SettingsMenu::handleInput() {
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    // Handle text input mode separately
    if (textEditing) {
        handleTextInput();
        return;
    }
    
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    auto& item = items[selectedIndex];
    
    // Navigation with ; (up) and . (down)
    if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        if (editing && item.type == SettingType::VALUE) {
            // Adjust value down when editing
            item.value = max(item.minVal, item.value - item.step);
        } else {
            // Move selection up
            editing = false;
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) {
                    scrollOffset = selectedIndex;
                }
            }
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        if (editing && item.type == SettingType::VALUE) {
            // Adjust value up when editing
            item.value = min(item.maxVal, item.value + item.step);
        } else {
            // Move selection down
            editing = false;
            if (selectedIndex < items.size() - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                    scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
                }
            }
        }
    }
    
    // Enter to select/toggle/confirm
    if (keys.enter) {
        if (item.type == SettingType::ACTION) {
            // Save & Exit
            saveToConfig();
            exitRequested = true;
        } else if (item.type == SettingType::TOGGLE) {
            // Toggle ON/OFF
            item.value = item.value == 0 ? 1 : 0;
        } else if (item.type == SettingType::VALUE) {
            if (editing) {
                // Confirm and exit edit mode
                editing = false;
            } else {
                // Enter edit mode
                editing = true;
            }
        } else if (item.type == SettingType::TEXT) {
            // Enter text editing mode
            textEditing = true;
            textBuffer = item.textValue;
            cursorPos = textBuffer.length();
            keyWasPressed = true;  // Prevent immediate character input
        }
    }
    
    // ESC/backtick to exit edit mode or menu
    if (M5Cardputer.Keyboard.isKeyPressed('`') || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        if (editing) {
            editing = false;
        } else {
            exitRequested = true;
        }
    }
}

void SettingsMenu::handleTextInput() {
    // Get keyboard state first - this captures the current state including shift
    auto keys = M5Cardputer.Keyboard.keysState();
    auto& item = items[selectedIndex];
    
    // Check if any key is currently pressed
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    // For text input, we need to debounce based on whether we have printable chars,
    // not just any key press. This allows Shift to be held while typing.
    bool hasPrintableChar = !keys.word.empty();
    bool hasActionKey = keys.enter || keys.del;
    
    // Only debounce if we have something to process
    if (!hasPrintableChar && !hasActionKey) {
        // Just modifier keys pressed (shift, fn, etc.) - don't set debounce
        return;
    }
    
    // Debounce - only act on key down edge for actual characters/actions
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    // Enter to confirm text
    if (keys.enter) {
        item.textValue = textBuffer;
        textEditing = false;
        textBuffer = "";
        cursorPos = 0;
        return;
    }
    
    // Backspace to delete character
    if (keys.del) {
        if (textBuffer.length() > 0) {
            textBuffer.remove(textBuffer.length() - 1);
            cursorPos = textBuffer.length();
        }
        return;
    }
    
    // Check for backtick to cancel
    for (char c : keys.word) {
        if (c == '`') {
            textEditing = false;
            textBuffer = "";
            cursorPos = 0;
            return;
        }
    }
    
    // Add typed characters from word vector
    // The keyboard library handles shift/fn for uppercase and special chars
    if (textBuffer.length() < 32) {
        for (char c : keys.word) {
            // Accept all printable ASCII characters except backtick
            if (c >= 32 && c <= 126 && c != '`' && textBuffer.length() < 32) {
                textBuffer += c;
            }
        }
        cursorPos = textBuffer.length();
    }
}

void SettingsMenu::draw(M5Canvas& canvas) {
    canvas.setTextColor(COLOR_FG);
    canvas.setTextSize(1);
    
    int y = 2;
    int lineHeight = 16;
    
    for (uint8_t i = 0; i < VISIBLE_ITEMS && (scrollOffset + i) < items.size(); i++) {
        uint8_t idx = scrollOffset + i;
        auto& item = items[idx];
        
        bool isSelected = (idx == selectedIndex);
        
        // Highlight selected row
        if (isSelected) {
            canvas.fillRect(0, y, DISPLAY_W, lineHeight, COLOR_FG);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.setTextColor(COLOR_FG);
        }
        
        // Draw label
        canvas.setTextDatum(top_left);
        canvas.drawString(item.label, 4, y + 2);
        
        // Draw value on the right
        String valStr;
        if (item.type == SettingType::TOGGLE) {
            valStr = item.value ? "ON" : "OFF";
        } else if (item.type == SettingType::VALUE) {
            if (isSelected && editing) {
                valStr = "[" + String(item.value) + item.suffix + "]";
            } else {
                valStr = String(item.value) + item.suffix;
            }
        } else if (item.type == SettingType::TEXT) {
            if (isSelected && textEditing) {
                // Show text with cursor
                String display = textBuffer;
                if (display.length() > 12) {
                    display = "..." + display.substring(display.length() - 9);
                }
                valStr = "[" + display + "_]";
            } else {
                // Show stored value, masked for password
                if (item.label.indexOf("Pass") >= 0 && item.textValue.length() > 0) {
                    valStr = "****";
                } else if (item.textValue.length() > 12) {
                    valStr = item.textValue.substring(0, 9) + "...";
                } else if (item.textValue.length() > 0) {
                    valStr = item.textValue;
                } else {
                    valStr = "<empty>";
                }
            }
        } else if (item.type == SettingType::ACTION) {
            // Action - label is the whole thing, centered
            valStr = "";
        }
        
        if (valStr.length() > 0) {
            canvas.setTextDatum(top_right);
            canvas.drawString(valStr, DISPLAY_W - 4, y + 2);
        }
        
        y += lineHeight;
    }
    
    // Scroll indicators
    canvas.setTextColor(COLOR_FG);
    canvas.setTextDatum(top_center);
    if (scrollOffset > 0) {
        canvas.drawString("^", DISPLAY_W / 2, 0);
    }
    if (scrollOffset + VISIBLE_ITEMS < items.size()) {
        canvas.drawString("v", DISPLAY_W / 2, MAIN_H - 10);
    }
}
