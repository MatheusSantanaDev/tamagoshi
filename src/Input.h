#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>
#include "config.h"

enum ButtonAction {
    ACTION_NONE = 0,
    ACTION_FEED,      // Botão 1 - Alimentar
    ACTION_PLAY,      // Botão 2 - Brincar
    ACTION_STATUS,    // Botão 3 - Status (toggle)
    ACTION_RESET      // Pressão longa Botão 3 - Reset
};

class Input {
public:
    Input();

    void begin();
    ButtonAction read();

private:
    struct ButtonState {
        uint8_t pin;
        int lastRawState;
        int stableState;
        unsigned long lastDebounceTime;
        unsigned long pressStartTime;
        bool wasLongPress;
    };

    ButtonState _buttons[3];
    ButtonAction _pendingActions[3];
    int _pendingCount;

    static const int NUM_BUTTONS = 3;
};

#endif
