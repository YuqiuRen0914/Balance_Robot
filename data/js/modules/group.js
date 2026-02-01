// /assets/js/modules/group.js
import { state, domElements } from "../config.js";
import { appendLog } from "../ui.js";
import { sendWebSocketMessage } from "../services/websocket.js";

const HEARTBEAT_INTERVAL_MS = 200; // 心跳间隔，保持在超时阈值之下
let heartbeatTimer = null;
let pendingInvite = null;
let pendingRequest = null;
let lastPeers = [];
let requestModalVisible = false;
let lastRequestKey = "";
let latchedRequest = null; // 记录当前等待审批的入队请求

function pushGroupConfig(enableOverride) {
  // 仅主车才下发配置
  if (state.formation.role !== "leader") return;
  const payload = {
    type: "group_cfg",
    group_id: state.formation.groupId,
    name: state.formation.groupName,
    timeout_ms: state.formation.timeoutMs,
    role: "leader",
    index: 0,
    count: state.formation.memberCount || 1,
    enable: typeof enableOverride === "boolean" ? enableOverride : state.carGroupMode,
  };
  sendWebSocketMessage(payload);
  appendLog(`[FORMATION] 推送编队配置: #${payload.group_id} (${payload.name || "无名"})`);
}

function isSelfMac(mac) {
  if (!mac || !state.selfMac) return false;
  return mac.toUpperCase() === state.selfMac.toUpperCase();
}

function setMode(mode) {
  state.mode = mode;
<<<<<<< Updated upstream
  if (domElements.modeSoloBtn && domElements.modeFormationBtn) {
    domElements.modeSoloBtn.classList.toggle("active", mode === "solo");
    domElements.modeFormationBtn.classList.toggle("active", mode === "formation");
  }
  if (mode === "solo") {
    stopFleet(state.formation.groupId);
    appendLog("[FORMATION] 切换到独立模式，已广播急停");
  } else {
    appendLog("[FORMATION] 编队模式已启用，摇杆指令将广播到所有成员");
=======
  state.formation.role = mode === "formation" ? "leader" : "follower";
  if (mode === "formation") {
    const count = Number(state.formation.memberCount);
    state.formation.memberCount = Number.isFinite(count) && count > 0 ? count : 1;
    state.formation.memberIndex = 0;
>>>>>>> Stashed changes
  }
  if (domElements.formationConfigBlock)
    domElements.formationConfigBlock.classList.toggle("hidden", mode !== "formation");
  if (domElements.formationFoot)
    domElements.formationFoot.classList.toggle("hidden", mode !== "formation");
  if (domElements.formationSummary)
    domElements.formationSummary.classList.toggle("hidden", mode !== "formation");
  appendLog(`[FORMATION] 已切换到 ${mode === "solo" ? "单机" : "队长广播"} 模式`);
}

function renderPeers(peers = []) {
  const list = domElements.peerList;
  if (!list) return;
  // 过滤掉自身
  const filtered = Array.isArray(peers)
    ? peers.filter((p) => !isSelfMac(p.mac))
    : [];
  lastPeers = filtered;
  if (domElements.peerCount) domElements.peerCount.textContent = `${filtered.length} 台`;
  let banner = "";
  if (state.formation.role === "leader" && state.formation.requestPending && state.formation.requestFrom) {
    banner = `
      <div class="peer-item request-banner">
        <div class="peer-meta">
          <div class="peer-name">${state.formation.requestName || "收到入队申请"}</div>
          <div class="peer-mac">${state.formation.requestFrom}</div>
          ${state.formation.requestFromIp ? `<div class="peer-ip">${state.formation.requestFromIp}</div>` : ""}
          <div class="peer-tags"><span class="peer-tag">组 ${state.formation.requestGroup ?? "?"}</span></div>
        </div>
        <div class="peer-actions">
          <button class="btn" data-role="approve-request">同意</button>
          <button class="btn ghost" data-role="reject-request">拒绝</button>
        </div>
      </div>`;
  }

<<<<<<< Updated upstream
  if (!nodes.length) {
    list.innerHTML = '<div class="ghost-text">尚未添加车辆，填入 IP 后点击“添加”</div>';
    return;
  }

  list.innerHTML = nodes
    .map((node) => {
      const cfg = node.config || {};
      const group = node.group || {};
      const online = node.status === "online";
      const stale = !node.lastSeen || now - node.lastSeen > 2500;
      const imuOk = typeof node.imu?.pitch === "number" ? Math.abs(node.imu.pitch) < 25 : false;
      const failsafe = group.failsafe;
      const dotClass = `status-dot${online ? " online" : ""}${failsafe ? " failsafe" : ""}`;
      const enabled = cfg.enabled !== false;
      return `
        <div class="fleet-row" data-host="${node.host}">
          <div class="left">
            <span class="${dotClass}"></span>
            <div class="fleet-host">${node.host}</div>
            <div class="fleet-config">
              <label>角色
                <select data-role data-host="${node.host}">
                  <option value="leader" ${cfg.role === "leader" ? "selected" : ""}>头车</option>
                  <option value="follower" ${cfg.role !== "leader" ? "selected" : ""}>从车</option>
                </select>
              </label>
              <label>序号
                <input data-index data-host="${node.host}" type="number" min="0" value="${cfg.index ?? 0}">
              </label>
              <label class="switch tiny">
                <input data-enabled data-host="${node.host}" type="checkbox" ${enabled ? "checked" : ""}><span class="slider"></span>
              </label>
            </div>
          </div>
          <div class="fleet-tags">
            <span class="tag ${group.group_id ? "" : "muted"}">组 #${group.group_id ?? state.formation.groupId}</span>
            <span class="tag ${imuOk ? "ok" : "warn"}">${imuOk ? "IMU 正常" : "IMU 校准中"}</span>
            <span class="tag ${failsafe ? "warn" : "muted"}">${failsafe ? "失联安全模式" : "实时"}</span>
            <span class="tag muted">${stale ? "待更新…" : `${Math.round((now - (node.lastSeen || now)))} ms`}</span>
=======
  if (!filtered.length) {
    list.innerHTML =
      banner ||
      '<div class="ghost-text">暂无发现附近小车，请保持同一信道并打开队长广播</div>';
    return;
  }

  list.innerHTML = banner + filtered
    .map(
      (p, idx) => `
      <div class="peer-item" data-idx="${idx}">
        <div class="peer-meta">
          <div class="peer-name">${p.name || "未命名"}</div>
          <div class="peer-mac">${p.mac}</div>
          <div class="peer-ip">${p.ip || "未知 IP"}</div>
          <div class="peer-tags">
            <span class="peer-tag">${p.is_leader ? "主车" : "从车"}</span>
            <span class="peer-tag">组 ${p.group_id ?? "?"}</span>
>>>>>>> Stashed changes
          </div>
        </div>
        <div class="peer-actions">
          ${
            (!state.carGroupMode && !state.formation.joined)
              ? `<button class="btn" data-role="apply" data-mac="${p.mac}" data-ip="${p.ip || ""}">申请加入</button>`
              : ""
          }
        </div>
      </div>`
    )
    .join("");
  list.querySelectorAll('button[data-role="apply"]').forEach((btn) =>
    btn.addEventListener("click", () => {
      const mac = btn.dataset.mac;
      const ip = btn.dataset.ip;
      sendWebSocketMessage({
        type: "group_request_join_target",
        mac,
        ip,
        group_id: state.formation.groupId,
        name: state.formation.groupName,
      });
      appendLog(`[FORMATION] 已向 ${mac} 发送入队申请`);
    })
  );
  const approveBtn = list.querySelector('button[data-role="approve-request"]');
  const rejectBtn = list.querySelector('button[data-role="reject-request"]');
  if (approveBtn) {
    approveBtn.addEventListener("click", () => {
      const count = Number(state.formation.memberCount) || 1;
      sendWebSocketMessage({
        type: "group_request_reply",
        accept: true,
        count: count + 1,
      });
      appendLog("[FORMATION] 已同意入队申请");
      state.formation.memberCount = count + 1;
      renderLocalSummary();
      state.formation.requestPending = false;
    });
  }
  if (rejectBtn) {
    rejectBtn.addEventListener("click", () => {
      sendWebSocketMessage({
        type: "group_request_reply",
        accept: false,
      });
      appendLog("[FORMATION] 已拒绝入队申请");
      state.formation.requestPending = false;
      hideRequestApproval();
    });
  }
}

function hydrateFormationDom() {
  [
    "fleetHostInput",
    "fleetAddBtn",
    "fleetCount",
    "fleetList",
    "formationGroupId",
    "formationTimeout",
    "formationSyncBtn",
    "formationStopBtn",
    "formationGroupName",
    "formationSummary",
    "formationSummaryName",
    "formationSummaryDetail",
    "formationConfigBlock",
    "formationFoot",
    "groupInviteModal",
    "groupInviteTitle",
    "groupInviteDesc",
    "groupInviteAccept",
    "groupInviteReject",
    "groupRequestModal",
    "groupRequestTitle",
    "groupRequestDesc",
    "groupRequestApprove",
    "groupRequestReject",
  ].forEach((id) => {
    if (!domElements[id]) {
      domElements[id] = document.getElementById(id);
    }
  });
}

function bindEvents() {
  if (domElements.fleetAddBtn && domElements.fleetHostInput) {
    domElements.fleetAddBtn.addEventListener("click", () => {
      const host = domElements.fleetHostInput.value.trim();
      if (!host) return;
      addFleetNode(host, { index: fleetSnapshot().length });
      appendLog(`[FLEET] 添加车辆 ${host} 中…`);
      domElements.fleetHostInput.value = "";
    });
    domElements.fleetHostInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        domElements.fleetAddBtn.click();
      }
    });
  }

  if (domElements.formationGroupId) {
    domElements.formationGroupId.addEventListener("change", (e) => {
      const v = Number(e.target.value);
      if (Number.isFinite(v)) state.formation.groupId = v;
      pushGroupConfig();
    });
  }
  if (domElements.formationGroupName) {
    domElements.formationGroupName.addEventListener("input", (e) => {
      state.formation.groupName = e.target.value || "";
      renderLocalSummary();
      // 名称改变时也推送，避免后端仍是旧值
      pushGroupConfig();
    });
  }
  if (domElements.formationTimeout) {
    domElements.formationTimeout.addEventListener("change", (e) => {
      const v = Number(e.target.value);
      if (Number.isFinite(v)) state.formation.timeoutMs = v;
      pushGroupConfig();
    });
  }

  if (domElements.formationSyncBtn) {
    domElements.formationSyncBtn.addEventListener("click", () => {
<<<<<<< Updated upstream
      syncFleetConfig(state.formation.groupId, state.formation.timeoutMs);
      appendLog("[FORMATION] 已同步配置到编队成员");
=======
      pushGroupConfig(true);
      appendLog("[FORMATION] 已将编队配置发送到主车");
>>>>>>> Stashed changes
    });
  }

  if (domElements.formationStopBtn) {
    domElements.formationStopBtn.addEventListener("click", () => {
<<<<<<< Updated upstream
      stopFleet(state.formation.groupId);
      appendLog("[FORMATION] 急停广播完成");
=======
      sendWebSocketMessage({
        type: "group_cmd",
        enable: false,
        group_id: state.formation.groupId,
        name: state.formation.groupName,
        v: 0,
        w: 0,
      });
      appendLog("[FORMATION] 已发送紧急制动");
>>>>>>> Stashed changes
    });
  }

  if (domElements.groupInviteAccept) {
    domElements.groupInviteAccept.addEventListener("click", () => {
      hideInvite();
      pendingInvite = null;
      setMode("formation");
      sendWebSocketMessage({
        type: "group_invite_reply",
        accept: true,
      });
    });
  }
  if (domElements.groupInviteReject) {
    domElements.groupInviteReject.addEventListener("click", () => {
      hideInvite();
      if (pendingInvite) {
        sendWebSocketMessage({
          type: "group_invite_reply",
          accept: false,
        });
      }
      pendingInvite = null;
    });
  }

  if (domElements.groupRequestApprove) {
    domElements.groupRequestApprove.addEventListener("click", () => {
      const count = Number(state.formation.memberCount) || 1;
      sendWebSocketMessage({
        type: "group_request_reply",
        accept: true,
        count: count + 1,
      });
      appendLog("[FORMATION] 已同意入队申请");
      state.formation.memberCount = count + 1;
      renderLocalSummary();
      state.formation.requestPending = false;
      pendingRequest = null;
      lastRequestKey = "";
      hideRequestApproval();
    });
  }
  if (domElements.groupRequestReject) {
    domElements.groupRequestReject.addEventListener("click", () => {
      sendWebSocketMessage({
        type: "group_request_reply",
        accept: false,
      });
      appendLog("[FORMATION] 已拒绝入队申请");
      state.formation.requestPending = false;
      pendingRequest = null;
      lastRequestKey = "";
      hideRequestApproval();
    });
  }
}

function startHeartbeat() {
  if (heartbeatTimer) return;
  heartbeatTimer = setInterval(() => {
    if (state.mode !== "formation") return;
    // 维持刷新，即便摇杆未移动也刷新 last_msg_ms，避免误判失联
    sendFormationControl(state.joystick.y, state.joystick.x);
  }, HEARTBEAT_INTERVAL_MS);
}

export function initFormation() {
  hydrateFormationDom();

  state.formation.groupId = Number(domElements.formationGroupId.value || 1);
  state.formation.timeoutMs = Number(domElements.formationTimeout.value || 800);

  bindEvents();
  startHeartbeat();
  setMode("solo");
  renderLocalSummary();
  renderPeers(state.peers);
}

// 监听 car_group 开关事件（由 ui.js 派发）
window.addEventListener("car-group-toggle", (e) => {
  const on = !!(e.detail && e.detail.on);
  setMode(on ? "formation" : "solo");
});

export function sendFormationControl(linear, yaw) {
  sendWebSocketMessage({
    type: "group_cmd",
    enable: true,
    group_id: state.formation.groupId,
    name: state.formation.groupName,
    v: linear,
    w: yaw,
    timeout_ms: state.formation.timeoutMs,
  });
}

function showInvite({ groupId, name, count }) {
  const modal = domElements.groupInviteModal;
  if (!modal) return;
  if (domElements.groupInviteTitle)
    domElements.groupInviteTitle.textContent = name ? `加入「${name}」?` : `加入组 #${groupId}?`;
  if (domElements.groupInviteDesc)
    domElements.groupInviteDesc.textContent = `收到主车的编队配置：组 #${groupId}${name ? ` · ${name}` : ""}，成员数 ${count ?? "?"}。是否加入？`;
  modal.classList.remove("hidden");
}

function hideInvite() {
  const modal = domElements.groupInviteModal;
  if (!modal) return;
  modal.classList.add("hidden");
}

function showRequestApproval({ from, groupId }) {
  const modal = domElements.groupRequestModal;
  if (!modal) return;
  if (domElements.groupRequestTitle)
    domElements.groupRequestTitle.textContent = state.formation.requestName
      ? `加入请求 · ${state.formation.requestName}`
      : "收到入队申请";
  if (domElements.groupRequestDesc)
    domElements.groupRequestDesc.textContent =
      `${state.formation.requestName || from || "未知小车"} (${state.formation.requestFromIp || "未知 IP"}) 申请加入编队 #${groupId ?? state.formation.groupId}，是否批准？`;
  modal.classList.remove("hidden");
  requestModalVisible = true;
}

function hideRequestApproval() {
  const modal = domElements.groupRequestModal;
  if (!modal) return;
  modal.classList.add("hidden");
  requestModalVisible = false;
  latchedRequest = null;
}

function renderLocalSummary() {
  const wrap = domElements.formationSummary;
  if (!wrap) return;
  const nameEl = domElements.formationSummaryName;
  const detailEl = domElements.formationSummaryDetail;
  const name = state.formation.groupName || `组 #${state.formation.groupId}`;
  if (nameEl) nameEl.textContent = name;
  if (detailEl)
    detailEl.textContent = `编号 #${state.formation.groupId} · 成员 ${state.formation.memberCount ?? "?"} · 角色 ${state.formation.role || "未知"}`;
  wrap.classList.toggle("hidden", !state.formation.joined);
}

export function handleGroupState(group) {
  if (!group) return;
  state.formation.groupId = group.group_id ?? state.formation.groupId;
  state.formation.timeoutMs = group.timeout_ms ?? state.formation.timeoutMs;
  state.formation.groupName = group.name || state.formation.groupName || "";
  state.formation.memberCount = group.count;
  state.formation.role = group.role;
  state.formation.joined = !!group.enabled;
  state.formation.requestPending = !!(group.request_pending && group.role === "leader");
  state.formation.requestFrom = group.request_from || "";
  // optional IPs
  state.formation.requestFromIp = group.request_from_ip || "";
  state.formation.leaderIp = group.leader_ip || state.formation.leaderIp || "";
  state.formation.requestGroup = group.request_group ?? group.group_id ?? state.formation.groupId;
  state.formation.requestName = group.request_name || state.formation.requestName || "";

  if (!group.enabled) {
    hideInvite();
    pendingInvite = null;
  }

  // 处理主车侧收到的入队申请
  pendingRequest = state.formation.requestPending
    ? { from: state.formation.requestFrom, groupId: state.formation.requestGroup }
    : null;
  if (state.formation.role === "leader") {
    if (pendingRequest) {
      const currentKey = `${pendingRequest.from}_${pendingRequest.groupId}`;
      // 记住最新请求，直到用户处理
      if (!latchedRequest || currentKey !== lastRequestKey) {
        latchedRequest = { ...pendingRequest };
        lastRequestKey = currentKey;
      }
    }

    if (latchedRequest) {
      showRequestApproval(latchedRequest);
    } else if (requestModalVisible) {
      hideRequestApproval();
      lastRequestKey = "";
    }
  } else if (requestModalVisible) {
    hideRequestApproval();
    lastRequestKey = "";
  }

  const active = document.activeElement;
  if (domElements.formationGroupId && active !== domElements.formationGroupId)
    domElements.formationGroupId.value = state.formation.groupId;
  if (domElements.formationTimeout && active !== domElements.formationTimeout)
    domElements.formationTimeout.value = state.formation.timeoutMs;
  if (domElements.formationGroupName && active !== domElements.formationGroupName)
    domElements.formationGroupName.value = state.formation.groupName;
  // 只在自己是主车时同步开关状态，避免从车被动被打开
  if (domElements.carGroupSwitch && state.formation.role === "leader")
    domElements.carGroupSwitch.checked = !!state.formation.joined;

  renderLocalSummary();

  // 仅在明确收到“邀请”时弹窗；单纯收到对方已启用编队的状态不再弹窗，避免拒绝后重复出现
  const shouldPrompt =
    group.invite_pending &&
    group.role !== "leader" &&
    state.mode === "solo" &&
    (!pendingInvite || pendingInvite.groupId !== (group.invite_group ?? group.group_id));

  if (shouldPrompt) {
    pendingInvite = {
      groupId: group.invite_group ?? group.group_id,
      name: group.invite_name ?? group.name,
      count: group.count,
    };
    showInvite(pendingInvite);
  }
}

export function handleDiscovery(peers) {
  if (!Array.isArray(peers)) return;
  state.peers = peers;
  renderPeers(peers);
}
