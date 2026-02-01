#include "my_car_group.h"
#include "my_group_link.h"
#include "my_motion.h"
#include "my_tool.h"
#include <algorithm>
#include <strings.h>
#include <string.h>
#include <WiFi.h>
#include <lwip/def.h>

namespace
{
    constexpr float kCmdLimit = 1.0f;           // 线速度/转向指令归一化范围
    constexpr float kSlewAlpha = 0.15f;         // 平滑系数，越大响应越快
    constexpr uint32_t kDefaultTimeoutMs = 800; // 若超过该时长未收到新指令则进入安全模式
    constexpr size_t kGroupNameMax = sizeof(robot.group_cfg.name);

    inline float clamp_cmd(float v)
    {
        return my_lim(v, -kCmdLimit, kCmdLimit);
    }

    void reset_cmds()
    {
        robot.group_cfg.target_linear = 0.0f;
        robot.group_cfg.target_yaw = 0.0f;
        robot.group_cfg.applied_linear = 0.0f;
        robot.group_cfg.applied_yaw = 0.0f;
    }

    void set_group_name(const char *name)
    {
        if (!name)
            return;
        strlcpy(robot.group_cfg.name, name, kGroupNameMax);
    }

    void broadcast_if_leader(const group_command &cmd)
    {
        if (robot.group_cfg.role != group_role::leader)
            return;
        group_command downlink = cmd;
        downlink.role = group_role::follower; // 下行指令仅用于从车
        group_link_broadcast(downlink);
    }
}

group_role group_role_from_string(const char *role)
{
    if (!role || !*role)
        return robot.group_cfg.role;
    if (!strcasecmp(role, "leader") || !strcasecmp(role, "head"))
        return group_role::leader;
    return group_role::follower;
}

const char *group_role_to_string(group_role role)
{
    return role == group_role::leader ? "leader" : "follower";
}

void my_group_init()
{
    robot.group_cfg.enabled = false;
    robot.group_cfg.group_number = 1;
    robot.group_cfg.group_count = 1; // 默认仅自身
    robot.group_cfg.member_index = 0;
    robot.group_cfg.role = group_role::leader;
    robot.group_cfg.name[0] = '\0';
    robot.group_cfg.timeout_ms = kDefaultTimeoutMs;
    robot.group_cfg.last_msg_ms = millis();
    robot.group_cfg.failsafe = false;
    memset(robot.group_cfg.leader_mac, 0, sizeof(robot.group_cfg.leader_mac));
    robot.group_cfg.leader_mac_valid = false;
    robot.group_cfg.leader_ip = 0;
    robot.group_cfg.leader_ip_valid = false;
    robot.group_cfg.invite_pending = false;
    robot.group_cfg.invite_from_is_leader = false;
    robot.group_cfg.invite_group = 0;
    robot.group_cfg.invite_name[0] = '\0';
    memset(robot.group_cfg.invite_from, 0, sizeof(robot.group_cfg.invite_from));
    robot.group_cfg.invite_from_ip = 0;
    robot.group_cfg.request_pending = false;
    robot.group_cfg.request_group = 0;
    robot.group_cfg.request_name[0] = '\0';
    memset(robot.group_cfg.request_from, 0, sizeof(robot.group_cfg.request_from));
    robot.group_cfg.request_from_ip = 0;
    reset_cmds();
}

void group_apply_config(const group_command &cmd, bool propagate)
{
    robot.group_cfg.enabled = cmd.enable;
    robot.group_cfg.group_number = cmd.group_number;
    robot.group_cfg.role = cmd.role;
    if (robot.group_cfg.role == group_role::leader)
    {
        robot.group_cfg.leader_mac_valid = false;
        memset(robot.group_cfg.leader_mac, 0, sizeof(robot.group_cfg.leader_mac));
        robot.group_cfg.invite_pending = false;
        robot.group_cfg.request_pending = false;
    }
    // 组规模与编号容错：主车至少包含自己
    if (cmd.role == group_role::leader)
    {
        robot.group_cfg.member_index = 0;
        robot.group_cfg.group_count = cmd.member_count > 0 ? cmd.member_count : std::max(1, robot.group_cfg.group_count);
    }
    else
    {
        robot.group_cfg.member_index = cmd.member_index;
        if (cmd.member_count > 0)
            robot.group_cfg.group_count = cmd.member_count;
    }
    if (cmd.name)
        set_group_name(cmd.name);
    if (cmd.timeout_ms > 0)
    {
        const uint32_t bounded = static_cast<uint32_t>(my_lim(static_cast<float>(cmd.timeout_ms), 200.0f, 3000.0f));
        robot.group_cfg.timeout_ms = bounded;
    }
    if (!robot.group_cfg.enabled)
    {
        reset_cmds();
        robot.group_cfg.failsafe = false;
    }
    robot.group_cfg.last_msg_ms = millis();
    if (propagate)
        broadcast_if_leader(cmd);
}

void group_apply_command(const group_command &cmd)
{
    // 仅接受目标编队号的指令；未配置时（group_number==0）接受任何组并自动对齐
    if (robot.group_cfg.group_number != 0 && cmd.group_number != robot.group_cfg.group_number)
        return;

    if (robot.group_cfg.group_number == 0)
        robot.group_cfg.group_number = cmd.group_number;

    group_apply_config(cmd, false);
    robot.group_cfg.target_linear = clamp_cmd(cmd.linear);
    robot.group_cfg.target_yaw = clamp_cmd(cmd.yaw);
    robot.group_cfg.failsafe = false;
    robot.group_cfg.last_msg_ms = millis();
    broadcast_if_leader(cmd);
}

void group_tick()
{
    if (!robot.group_cfg.enabled)
        return;

    const uint32_t now = millis();
    const bool expired = (now - robot.group_cfg.last_msg_ms) > robot.group_cfg.timeout_ms;

    const float lin_goal = expired ? 0.0f : robot.group_cfg.target_linear;
    const float yaw_goal = expired ? 0.0f : robot.group_cfg.target_yaw;

    robot.group_cfg.applied_linear += (lin_goal - robot.group_cfg.applied_linear) * kSlewAlpha;
    robot.group_cfg.applied_yaw += (yaw_goal - robot.group_cfg.applied_yaw) * kSlewAlpha;

    robot.joy.y = robot.group_cfg.applied_linear;
    robot.joy.x = robot.group_cfg.applied_yaw;
    robot.group_cfg.failsafe = expired;
}

void group_write_state(JsonObject obj)
{
    const uint32_t now = millis();
    obj["enabled"] = robot.group_cfg.enabled;
    obj["group_id"] = robot.group_cfg.group_number;
    obj["name"] = robot.group_cfg.name;
    obj["count"] = robot.group_cfg.group_count;
    obj["index"] = robot.group_cfg.member_index;
    obj["role"] = group_role_to_string(robot.group_cfg.role);
    obj["v"] = robot.group_cfg.target_linear;
    obj["w"] = robot.group_cfg.target_yaw;
    obj["applied_v"] = robot.group_cfg.applied_linear;
    obj["applied_w"] = robot.group_cfg.applied_yaw;
    obj["timeout_ms"] = robot.group_cfg.timeout_ms;
    obj["age_ms"] = now - robot.group_cfg.last_msg_ms;
    obj["failsafe"] = robot.group_cfg.failsafe;
    obj["invite_pending"] = robot.group_cfg.invite_pending;
    obj["invite_group"] = robot.group_cfg.invite_group;
    obj["invite_name"] = robot.group_cfg.invite_name;
    obj["invite_from_is_leader"] = robot.group_cfg.invite_from_is_leader;
    char invite_from_str[18];
    sprintf(invite_from_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            robot.group_cfg.invite_from[0], robot.group_cfg.invite_from[1], robot.group_cfg.invite_from[2],
            robot.group_cfg.invite_from[3], robot.group_cfg.invite_from[4], robot.group_cfg.invite_from[5]);
    obj["invite_from"] = invite_from_str;
    if (robot.group_cfg.invite_from_ip)
        obj["invite_from_ip"] = IPAddress(ntohl(robot.group_cfg.invite_from_ip)).toString();
    char leader_mac_str[18];
    sprintf(leader_mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            robot.group_cfg.leader_mac[0], robot.group_cfg.leader_mac[1], robot.group_cfg.leader_mac[2],
            robot.group_cfg.leader_mac[3], robot.group_cfg.leader_mac[4], robot.group_cfg.leader_mac[5]);
    obj["leader_mac"] = leader_mac_str;
    if (robot.group_cfg.leader_ip_valid)
        obj["leader_ip"] = IPAddress(ntohl(robot.group_cfg.leader_ip)).toString();
    obj["request_pending"] = robot.group_cfg.request_pending;
    obj["request_group"] = robot.group_cfg.request_group;
    obj["request_name"] = robot.group_cfg.request_name;
    char req_from_str[18];
    sprintf(req_from_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            robot.group_cfg.request_from[0], robot.group_cfg.request_from[1], robot.group_cfg.request_from[2],
            robot.group_cfg.request_from[3], robot.group_cfg.request_from[4], robot.group_cfg.request_from[5]);
    obj["request_from"] = req_from_str;
    if (robot.group_cfg.request_from_ip)
        obj["request_from_ip"] = IPAddress(ntohl(robot.group_cfg.request_from_ip)).toString();
}
