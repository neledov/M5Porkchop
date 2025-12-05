// Menu system implementation

#include "menu.h"
#include <M5Cardputer.h>
#include "display.h"

// Static member initialization
std::vector<MenuItem> Menu::menuItems;
String Menu::menuTitle = "Menu";
uint8_t Menu::selectedIndex = 0;
uint8_t Menu::scrollOffset = 0;
bool Menu::active = false;

void Menu::init() {
    menuItems.clear();
    selectedIndex = 0;
    scrollOffset = 0;
}

void Menu::setItems(const std::vector<MenuItem>& items) {
    menuItems = items;
    selectedIndex = 0;
    scrollOffset = 0;
}

void Menu::setTitle(const String& title) {
    menuTitle = title;
}

void Menu::show() {
    active = true;
    selectedIndex = 0;
    scrollOffset = 0;
}

void Menu::hide() {
    active = false;
}

int Menu::getSelectedId() {
    if (selectedIndex < menuItems.size()) {
        return menuItems[selectedIndex].actionId;
    }
    return -1;
}

void Menu::update() {
    if (!active) return;
    handleInput();
}

void Menu::handleInput() {
    if (!M5Cardputer.Keyboard.isChange()) return;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // Navigation
    if (M5Cardputer.Keyboard.isKeyPressed(';')) {  // Up
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('.')) {  // Down
        if (selectedIndex < menuItems.size() - 1) {
            selectedIndex++;
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            }
        }
    }
}

void Menu::draw(M5Canvas& canvas) {
    if (!active) return;
    
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_FG);
    
    // Title
    canvas.setTextDatum(top_center);
    canvas.setTextSize(1);
    canvas.drawString(menuTitle, DISPLAY_W / 2, 5);
    canvas.drawLine(10, 15, DISPLAY_W - 10, 15, COLOR_FG);
    
    // Menu items
    canvas.setTextDatum(top_left);
    int yOffset = 20;
    int lineHeight = 14;
    
    for (uint8_t i = 0; i < VISIBLE_ITEMS && (scrollOffset + i) < menuItems.size(); i++) {
        uint8_t idx = scrollOffset + i;
        int y = yOffset + i * lineHeight;
        
        if (idx == selectedIndex) {
            canvas.fillRect(5, y - 1, DISPLAY_W - 10, lineHeight, COLOR_ACCENT);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.setTextColor(COLOR_FG);
        }
        
        canvas.drawString("> " + menuItems[idx].label, 10, y);
    }
    
    // Scroll indicators
    canvas.setTextColor(COLOR_FG);
    if (scrollOffset > 0) {
        canvas.drawString("^", DISPLAY_W - 15, 20);
    }
    if (scrollOffset + VISIBLE_ITEMS < menuItems.size()) {
        canvas.drawString("v", DISPLAY_W - 15, yOffset + (VISIBLE_ITEMS - 1) * lineHeight);
    }
    
    // Instructions
    canvas.setTextDatum(bottom_center);
    canvas.drawString("[;/.]Nav [ENTER]Select [`]Back", DISPLAY_W / 2, MAIN_H - 2);
}
