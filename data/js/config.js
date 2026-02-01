// /assets/js/config.js

/**
 * 全局状态管理
 */
export const state = {
  connected: false,
  chartsOn: false,
  carGroupMode: false,
  mode: "solo",
  selfMac: "",
  formation: {
    groupId: 1,
    timeoutMs: 800,
    groupName: "",
    joined: false,
    memberCount: 1,
    memberIndex: 0,
    requestPending: false,
    requestFrom: "",
    requestFromIp: "",
    requestGroup: 0,
    requestName: "",
    leaderIp: "",
  },
  peers: [],
  fleet: [],
  isPageVisible: true,
  joystick: { x: 0, y: 0, a: 0, isDragging: false, lastSendTime: 0 },
  pidParams: {},
  attitudeZero: { roll: 0, yaw: 0 },
  rgb: { mode: 0, count: 5, max: 12 },
<<<<<<< Updated upstream
=======
  wifi: { ssid: "", password: "", open: false, ip: "", staSsid: "", staIp: "", staConnected: false },
  battery: { voltage: 0, percent: 0 },
  pidLimits: {}, // 运行时从后端获取的 PID 上下限
>>>>>>> Stashed changes
  charts: {
    chart1: null,
    chart2: null,
    chart3: null,
  },
  three: {
    scene: null,
    camera: null,
    renderer: null,
    robot: null,
    pcb: null,
  },
};

// 2. DOM 元素缓存
function getElement(id) {
  const element = document.getElementById(id);
  if (!element) {
    // 允许在测试环境中某些元素不存在
    console.warn(`Element with ID '${id}' not found.`);
    return null;
  }
  return element;
}
/**
 * DOM 元素引用
 * 统一在此处获取，方便管理和维护
 */
export const domElements = {
  // Toolbar
  btnSetRate: getElement("btnSetRate"),
  rateHzInput: getElement("rateHz"),
  runSwitch: getElement("runSwitch"),
  carGroupSwitch: getElement("carGroupSwitch"),
  chartSwitch: getElement("chartSwitch"),
  fallDetectSwitch: getElement("fallDetectSwitch"),
  statusLabel: getElement("status"),

  // Indicators
  fallLamp: getElement("fallLamp"),
  fallLabel: getElement("fallLabel"),

  // Charts
  chart1: {
    canvas: getElement("chart1"),
    title: getElement("chartTitle1"),
  },
  chart2: {
    canvas: getElement("chart2"),
    title: getElement("chartTitle2"),
  },
  chart3: {
    canvas: getElement("chart3"),
    title: getElement("chartTitle3"),
  },

  // PID Controls
  pidCard: getElement("pidCard"),
  btnPidSend: getElement("btnPidSend"),
  btnPidPull: getElement("btnPidPull"),

  // 3D View
  robotCanvas: getElement("robotCanvas"),
  btnZeroAtt: getElement("btnZeroAtt"),
  attOut: getElement("attOut"),
  // Joystick
  joystick: getElement("joystick"),
  stick: getElement("stick"),
  joyOut: getElement("joyOut"),

  // Log
  log: getElement("log"),

  // RGB
  rgbCard: getElement("rgbCard"),
  rgbModeList: getElement("rgbModeList"),
  rgbModeDesc: getElement("rgbModeDesc"),
  rgbModeTag: getElement("rgbModeTag"),
  rgbCountInput: getElement("rgbCountInput"),
  rgbCountRange: getElement("rgbCountRange"),
  rgbCountValue: getElement("rgbCountValue"),

  // Formation
  fleetHostInput: getElement("fleetHostInput"),
  fleetAddBtn: getElement("fleetAddBtn"),
  fleetCount: getElement("fleetCount"),
  fleetList: getElement("fleetList"),
  formationGroupId: getElement("formationGroupId"),
  formationTimeout: getElement("formationTimeout"),
  formationSyncBtn: getElement("formationSyncBtn"),
  formationStopBtn: getElement("formationStopBtn"),
  formationGroupName: getElement("formationGroupName"),
  formationSummary: getElement("formationSummary"),
  formationSummaryName: getElement("formationSummaryName"),
  formationSummaryDetail: getElement("formationSummaryDetail"),
  formationConfigBlock: getElement("formationConfigBlock"),
  formationFoot: getElement("formationFoot"),
  peerList: getElement("peerList"),
  peerCount: getElement("peerCount"),
  groupInviteModal: getElement("groupInviteModal"),
  groupInviteTitle: getElement("groupInviteTitle"),
  groupInviteDesc: getElement("groupInviteDesc"),
  groupInviteAccept: getElement("groupInviteAccept"),
  groupInviteReject: getElement("groupInviteReject"),
<<<<<<< Updated upstream
=======
  groupRequestModal: getElement("groupRequestModal"),
  groupRequestTitle: getElement("groupRequestTitle"),
  groupRequestDesc: getElement("groupRequestDesc"),
  groupRequestApprove: getElement("groupRequestApprove"),
  groupRequestReject: getElement("groupRequestReject"),

  // WiFi
  wifiSsidInput: getElement("wifiSsid"),
  wifiPasswordInput: getElement("wifiPassword"),
  wifiSaveBtn: getElement("wifiSaveBtn"),
  wifiHint: getElement("wifiHint"),
  wifiIp: getElement("wifiIp"),
  wifiTogglePwd: getElement("wifiTogglePwd"),
>>>>>>> Stashed changes
};

/**
 * 全局常量
 */
export const CONSTANTS = {
  MAX_CHART_POINTS: 300,
  JOYSTICK_SEND_INTERVAL: 50, // ms, 20Hz
};

// PID 参数键名列表
export const PID_KEYS = Array.from({ length: 12 }, (_, i) => {
    const key = `key${String(i + 1).padStart(2, "0")}`;
    return { k: key, fix: 3 };
});
