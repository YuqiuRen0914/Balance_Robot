// /assets/js/modules/pid.js
import { state, domElements, PID_KEYS } from "../config.js";
import { sendWebSocketMessage } from "../services/websocket.js";

// Helper to query PID related elements by key
const getPidElements = (k) => ({
  sv: document.getElementById(`sv_${k}`),
  nv: document.getElementById(`nv_${k}`),
  rg: document.getElementById(`rg_${k}`),
});

const clampByLimit = (k, value) => {
  if (!Number.isFinite(value)) return 0;
  const lim = state.pidLimits?.[k];
  if (!lim) return value;
  if (value < lim.min) return lim.min;
  if (value > lim.max) return lim.max;
  return value;
};

/**
 * Binds events for a single PID key (slider and number input)
 * @param {object} keyInfo - { k: 'key01', fix: 3 }
 */
function bindPidKey(keyInfo) {
  const { k, fix } = keyInfo;
  const { sv, nv, rg } = getPidElements(k);

  if (!nv || !rg) return;

  // Apply min/max limits to UI controls
  const lim = state.pidLimits?.[k];
  if (lim) {
    rg.min = lim.min;
    rg.max = lim.max;
    nv.min = lim.min;
    nv.max = lim.max;
  }

  // Initialize state with value from the number input (clamped)
  const initialValue = clampByLimit(k, parseFloat(nv.value || "0"));
  state.pidParams[k] = initialValue;
  nv.value = initialValue.toFixed(fix);
  if (sv) sv.textContent = initialValue.toFixed(fix);

  // Sync slider's position from a given value, clamping it within min/max
  const updateSliderPosition = (value) => {
    const clampedValue = clampByLimit(k, value);
    rg.value = String(clampedValue);
  };

  updateSliderPosition(initialValue);

  // --- Event Listeners ---

  // Slider changes -> update number input and state
  rg.addEventListener("input", () => {
    const value = clampByLimit(k, parseFloat(rg.value));
    rg.value = String(value);
    nv.value = value.toFixed(fix);
    if (sv) sv.textContent = value.toFixed(fix);
    state.pidParams[k] = value;
  });

  // Number input changes -> update display, state, and slider position
  nv.addEventListener("input", () => {
    const value = clampByLimit(k, parseFloat(nv.value));
    if (!Number.isFinite(value)) return;
    nv.value = value.toFixed(fix);
    if (sv) sv.textContent = value.toFixed(fix);
    updateSliderPosition(value);
    state.pidParams[k] = value;
  });

  // Format and commit number input on change, blur, or Enter key
  const commitNumberInput = () => {
    const value = clampByLimit(k, parseFloat(nv.value));
    nv.value = Number.isFinite(value) ? value.toFixed(fix) : "0.000";
    rg.value = String(value);
    // Manually trigger the 'input' event on the slider to sync everything (state + sv)
    rg.dispatchEvent(new Event("input"));
  };

  nv.addEventListener("change", commitNumberInput);
  nv.addEventListener("blur", commitNumberInput);
  nv.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      commitNumberInput();
      e.preventDefault(); // Prevent form submission if any
    }
  });
}

/**
 * Initializes all PID control interactions.
 */
export function initPID() {
  PID_KEYS.forEach(bindPidKey);
  domElements.btnPidSend.onclick = () => {
    const param = {};
    PID_KEYS.forEach(({ k }) => {
      const v = state.pidParams[k];
      const num = Number.isFinite(v) ? v : 0;
      param[k] = num;
    });
    sendWebSocketMessage({ type: "set_pid", param });
  };
  domElements.btnPidPull.onclick = () =>
    sendWebSocketMessage({ type: "get_pid" });
}

/**
 * Updates the UI with PID parameters received from the backend.
 * @param {object} params - Object with PID key-value pairs.
 */
export function fillPidToUI(params) {
  Object.keys(params).forEach((k) => {
    const keyInfo = PID_KEYS.find((item) => item.k === k);
    if (!keyInfo) return; // Ignore unknown keys

    const v = clampByLimit(k, +params[k]);
    if (!Number.isFinite(v)) return;

    const { sv, nv, rg } = getPidElements(k);

    if (sv) sv.textContent = v.toFixed(keyInfo.fix);
    if (nv) nv.value = v.toFixed(keyInfo.fix);
    if (rg) rg.value = String(v);

    state.pidParams[k] = v;
  });
}

/**
 * 应用 slider group names 和 labels from a configuration object.
 * @param {Array<object>} config
 */
export function applySliderConfig(config) {
  if (!Array.isArray(config)) return;
  config.forEach((groupConfig, groupIndex) => {
    // 增加对 pidGroup5 的支持
    const titleEl = document.querySelector(`#pidGroup${groupIndex + 1}`);
    if (titleEl && groupConfig.group) titleEl.textContent = groupConfig.group;

    if (Array.isArray(groupConfig.names)) {
      groupConfig.names.forEach((name, sliderIndex) => {
        const labelEl = document.querySelector(
          `#pid${groupIndex + 1}Label${sliderIndex + 1}`
        );
        if (labelEl) labelEl.textContent = name;
      });
    }
  });
}

/**
 * 应用从后端下发的 PID 上下限，并更新控件属性
 * @param {Array<{min:number,max:number}>} limitsArr
 */
export function applyPidLimits(limitsArr) {
  if (!Array.isArray(limitsArr) || limitsArr.length !== PID_KEYS.length) return;
  PID_KEYS.forEach(({ k }, idx) => {
    const lim = limitsArr[idx];
    if (!lim || !Number.isFinite(lim.min) || !Number.isFinite(lim.max)) return;
    state.pidLimits[k] = { min: lim.min, max: lim.max };

    state.pidParams[k] = clampByLimit(k, state.pidParams[k]);

    const { nv, rg, sv } = getPidElements(k);
    const keyInfo = PID_KEYS[idx];
    if (rg) {
      rg.min = lim.min;
      rg.max = lim.max;
      const v = clampByLimit(k, parseFloat(rg.value));
      rg.value = String(v);
      if (sv && keyInfo) sv.textContent = v.toFixed(keyInfo.fix);
    }
    if (nv) {
      nv.min = lim.min;
      nv.max = lim.max;
      const v = clampByLimit(k, parseFloat(nv.value));
      nv.value = Number.isFinite(v) ? v.toFixed(3) : nv.value;
      if (sv && keyInfo) sv.textContent = clampByLimit(k, parseFloat(nv.value)).toFixed(keyInfo.fix);
    }
  });
}
