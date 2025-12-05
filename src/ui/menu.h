// Menu system
#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include <functional>

struct MenuItem {
    String label;
    uint8_t actionId;
};

class Menu {
public:
    static void init();
    static void update();
    static void draw(M5Canvas& canvas);
    
    static void setItems(const std::vector<MenuItem>& items);
    static void setTitle(const String& title);
    
    static int getSelectedId();
    static bool isActive() { return active; }
    static void show();
    static void hide();
    
private:
    static std::vector<MenuItem> menuItems;
    static String menuTitle;
    static uint8_t selectedIndex;
    static uint8_t scrollOffset;
    static bool active;
    
    static const uint8_t VISIBLE_ITEMS = 6;
    
    static void handleInput();
};
