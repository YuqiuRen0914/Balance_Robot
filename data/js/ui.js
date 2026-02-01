// /assets/js/ui.js
import { state, domElements } from "./config.js";
import { sendWebSocketMessage } from "./services/websocket.js";

/**
 * 更新状态显示文本
 * @param {string} text - The status text to display.
 */
export function setStatus(text) {
  domElements.statusLabel.textContent = `状态: ${text}`;
}

/**
 * 在日志区域打印一行新日志（覆盖旧的）
 * @param {string} text
 */
export function logLine(text) {
  domElements.log.textContent = String(text);
}

/**
 * 在日志区域追加一行日志
 * @param {string} text
 */
export function appendLog(text) {
  const el = domElements.log;
  const isScrolledToBottom =
    el.scrollTop + el.clientHeight >= el.scrollHeight - 2;
  el.textContent += `\n${text}`;
  if (isScrolledToBottom) {
    el.scrollTop = el.scrollHeight;
  }
}

/**
 * 更新摔倒状态指示灯
 * @param {boolean|null} fallState - null/undefined for off, true for fallen (red), false for stable (green).
 */
export function updateFallIndicator(fallState) {
  const { fallLamp, fallLabel } = domElements;
  fallLamp.classList.remove("green", "red", "off");

  if (fallState === null || typeof fallState === "undefined") {
    fallLamp.classList.add("off");
    fallLabel.textContent = "—";
  } else if (fallState) {
    fallLamp.classList.add("red");
    fallLabel.textContent = "已摔倒";
  } else {
    fallLamp.classList.add("green");
    fallLabel.textContent = "稳定";
  }
}

/**
 * 绑定顶部工具栏的事件监听
 */
function bindToolbarEvents() {
  domElements.btnSetRate.onclick = () => {
    const ms = Math.max(
      20,
      Math.min(1000, parseInt(domElements.rateHzInput.value || "100", 100))
    );
    sendWebSocketMessage({ type: "telem_hz", ms });
    appendLog(`[SEND] telem_hz ${ms} Hz`);
  };

  domElements.runSwitch.onchange = () => {
    sendWebSocketMessage({
      type: "robot_run",
      running: domElements.runSwitch.checked,
    });
    appendLog(
      `[SEND] robot_run ${domElements.runSwitch.checked ? "on" : "off"}`
    );
  };

  if (domElements.carGroupSwitch) {
    domElements.carGroupSwitch.onchange = () => {
      state.carGroupMode = domElements.carGroupSwitch.checked;
      // 打开时进入编队模式（沿用原编队配置），关闭时退队
      if (state.carGroupMode) {
        domElements.modeFormationBtn?.click();
      } else {
        domElements.modeSoloBtn?.click();
      }
      sendWebSocketMessage({
        type: "group_cfg",
        enable: state.carGroupMode,
        group_id: state.formation.groupId,
        timeout_ms: state.formation.timeoutMs,
        name: state.formation.groupName,
      });
      appendLog(`[SEND] car_group ${state.carGroupMode ? "enable" : "disable"} (group ${state.formation.groupId})`);
    };
  }

  domElements.chartSwitch.onchange = () => {
    state.chartsOn = domElements.chartSwitch.checked;
    sendWebSocketMessage({ type: "charts_send", on: state.chartsOn });
    appendLog(`[INFO] charts ${state.chartsOn ? "enabled" : "disabled"}`);
    if (!state.chartsOn) {
      updateFallIndicator(null);
    }
  };

  domElements.fallDetectSwitch.onchange = () => {
    sendWebSocketMessage({
      type: "fall_check",
      enable: domElements.fallDetectSwitch.checked,
    });
    appendLog(
      `[SEND] fall_check ${domElements.fallDetectSwitch.checked ? "on" : "off"}`
    );
  };

  domElements.btnZeroAtt.onclick = () => {
    sendWebSocketMessage({ type: "imu_restart" });
    appendLog("[INFO] attitude zeroed (roll/yaw)");
  };
}

/**
 * 初始化按钮的按压视觉效果 (PC + 触摸)
 */
function initButtonPressEffects() {
  const addPressEffect = (el) => {
    el.addEventListener("pointerdown", () => el.classList.add("is-press"));
    const removePressEffect = () => el.classList.remove("is-press");
    el.addEventListener("pointerup", removePressEffect);
    el.addEventListener("pointerleave", removePressEffect);
    el.addEventListener("pointercancel", removePressEffect);
  };

  document.querySelectorAll(".btn").forEach(addPressEffect);
}

/**
 * 初始化所有UI相关的事件绑定
 */
export function initUI() {
  bindToolbarEvents();
  initButtonPressEffects();

  // 监听页面可见性变化
  document.addEventListener("visibilitychange", () => {
    state.isPageVisible = !document.hidden;
  });
}
