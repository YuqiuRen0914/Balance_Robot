#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "my_boardRGB.h"
#include "my_config.h"

namespace
{
    constexpr uint8_t kBrightness = 64;
    constexpr uint32_t kBlinkPeriodMs = 1000;
    constexpr uint32_t kBlinkOnMs = 200;
    constexpr uint8_t kBlinkTimes = 5;

    Adafruit_NeoPixel board_led(BOARD_RGB_COUNT, BOARD_RGB_PIN, NEO_GRB + NEO_KHZ800);

    bool strip_ready = false;
    bool fault_blink = false;
    uint32_t blink_start_ms = 0;
    bool last_on_state = false;

    void show_color(uint8_t r, uint8_t g, uint8_t b)
    {
        if (!strip_ready)
        {
            return;
        }
        const uint16_t count = board_led.numPixels();
        for (uint16_t i = 0; i < count; i++)
        {
            board_led.setPixelColor(i, board_led.Color(r, g, b));
        }
        board_led.show();
    }

    void turn_off()
    {
        last_on_state = false;
        show_color(0, 0, 0);
    }
}

void my_boardRGB_init()
{
    board_led.begin();
    board_led.setBrightness(kBrightness);
    strip_ready = true;
    turn_off();
}

void my_boardRGB_notify_peripheral_missing()
{
    fault_blink = true;
    blink_start_ms = millis();
    last_on_state = false;
    if (strip_ready)
    {
        show_color(0, 0, 0);
    }
}

void my_boardRGB_update()
{
    if (!strip_ready || !fault_blink)
    {
        return;
    }

    const uint32_t now = millis();
    const uint32_t elapsed = now - blink_start_ms;
    const uint32_t cycle = elapsed / kBlinkPeriodMs;

    if (cycle >= kBlinkTimes)
    {
        fault_blink = false;
        turn_off();
        return;
    }

    const bool should_on = (elapsed % kBlinkPeriodMs) < kBlinkOnMs;
    if (should_on != last_on_state)
    {
        if (should_on)
        {
            show_color(255, 0, 0);
        }
        else
        {
            turn_off();
        }
        last_on_state = should_on;
    }
}
