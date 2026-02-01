#include "my_motion.h"
#include "my_mpu6050.h"
#include "my_motor.h"
#include "my_control.h"
#include "my_car_group.h"
#include "my_group_link.h"
#include "my_tool.h"
#include "my_pid_limits.h"

robot_state robot = {
    // 状态指示位
    .dt_ms = 2,                // 运动控制频率
    .data_ms = 100,            // 网页推送频率
    .run = false,              // 运行指示位
    .car_group_manual = false, // 手动车组模式标志（保留接口）
    .car_group_mode = false,   // 车组模式标志
    .chart_enable = false,     // 图表推送位
    .joy_stop_control = false, // 原地停车标志
    .wel_up = false,           // 轮部离地标志
    .pitch_zero = -5.5,       // pitch零点
    .group_cfg = {
        .enabled = false,
        .group_number = 1,
        .group_count = 1,
        .member_index = 0,
        .role = group_role::leader,
        .target_linear = 0.0f,
        .target_yaw = 0.0f,
        .applied_linear = 0.0f,
        .applied_yaw = 0.0f,
        .last_msg_ms = 0,
        .timeout_ms = 800,
        .failsafe = false,
        .leader_mac = {0},
        .leader_mac_valid = false,
        .invite_pending = false,
        .invite_from_is_leader = false,
        .invite_group = 0,
        .invite_name = {0},
        .invite_from = {0},
        .request_pending = false,
        .request_group = 0,
        .request_from = {0},
    },         // 编队状态
    // 轮子数据
    .wel = {0, 0, 0, 0},                                 // 轮子数据 wel1 , wel2, pos1, pos2
    .motor = {
        .base_duty = 0.0f,
        .yaw_duty = 0.0f,
        .L_duty = 0.0f,
        .R_duty = 0.0f,
        .L_cmd = 0.0f,
        .R_cmd = 0.0f,
        .L_deadzone_fwd = 0.0f,
        .L_deadzone_rev = 0.0f,
        .R_deadzone_fwd = 0.0f,
        .R_deadzone_rev = 0.0f,
    },                             
    // IMU数据 anglex, angley, anglez, gyrox, gyroy, gyroz
    .imu_zero = {0, 0, 0, 0, 0, 0},
    .imu_l = {0, 0, 0, 0, 0, 0},
    .imu = {0, 0, 0, 0, 0, 0},
    // 摇杆控制 x, y, a, r, x_coef, y_coef
    .joy = {0, 0, 0, 0, 0.1, 10.0},
    .joy_l = {0, 0, 0, 0, 0.1, 10.0},
    // 摔倒检测 is, count, enable
    .fallen = {false, 0, false},
    // 灯珠数量，模式
    .rgb = {0, 0},
    // pid状态检测 now,last,target,error,duty
    .ang = {0, 0, 0, 0, 0}, // 直立环状态
    .spd = {0, 0, 0, 0, 0}, // 速度环状态
    .pos = {0, 0, 0, 0, 0}, // 位置环状态
    .yaw = {0, 0, 0, 0, 0}, // 偏航环状态
    // pid参数设定
    .ang_pid = {0.4f, 10.0f, 0.012f, 100000, 250}, // 直立环参数
    .spd_pid = {0.003f, 0.00f, 0.00f, 100000, 5}, // 速度环参数
    .pos_pid = {0.00f, 0.00f, 0.00f, 100000, 5}, // 位置环参数
    .yaw_pid = {0.025f, 0.00f, 0.00f, 100000, 5}, // 偏航环参数：P为转向力度，D为阻尼
};

void my_motion_init()
{
    clamp_all_pid(robot);  // 确保硬编码默认值符合安全上限
    pid_state_update();    // 同步到控制器

    my_mpu6050_init();

    my_motor_init();

    my_group_init();
}

void my_motion_update()
{
    static bool last_car_group_mode = false;

    // 编队启用即进入车组模式（关闭自平衡），关闭编队恢复自平衡
    robot.car_group_mode = robot.group_cfg.enabled || robot.car_group_manual;

    my_mpu6050_update();
    // 更新robot状态数据
    robot_state_update();
    // 编队指令映射到本地摇杆
    group_tick();
    group_link_poll();

    // 模式切换时清积分，避免残余输出
    if (robot.car_group_mode != last_car_group_mode)
    {
        control_idle_reset();
        robot.joy_stop_control = false;
    }

    if (robot.car_group_mode)
    {
        // 车组模式：关闭自平衡，直接差速驱动
        const float lin = my_lim(robot.joy.y, -1.0f, 1.0f);
        const float yaw = my_lim(robot.joy.x, -1.0f, 1.0f);
        const float left = my_lim(lin + yaw, -1.0f, 1.0f);
        const float right = my_lim(lin - yaw, -1.0f, 1.0f);

        motor_left_u = left;
        motor_right_u = right;
        robot.motor.base_duty = 0.0f;
        robot.motor.yaw_duty = 0.0f;
    }
    else
    {
        // 自平衡模式：串级 PID 控制
        robot_pos_control();
        pitch_control();
        yaw_control();
        duty_add();
        pitch_zero_adapt();
    }
    // 摔倒检测
    fall_check();
    // 测试模式
    // 运行检查
    if (!robot.run)
    {
        control_idle_reset(); // 清积分并重置目标，避免停机时积分累积导致启用瞬间大力输出
        robot.motor.L_duty = 0.0f;
        robot.motor.R_duty = 0.0f;
    }
    // 电机执行
    my_motor_update();
    // 记录本帧摇杆，用于下次检测松杆/回零
    robot.joy_l = robot.joy;
    last_car_group_mode = robot.car_group_mode;
}
