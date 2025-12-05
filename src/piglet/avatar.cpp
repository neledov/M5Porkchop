// Piglet ASCII avatar implementation

#include "avatar.h"
#include "../ui/display.h"

// Static members
AvatarState Avatar::currentState = AvatarState::NEUTRAL;
bool Avatar::isBlinking = false;
bool Avatar::earsUp = true;
uint32_t Avatar::lastBlinkTime = 0;
uint32_t Avatar::blinkInterval = 3000;

// Avatar ASCII frames (5 lines each, centered on 240px width)
const char* AVATAR_NEUTRAL[] = {
    "   ^  ^   ",
    "  (o oo)  ",
    " -(____)- ",
    "   |  |   ",
    "   ''  '' "
};

const char* AVATAR_HAPPY[] = {
    "   ^  ^   ",
    "  (^ oo^) ",
    " -(____)<-",
    "   |  |   ",
    "  ~~  ~~  "
};

const char* AVATAR_EXCITED[] = {
    "   !  !   ",
    "  (@oo @) ",
    "<-(____)->",
    "   |  |   ",
    "  ** **   "
};

const char* AVATAR_HUNTING[] = {
    "   >  <   ",
    "  (>oo <) ",
    " \\(____)/",
    "   |  |   ",
    "   ..  .. "
};

const char* AVATAR_SLEEPY[] = {
    "   v  v   ",
    "  (-oo -) ",
    " -(____)-z",
    "   |  |  z",
    "   ''  ''z"
};

const char* AVATAR_SAD[] = {
    "   v  v   ",
    "  (T ooT) ",
    " -(____)- ",
    "   |  |   ",
    "   ''  '' "
};

const char* AVATAR_ANGRY[] = {
    "   \\  /   ",
    "  (>oo <) ",
    " #(____)# ",
    "   |  |   ",
    "   ** **  "
};

const char* AVATAR_BLINK[] = {
    "   ^  ^   ",
    "  (- oo-) ",
    " -(____)- ",
    "   |  |   ",
    "   ''  '' "
};

void Avatar::init() {
    currentState = AvatarState::NEUTRAL;
    isBlinking = false;
    earsUp = true;
    lastBlinkTime = millis();
    blinkInterval = random(2000, 5000);
}

void Avatar::setState(AvatarState state) {
    currentState = state;
}

void Avatar::blink() {
    isBlinking = true;
}

void Avatar::wiggleEars() {
    earsUp = !earsUp;
}

void Avatar::draw(M5Canvas& canvas) {
    // Check if we should blink
    uint32_t now = millis();
    if (now - lastBlinkTime > blinkInterval) {
        isBlinking = true;
        lastBlinkTime = now;
        blinkInterval = random(2000, 5000);
    }
    
    // Select frame based on state
    const char** frame;
    if (isBlinking && currentState != AvatarState::SLEEPY) {
        frame = AVATAR_BLINK;
        isBlinking = false;  // One-shot blink
    } else {
        switch (currentState) {
            case AvatarState::HAPPY:    frame = AVATAR_HAPPY; break;
            case AvatarState::EXCITED:  frame = AVATAR_EXCITED; break;
            case AvatarState::HUNTING:  frame = AVATAR_HUNTING; break;
            case AvatarState::SLEEPY:   frame = AVATAR_SLEEPY; break;
            case AvatarState::SAD:      frame = AVATAR_SAD; break;
            case AvatarState::ANGRY:    frame = AVATAR_ANGRY; break;
            default:                    frame = AVATAR_NEUTRAL; break;
        }
    }
    
    drawFrame(canvas, frame, 5);
}

void Avatar::drawFrame(M5Canvas& canvas, const char** frame, uint8_t lines) {
    canvas.setTextDatum(top_center);
    canvas.setTextSize(2);  // Larger ASCII art
    canvas.setTextColor(COLOR_ACCENT);
    
    int startY = 5;  // Start from top of main canvas
    int lineHeight = 16;
    
    for (uint8_t i = 0; i < lines; i++) {
        canvas.drawString(frame[i], DISPLAY_W / 2, startY + i * lineHeight);
    }
}
