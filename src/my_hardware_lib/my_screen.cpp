#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "my_screen.h"
#include "my_config.h"
#include "my_motion.h"
#include "my_bat.h"
#include "my_I2C.h"

namespace
{
    constexpr uint32_t FRAME_INTERVAL_MS = SCREEN_REFRESH_TIME;
    constexpr float BATT_MIN_V = 9.0f;
    constexpr float BATT_MAX_V = 12.6f;

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &ScreenWire, -1);

    bool screen_ready = false;
    uint32_t last_frame_ms = 0;

    uint8_t normalized_address(uint8_t raw)
    {
        if (raw == 0x78 || raw == 0x7A || raw > 0x7F)
        {
            return static_cast<uint8_t>(raw >> 1);
        }
        return raw;
    }

    void draw_battery(float voltage)
    {
        const int body_x = 0;
        const int body_y = 2;
        const int body_w = 24;
        const int body_h = 12;
        const int tip_w = 2;

        const float pct = constrain((voltage - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V), 0.0f, 1.0f);

        display.drawRoundRect(body_x, body_y, body_w, body_h, 2, SSD1306_WHITE);
        display.fillRect(body_x + body_w, body_y + 3, tip_w, body_h - 6, SSD1306_WHITE);

        const int fill_w = static_cast<int>((body_w - 4) * pct + 0.5f);
        if (fill_w > 0)
        {
            display.fillRect(body_x + 2, body_y + 2, fill_w, body_h - 4, SSD1306_WHITE);
        }

        char text[12];
        snprintf(text, sizeof(text), "%.2fV", voltage);
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(body_w + 6, 0);
        display.print(text);
    }

    void draw_group_badge(int group_number, bool enabled)
    {
        const int badge_y = 16;
        const int badge_h = SCREEN_HEIGHT - badge_y;
        const int radius = 6;

        if (enabled)
        {
            display.fillRoundRect(0, badge_y, SCREEN_WIDTH, badge_h, radius, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.drawRoundRect(0, badge_y, SCREEN_WIDTH, badge_h, radius, SSD1306_WHITE);
            display.setTextColor(SSD1306_WHITE);
        }

        display.setTextSize(1);
        display.setCursor(8, badge_y + 4);
        display.print("FORMATION");

        const int16_t dot_x = SCREEN_WIDTH - 14;
        const int16_t dot_y = badge_y + badge_h / 2;
        if (enabled)
            display.fillCircle(dot_x, dot_y, 3, SSD1306_BLACK);
        else
            display.drawCircle(dot_x, dot_y, 3, SSD1306_WHITE);

        char badge[12];
        snprintf(badge, sizeof(badge), "#%02d", group_number);
        display.setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(badge, 0, 0, &x1, &y1, &w, &h);
        const int16_t text_x = static_cast<int16_t>((SCREEN_WIDTH - w) / 2);
        const int16_t text_y = static_cast<int16_t>(badge_y + (badge_h - h) / 2);
        display.setCursor(text_x, text_y);
        display.print(badge);

        display.setTextColor(SSD1306_WHITE);
    }
}

void my_screen_init()
{
    const uint8_t addr = normalized_address(SCREEN_I2C_ADDRESS);
    if (!display.begin(SSD1306_SWITCHCAPVCC, addr))
    {
        Serial.println("SSD1306 init failed");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("OLED READY");
    display.setCursor(0, 10);
    display.print("ADDR 0x");
    display.print(addr, HEX);
    display.display();

    screen_ready = true;
    last_frame_ms = millis() - FRAME_INTERVAL_MS;
}

void my_screen_update()
{
    if (!screen_ready)
    {
        return;
    }

    const uint32_t now_ms = millis();
    if (now_ms - last_frame_ms < FRAME_INTERVAL_MS)
    {
        return;
    }
    last_frame_ms = now_ms;

    my_bat_update();

    display.clearDisplay();
    draw_battery(battery_voltage);
    draw_group_badge(robot.group_cfg.group_number, robot.group_cfg.enabled);
    display.display();
}
