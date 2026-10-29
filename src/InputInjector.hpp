#pragma once

#include <cstdint>

class InputInjector {
public:
    virtual ~InputInjector() = default;

    // button: Linux input-event-codes (e.g. BTN_LEFT, BTN_RIGHT, BTN_MIDDLE)
    // pressed: true = press (down), false = release (up)
    // Returns true if the message was accepted for delivery locally
    virtual bool sendButton(uint32_t button, bool pressed) = 0;

    // dx, dy: relative motion in pixels
    // Returns true if the message was accepted for delivery locally
    virtual bool sendMotion(double dx, double dy) = 0;
};
