#pragma once

// 板载单颗 RGB 状态指示灯控制
void my_boardRGB_init();
void my_boardRGB_update();

// 触发一次外设缺失告警：红色 1Hz 闪烁 5 次后熄灭
void my_boardRGB_notify_peripheral_missing();
