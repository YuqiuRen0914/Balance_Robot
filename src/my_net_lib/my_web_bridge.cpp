#include <cmath>
#include "my_net_config.h"
#include "my_car_group.h"

static constexpr float JOY_X_DEADBAND = 0.10f;
static constexpr float JOY_Y_DEADBAND = 0.02f;
static constexpr float JOY_A_DEADBAND = 0.02f;
static constexpr float JOY_AXIS_LOCK_FRACTION = 0.2f; // 副轴必须超过主轴的比例才放行
static constexpr float JOY_AXIS_LOCK_FLOOR = 0.05f;    // 副轴绝对值低于该值直接清零

// 12+2 路遥测数据
void my_web_data_update()
{
    JsonDocument doc;

    // 组包 -> 广播
    doc["type"] = "telemetry";
    doc["fallen"] = FALLEN;
    doc["pitch"] = ANGLE_X;
    doc["roll"] = ANGLE_Y;
    doc["yaw"] = ANGLE_Z;
    JsonObject g = doc["group"].to<JsonObject>();
    group_write_state(g);
    // 根据 charts_send 决定是否打包 n 路曲线数据
    if (robot.chart_enable)
    {
        JsonArray arr = doc["d"].to<JsonArray>();
        arr.add(CHART_11);
        arr.add(CHART_12);
        arr.add(CHART_13);
        arr.add(CHART_21);
        arr.add(CHART_22);
        arr.add(CHART_23);
        arr.add(CHART_31);
        arr.add(CHART_32);
        arr.add(CHART_33);
    }
    wsBroadcast(doc);
}
// PID 设置（顺序：角度P/I/D，速度P/I/D，位置P/I/D）
void web_pid_set(JsonObject param)
{
    SLIDER_11 = param["key01"].as<float>();
    SLIDER_12 = param["key02"].as<float>();
    SLIDER_13 = param["key03"].as<float>();
    SLIDER_21 = param["key04"].as<float>();
    SLIDER_22 = param["key05"].as<float>();
    SLIDER_23 = param["key06"].as<float>();
    SLIDER_31 = param["key07"].as<float>();
    SLIDER_32 = param["key08"].as<float>();
    SLIDER_33 = param["key09"].as<float>();
    SLIDER_41 = param["key10"].as<float>();
    SLIDER_42 = param["key11"].as<float>();
    SLIDER_43 = param["key12"].as<float>();
    pid_state_update();
}
// PID 读取
void web_pid_get(AsyncWebSocketClient *c)
{
    JsonDocument out;
    JsonObject pr = out["param"].to<JsonObject>();
    out["type"] = "pid";
    pr["key01"] = SLIDER_11;
    pr["key02"] = SLIDER_12;
    pr["key03"] = SLIDER_13;
    pr["key04"] = SLIDER_21;
    pr["key05"] = SLIDER_22;
    pr["key06"] = SLIDER_23;
    pr["key07"] = SLIDER_31;
    pr["key08"] = SLIDER_32;
    pr["key09"] = SLIDER_33;
    pr["key10"] = SLIDER_41;
    pr["key11"] = SLIDER_42;
    pr["key12"] = SLIDER_43;

    wsSendTo(c, out);
}
// 摇杆
void web_joystick(float x, float y, float a)
{
    const float x_clamped = my_lim(x, -1.0f, 1.0f);
    const float y_clamped = my_lim(y, -1.0f, 1.0f);
    const float a_clamped = my_lim(a, -1.0f, 1.0f);

    float x_filtered = my_db(x_clamped, JOY_X_DEADBAND);
    float y_filtered = my_db(y_clamped, JOY_Y_DEADBAND);
    float a_filtered = my_db(a_clamped, JOY_A_DEADBAND);

    // 轴向锁定：主轴明显占优时抑制副轴的小幅偏移
    if (fabsf(y_filtered) > JOY_AXIS_LOCK_FLOOR &&
        fabsf(x_filtered) < fmaxf(JOY_AXIS_LOCK_FLOOR, fabsf(y_filtered) * JOY_AXIS_LOCK_FRACTION))
    {
        x_filtered = 0.0f;
    }
    if (fabsf(x_filtered) > JOY_AXIS_LOCK_FLOOR &&
        fabsf(y_filtered) < fmaxf(JOY_AXIS_LOCK_FLOOR, fabsf(x_filtered) * JOY_AXIS_LOCK_FRACTION))
    {
        y_filtered = 0.0f;
    }

    robot.joy.x = x_filtered;
    robot.joy.y = y_filtered;
    robot.joy.a = a_filtered;
}
