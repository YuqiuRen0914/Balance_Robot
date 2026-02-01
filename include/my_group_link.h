#pragma once

#include "my_car_group.h"

// 编队链路（ESP-NOW）初始化与握手/广播接口
void group_link_init();
bool group_link_ready();
void group_link_broadcast(const group_command &cmd);

// 周期任务：广播存在并维护发现表（需在循环中定期调用）
void group_link_poll();

// 写入发现到的附近设备（最近 3s 内），用于 telemetry -> UI
void group_discovery_write(JsonArray arr);

// 主车发起邀请（定向）
bool group_link_send_invite_to(const uint8_t mac[6], int group_id, const char *name, int member_count, uint32_t timeout_ms, uint32_t ip_be = 0);

void group_link_send_invite(int group_id, const char *name, int member_count, uint32_t timeout_ms);
void group_link_send_request(int group_id, const char *name);
void group_link_send_request_to(const uint8_t mac[6], int group_id, const char *name, uint32_t ip_be = 0);
// follower 对 pending 邀请回应；返回是否存在待处理邀请
bool group_link_accept_invite(bool accept);
// leader 对 pending 申请回应；返回是否存在待处理申请
bool group_link_reply_request(bool accept, int assigned_index = -1, int member_count = -1);

// 工具：解析/格式化 MAC
bool group_link_parse_mac(const char *str, uint8_t out[6]);
void group_link_format_mac(const uint8_t mac[6], char out[18]);
// 获取本机 STA MAC（若可用），返回是否成功
bool group_link_get_self_mac(uint8_t out[6]);
