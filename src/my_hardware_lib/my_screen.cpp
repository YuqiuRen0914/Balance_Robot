#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "my_screen.h"
#include "my_config.h"
#include "my_bat.h"
#include "my_I2C.h"
#include "my_net.h"

namespace
{
    constexpr uint32_t FRAME_INTERVAL_MS = SCREEN_REFRESH_TIME;
    constexpr uint8_t HEADER_HEIGHT = 14;
    constexpr uint8_t SSID_MAX_LEN = 14;
    constexpr uint8_t STRIPE_GAP = 6;
    constexpr uint8_t FRAME_THICKNESS = 2;
    constexpr uint8_t FLOW_SEG_LEN = 16;

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &ScreenWire, -1);

    bool screen_ready = false;
    uint32_t last_frame_ms = 0;
    uint8_t anim_phase_fast = 0;

    uint8_t normalized_address(uint8_t raw)
    {
        if (raw == 0x78 || raw == 0x7A || raw > 0x7F)
        {
            return static_cast<uint8_t>(raw >> 1);
        }
        return raw;
    }

    String compact_ssid(const String &ssid)
    {
        if (ssid.length() <= SSID_MAX_LEN)
        {
            return ssid;
        }
        return ssid.substring(0, SSID_MAX_LEN - 2) + "..";
    }

    void draw_stripes(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t gap, uint8_t phase)
    {
        const uint8_t offset_seed = phase % gap;
        for (int16_t offset = height + offset_seed; offset < width + height + gap; offset += gap)
        {
            const int16_t x0 = x + offset - height;
            const int16_t y0 = y;
            const int16_t x1 = x + offset;
            const int16_t y1 = y + height;
            display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
        }
    }

    void draw_scanline(uint8_t phase)
    {
        const int16_t sweep = (phase * 3) % (SCREEN_WIDTH - 32);
        display.drawFastHLine(6 + sweep, HEADER_HEIGHT - 1, 22, SSD1306_WHITE);
    }

    void draw_flow_underline(uint8_t phase)
    {
        const int16_t lane_y = HEADER_HEIGHT + 1;
        const int16_t left = 6;
        const int16_t span = SCREEN_WIDTH - 30 - FLOW_SEG_LEN; // leave space for right-side animation
        const int16_t offset = (phase * 2) % span;
        display.drawFastHLine(left + offset, lane_y, FLOW_SEG_LEN, SSD1306_WHITE);
    }

    void draw_pulse_right(uint8_t phase)
    {
        // Sliding chevron on the right side; keep away from text area.
        const int16_t x_start = SCREEN_WIDTH - 26;
        const int16_t y_start = HEADER_HEIGHT + 2;
        const uint8_t travel = 14;
        const uint8_t pos = (phase * 2) % travel;
        display.drawTriangle(x_start + pos, y_start, x_start + pos + 8, y_start + 4, x_start + pos, y_start + 8, SSD1306_WHITE);
    }

    void draw_mecha_shell(uint8_t phase)
    {
        // Double frame for a thicker, bolder box
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        display.drawRect(FRAME_THICKNESS - 1, FRAME_THICKNESS - 1, SCREEN_WIDTH - 2 * (FRAME_THICKNESS - 1), SCREEN_HEIGHT - 2 * (FRAME_THICKNESS - 1), SSD1306_WHITE);
        display.drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, SSD1306_WHITE);
        display.drawFastHLine(0, HEADER_HEIGHT + 1, SCREEN_WIDTH, SSD1306_WHITE);

        // Corner and spine accents
        display.drawFastHLine(3, 3, 18, SSD1306_WHITE);
        display.drawFastVLine(3, 3, 8, SSD1306_WHITE);
        display.drawFastHLine(SCREEN_WIDTH - 21, SCREEN_HEIGHT - 4, 18, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 4, SCREEN_HEIGHT - 12, 10, SSD1306_WHITE);
        display.drawFastVLine(6, HEADER_HEIGHT + 2, SCREEN_HEIGHT - HEADER_HEIGHT - 6, SSD1306_WHITE);

        // Mecha stripes stay clear of text
        draw_stripes(SCREEN_WIDTH - 40, 3, 32, HEADER_HEIGHT - 7, STRIPE_GAP, phase);
        draw_scanline(phase);
        draw_flow_underline(phase);
    }

    void draw_net_info(const wifi_runtime_config &cfg, const IPAddress &ip, uint8_t phase)
    {
        (void)phase;
        const String ssid = compact_ssid(cfg.ssid);
        const uint8_t ap_y = 4;                  // top cluster
        const uint8_t ip_y = HEADER_HEIGHT + 4;  // bottom cluster

        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        display.setCursor(10, ap_y);
        display.print("AP>");
        display.drawFastHLine(8, ap_y + 7, 16, SSD1306_WHITE);
        display.setCursor(32, ap_y);
        display.print(ssid);

        display.setCursor(10, ip_y);
        display.print("IP>");
        display.drawFastHLine(8, ip_y + 7, 16, SSD1306_WHITE);
        display.setCursor(32, ip_y);
        display.print(ip);
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
    anim_phase_fast++;
    draw_mecha_shell(anim_phase_fast);
    draw_net_info(wifi_current_config(), wifi_ap_ip(), anim_phase_fast);
    display.display();
}
