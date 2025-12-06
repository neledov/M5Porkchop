// Piglet mood implementation

#include "mood.h"
#include "../core/config.h"
#include "../ui/display.h"

// Static members
String Mood::currentPhrase = "OINK!";
int Mood::happiness = 50;
uint32_t Mood::lastPhraseChange = 0;
uint32_t Mood::phraseInterval = 5000;
uint32_t Mood::lastActivityTime = 0;

// Phrase categories
const char* PHRASES_HAPPY[] = {
    "OINK OINK!",
    "Sniffin' packets!",
    "Got a good one!",
    "More handshakes!",
    "I'm a good piggy!",
    "Delicious data~",
    "OOOIINK!",
    "Truffle found!"
};

const char* PHRASES_EXCITED[] = {
    "JACKPOT!!!",
    "WPA2 YUMMY!",
    "HASHCAT FOOD!",
    "CAPTURE THIS!",
    "OMG OMG OMG!",
    "BACON BITS!!"
};

const char* PHRASES_HUNTING[] = {
    "Searching...",
    "Sniff sniff...",
    "Where's that AP?",
    "Patience piggy...",
    "Monitoring...",
    "Waiting..."
};

const char* PHRASES_SLEEPY[] = {
    "zzZzZ...",
    "*yawn*",
    "So quiet...",
    "Bored oink...",
    "Need WiFi...",
    "Sleepy piggy..."
};

const char* PHRASES_SAD[] = {
    "No networks...",
    "GPS lost...",
    "Lonely piggy...",
    "Need friends...",
    "Where is wifi?",
    "Sad oink..."
};

const char* PHRASES_IDLE[] = {
    "Ready to hunt!",
    "Press [O] OINK",
    "Press [W] WARHOG",
    "Waiting orders",
    "Porkchop ready!",
    "What's cooking?"
};

void Mood::init() {
    currentPhrase = "OINK!";
    happiness = 50;
    lastPhraseChange = millis();
    phraseInterval = 5000;
    lastActivityTime = millis();
}

void Mood::update() {
    uint32_t now = millis();
    
    // Check for inactivity
    uint32_t inactiveSeconds = (now - lastActivityTime) / 1000;
    if (inactiveSeconds > 60) {
        onNoActivity(inactiveSeconds);
    }
    
    // Natural happiness decay
    if (now - lastPhraseChange > phraseInterval) {
        happiness = constrain(happiness - 1, -100, 100);
        selectPhrase();
        lastPhraseChange = now;
    }
    
    updateAvatarState();
}

void Mood::onHandshakeCaptured(const char* apName) {
    happiness = min(happiness + 30, 100);
    lastActivityTime = millis();
    
    // Show AP name in phrase if available
    if (apName && strlen(apName) > 0) {
        String ap = String(apName);
        if (ap.length() > 12) ap = ap.substring(0, 12) + "..";
        const char* templates[] = {
            "Got %s!",
            "%s pwned!",
            "Yummy %s!",
            "%s captured!"
        };
        int idx = random(0, 4);
        char buf[48];
        snprintf(buf, sizeof(buf), templates[idx], ap.c_str());
        currentPhrase = buf;
    } else {
        int idx = random(0, sizeof(PHRASES_EXCITED) / sizeof(PHRASES_EXCITED[0]));
        currentPhrase = PHRASES_EXCITED[idx];
    }
    lastPhraseChange = millis();
    
    // Double beep for handshake! (user requested two beeps)
    M5.Speaker.tone(1500, 100);  // First beep
    delay(120);
    M5.Speaker.tone(2000, 100);  // Second beep (higher pitch)
}

void Mood::onNewNetwork(const char* apName, int8_t rssi, uint8_t channel) {
    happiness = min(happiness + 10, 100);
    lastActivityTime = millis();
    
    // Show AP name with info in funny phrases
    if (apName && strlen(apName) > 0) {
        String ap = String(apName);
        if (ap.length() > 10) ap = ap.substring(0, 10) + "..";
        
        const char* templates[] = {
            "Sniffed %s on CH%d!",
            "%s @ %ddB yummy!",
            "Oink! %s CH%d",
            "Tasty %s %ddB!",
            "Nom nom %s!"
        };
        int idx = random(0, 5);
        char buf[64];
        if (idx == 1 || idx == 3) {
            snprintf(buf, sizeof(buf), templates[idx], ap.c_str(), rssi);
        } else if (idx == 0 || idx == 2) {
            snprintf(buf, sizeof(buf), templates[idx], ap.c_str(), channel);
        } else {
            snprintf(buf, sizeof(buf), templates[idx], ap.c_str());
        }
        currentPhrase = buf;
    } else {
        // Hidden network
        char buf[48];
        snprintf(buf, sizeof(buf), "Hidden net CH%d %ddB", channel, rssi);
        currentPhrase = buf;
    }
    lastPhraseChange = millis();
}

void Mood::setStatusMessage(const String& msg) {
    currentPhrase = msg;
    lastPhraseChange = millis();
}

void Mood::onMLPrediction(float confidence) {
    lastActivityTime = millis();
    
    // High confidence = happy
    if (confidence > 0.8f) {
        happiness = min(happiness + 15, 100);
        int idx = random(0, sizeof(PHRASES_EXCITED) / sizeof(PHRASES_EXCITED[0]));
        currentPhrase = PHRASES_EXCITED[idx];
    } else if (confidence > 0.5f) {
        happiness = min(happiness + 5, 100);
        int idx = random(0, sizeof(PHRASES_HAPPY) / sizeof(PHRASES_HAPPY[0]));
        currentPhrase = PHRASES_HAPPY[idx];
    }
    
    lastPhraseChange = millis();
}

void Mood::onNoActivity(uint32_t seconds) {
    if (seconds > 300) {
        // Very bored after 5 minutes
        happiness = max(happiness - 2, -100);
        if (happiness < -20) {
            int idx = random(0, sizeof(PHRASES_SLEEPY) / sizeof(PHRASES_SLEEPY[0]));
            currentPhrase = PHRASES_SLEEPY[idx];
        }
    } else if (seconds > 120) {
        // Getting bored after 2 minutes
        happiness = max(happiness - 1, -100);
    }
}

void Mood::onWiFiLost() {
    happiness = max(happiness - 20, -100);
    lastActivityTime = millis();
    
    int idx = random(0, sizeof(PHRASES_SAD) / sizeof(PHRASES_SAD[0]));
    currentPhrase = PHRASES_SAD[idx];
    lastPhraseChange = millis();
}

void Mood::onGPSFix() {
    happiness = min(happiness + 10, 100);
    lastActivityTime = millis();
    currentPhrase = "GPS lock! Let's go!";
    lastPhraseChange = millis();
}

void Mood::onGPSLost() {
    happiness = max(happiness - 10, -100);
    currentPhrase = "Lost GPS...";
    lastPhraseChange = millis();
}

void Mood::onLowBattery() {
    currentPhrase = "Feed me power!";
    lastPhraseChange = millis();
}

void Mood::selectPhrase() {
    const char** phrases;
    int count;
    
    if (happiness > 70) {
        phrases = PHRASES_EXCITED;
        count = sizeof(PHRASES_EXCITED) / sizeof(PHRASES_EXCITED[0]);
    } else if (happiness > 30) {
        phrases = PHRASES_HAPPY;
        count = sizeof(PHRASES_HAPPY) / sizeof(PHRASES_HAPPY[0]);
    } else if (happiness > -10) {
        phrases = PHRASES_HUNTING;
        count = sizeof(PHRASES_HUNTING) / sizeof(PHRASES_HUNTING[0]);
    } else if (happiness > -50) {
        phrases = PHRASES_SLEEPY;
        count = sizeof(PHRASES_SLEEPY) / sizeof(PHRASES_SLEEPY[0]);
    } else {
        phrases = PHRASES_SAD;
        count = sizeof(PHRASES_SAD) / sizeof(PHRASES_SAD[0]);
    }
    
    int idx = random(0, count);
    currentPhrase = phrases[idx];
}

void Mood::updateAvatarState() {
    if (happiness > 70) {
        Avatar::setState(AvatarState::EXCITED);
    } else if (happiness > 30) {
        Avatar::setState(AvatarState::HAPPY);
    } else if (happiness > -10) {
        Avatar::setState(AvatarState::NEUTRAL);
    } else if (happiness > -50) {
        Avatar::setState(AvatarState::SLEEPY);
    } else {
        Avatar::setState(AvatarState::SAD);
    }
}

void Mood::draw(M5Canvas& canvas) {
    // Calculate bubble size based on message length
    String phrase = currentPhrase;
    int maxCharsPerLine = 16;
    int numLines = 1;
    if (phrase.length() > maxCharsPerLine) numLines = 2;
    if (phrase.length() > maxCharsPerLine * 2) numLines = 3;
    
    int bubbleX = 115;  // Start of bubble (after bigger piglet)
    int bubbleY = 3;
    int bubbleW = DISPLAY_W - bubbleX - 4;
    int bubbleH = 14 + (numLines * 14);  // Dynamic height based on lines
    
    // Cap bubble height to fit screen
    if (bubbleH > MAIN_H - 10) bubbleH = MAIN_H - 10;
    
    // Draw bubble outline
    canvas.drawRoundRect(bubbleX, bubbleY, bubbleW, bubbleH, 6, COLOR_FG);
    
    // Draw < arrow pointing to piglet
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_FG);
    canvas.drawString("<", bubbleX - 6, bubbleY + bubbleH / 2 - 4);
    
    // Draw phrase inside bubble with word wrapping
    canvas.setTextDatum(top_left);
    canvas.setTextColor(COLOR_ACCENT);
    
    int textX = bubbleX + 6;
    int textY = bubbleY + 6;
    int lineHeight = 12;
    
    // Word wrap logic
    String remaining = phrase;
    int lineNum = 0;
    while (remaining.length() > 0 && lineNum < 4) {
        String line;
        if (remaining.length() <= maxCharsPerLine) {
            line = remaining;
            remaining = "";
        } else {
            int splitPos = remaining.lastIndexOf(' ', maxCharsPerLine);
            if (splitPos <= 0) splitPos = maxCharsPerLine;
            line = remaining.substring(0, splitPos);
            remaining = remaining.substring(splitPos + 1);
        }
        canvas.drawString(line, textX, textY + lineNum * lineHeight);
        lineNum++;
    }
}

const String& Mood::getCurrentPhrase() {
    return currentPhrase;
}

int Mood::getCurrentHappiness() {
    return happiness;
}
