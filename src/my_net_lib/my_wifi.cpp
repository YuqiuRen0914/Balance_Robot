<<<<<<< Updated upstream
=======
#include <cstring>
#include <WiFi.h>
#include <Preferences.h>
>>>>>>> Stashed changes
#include "my_net_config.h"
#include "my_group_link.h"

<<<<<<< Updated upstream
void my_wifi_init()
{
    // 初始化WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    WiFi.setSleep(false);
    Serial.println("Connected to WiFi");
    // 打印ESP-01S的IP地址
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
=======
// NVS namespace & keys
static constexpr const char *WIFI_PREF_NS = "wifi_cfg";
static constexpr const char *WIFI_PREF_SSID = "ssid";
static constexpr const char *WIFI_PREF_PWD = "pwd";
static constexpr size_t WIFI_SSID_MAX_LEN = 32;
static constexpr size_t WIFI_PWD_MIN_LEN = 8;
static constexpr size_t WIFI_PWD_MAX_LEN = 63;

// 当前运行配置（可能来自默认值或存储）
// AP 配置（手机连接）
static wifi_runtime_config g_wifi_cfg{WIFI_AP_PREFIX, WIFI_AP_PASSWORD, strlen(WIFI_AP_PASSWORD) < WIFI_PWD_MIN_LEN};
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
    wifi_runtime_config cfg = normalize_wifi_config(WIFI_AP_PREFIX, WIFI_AP_PASSWORD);
    if (pref.begin(WIFI_PREF_NS, true))
    {
        cfg = normalize_wifi_config(pref.getString(WIFI_PREF_SSID, WIFI_AP_PREFIX), pref.getString(WIFI_PREF_PWD, WIFI_AP_PASSWORD));
        pref.end();
    }
    String err;
    if (!validate_wifi_config(cfg, err))
        cfg = normalize_wifi_config(WIFI_AP_PREFIX, WIFI_AP_PASSWORD);
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

static bool connect_station(String &err)
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(true);
    delay(50);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASSWORD);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 12000)
    {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        err = "连接超时";
        return false;
    }
    return true;
}

static String make_unique_ap_ssid()
{
    uint8_t mac[6]{};
    WiFi.macAddress(mac);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%02X%02X", WIFI_AP_PREFIX, mac[4], mac[5]);
    return String(buf);
}

static void ensure_unique_ap_ssid(wifi_runtime_config &cfg)
{
    if (!cfg.ssid.startsWith(WIFI_AP_PREFIX))
        return;
    // 若仅有前缀或长度不足，则自动补唯一后缀
    if (cfg.ssid.length() <= strlen(WIFI_AP_PREFIX) || cfg.ssid.length() > WIFI_SSID_MAX_LEN - 1)
        cfg.ssid = make_unique_ap_ssid();
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
    if (!WiFi.softAP(cfg.ssid.c_str(), ap_password, WIFI_FIXED_CHANNEL))
        return false;
    g_wifi_cfg = cfg;
    g_ap_ip = WiFi.softAPIP();
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

IPAddress wifi_sta_ip()
{
    return WiFi.localIP();
}

bool wifi_update_and_apply(const String &ssid, const String &password, String &err)
{
    wifi_runtime_config cfg = normalize_wifi_config(ssid, password);
    if (!validate_wifi_config(cfg, err))
        return false;
    ensure_unique_ap_ssid(cfg);

    if (!persist_wifi_config(cfg))
    {
        err = "保存到 NVS 失败";
        return false;
    }

    if (!connect_station(err))
        return false;

    // 热点配置更新
    WiFi.softAPdisconnect(true);
    delay(50);
    if (!start_softap(cfg))
    {
        err = "热点重启失败";
        return false;
    }

    group_link_init(); // Wi-Fi 重新启动后恢复组网
    return true;
}

void my_wifi_init()
{
    wifi_runtime_config cfg = load_saved_wifi(); // AP 配置
    ensure_unique_ap_ssid(cfg);
    String err;
    // 先连接园区网（STA）
    if (!connect_station(err))
    {
        Serial.printf("[WIFI] 连接园区网 %s 失败: %s\n", WIFI_DEFAULT_SSID, err.c_str());
    }
    // 再启动本地热点（用于手机直连控制）
    if (!start_softap(cfg))
    {
        Serial.println("[WIFI] 本地热点启动失败");
    }

    Serial.print("[WIFI] STA SSID: ");
    Serial.println(WIFI_DEFAULT_SSID);
    Serial.print("[WIFI] STA IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] AP SSID: ");
    Serial.println(cfg.ssid);
    Serial.print("[WIFI] AP IP: ");
    Serial.println(WiFi.softAPIP());
>>>>>>> Stashed changes
}
