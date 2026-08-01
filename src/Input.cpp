#include "Input.h"

Input::Input() : _pendingCount(0) {}

void Input::begin() {
    pinMode(BTN_FEED, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_STATUS, INPUT_PULLUP);

    _buttons[0] = {BTN_FEED, HIGH, HIGH, 0, 0, false};
    _buttons[1] = {BTN_PLAY, HIGH, HIGH, 0, 0, false};
    _buttons[2] = {BTN_STATUS, HIGH, HIGH, 0, 0, false};

    _pendingCount = 0;
}

ButtonAction Input::read() {
    // Se há ações pendentes de leituras anteriores, retorna a primeira
    if (_pendingCount > 0) {
        _pendingCount--;
        return _pendingActions[_pendingCount];
    }

    unsigned long now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        ButtonState* btn = &_buttons[i];
        int rawState = digitalRead(btn->pin);

        // Debounce
        if (rawState != btn->lastRawState) {
            btn->lastDebounceTime = now;
        }

        if ((now - btn->lastDebounceTime) > DEBOUNCE_MS) {
            if (rawState != btn->stableState) {
                btn->stableState = rawState;

                // Botão foi pressionado (LOW com pull-up)
                if (rawState == LOW) {
                    btn->pressStartTime = now;
                    btn->wasLongPress = false;
                } else {
                    // Botão foi solto
                    if (!btn->wasLongPress) {
                        // Short press
                        ButtonAction action = ACTION_NONE;
                        switch (btn->pin) {
                            case BTN_FEED:   action = ACTION_FEED; break;
                            case BTN_PLAY:   action = ACTION_PLAY; break;
                            case BTN_STATUS: action = ACTION_STATUS; break;
                        }
                        if (action != ACTION_NONE) {
                            _pendingActions[_pendingCount++] = action;
                        }
                    }
                }
            }
        }

        // Detecta long press
        if (btn->stableState == LOW && !btn->wasLongPress) {
            if ((now - btn->pressStartTime) > LONG_PRESS_MS) {
                btn->wasLongPress = true;
                if (btn->pin == BTN_STATUS) {
                    _pendingActions[_pendingCount++] = ACTION_RESET;
                }
            }
        }

        btn->lastRawState = rawState;
    }

    if (_pendingCount > 0) {
        _pendingCount--;
        return _pendingActions[_pendingCount];
    }

    return ACTION_NONE;
}
