// /assets/js/modules/group.js
import { state, domElements } from "../config.js";
import { appendLog } from "../ui.js";
import {
  addFleetNode,
  broadcastFormationControl,
  fleetSnapshot,
  setFleetUpdateHandler,
  stopFleet,
  syncFleetConfig,
  updateFleetNodeConfig,
} from "../services/fleet.js";
import { sendWebSocketMessage } from "../services/websocket.js";

const HEARTBEAT_INTERVAL_MS = 200; // 心跳间隔，保持在超时阈值之下
let heartbeatTimer = null;
let pendingInvite = null;

function setMode(mode) {
  state.mode = mode;
  if (domElements.modeSoloBtn && domElements.modeFormationBtn) {
    domElements.modeSoloBtn.classList.toggle("active", mode === "solo");
    domElements.modeFormationBtn.classList.toggle("active", mode === "formation");
  }
  if (mode === "solo") {
    stopFleet(state.formation.groupId);
    appendLog("[FORMATION] 切换到单机驾驶，已广播紧急制动");
  } else {
    appendLog("[FORMATION] 编队驾驶已启用，操纵指令将广播到所有成员");
  }
}

function renderFleet(nodes) {
  const list = domElements.fleetList;
  if (!list) return;
  const now = Date.now();
  const enabledNodes = nodes.filter((n) => n.config?.enabled !== false);
  const onlineCount = enabledNodes.filter((n) => n.status === "online").length;
  if (domElements.fleetCount) domElements.fleetCount.textContent = `${onlineCount}/${enabledNodes.length}`;

  if (!nodes.length) {
    list.innerHTML = '<div class="ghost-text">尚未添加机甲，填入地址后点击“加入编队”</div>';
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
            <span class="tag ${group.group_id ? "" : "muted"}">编队 #${group.group_id ?? state.formation.groupId}</span>
            <span class="tag ${imuOk ? "ok" : "warn"}">${imuOk ? "IMU 正常" : "IMU 校准中"}</span>
            <span class="tag ${failsafe ? "warn" : "muted"}">${failsafe ? "失联安全模式" : "实时"}</span>
            <span class="tag muted">${stale ? "待更新…" : `${Math.round((now - (node.lastSeen || now)))} ms`}</span>
          </div>
        </div>
      `;
    })
    .join("");

  list.querySelectorAll("select[data-role]").forEach((sel) => {
    const host = sel.dataset.host;
    sel.addEventListener("change", (e) => updateFleetNodeConfig(host, { role: e.target.value }));
  });
  list.querySelectorAll("input[data-index]").forEach((inp) => {
    const host = inp.dataset.host;
    inp.addEventListener("change", (e) => {
      const val = Number(e.target.value);
      updateFleetNodeConfig(host, { index: Number.isFinite(val) ? val : 0 });
    });
  });
  list.querySelectorAll("input[data-enabled]").forEach((inp) => {
    const host = inp.dataset.host;
    inp.addEventListener("change", (e) => updateFleetNodeConfig(host, { enabled: e.target.checked }));
  });
}

function hydrateFormationDom() {
  [
    "modeSoloBtn",
    "modeFormationBtn",
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
    "groupInviteModal",
    "groupInviteTitle",
    "groupInviteDesc",
    "groupInviteAccept",
    "groupInviteReject",
  ].forEach((id) => {
    if (!domElements[id]) {
      domElements[id] = document.getElementById(id);
    }
  });
}

function bindEvents() {
  if (domElements.modeSoloBtn) domElements.modeSoloBtn.addEventListener("click", () => setMode("solo"));
  if (domElements.modeFormationBtn) domElements.modeFormationBtn.addEventListener("click", () => setMode("formation"));

  if (domElements.fleetAddBtn && domElements.fleetHostInput) {
    domElements.fleetAddBtn.addEventListener("click", () => {
      const host = domElements.fleetHostInput.value.trim();
      if (!host) return;
      addFleetNode(host, { index: fleetSnapshot().length });
      appendLog(`[FLEET] 添加机甲 ${host} 中…`);
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
    });
  }
  if (domElements.formationGroupName) {
    domElements.formationGroupName.addEventListener("input", (e) => {
      state.formation.groupName = e.target.value || "";
      renderLocalSummary();
    });
  }
  if (domElements.formationTimeout) {
    domElements.formationTimeout.addEventListener("change", (e) => {
      const v = Number(e.target.value);
      if (Number.isFinite(v)) state.formation.timeoutMs = v;
    });
  }

  if (domElements.formationSyncBtn) {
    domElements.formationSyncBtn.addEventListener("click", () => {
      syncFleetConfig(state.formation.groupId, state.formation.timeoutMs);
      appendLog("[FORMATION] 已同步编队指令到成员");
    });
  }

  if (domElements.formationStopBtn) {
    domElements.formationStopBtn.addEventListener("click", () => {
      stopFleet(state.formation.groupId);
      appendLog("[FORMATION] 紧急制动广播完成");
    });
  }

  if (domElements.groupInviteAccept) {
    domElements.groupInviteAccept.addEventListener("click", () => {
      hideInvite();
      pendingInvite = null;
      setMode("formation");
      sendWebSocketMessage({
        type: "group_cfg",
        group_id: state.formation.groupId,
        enable: true,
      });
    });
  }
  if (domElements.groupInviteReject) {
    domElements.groupInviteReject.addEventListener("click", () => {
      hideInvite();
      if (pendingInvite) {
        sendWebSocketMessage({
          type: "group_cfg",
          group_id: pendingInvite.groupId,
          enable: false,
        });
      }
      pendingInvite = null;
    });
  }
}

function hasActiveFormationTargets() {
  return fleetSnapshot().some(
    (node) => node.config?.enabled && node.ws && node.ws.readyState === WebSocket.OPEN
  );
}

function startHeartbeat() {
  if (heartbeatTimer) return;
  heartbeatTimer = setInterval(() => {
    if (state.mode !== "formation") return;
    if (!hasActiveFormationTargets()) return;
    // 维持刷新，即便摇杆未移动也刷新 last_msg_ms，避免误判失联
    sendFormationControl(state.joystick.y, state.joystick.x);
  }, HEARTBEAT_INTERVAL_MS);
}

export function initFormation() {
  hydrateFormationDom();
  if (!domElements.fleetList || !domElements.fleetAddBtn || !domElements.fleetHostInput) {
    appendLog("[FORMATION] 页面缺少编队控件，跳过初始化");
    return;
  }

  state.formation.groupId = Number(domElements.formationGroupId.value || 1);
  state.formation.timeoutMs = Number(domElements.formationTimeout.value || 800);

  setFleetUpdateHandler(renderFleet);
  renderFleet(fleetSnapshot());
  bindEvents();
  startHeartbeat();
  setMode("solo");
  renderLocalSummary();

  // 便捷：预填当前 host，方便把本机也加入编队
  if (domElements.fleetHostInput) domElements.fleetHostInput.placeholder = location.host || "192.168.x.x";
}

export function sendFormationControl(linear, yaw) {
  broadcastFormationControl({
    linear,
    yaw,
    enable: true,
    groupId: state.formation.groupId,
    timeoutMs: state.formation.timeoutMs,
  });
}

function showInvite({ groupId, name, count }) {
  const modal = domElements.groupInviteModal;
  if (!modal) return;
  if (domElements.groupInviteTitle)
    domElements.groupInviteTitle.textContent = name ? `加入「${name}」?` : `加入编队 #${groupId}?`;
  if (domElements.groupInviteDesc)
    domElements.groupInviteDesc.textContent = `收到主车的编队指令：编队 #${groupId}${name ? ` · ${name}` : ""}，成员数 ${count ?? "?"}。是否加入？`;
  modal.classList.remove("hidden");
}

function hideInvite() {
  const modal = domElements.groupInviteModal;
  if (!modal) return;
  modal.classList.add("hidden");
}

function renderLocalSummary() {
  const wrap = domElements.formationSummary;
  if (!wrap) return;
  const nameEl = domElements.formationSummaryName;
  const detailEl = domElements.formationSummaryDetail;
  const name = state.formation.groupName || `编队 #${state.formation.groupId}`;
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

  if (!group.enabled) {
    hideInvite();
    pendingInvite = null;
  }

  if (domElements.formationGroupId)
    domElements.formationGroupId.value = state.formation.groupId;
  if (domElements.formationTimeout)
    domElements.formationTimeout.value = state.formation.timeoutMs;
  if (domElements.formationGroupName)
    domElements.formationGroupName.value = state.formation.groupName;
  if (domElements.carGroupSwitch)
    domElements.carGroupSwitch.checked = !!state.formation.joined;

  renderLocalSummary();

  const shouldPrompt =
    group.enabled &&
    group.role !== "leader" &&
    state.mode === "solo" &&
    (!pendingInvite || pendingInvite.groupId !== group.group_id);

  if (shouldPrompt) {
    pendingInvite = { groupId: group.group_id, name: group.name, count: group.count };
    showInvite(pendingInvite);
  }
}
