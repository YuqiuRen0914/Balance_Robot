// PID parameter safety limits and clamp helpers
#pragma once

#include "my_config.h"
#include "my_tool.h"

struct pid_limit
{
    float p;
    float i;
    float d;
};

constexpr pid_limit PID_LIMIT_ANG{1.0f, 12.0f, 0.020f};  // 直立环
constexpr pid_limit PID_LIMIT_SPD{0.005f, 0.005f, 0.005f}; // 速度环
constexpr pid_limit PID_LIMIT_POS{0.005f, 0.005f, 0.005f}; // 位置环
constexpr pid_limit PID_LIMIT_YAW{0.03f, 0.005f, 0.005f};  // 转向环

inline float clamp_gain(float value, float max_abs)
{
    return my_lim(value, max_abs); // 对称限幅，允许负值
}

inline void clamp_pid(pid_config &cfg, const pid_limit &lim)
{
    cfg.p = clamp_gain(cfg.p, lim.p);
    cfg.i = clamp_gain(cfg.i, lim.i);
    cfg.d = clamp_gain(cfg.d, lim.d);
}

inline void clamp_all_pid(robot_state &r)
{
    clamp_pid(r.ang_pid, PID_LIMIT_ANG);
    clamp_pid(r.spd_pid, PID_LIMIT_SPD);
    clamp_pid(r.pos_pid, PID_LIMIT_POS);
    clamp_pid(r.yaw_pid, PID_LIMIT_YAW);
}

// 按 key 序号（key01 -> 0）返回该参数的正向最大值，前端可据此生成对称 min/max
constexpr float PID_LIMIT_LOOKUP[12] = {
    PID_LIMIT_ANG.p, PID_LIMIT_ANG.i, PID_LIMIT_ANG.d,
    PID_LIMIT_SPD.p, PID_LIMIT_SPD.i, PID_LIMIT_SPD.d,
    PID_LIMIT_POS.p, PID_LIMIT_POS.i, PID_LIMIT_POS.d,
    PID_LIMIT_YAW.p, PID_LIMIT_YAW.i, PID_LIMIT_YAW.d};

inline constexpr float pid_limit_max_for_key(int idx)
{
    return (idx >= 0 && idx < 12) ? PID_LIMIT_LOOKUP[idx] : 0.0f;
}
