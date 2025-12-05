// Porkchop core state machine implementation

#include "porkchop.h"
#include <M5Cardputer.h>
#include "../ui/display.h"
#include "../ui/menu.h"
#include "../piglet/mood.h"
#include "../piglet/avatar.h"
#include "../modes/oink.h"
#include "../modes/warhog.h"

Porkchop::Porkchop() 
    : currentMode(PorkchopMode::IDLE)
    , previousMode(PorkchopMode::IDLE)
    , startTime(0)
    , handshakeCount(0)
    , networkCount(0) {
}

void Porkchop::init() {
    startTime = millis();
    
    // Register default event handlers
    registerCallback(PorkchopEvent::HANDSHAKE_CAPTURED, [this](PorkchopEvent, void*) {
        handshakeCount++;
    });
    
    registerCallback(PorkchopEvent::NETWORK_FOUND, [this](PorkchopEvent, void*) {
        networkCount++;
    });
    
    // Setup main menu
    std::vector<MenuItem> mainMenuItems = {
        {"OINK Mode", 1},
        {"WARHOG Mode", 2},
        {"Settings", 3},
        {"About", 4}
    };
    Menu::setItems(mainMenuItems);
    Menu::setTitle("PORKCHOP");
    
    Avatar::setState(AvatarState::HAPPY);
    
    Serial.println("[PORKCHOP] Initialized");
}

void Porkchop::update() {
    processEvents();
    handleInput();
    updateMode();
}

void Porkchop::setMode(PorkchopMode mode) {
    if (mode == currentMode) return;
    
    previousMode = currentMode;
    currentMode = mode;
    
    // Cleanup previous mode
    switch (previousMode) {
        case PorkchopMode::OINK_MODE:
            OinkMode::stop();
            break;
        case PorkchopMode::WARHOG_MODE:
            WarhogMode::stop();
            break;
        case PorkchopMode::MENU:
            Menu::hide();
            break;
        default:
            break;
    }
    
    // Init new mode
    switch (currentMode) {
        case PorkchopMode::IDLE:
            Avatar::setState(AvatarState::NEUTRAL);
            break;
        case PorkchopMode::OINK_MODE:
            Avatar::setState(AvatarState::HUNTING);
            OinkMode::start();
            break;
        case PorkchopMode::WARHOG_MODE:
            Avatar::setState(AvatarState::EXCITED);
            WarhogMode::start();
            break;
        case PorkchopMode::MENU:
            Menu::show();
            break;
        case PorkchopMode::SETTINGS:
            break;
    }
    
    postEvent(PorkchopEvent::MODE_CHANGE, nullptr);
}

void Porkchop::postEvent(PorkchopEvent event, void* data) {
    eventQueue.push_back({event, data});
}

void Porkchop::registerCallback(PorkchopEvent event, EventCallback callback) {
    callbacks.push_back({event, callback});
}

void Porkchop::processEvents() {
    for (const auto& item : eventQueue) {
        for (const auto& cb : callbacks) {
            if (cb.first == item.event) {
                cb.second(item.event, item.data);
            }
        }
    }
    eventQueue.clear();
}

void Porkchop::handleInput() {
    if (!M5Cardputer.Keyboard.isChange()) return;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // Menu toggle with backtick
    if (M5Cardputer.Keyboard.isKeyPressed('`')) {
        if (currentMode == PorkchopMode::MENU) {
            setMode(previousMode);
        } else {
            setMode(PorkchopMode::MENU);
        }
        return;
    }
    
    // Mode shortcuts when in IDLE or MENU
    if (currentMode == PorkchopMode::IDLE || currentMode == PorkchopMode::MENU) {
        for (auto c : keys.word) {
            switch (c) {
                case 'o': // Oink mode
                case 'O':
                    setMode(PorkchopMode::OINK_MODE);
                    break;
                case 'w': // Warhog mode
                case 'W':
                    setMode(PorkchopMode::WARHOG_MODE);
                    break;
                case 's': // Settings
                case 'S':
                    setMode(PorkchopMode::SETTINGS);
                    break;
            }
        }
    }
    
    // ESC to return to idle
    if (keys.fn && M5Cardputer.Keyboard.isKeyPressed('`')) {
        setMode(PorkchopMode::IDLE);
    }
}

void Porkchop::updateMode() {
    switch (currentMode) {
        case PorkchopMode::OINK_MODE:
            OinkMode::update();
            break;
        case PorkchopMode::WARHOG_MODE:
            WarhogMode::update();
            break;
        default:
            break;
    }
}

uint32_t Porkchop::getUptime() const {
    return (millis() - startTime) / 1000;
}
