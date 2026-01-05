#include <cstring>
#include <WiFi.h>
#include <Preferences.h>
#include "esp_timer.h"
#include "my_net_config.h"

// NVS namespace & keys
static constexpr const char *WIFI_PREF_NS = "wifi_cfg";
static constexpr const char *WIFI_PREF_SSID = "ssid";
static constexpr const char *WIFI_PREF_PWD = "pwd";
static constexpr size_t WIFI_SSID_MAX_LEN = 32;
static constexpr size_t WIFI_PWD_MIN_LEN = 8;
static constexpr size_t WIFI_PWD_MAX_LEN = 63;

// 当前运行配置（可能来自默认值或存储）
static wifi_runtime_config g_wifi_cfg{SSID, PASSWORD, strlen(PASSWORD) < WIFI_PWD_MIN_LEN};
static IPAddress g_ap_ip(192, 168, 4, 1);

static wifi_runtime_config normalize_wifi_config(String ssid, String password)
{
    ssid.trim();
    password.trim();
    if (ssid.length() > WIFI_SSID_MAX_LEN)
        ssid = ssid.substring(0, WIFI_SSID_MAX_LEN);
    if (password.length() > WIFI_PWD_MAX_LEN)
        password = password.substring(0, WIFI_PWD_MAX_LEN);
    wifi_runtime_config cfg{
        .ssid = ssid,
        .password = password,
        .open = password.length() < WIFI_PWD_MIN_LEN,
    };
    return cfg;
}

static bool validate_wifi_config(const wifi_runtime_config &cfg, String &err)
{
    if (cfg.ssid.isEmpty())
    {
        err = "SSID 不能为空";
        return false;
    }
    if (cfg.ssid.length() > WIFI_SSID_MAX_LEN)
    {
        err = "SSID 过长 (<=32)";
        return false;
    }
    if (!cfg.open && cfg.password.length() < WIFI_PWD_MIN_LEN)
    {
        err = "密码至少 8 位（或留空为开放热点）";
        return false;
    }
    if (cfg.password.length() > WIFI_PWD_MAX_LEN)
    {
        err = "密码过长 (<=63)";
        return false;
    }
    return true;
}

static wifi_runtime_config load_saved_wifi()
{
    Preferences pref;
    wifi_runtime_config cfg = normalize_wifi_config(SSID, PASSWORD);
    if (pref.begin(WIFI_PREF_NS, true))
    {
        cfg = normalize_wifi_config(pref.getString(WIFI_PREF_SSID, SSID), pref.getString(WIFI_PREF_PWD, PASSWORD));
        pref.end();
    }
    String err;
    if (!validate_wifi_config(cfg, err))
        cfg = normalize_wifi_config(SSID, PASSWORD);
    return cfg;
}

static bool persist_wifi_config(const wifi_runtime_config &cfg)
{
    Preferences pref;
    if (!pref.begin(WIFI_PREF_NS, false))
        return false;
    pref.putString(WIFI_PREF_SSID, cfg.ssid);
    pref.putString(WIFI_PREF_PWD, cfg.password);
    pref.end();
    return true;
}

static bool start_softap(const wifi_runtime_config &cfg)
{
    const char *ap_password = cfg.open ? nullptr : cfg.password.c_str();
    IPAddress gw = g_ap_ip;
    IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(g_ap_ip, gw, subnet))
    {
        Serial.println("[WIFI] softAPConfig failed, fallback to 192.168.4.1");
        g_ap_ip = IPAddress(192, 168, 4, 1);
        gw = g_ap_ip;
        if (!WiFi.softAPConfig(g_ap_ip, gw, subnet))
            return false;
    }
    if (!WiFi.softAP(cfg.ssid.c_str(), ap_password))
        return false;
    g_wifi_cfg = cfg;
    return true;
}

const wifi_runtime_config &wifi_current_config()
{
    return g_wifi_cfg;
}

IPAddress wifi_ap_ip()
{
    return g_ap_ip;
}

static IPAddress make_random_ap_ip()
{
    // 基于启动时间的简单随机，避开 0/255，并固定 /24 网段
    const uint32_t t = static_cast<uint32_t>(esp_timer_get_time() ^ millis());
    const uint8_t oct3 = static_cast<uint8_t>(10 + (t % 200));        // 10~209
    const uint8_t oct4 = static_cast<uint8_t>(1 + ((t / 7) % 200));   // 1~200，避免 0/255
    return IPAddress(192, 168, oct3, oct4);
}

bool wifi_update_and_apply(const String &ssid, const String &password, String &err)
{
    wifi_runtime_config cfg = normalize_wifi_config(ssid, password);
    if (!validate_wifi_config(cfg, err))
        return false;

    if (!persist_wifi_config(cfg))
    {
        err = "保存到 NVS 失败";
        return false;
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAPdisconnect(true);
    delay(50);
    if (!start_softap(cfg))
    {
        err = "热点重启失败";
        return false;
    }
    WiFi.setSleep(false);
    return true;
}

void my_wifi_init()
{
    // 初始化热点模式
    WiFi.mode(WIFI_AP);
    g_ap_ip = make_random_ap_ip();
    wifi_runtime_config cfg = load_saved_wifi();

    if (!start_softap(cfg))
    {
        Serial.println("[WIFI] 热点启动失败，尝试默认值");
        cfg = normalize_wifi_config(SSID, PASSWORD);
        if (!start_softap(cfg))
        {
            Serial.println("[WIFI] 使用默认值启动热点失败");
            return;
        }
    }

    WiFi.setSleep(false);
    Serial.print("[WIFI] AP SSID: ");
    Serial.println(cfg.ssid);
    if (cfg.open)
        Serial.println("[WIFI] 开放热点（无密码）");
    else
    {
        Serial.print("[WIFI] AP Password: ");
        Serial.println(cfg.password);
    }
    Serial.print("[WIFI] AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}
