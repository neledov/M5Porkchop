// Piglet ASCII avatar
#pragma once

#include <M5Unified.h>

enum class AvatarState {
    NEUTRAL,
    HAPPY,
    EXCITED,
    HUNTING,
    SLEEPY,
    SAD,
    ANGRY
};

class Avatar {
public:
    static void init();
    static void draw(M5Canvas& canvas);
    static void setState(AvatarState state);
    static AvatarState getState() { return currentState; }
    
    static void blink();
    static void wiggleEars();
    
private:
    static AvatarState currentState;
    static bool isBlinking;
    static bool earsUp;
    static uint32_t lastBlinkTime;
    static uint32_t blinkInterval;
    
    static void drawFrame(M5Canvas& canvas, const char** frame, uint8_t lines);
};

// Avatar frames - 5 lines each
extern const char* AVATAR_NEUTRAL[];
extern const char* AVATAR_HAPPY[];
extern const char* AVATAR_EXCITED[];
extern const char* AVATAR_HUNTING[];
extern const char* AVATAR_SLEEPY[];
extern const char* AVATAR_SAD[];
extern const char* AVATAR_ANGRY[];
extern const char* AVATAR_BLINK[];
