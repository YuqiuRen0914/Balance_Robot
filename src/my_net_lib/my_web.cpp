<<<<<<< Updated upstream
#include "my_net_config.h"
#include "my_rgb.h"
#include "my_car_group.h"
=======
#include <WiFi.h>
#include <AsyncJson.h>
#include <lwip/def.h>
#include "my_net_config.h"
#include "my_rgb.h"
#include "my_car_group.h"
#include "my_bat.h"
#include "my_pid_limits.h"
#include "my_group_link.h"
>>>>>>> Stashed changes
// ======================= 内部状态 =======================
// Web/WS 服务实例（仅本翻译单元可见）
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static constexpr int CHART_COUNT = 3;
static constexpr int SLIDER_GROUP_COUNT = 4;

ChartConfig chart_config[CHART_COUNT] = {{CHART_NAME1, {CHART_NAME11, CHART_NAME12, CHART_NAME13}},
                                         {CHART_NAME2, {CHART_NAME21, CHART_NAME22, CHART_NAME23}},
                                         {CHART_NAME3, {CHART_NAME31, CHART_NAME32, CHART_NAME33}}};

SliderGroup slider_group[SLIDER_GROUP_COUNT] = {{SLIDER_NAME1, {SLIDER_NAME11, SLIDER_NAME12, SLIDER_NAME13}},
                                                {SLIDER_NAME2, {SLIDER_NAME21, SLIDER_NAME22, SLIDER_NAME23}},
                                                {SLIDER_NAME3, {SLIDER_NAME31, SLIDER_NAME32, SLIDER_NAME33}},
                                                {SLIDER_NAME4, {SLIDER_NAME41, SLIDER_NAME42, SLIDER_NAME43}}};

struct RgbModeInfo
{
    const char *name;
    const char *desc;
};

static const RgbModeInfo RGB_MODE_INFO[] = {
    {"霓虹循环", "多彩光环顺时针流动，呈现霓虹轨迹"},
    {"呼吸律动", "青蓝呼吸脉冲，柔和亮灭"},
    {"流星雨", "暖白高速闪过，带尾焰拖曳"},
    {"心跳红", "双击式跳动，红色能量脉冲"}};

static constexpr size_t RGB_MODE_COUNT = sizeof(RGB_MODE_INFO) / sizeof(RGB_MODE_INFO[0]);

static int clamp_rgb_mode(int mode)
{
    if (mode < RGB_MODE_NEON || mode > RGB_MODE_HEARTBEAT)
        return RGB_MODE_NEON;
    return mode;
}

static int clamp_rgb_count(int count)
{
    if (count <= 0)
        return RGB_LED_COUNT;
    if (count > RGB_LED_COUNT)
        return RGB_LED_COUNT;
    return count;
}

static void send_group_state(AsyncWebSocketClient *c)
{
    JsonDocument out;
    out["type"] = "group_state";
    JsonObject g = out["group"].to<JsonObject>();
    group_write_state(g);
    if (c)
        wsSendTo(c, out);
    else if (wsCanBroadcast())
        wsBroadcast(out);
}


// ======================= 事件处理 =======================
// 连接事件：仅在 WS_EVT_CONNECT 时发送一次 UI 配置；其后不再发送
void we_evt_connect(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    JsonDocument doc;
    doc["type"] = "ui_config";
    // charts
    JsonArray charts = doc["charts"].to<JsonArray>();

    for (int i = 0; i < CHART_COUNT; ++i)
    {
        JsonObject o = charts.add<JsonObject>();
        o["title"] = chart_config[i].title;
        JsonArray l = o["legends"].to<JsonArray>();
        for (int j = 0; j < 3; ++j)
            l.add(chart_config[i].legend[j]);
    }
    // sliders
    JsonArray sliders = doc["sliders"].to<JsonArray>();
    for (int i = 0; i < SLIDER_GROUP_COUNT; ++i)
    {
        JsonObject g = sliders.add<JsonObject>();
        g["group"] = slider_group[i].group;
        JsonArray n = g["names"].to<JsonArray>();
        for (int j = 0; j < 3; ++j)
            n.add(slider_group[i].names[j]);
    }

    JsonObject rgb = doc["rgb"].to<JsonObject>();
    JsonArray modes = rgb["modes"].to<JsonArray>();
    for (size_t i = 0; i < RGB_MODE_COUNT; ++i)
    {
        JsonObject m = modes.add<JsonObject>();
        m["id"] = (int)i;
        m["name"] = RGB_MODE_INFO[i].name;
        m["desc"] = RGB_MODE_INFO[i].desc;
    }
    robot.rgb.mode = clamp_rgb_mode(robot.rgb.mode);
    robot.rgb.rgb_count = clamp_rgb_count(robot.rgb.rgb_count);
    JsonObject rgb_state = rgb["state"].to<JsonObject>();
    rgb_state["mode"] = robot.rgb.mode;
    rgb_state["count"] = robot.rgb.rgb_count;
    rgb["max_count"] = RGB_LED_COUNT;

    // pid limits (对称范围)，顺序与 key01-key12 一致
    JsonArray pid_limits = doc["pid_limits"].to<JsonArray>();
    for (int i = 0; i < 12; ++i)
    {
        const float lim = pid_limit_max_for_key(i);
        JsonObject p = pid_limits.add<JsonObject>();
        p["min"] = -lim;
        p["max"] = lim;
    }

    doc.shrinkToFit(); // 发送前收紧空间，减轻带宽
    // 先发 ui_config（首连一次，配置标题/图例/分组名称）
    wsSendTo(c, doc);
    // 再发一个简单的 info，便于前端状态显示
    JsonDocument ack;
    ack["type"] = "info";
    ack["text"] = "connected";
    wsSendTo(c, ack);
}

// 断联事件
void we_evt_disconnect(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    // 可按需记录日志或清理该 client 的状态
}

// 消息事件
void ws_evt_data(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, data, len);
    const char *typeStr = doc["type"] | "";
    // 仅处理完整文本帧
    if (!(info->final && info->index == 0 && info->len == len))
        return;
    if (info->opcode != WS_TEXT)
        return;
    if (e)
        return;
    if (!*typeStr)
        return;
    // 调试用
    Serial.println("--- Printing JsonDocument ---");
    serializeJsonPretty(doc, Serial);

    // ===========逻辑处理区域===========
    // 1) 设置遥测频率（上限锁 60Hz，避免队列堆积）
    if (!strcmp(typeStr, "telem_hz"))
        robot.data_ms = 1000 / my_lim(doc["ms"], REFRESH_RATE_MIN, REFRESH_RATE_MAX);

    // 2) 运行开关（只影响执行器；不影响遥测是否发送）
    else if (!strcmp(typeStr, "robot_run"))
        robot.run = doc["running"] | false; // 默认关闭

    // 3) 图表推送开关（关闭时后台仅发 3 路，显著减载）
    else if (!strcmp(typeStr, "charts_send"))
        robot.chart_enable = doc["on"] | false; // 默认关闭

    // 4) 摔倒检测开关
    else if (!strcmp(typeStr, "fall_check"))
        robot.fallen.enable = doc["enable"];

    // 5) 姿态零偏（预留）
    else if (!strcmp(typeStr, "imu_restart"))
        return; // TODO

    // 6) 摇杆
    else if (!strcmp(typeStr, "joy"))
    {
        // 从车角色时屏蔽本地摇杆，防止自行动作
        if (robot.group_cfg.role == group_role::follower)
            return;
        web_joystick(doc["x"] | 0.0f, doc["y"] | 0.0f, doc["a"] | 0.0f);
    }

    // 7) 设置 PID
    else if (!strcmp(typeStr, "set_pid"))
        web_pid_set(doc["param"].as<JsonObject>());

    // 8) 读取 PID（回填给前端）
    else if (!strcmp(typeStr, "get_pid"))
        web_pid_get(c);

    else if (!strcmp(typeStr, "group_cfg"))
    {
        group_command cfg{
            .enable = doc["enable"] | robot.group_cfg.enabled,
            .group_number = doc["group_id"] | robot.group_cfg.group_number,
            .role = group_role_from_string(doc["role"] | nullptr),
            .member_index = doc["index"] | robot.group_cfg.member_index,
            .member_count = doc["count"] | robot.group_cfg.group_count,
            .name = doc["name"] | robot.group_cfg.name,
            .linear = robot.group_cfg.target_linear,
            .yaw = robot.group_cfg.target_yaw,
            .timeout_ms = doc["timeout_ms"] | robot.group_cfg.timeout_ms,
        };
        group_apply_config(cfg);
        send_group_state(nullptr);
    }

    else if (!strcmp(typeStr, "group_cmd"))
    {
        group_command cmd{
            .enable = doc["enable"] | true,
            .group_number = doc["group_id"] | robot.group_cfg.group_number,
            .role = group_role_from_string(doc["role"] | nullptr),
            .member_index = doc["index"] | robot.group_cfg.member_index,
            .member_count = doc["count"] | robot.group_cfg.group_count,
            .name = doc["name"] | robot.group_cfg.name,
            .linear = doc["v"] | 0.0f,
            .yaw = doc["w"] | 0.0f,
            .timeout_ms = doc["timeout_ms"] | robot.group_cfg.timeout_ms,
        };
        group_apply_command(cmd);
        send_group_state(nullptr);
    }

    else if (!strcmp(typeStr, "group_query"))
        send_group_state(c);
    else if (!strcmp(typeStr, "group_invite_target"))
    {
        const char *macStr = doc["mac"] | "";
        const char *ipStr = doc["ip"] | "";
        uint8_t mac[6];
        uint32_t ip_be = 0;
        IPAddress ip;
        if (ip.fromString(ipStr))
            ip_be = htonl((uint32_t)ip);
        if (group_link_parse_mac(macStr, mac))
        {
            const int gid = doc["group_id"] | robot.group_cfg.group_number;
            const int count = doc["count"] | robot.group_cfg.group_count;
            const uint32_t to = doc["timeout_ms"] | robot.group_cfg.timeout_ms;
            const char *name = doc["name"] | robot.group_cfg.name;
            group_link_send_invite_to(mac, gid, name, count, to, ip_be);
        }
    }
    else if (!strcmp(typeStr, "group_invite_reply"))
    {
        const bool accept = doc["accept"] | false;
        group_link_accept_invite(accept);
        send_group_state(nullptr);
    }
    else if (!strcmp(typeStr, "group_request_join"))
    {
        const int gid = doc["group_id"] | robot.group_cfg.group_number;
        const char *name = doc["name"] | robot.group_cfg.name;
        group_link_send_request(gid, name);
    }
    else if (!strcmp(typeStr, "group_request_join_target"))
    {
        const char *macStr = doc["mac"] | "";
        const char *ipStr = doc["ip"] | "";
        uint8_t mac[6];
        uint32_t ip_be = 0;
        IPAddress ip;
        if (ip.fromString(ipStr))
            ip_be = htonl((uint32_t)ip);
        if (group_link_parse_mac(macStr, mac))
        {
            const int gid = doc["group_id"] | robot.group_cfg.group_number;
            const char *name = doc["name"] | robot.group_cfg.name;
            group_link_send_request_to(mac, gid, name, ip_be);
        }
    }
    else if (!strcmp(typeStr, "group_request_reply"))
    {
        const bool accept = doc["accept"] | false;
        const int idx = doc["index"] | -1;
        const int count = doc["count"] | -1;
        group_link_reply_request(accept, idx, count);
        send_group_state(nullptr);
    }

    else if (!strcmp(typeStr, "rgb_set"))
    {
        robot.rgb.mode = clamp_rgb_mode(doc["mode"] | robot.rgb.mode);
        robot.rgb.rgb_count = clamp_rgb_count(doc["count"] | robot.rgb.rgb_count);

        JsonDocument out;
        out["type"] = "rgb_state";
        out["mode"] = robot.rgb.mode;
        out["count"] = robot.rgb.rgb_count;
        if (wsCanBroadcast())
            wsBroadcast(out);
        else
            wsSendTo(c, out);
    }
}

// ping/pong事件
void ws_evt_pong(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    // 可选：心跳统计
}

// 错误事件
void ws_evt_error(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    // 可选：错误日志
}

// ======================= WebSocket 事件绑定 =======================
static void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        we_evt_connect(s, c, type, arg, data, len);
        break;
    case WS_EVT_DISCONNECT:
        we_evt_disconnect(s, c, type, arg, data, len);
        break;
    case WS_EVT_DATA:
        ws_evt_data(s, c, type, arg, data, len);
        break;
    case WS_EVT_PONG:
        ws_evt_pong(s, c, type, arg, data, len);
        break;
    case WS_EVT_ERROR:
        ws_evt_error(s, c, type, arg, data, len);
        break;
    default:
        break;
    }
}

// ======================= HTTP 处理器 =======================
static void handleApiState(AsyncWebServerRequest *req)
{
    JsonDocument d;
    String s;
    d["ms"] = robot.data_ms;
    d["running"] = robot.run;
    d["chart_enable"] = robot.chart_enable;
    d["fallen_enable"] = robot.fallen.enable;
    d["rgb_mode"] = clamp_rgb_mode(robot.rgb.mode);
    d["rgb_count"] = clamp_rgb_count(robot.rgb.rgb_count);
    d["rgb_max"] = RGB_LED_COUNT;
<<<<<<< Updated upstream
=======
    uint8_t self_mac[6];
    if (group_link_get_self_mac(self_mac))
    {
        char macstr[18];
        group_link_format_mac(self_mac, macstr);
        d["self_mac"] = macstr;
    }
    JsonObject w = d["wifi"].to<JsonObject>();
    const auto &cfg = wifi_current_config();
    w["ssid"] = cfg.ssid;
    w["password"] = cfg.password;
    w["open"] = cfg.open;
    w["password_length"] = cfg.password.length();
    w["ip"] = wifi_ap_ip().toString();
    w["sta_ssid"] = WIFI_DEFAULT_SSID;
    w["sta_ip"] = wifi_sta_ip().toString();
    w["sta_connected"] = WiFi.status() == WL_CONNECTED;
    w["sta_ssid"] = WIFI_DEFAULT_SSID;
    w["sta_ip"] = wifi_sta_ip().toString();
    w["sta_connected"] = WiFi.status() == WL_CONNECTED;
    w["sta_ssid"] = WIFI_DEFAULT_SSID;
    w["sta_ip"] = wifi_sta_ip().toString();
    w["sta_connected"] = WiFi.status() == WL_CONNECTED;
>>>>>>> Stashed changes
    JsonObject g = d["group"].to<JsonObject>();
    g["name"] = robot.group_cfg.name;
    group_write_state(g);
    serializeJson(d, s);
    req->send(200, "application/json; charset=utf-8", s);
}

static void handleRootRequest(AsyncWebServerRequest *req)
{
    if (!handleFileRead(req, "/"))
        req->send(404, "text/plain; charset=utf-8", "home.html not found (upload LittleFS data)");
}

static void handleNotFound(AsyncWebServerRequest *req)
{
    if (!handleFileRead(req, req->url()))
        req->send(404, "text/plain; charset=utf-8", "404 Not Found");
}

// ======================= 路由 & 初始化入口 =======================
void my_web_asyn_init()
{
    if (!FSYS.begin(true)) // 1) 文件系统
        Serial.println("[WEB] LittleFS mount failed (formatted?)");
    else if (!(FSYS.exists("/home.html") || FSYS.exists("/home.html.gz")))
        Serial.println("[WEB] home.html missing in LittleFS, upload data folder with `pio run -t uploadfs`");

    ws.onEvent(onWsEvent); // 2) WebSocket
    server.addHandler(&ws);

    server.on("/api/state", HTTP_GET, handleApiState); // 3) 基础 API
    server.on("/", HTTP_GET, handleRootRequest);       // 4) 静态文件
    server.onNotFound(handleNotFound);
    server.begin(); // 5) 启动 HTTP
}
