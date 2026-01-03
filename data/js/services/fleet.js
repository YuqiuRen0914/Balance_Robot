// /assets/js/services/fleet.js
import { state } from "../config.js";
import { sendWebSocketMessage } from "./websocket.js";
import { appendLog } from "../ui.js";

const nodes = new Map();
let updateHandler = null;

function notify() {
  state.fleet = Array.from(nodes.values());
  if (updateHandler) updateHandler(state.fleet);
}

function sendToSelf(payload) {
  // 通过本机 WebSocket 也下发一次，确保主车收到编队指令
  sendWebSocketMessage(payload);
}

function formationMemberCount(includeSelf = true) {
  // 只统计已勾选的成员；如果列表里已包含本机则不重复计入
  const enabledNodes = Array.from(nodes.values()).filter((n) => n.config?.enabled !== false);
  const selfListed = enabledNodes.some((n) => n.host === location.host);
  return enabledNodes.length + (includeSelf && !selfListed ? 1 : 0);
}

function normalizeHost(input) {
  if (!input) return "";
  let host = input.trim();
  if (!host) return "";
  host = host.replace(/^https?:\/\//i, "");
  host = host.replace(/\/+$/, "");
  return host;
}

function applyGroupInfo(node, obj = {}) {
  node.group = {
    enabled: !!obj.enabled,
    group_id: obj.group_id ?? node.group?.group_id ?? state.formation.groupId,
    count: obj.count ?? node.group?.count ?? 0,
    index: obj.index ?? node.group?.index ?? 0,
    role: obj.role ?? node.group?.role ?? "follower",
    timeout_ms: obj.timeout_ms ?? node.group?.timeout_ms ?? state.formation.timeoutMs,
    failsafe: !!obj.failsafe,
    age_ms: obj.age_ms ?? node.group?.age_ms ?? 0,
  };
}

function defaultConfig() {
  return { role: "follower", index: nodes.size, enabled: true };
}

function updateNodeConfig(host, partial) {
  const node = nodes.get(host);
  if (!node) return;
  node.config = { ...node.config, ...partial };
  notify();
}

function handleMessage(node, msg) {
  if (msg.type === "telemetry") {
    node.lastSeen = Date.now();
    node.imu = {
      pitch: msg.pitch,
      roll: msg.roll,
      yaw: msg.yaw,
    };
    if (msg.group) applyGroupInfo(node, msg.group);
  } else if (msg.type === "group_state") {
    node.lastSeen = Date.now();
    applyGroupInfo(node, msg.group ?? msg);
  } else if (msg.type === "info") {
    node.lastSeen = Date.now();
  }
  notify();
}

function openSocket(node) {
  const protocol = location.protocol === "https:" ? "wss://" : "ws://";
  const url = `${protocol}${node.host}/ws`;
  try {
    node.status = "connecting";
    notify();
    node.ws = new WebSocket(url);
  } catch (e) {
    node.status = "offline";
    notify();
    appendLog(`[FLEET] 连接 ${node.host} 失败: ${e.message}`);
    return;
  }

  node.ws.onopen = () => {
    node.status = "online";
    node.lastSeen = Date.now();
    notify();
    node.ws?.send(JSON.stringify({ type: "group_query" }));
    appendLog(`[FLEET] 已连接 ${node.host}`);
  };

  node.ws.onclose = () => {
    node.status = "offline";
    node.ws = null;
    notify();
    setTimeout(() => openSocket(node), 1500);
  };

  node.ws.onerror = () => {
    node.status = "error";
    notify();
  };

  node.ws.onmessage = (evt) => {
    try {
      const payload = JSON.parse(evt.data);
      handleMessage(node, payload);
    } catch (err) {
      appendLog(`[FLEET] 解析 ${node.host} 数据失败: ${err.message}`);
    }
  };
}

export function setFleetUpdateHandler(handler) {
  updateHandler = handler;
}

export function addFleetNode(rawHost, preset = {}) {
  const host = normalizeHost(rawHost);
  if (!host) return null;
  if (nodes.has(host)) {
    updateNodeConfig(host, preset);
    return nodes.get(host);
  }
  const node = {
    id: host,
    host,
    status: "connecting",
    lastSeen: 0,
    imu: {},
    group: {},
    config: { ...defaultConfig(), ...preset },
    ws: null,
  };
  nodes.set(host, node);
  openSocket(node);
  notify();
  return node;
}

export function removeFleetNode(host) {
  const node = nodes.get(host);
  if (node?.ws) {
    try {
      node.ws.close();
    } catch (_) {
      // ignore
    }
  }
  nodes.delete(host);
  notify();
}

function sendToNode(node, payload) {
  if (!node || !node.ws || node.ws.readyState !== WebSocket.OPEN) return;
  node.ws.send(JSON.stringify(payload));
}

export function syncFleetConfig(groupId, timeoutMs) {
  const gid = Number(groupId ?? state.formation.groupId);
  const ttl = Number(timeoutMs ?? state.formation.timeoutMs);
  const name = state.formation.groupName || "";
  const memberCount = formationMemberCount();
  nodes.forEach((node) => {
    if (!node.config.enabled) return;
    sendToNode(node, {
      type: "group_cfg",
      group_id: gid,
      name,
      role: node.config.role,
      index: node.config.index,
      count: memberCount,
      enable: true,
      timeout_ms: ttl,
    });
  });
  sendToSelf({
    type: "group_cfg",
    group_id: gid,
    name,
    role: "leader",
    index: 0,
    count: memberCount,
    enable: true,
    timeout_ms: ttl,
  });
}

export function broadcastFormationControl({ linear = 0, yaw = 0, enable = true, groupId, timeoutMs } = {}) {
  const gid = Number(groupId ?? state.formation.groupId);
  const ttl = Number(timeoutMs ?? state.formation.timeoutMs);
  const name = state.formation.groupName || "";
  const memberCount = formationMemberCount();
  nodes.forEach((node) => {
    if (!node.config.enabled) return;
    const payload = {
      type: "group_cmd",
      enable,
      group_id: gid,
      name,
      role: node.config.role,
      index: node.config.index,
      count: memberCount,
      v: linear,
      w: yaw,
      timeout_ms: ttl,
    };
    sendToNode(node, payload);
  });
  sendToSelf({
    type: "group_cmd",
    enable,
    group_id: gid,
    name,
    role: "leader",
    index: 0,
    count: memberCount,
    v: linear,
    w: yaw,
    timeout_ms: ttl,
  });
}

export function stopFleet(groupId) {
  broadcastFormationControl({ linear: 0, yaw: 0, enable: false, groupId });
}

export function updateFleetNodeConfig(host, partial) {
  updateNodeConfig(host, partial);
}

export function fleetSnapshot() {
  return Array.from(nodes.values());
}
