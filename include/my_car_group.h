#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "my_config.h"

// 统一封装一次收到的编队指令/配置
struct group_command
{
    bool enable;
    int group_number;
    group_role role;
    int member_index;
    int member_count;
    const char *name;
    float linear;     // -1 ~ 1 归一化线速度
    float yaw;        // -1 ~ 1 归一化偏航
    uint32_t timeout_ms;
};

void my_group_init();
void group_apply_config(const group_command &cmd, bool propagate = true);
void group_apply_command(const group_command &cmd);
void group_tick();

group_role group_role_from_string(const char *role);
const char *group_role_to_string(group_role role);

// 将当前编队状态写入 Json，方便遥测和应答
void group_write_state(JsonObject obj);
