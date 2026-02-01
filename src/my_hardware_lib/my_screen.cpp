#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pgmspace.h>

#include "my_screen.h"
#include "my_config.h"
#include "my_motion.h"
#include "my_bat.h"
#include "my_I2C.h"
<<<<<<< Updated upstream
=======
#include "my_net.h"
#include "my_boardRGB.h"
#include "company_logo.h"
>>>>>>> Stashed changes

namespace
{
    constexpr uint32_t FRAME_INTERVAL_MS = SCREEN_REFRESH_TIME;
<<<<<<< Updated upstream
    constexpr float BATT_MIN_V = 9.0f;
    constexpr float BATT_MAX_V = 12.6f;
=======
    constexpr uint8_t HEADER_HEIGHT = 14;
    constexpr uint8_t SSID_MAX_LEN = 14;
    constexpr uint8_t STRIPE_GAP = 6;
    constexpr uint8_t FRAME_THICKNESS = 2;
    constexpr uint8_t FLOW_SEG_LEN = 16;
    constexpr uint8_t CONTAINER_RADIUS = 3;
    constexpr uint8_t CONTAINER_PADDING = 2; // 给 logo 与边框留出缓冲
>>>>>>> Stashed changes

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &ScreenWire, -1);

    bool screen_ready = false;
    uint32_t last_frame_ms = 0;
<<<<<<< Updated upstream
=======
    uint8_t anim_phase_fast = 0;
    constexpr int16_t LOGO_W = COMPANY_LOGO_W;
    constexpr int16_t LOGO_H = COMPANY_LOGO_H;
    constexpr int16_t PROGRESS_H = 12;
    constexpr uint8_t PROGRESS_STEP = 2;      // 每次递增 2%，更平滑
    constexpr uint16_t BOOT_FRAME_DELAY_MS = 90; // 单帧延时，控制总时长
    constexpr uint16_t BOOT_END_PAUSE_MS = 400;  // 满格后停留时间

>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
=======
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

    void draw_container(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        // 更细的单层边框，内侧填充黑底
        display.drawRoundRect(x, y, w, h, CONTAINER_RADIUS, SSD1306_WHITE);
        display.fillRoundRect(x + 1, y + 1, w - 2, h - 2, CONTAINER_RADIUS - 1, SSD1306_BLACK);
    }

    void draw_bar_vignette(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        // 在进度条外围绘制点状晕影，丰富层次
        const int16_t hx = x - 2;
        const int16_t hy = y - 2;
        const int16_t hw = w + 4;
        const int16_t hh = h + 4;
        for (int16_t px = hx; px < hx + hw; px += 2)
        {
            display.drawPixel(px, hy, SSD1306_WHITE);
            display.drawPixel(px, hy + hh - 1, SSD1306_WHITE);
        }
        for (int16_t py = hy; py < hy + hh; py += 2)
        {
            display.drawPixel(hx, py, SSD1306_WHITE);
            display.drawPixel(hx + hw - 1, py, SSD1306_WHITE);
        }
    }

    void draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint8_t phase)
    {
        const int16_t inner_x = x + 2;
        const int16_t inner_y = y + 2;
        const int16_t inner_w = w - 4;
        const int16_t inner_h = h - 4;
        const int16_t fill_w = inner_w * percent / 100;

        draw_bar_vignette(inner_x, inner_y, inner_w, inner_h);
        display.fillRoundRect(inner_x, inner_y, inner_w, inner_h, CONTAINER_RADIUS - 2, SSD1306_BLACK);
        if (fill_w > 0)
        {
            display.fillRoundRect(inner_x, inner_y, fill_w, inner_h, CONTAINER_RADIUS - 3, SSD1306_WHITE);
            for (int16_t sx = inner_x + (phase % STRIPE_GAP); sx < inner_x + fill_w; sx += STRIPE_GAP)
            {
                display.drawFastVLine(sx, inner_y, inner_h, SSD1306_BLACK);
            }
            if ((phase & 0x01) == 0)
            {
                for (int16_t gx = inner_x; gx < inner_x + fill_w; gx += 8)
                {
                    display.drawFastHLine(gx, inner_y, 3, SSD1306_WHITE);
                }
            }
        }

        // 百分比数值：气泡样式更可爱
        const int16_t bubble_w = 26;
        const int16_t bubble_h = 12;
        const int16_t bubble_x = x + w - bubble_w;
        const int16_t bubble_y = (y > bubble_h + 2) ? (y - bubble_h - 2) : 0;
        display.fillRoundRect(bubble_x, bubble_y, bubble_w, bubble_h, 3, SSD1306_WHITE);
        display.drawRoundRect(bubble_x, bubble_y, bubble_w, bubble_h, 3, SSD1306_WHITE);
        display.setTextSize(1);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        char buf[6];
        snprintf(buf, sizeof(buf), "%3d%%", percent);
        display.setCursor(bubble_x + 4, bubble_y + 2);
        display.print(buf);
    }

    void draw_boot_logo(int16_t x, int16_t y)
    {
        display.drawBitmap(x, y, epd_bitmap_1, LOGO_W, LOGO_H, SSD1306_WHITE);
    }

    void run_boot_animation()
    {
        const int16_t container_x = 2;
        const int16_t container_y = (SCREEN_HEIGHT - LOGO_H) / 2 - 1;
        const int16_t container_w = SCREEN_WIDTH - 4;
        const int16_t container_h = LOGO_H + 4; // 多留 2px，避免边框贴 logo
        const int16_t logo_x = container_x + CONTAINER_PADDING;
        const int16_t logo_y = container_y + CONTAINER_PADDING;
        const int16_t bar_x = logo_x + LOGO_W + CONTAINER_PADDING;
        const int16_t bar_w = container_x + container_w - CONTAINER_PADDING - bar_x;
        const int16_t bar_h = PROGRESS_H;
        const int16_t bar_y = (SCREEN_HEIGHT - bar_h) / 2;

        for (uint8_t p = 0, phase = 0; p <= 100; p += PROGRESS_STEP, ++phase)
        {
            display.clearDisplay();
            draw_container(container_x, container_y, container_w, container_h);
            draw_boot_logo(logo_x, logo_y);
            draw_progress_bar(bar_x, bar_y, bar_w, bar_h, p, phase);
            display.display();
            delay(BOOT_FRAME_DELAY_MS);
        }
        // 收尾：全白闪屏后进入正常界面
        display.clearDisplay();
        display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        display.display();
        delay(120);
    }

    void draw_net_info(const wifi_runtime_config &cfg, const IPAddress &ip, uint8_t phase)
    {
        (void)phase;
        const String ssid = compact_ssid(cfg.ssid);
        const uint8_t ap_y = 4;                  // top cluster
        const uint8_t ip_y = HEADER_HEIGHT + 4;  // bottom cluster
>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
=======

        display.setCursor(4, ap_y);
        display.print(ssid);

        display.setCursor(4, ip_y);
        display.print(ip);
>>>>>>> Stashed changes
    }
}

void my_screen_init()
{
    const uint8_t addr = normalized_address(SCREEN_I2C_ADDRESS);
    if (!display.begin(SSD1306_SWITCHCAPVCC, addr))
    {
        Serial.println("SSD1306 init failed");
        my_boardRGB_notify_peripheral_missing();
        return;
    }

    run_boot_animation();

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
