#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>
#include "config.h"

enum ButtonAction {
    ACTION_NONE = 0,
    ACTION_FEED,      // Botão 1 - Alimentar
    ACTION_PLAY,      // Botão 2 - Brincar
    ACTION_STATUS,    // Botão 3 - Status (toggle)
    ACTION_CLEAN,     // Pressão longa Botão 2 - Limpar
    ACTION_RESET,     // Pressão longa Botão 3 - Reset
    ACTION_DEV_STAGE, // [DEV] Força estágio (pendingValue = PokemonStage)
    ACTION_DEV_DIRT,  // [DEV] Seta sujeira (pendingValue = 0..100)
    ACTION_DEV_BAR,   // [DEV] Ajusta barra (pendingValue = idx<<8 | delta)
    ACTION_DEV_SLEEP  // [DEV] Força o modo dormir na hora
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
