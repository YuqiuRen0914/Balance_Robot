// /assets/js/main.js
import { state, domElements } from "./config.js";
import { initUI, updateFallIndicator, logLine } from "./ui.js";
import { initCharts, feedChartsData, applyChartConfig } from "./modules/charts.js";
import { init3D, setAttitude } from "./modules/robot3D.js";
import { initPID, fillPidToUI, applySliderConfig, applyPidLimits } from "./modules/pid.js";
import { initJoystick } from "./modules/joystick.js";
import {
  initRgbControls,
  applyRgbConfig,
  updateRgbState,
} from "./modules/rgb.js";
<<<<<<< Updated upstream
import { initFormation, handleGroupState } from "./modules/group.js";
=======
import { initFormation, handleGroupState, handleDiscovery } from "./modules/group.js";
import { initWifiSettings, applyWifiStateFromHttp } from "./modules/wifi.js";
>>>>>>> Stashed changes
import { connectWebSocket, syncInitialState } from "./services/websocket.js";

/**
 * 主初始化函数
 */
async function main() {
  logLine('initializing...');
  
  // 初始化各个模块
  initUI();
  initCharts();
  initPID();
  initRgbControls();
  initFormation();
  initJoystick();
  init3D();

  // 启动时将指示灯置为初始状态
  updateFallIndicator(null);
  
  // 从后端同步初始状态 (HTTP)
  await syncInitialState();
  
  // 建立 WebSocket 连接
  connectWebSocket({
    onTelemetry: (msg) => {
      if (typeof msg.self_mac === "string") {
        state.selfMac = msg.self_mac.toUpperCase();
      }
      // 姿态更新
      if (typeof msg.pitch !== 'undefined') {
        setAttitude(msg.pitch, msg.roll, msg.yaw);
      }
      
      // 图表和指示灯更新 (仅在图表开启时)
      if (state.chartsOn) {
        if (Array.isArray(msg.d) && msg.d.length >= 9) {
          feedChartsData(...msg.d);
        }
        if (typeof msg.fallen !== 'undefined') {
          updateFallIndicator(msg.fallen);
        }
      }
      if (Array.isArray(msg.peers)) {
        handleDiscovery(msg.peers);
      }
    },
    onUiConfig: (msg) => {
      if (msg.charts) applyChartConfig(msg.charts);
      if (msg.sliders) applySliderConfig(msg.sliders);
      if (msg.rgb) applyRgbConfig(msg.rgb);
      if (msg.pid_limits) applyPidLimits(msg.pid_limits);
    },
    onPidParams: (msg) => {
      if (msg.param) fillPidToUI(msg.param);
    },
    onRgbState: (msg) => updateRgbState(msg, true),
    onGroupState: (msg) => handleGroupState(msg),
  });

  logLine('ready');
}

// DOM加载完毕后执行主函数
window.addEventListener('DOMContentLoaded', main);
