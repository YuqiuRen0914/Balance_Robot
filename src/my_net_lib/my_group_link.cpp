#include <WiFi.h>
#include <WiFiUdp.h>
#include <lwip/def.h>
#include <algorithm>
#include <string.h>
#include "my_group_link.h"
#include "my_motion.h"
#include "my_net.h"
#include "my_net_config.h"

namespace
{
    constexpr uint8_t kFrameVersion = 2;
    constexpr size_t kNameLen = sizeof(((group_state *)nullptr)->name);
    constexpr uint16_t kUdpPort = 51020;
    constexpr uint32_t kPresenceIntervalMs = 600;

    enum class FrameType : uint8_t
    {
        cmd = 0,
        invite = 1,
        request = 2,
        accept = 3,
        reject = 4,
        presence = 5
    };

    enum FrameFlags : uint8_t
    {
        kFlagEnable = 0x01,
        kFlagFromLeader = 0x02
    };

    struct __attribute__((packed)) GroupFrame
    {
        uint8_t version;
        uint8_t type;
        uint8_t flags;
        uint8_t reserved1;
        int32_t group_number;
        int32_t member_index;
        int32_t member_count;
        float linear;
        float yaw;
        uint32_t timeout_ms;
        uint32_t seq;
        char name[kNameLen];
        uint8_t leader_mac[6];
        uint32_t ip_be; // 发送方 STA IP，网络序
    };

    static_assert(sizeof(GroupFrame) <= 160, "GroupFrame unexpectedly large");

    WiFiUDP g_udp;
    bool g_ready = false;
    uint32_t g_seq = 0;
    uint8_t g_self_mac[6] = {0};
    bool g_self_mac_valid = false;
    uint32_t g_self_ip_be = 0;

    constexpr size_t kPeerMax = 8;
    struct PeerInfo
    {
        uint8_t mac[6];
        uint32_t ip_be;
        char name[kNameLen];
        uint32_t last_seen;
        bool is_leader;
        int group_id;
    };
    PeerInfo g_peers[kPeerMax]{};

    uint32_t ip_to_be(const IPAddress &ip)
    {
        return htonl((uint32_t)ip);
    }

    IPAddress ip_from_be(uint32_t be)
    {
        return IPAddress(ntohl(be));
    }

    bool is_ip_zero(const IPAddress &ip)
    {
        return (uint32_t)ip == 0;
    }

    IPAddress broadcast_ip()
    {
        IPAddress ip = WiFi.localIP();
        IPAddress mask = WiFi.subnetMask();
        if (is_ip_zero(ip) || is_ip_zero(mask))
            return IPAddress(255, 255, 255, 255);
        IPAddress bc;
        for (int i = 0; i < 4; ++i)
            bc[i] = ip[i] | (~mask[i]);
        return bc;
    }

    void clear_invite()
    {
        robot.group_cfg.invite_pending = false;
        robot.group_cfg.invite_group = 0;
        robot.group_cfg.invite_name[0] = '\0';
        robot.group_cfg.invite_from_ip = 0;
        memset(robot.group_cfg.invite_from, 0, 6);
    }

    void clear_request()
    {
        robot.group_cfg.request_pending = false;
        robot.group_cfg.request_group = 0;
        robot.group_cfg.request_name[0] = '\0';
        robot.group_cfg.request_from_ip = 0;
        memset(robot.group_cfg.request_from, 0, 6);
    }

    void remember_invite(const GroupFrame &f, const uint8_t *mac, uint32_t remote_ip_be)
    {
        robot.group_cfg.invite_pending = true;
        robot.group_cfg.invite_group = f.group_number;
        strlcpy(robot.group_cfg.invite_name, f.name, sizeof(robot.group_cfg.invite_name));
        memcpy(robot.group_cfg.invite_from, mac, 6);
        robot.group_cfg.invite_from_ip = remote_ip_be;
    }

    void remember_request(const GroupFrame &f, const uint8_t *mac, uint32_t remote_ip_be)
    {
        robot.group_cfg.request_pending = true;
        robot.group_cfg.request_group = f.group_number;
        strlcpy(robot.group_cfg.request_name, f.name, sizeof(robot.group_cfg.request_name));
        memcpy(robot.group_cfg.request_from, mac, 6);
        robot.group_cfg.request_from_ip = remote_ip_be;
    }

    bool is_self_mac(const uint8_t *mac)
    {
        if (!mac || !g_self_mac_valid)
            return false;
        return memcmp(mac, g_self_mac, 6) == 0;
    }

    bool is_self_ip(uint32_t ip_be)
    {
        return g_self_ip_be != 0 && g_self_ip_be == ip_be;
    }

    void upsert_peer(const uint8_t mac[6], uint32_t ip_be, const char *name, bool is_leader, int group_id)
    {
        const uint32_t now = millis();
        for (auto &p : g_peers)
        {
            if (memcmp(p.mac, mac, 6) == 0 || p.ip_be == ip_be)
            {
                if (mac)
                    memcpy(p.mac, mac, 6);
                p.ip_be = ip_be;
                p.last_seen = now;
                p.is_leader = is_leader;
                p.group_id = group_id;
                if (name && *name)
                    strlcpy(p.name, name, kNameLen);
                return;
            }
        }
        // 替换最老的
        size_t idx = 0;
        uint32_t oldest = g_peers[0].last_seen;
        for (size_t i = 1; i < kPeerMax; ++i)
        {
            if (g_peers[i].last_seen < oldest)
            {
                oldest = g_peers[i].last_seen;
                idx = i;
            }
        }
        if (mac)
            memcpy(g_peers[idx].mac, mac, 6);
        g_peers[idx].ip_be = ip_be;
        g_peers[idx].last_seen = now;
        g_peers[idx].is_leader = is_leader;
        g_peers[idx].group_id = group_id;
        if (name)
            strlcpy(g_peers[idx].name, name, kNameLen);
    }

    void ensure_self_mac()
    {
        if (g_self_mac_valid)
            return;
        String mac = WiFi.macAddress();
        int vals[6];
        if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) == 6)
        {
            for (int i = 0; i < 6; ++i)
                g_self_mac[i] = static_cast<uint8_t>(vals[i]);
            g_self_mac_valid = true;
        }
    }

    bool send_frame(const IPAddress &target, const GroupFrame &f)
    {
        if (!g_ready)
            return false;
        if (!g_udp.beginPacket(target, kUdpPort))
            return false;
        g_udp.write(reinterpret_cast<const uint8_t *>(&f), sizeof(f));
        return g_udp.endPacket() == 1;
    }

    void handle_cmd_frame(const GroupFrame &frame, const uint8_t *mac_addr)
    {
        if (!robot.group_cfg.enabled || robot.group_cfg.role != group_role::follower)
            return;

        if (mac_addr)
        {
            memcpy(robot.group_cfg.leader_mac, mac_addr, 6);
            robot.group_cfg.leader_mac_valid = true;
        }
        if (frame.ip_be)
        {
            robot.group_cfg.leader_ip = frame.ip_be;
            robot.group_cfg.leader_ip_valid = true;
        }

        group_command cmd{
            .enable = (frame.flags & kFlagEnable) != 0,
            .group_number = frame.group_number,
            .role = group_role::follower,
            .member_index = frame.member_index,
            .member_count = frame.member_count,
            .name = frame.name,
            .linear = frame.linear,
            .yaw = frame.yaw,
            .timeout_ms = frame.timeout_ms,
        };
        group_apply_command(cmd);
    }

    void handle_invite_frame(const GroupFrame &frame, const uint8_t *mac_addr)
    {
        if (robot.group_cfg.role == group_role::leader && robot.group_cfg.enabled)
            return;
        if (mac_addr)
        {
            memcpy(robot.group_cfg.leader_mac, frame.leader_mac, 6);
            robot.group_cfg.leader_mac_valid = true;
        }
        if (frame.ip_be)
        {
            robot.group_cfg.leader_ip = frame.ip_be;
            robot.group_cfg.leader_ip_valid = true;
        }
        robot.group_cfg.invite_from_is_leader = (frame.flags & kFlagFromLeader) != 0;
        remember_invite(frame, mac_addr, frame.ip_be);
    }

    void handle_presence_frame(const GroupFrame &frame, const uint8_t *mac_addr)
    {
        if (!mac_addr && frame.ip_be == 0)
            return;
        upsert_peer(mac_addr, frame.ip_be, frame.name, (frame.flags & kFlagFromLeader) != 0, frame.group_number);
    }

    void handle_request_frame(const GroupFrame &frame, const uint8_t *mac_addr)
    {
        if (robot.group_cfg.role != group_role::leader)
            return;
        remember_request(frame, mac_addr, frame.ip_be);
        group_command cfg{
            .enable = robot.group_cfg.enabled,
            .group_number = robot.group_cfg.group_number,
            .role = robot.group_cfg.role,
            .member_index = robot.group_cfg.member_index,
            .member_count = robot.group_cfg.group_count,
            .name = robot.group_cfg.name,
            .linear = robot.group_cfg.target_linear,
            .yaw = robot.group_cfg.target_yaw,
            .timeout_ms = robot.group_cfg.timeout_ms,
        };
        group_apply_config(cfg, false);
        JsonDocument out;
        out["type"] = "group_state";
        JsonObject g = out["group"].to<JsonObject>();
        group_write_state(g);
        wsBroadcast(out);
    }

    void handle_accept_frame(const GroupFrame &frame, const uint8_t *mac_addr)
    {
        if (robot.group_cfg.role == group_role::leader)
        {
            if (frame.member_count > 0)
                robot.group_cfg.group_count = frame.member_count;
            else
                robot.group_cfg.group_count = std::max(1, robot.group_cfg.group_count + 1);
            clear_request();
            clear_invite();
            return;
        }

        if (mac_addr)
        {
            memcpy(robot.group_cfg.leader_mac, mac_addr, 6);
            robot.group_cfg.leader_mac_valid = true;
        }
        if (frame.ip_be)
        {
            robot.group_cfg.leader_ip = frame.ip_be;
            robot.group_cfg.leader_ip_valid = true;
        }
        robot.group_cfg.enabled = true;
        group_command cfg{
            .enable = (frame.flags & kFlagEnable) != 0,
            .group_number = frame.group_number,
            .role = group_role::follower,
            .member_index = frame.member_index,
            .member_count = frame.member_count,
            .name = frame.name,
            .linear = 0.0f,
            .yaw = 0.0f,
            .timeout_ms = frame.timeout_ms,
        };
        group_apply_config(cfg, false);
        clear_invite();
        clear_request();
    }

    void process_rx()
    {
        int packetSize = 0;
        while ((packetSize = g_udp.parsePacket()) > 0)
        {
            if (packetSize < (int)sizeof(GroupFrame))
            {
                g_udp.flush();
                continue;
            }
            GroupFrame frame{};
            g_udp.read(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
            if (frame.version != kFrameVersion)
                continue;

            const IPAddress remote_ip = g_udp.remoteIP();
            const uint32_t remote_ip_be = ip_to_be(remote_ip);
            if (frame.ip_be == 0)
                frame.ip_be = remote_ip_be;
            if (is_self_ip(frame.ip_be))
                continue;

            uint8_t mac_addr[6]{};
            memcpy(mac_addr, frame.leader_mac, 6);
            switch (static_cast<FrameType>(frame.type))
            {
            case FrameType::cmd:
                handle_cmd_frame(frame, mac_addr);
                break;
            case FrameType::invite:
                handle_invite_frame(frame, mac_addr);
                break;
            case FrameType::request:
                handle_request_frame(frame, mac_addr);
                break;
            case FrameType::accept:
                handle_accept_frame(frame, mac_addr);
                break;
            case FrameType::reject:
                clear_invite();
                clear_request();
                break;
            case FrameType::presence:
                handle_presence_frame(frame, mac_addr);
                break;
            default:
                break;
            }
        }
    }

    bool ensure_udp_ready()
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            g_ready = false;
            return false;
        }
        if (g_ready)
            return true;
        g_udp.stop();
        if (!g_udp.begin(kUdpPort))
        {
            Serial.println("[GROUP] UDP begin failed");
            return false;
        }
        ensure_self_mac();
        g_self_ip_be = ip_to_be(WiFi.localIP());
        g_ready = true;
        Serial.printf("[GROUP] UDP ready on %s:%u\n", WiFi.localIP().toString().c_str(), kUdpPort);
        return true;
    }
} // namespace

bool group_link_ready()
{
    return g_ready;
}

void group_link_init()
{
    g_ready = false;
    ensure_udp_ready();
}

void group_link_broadcast(const group_command &cmd)
{
    if (!ensure_udp_ready())
        return;
    if (robot.group_cfg.role != group_role::leader)
        return;

    GroupFrame frame{};
    frame.version = kFrameVersion;
    frame.type = static_cast<uint8_t>(FrameType::cmd);
    frame.flags = cmd.enable ? kFlagEnable : 0;
    frame.group_number = cmd.group_number;
    frame.member_index = cmd.member_index;
    frame.member_count = cmd.member_count;
    frame.linear = cmd.linear;
    frame.yaw = cmd.yaw;
    frame.timeout_ms = cmd.timeout_ms;
    frame.seq = ++g_seq;
    const char *name = cmd.name ? cmd.name : robot.group_cfg.name;
    strlcpy(frame.name, name, kNameLen);
    ensure_self_mac();
    if (g_self_mac_valid)
        memcpy(frame.leader_mac, g_self_mac, 6);
    frame.ip_be = g_self_ip_be;

    send_frame(broadcast_ip(), frame);
}

bool group_link_send_invite_to(const uint8_t mac[6], int group_id, const char *name, int member_count, uint32_t timeout_ms, uint32_t ip_be)
{
    if (!ensure_udp_ready())
        return false;
    GroupFrame f{};
    f.version = kFrameVersion;
    f.type = static_cast<uint8_t>(FrameType::invite);
    f.flags = kFlagEnable | kFlagFromLeader;
    f.group_number = group_id;
    f.member_index = 0;
    f.member_count = member_count > 0 ? member_count : std::max(1, robot.group_cfg.group_count);
    f.timeout_ms = timeout_ms;
    f.seq = ++g_seq;
    strlcpy(f.name, name ? name : robot.group_cfg.name, kNameLen);
    ensure_self_mac();
    if (g_self_mac_valid)
        memcpy(f.leader_mac, g_self_mac, 6);
    f.ip_be = g_self_ip_be;

    IPAddress target = ip_from_be(ip_be);
    if (is_ip_zero(target))
        target = broadcast_ip();
    return send_frame(target, f);
}

void group_link_send_invite(int group_id, const char *name, int member_count, uint32_t timeout_ms)
{
    if (!ensure_udp_ready())
        return;
    const int effective_count = member_count > 0 ? member_count : std::max(1, robot.group_cfg.group_count);
    GroupFrame f{};
    f.version = kFrameVersion;
    f.type = static_cast<uint8_t>(FrameType::invite);
    f.flags = kFlagEnable;
    f.group_number = group_id;
    f.member_index = 0;
    f.member_count = effective_count;
    f.linear = 0;
    f.yaw = 0;
    f.timeout_ms = timeout_ms;
    f.seq = ++g_seq;
    strlcpy(f.name, name ? name : robot.group_cfg.name, kNameLen);
    ensure_self_mac();
    if (robot.group_cfg.role == group_role::leader && g_self_mac_valid)
    {
        memcpy(f.leader_mac, g_self_mac, 6);
        f.flags |= kFlagFromLeader;
    }
    else
    {
        memcpy(f.leader_mac, robot.group_cfg.leader_mac, 6);
    }
    f.ip_be = g_self_ip_be;
    send_frame(broadcast_ip(), f);
}

void group_link_send_request(int group_id, const char *name)
{
    if (!ensure_udp_ready())
        return;
    const auto &cfg = wifi_current_config();
    GroupFrame f{};
    f.version = kFrameVersion;
    f.type = static_cast<uint8_t>(FrameType::request);
    f.flags = kFlagEnable;
    f.group_number = group_id;
    f.member_index = 0;
    f.member_count = 0;
    f.linear = 0;
    f.yaw = 0;
    f.timeout_ms = robot.group_cfg.timeout_ms;
    f.seq = ++g_seq;
    strlcpy(f.name, cfg.ssid.c_str(), kNameLen);
    ensure_self_mac();
    memcpy(f.leader_mac, g_self_mac, 6); // 标记发送者自身
    f.ip_be = g_self_ip_be;

    if (robot.group_cfg.leader_ip_valid)
    {
        send_frame(ip_from_be(robot.group_cfg.leader_ip), f);
    }
    else
    {
        send_frame(broadcast_ip(), f);
    }
}

void group_link_send_request_to(const uint8_t mac[6], int group_id, const char *name, uint32_t ip_be)
{
    if (!ensure_udp_ready())
        return;
    const auto &cfg = wifi_current_config();
    GroupFrame f{};
    f.version = kFrameVersion;
    f.type = static_cast<uint8_t>(FrameType::request);
    f.flags = kFlagEnable;
    f.group_number = group_id;
    f.member_index = 0;
    f.member_count = 0;
    f.linear = 0;
    f.yaw = 0;
    f.timeout_ms = robot.group_cfg.timeout_ms;
    f.seq = ++g_seq;
    strlcpy(f.name, cfg.ssid.c_str(), kNameLen);
    ensure_self_mac();
    memcpy(f.leader_mac, g_self_mac, 6); // 始终携带本机 MAC，便于主车识别请求方
    f.ip_be = g_self_ip_be;
    IPAddress target = ip_from_be(ip_be);
    if (is_ip_zero(target))
        target = broadcast_ip();
    send_frame(target, f);
}

bool group_link_accept_invite(bool accept)
{
    if (!robot.group_cfg.invite_pending || !ensure_udp_ready())
        return false;

    if (accept)
    {
        if (robot.group_cfg.invite_from_is_leader)
        {
            GroupFrame resp{};
            resp.version = kFrameVersion;
            resp.type = static_cast<uint8_t>(FrameType::accept);
            resp.flags = kFlagEnable;
            resp.group_number = robot.group_cfg.invite_group;
            resp.member_index = robot.group_cfg.member_index;
            resp.member_count = robot.group_cfg.group_count;
            resp.timeout_ms = robot.group_cfg.timeout_ms;
            resp.seq = ++g_seq;
            memcpy(resp.leader_mac, robot.group_cfg.invite_from, 6);
            strlcpy(resp.name, robot.group_cfg.invite_name, kNameLen);
            resp.ip_be = g_self_ip_be;

            memcpy(robot.group_cfg.leader_mac, robot.group_cfg.invite_from, 6);
            robot.group_cfg.leader_mac_valid = true;
            robot.group_cfg.leader_ip = robot.group_cfg.invite_from_ip;
            robot.group_cfg.leader_ip_valid = robot.group_cfg.invite_from_ip != 0;

            group_command cfg{
                .enable = true,
                .group_number = robot.group_cfg.invite_group,
                .role = group_role::follower,
                .member_index = robot.group_cfg.member_index,
                .member_count = robot.group_cfg.group_count,
                .name = robot.group_cfg.invite_name,
                .linear = 0.0f,
                .yaw = 0.0f,
                .timeout_ms = robot.group_cfg.timeout_ms,
            };
            group_apply_config(cfg, false);

            IPAddress target = ip_from_be(robot.group_cfg.invite_from_ip);
            if (is_ip_zero(target))
                target = broadcast_ip();
            send_frame(target, resp);
        }
        else
        {
            GroupFrame req{};
            req.version = kFrameVersion;
            req.type = static_cast<uint8_t>(FrameType::request);
            req.flags = kFlagEnable;
            req.group_number = robot.group_cfg.invite_group;
            req.member_index = 0;
            req.member_count = 0;
            req.linear = 0;
            req.yaw = 0;
            req.timeout_ms = robot.group_cfg.timeout_ms;
            req.seq = ++g_seq;
            memcpy(req.leader_mac, robot.group_cfg.leader_mac, 6);
            strlcpy(req.name, robot.group_cfg.invite_name, kNameLen);
            req.ip_be = g_self_ip_be;
            IPAddress target = ip_from_be(robot.group_cfg.leader_ip);
            if (is_ip_zero(target))
                target = broadcast_ip();
            send_frame(target, req);
        }
    }
    else
    {
        GroupFrame resp{};
        resp.version = kFrameVersion;
        resp.type = static_cast<uint8_t>(FrameType::reject);
        resp.flags = 0;
        resp.group_number = robot.group_cfg.invite_group;
        resp.seq = ++g_seq;
        resp.ip_be = g_self_ip_be;
        IPAddress target = ip_from_be(robot.group_cfg.invite_from_ip);
        if (is_ip_zero(target))
            target = broadcast_ip();
        send_frame(target, resp);
    }

    clear_invite();
    return true;
}

bool group_link_reply_request(bool accept, int assigned_index, int member_count)
{
    if (!robot.group_cfg.request_pending || !ensure_udp_ready())
        return false;

    const int new_count =
        accept ? (member_count >= 0 ? member_count : std::max(1, robot.group_cfg.group_count + 1))
               : robot.group_cfg.group_count;

    GroupFrame resp{};
    resp.version = kFrameVersion;
    resp.type = static_cast<uint8_t>(accept ? FrameType::accept : FrameType::reject);
    resp.flags = accept ? kFlagEnable : 0;
    resp.group_number = robot.group_cfg.request_group;
    resp.member_index = assigned_index >= 0 ? assigned_index : robot.group_cfg.member_index + 1;
    resp.member_count = new_count;
    resp.timeout_ms = robot.group_cfg.timeout_ms;
    resp.seq = ++g_seq;
    strlcpy(resp.name, robot.group_cfg.name, kNameLen);
    resp.ip_be = g_self_ip_be;

    IPAddress target = ip_from_be(robot.group_cfg.request_from_ip);
    if (is_ip_zero(target))
        target = broadcast_ip();
    send_frame(target, resp);
    if (accept)
        robot.group_cfg.group_count = new_count;
    clear_request();
    return true;
}

void group_link_poll()
{
    if (!ensure_udp_ready())
        return;

    process_rx();

    static uint32_t last_adv = 0;
    const uint32_t now = millis();
    if (!robot.group_cfg.enabled)
        return;
    if (now - last_adv < kPresenceIntervalMs)
        return;
    last_adv = now;

    GroupFrame p{};
    p.version = kFrameVersion;
    p.type = static_cast<uint8_t>(FrameType::presence);
    p.flags = robot.group_cfg.role == group_role::leader ? kFlagFromLeader : 0;
    p.group_number = robot.group_cfg.group_number;
    p.seq = ++g_seq;
    strlcpy(p.name, robot.group_cfg.name, kNameLen);
    p.ip_be = g_self_ip_be;
    ensure_self_mac();
    if (g_self_mac_valid)
        memcpy(p.leader_mac, g_self_mac, 6);

    send_frame(broadcast_ip(), p);
}

void group_discovery_write(JsonArray arr)
{
    const uint32_t now = millis();
    for (const auto &p : g_peers)
    {
        if (now - p.last_seen > 3000)
            continue;
        if (is_self_ip(p.ip_be))
            continue;
        JsonObject o = arr.add<JsonObject>();
        char macstr[18];
        snprintf(macstr, sizeof(macstr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);
        o["mac"] = macstr;
        o["name"] = p.name;
        o["is_leader"] = p.is_leader;
        o["group_id"] = p.group_id;
        o["age_ms"] = now - p.last_seen;
        o["ip"] = ip_from_be(p.ip_be).toString();
    }
}

bool group_link_parse_mac(const char *str, uint8_t out[6])
{
    if (!str)
        return false;
    int vals[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6)
        return false;
    for (int i = 0; i < 6; ++i)
        out[i] = static_cast<uint8_t>(vals[i]);
    return true;
}

void group_link_format_mac(const uint8_t mac[6], char out[18])
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool group_link_get_self_mac(uint8_t out[6])
{
    ensure_self_mac();
    if (!g_self_mac_valid)
        return false;
    memcpy(out, g_self_mac, 6);
    return true;
}
