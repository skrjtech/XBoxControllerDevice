// Example file - Public Domain
// Need help? https://tinyurl.com/bluepad32-help

#include <stddef.h>
#include <string.h>

#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <uni.h>

#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "sdkconfig.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t* d);

// ============================================================================
// Freenove FNK0089 メカナムモータ駆動 (Pico SDK / hardware_pwm)
//   検証済み: ピン極性(前進=GP18/21/7/9), メカナム符号行列, DRIVE_SPEED=60
//   H-bridge: 前進ピンにPWM & 後退ピン0 => 前転 / 逆 => 後転。
//   前進ピンはペアの若い番号とは限らない(FL以外は上位ピンが前進)。
// ============================================================================
#define FL_FWD 18u
#define FL_REV 19u
#define BL_FWD 21u
#define BL_REV 20u
#define FR_FWD 7u
#define FR_REV 6u
#define BR_FWD 9u
#define BR_REV 8u

#define PWM_WRAP    1000u   // 分解能 0..1000
#define DRIVE_SPEED 60      // 0..100 固定速度

static void motor_pin_init(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    // 125MHz / 6.25 = 20MHz, wrap=1000 -> 約20kHz (可聴域外)
    pwm_set_clkdiv(slice, 6.25f);
    pwm_set_wrap(slice, PWM_WRAP - 1);
    pwm_set_gpio_level(pin, 0);
    pwm_set_enabled(slice, true);
}

static void mecanum_motors_init(void) {
    motor_pin_init(FL_FWD); motor_pin_init(FL_REV);
    motor_pin_init(BL_FWD); motor_pin_init(BL_REV);
    motor_pin_init(FR_FWD); motor_pin_init(FR_REV);
    motor_pin_init(BR_FWD); motor_pin_init(BR_REV);
}

// speed -100..100. 正=前進ピンにPWM, 負=後退ピンにPWM, 0=停止(空転)
static void set_motor(uint fwd_pin, uint rev_pin, int speed) {
    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;
    uint level = (uint)((speed < 0 ? -speed : speed) * (int)PWM_WRAP / 100);
    if (speed > 0) {
        pwm_set_gpio_level(fwd_pin, level);
        pwm_set_gpio_level(rev_pin, 0);
    } else if (speed < 0) {
        pwm_set_gpio_level(fwd_pin, 0);
        pwm_set_gpio_level(rev_pin, level);
    } else {
        pwm_set_gpio_level(fwd_pin, 0);
        pwm_set_gpio_level(rev_pin, 0);
    }
}

static void set_wheels(int fl, int fr, int bl, int br) {
    set_motor(FL_FWD, FL_REV, fl);
    set_motor(FR_FWD, FR_REV, fr);
    set_motor(BL_FWD, BL_REV, bl);
    set_motor(BR_FWD, BR_REV, br);
}

// 検証済みメカナム符号行列 × DRIVE_SPEED
static void mv_forward(void)      { set_wheels( DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED); }
static void mv_backward(void)     { set_wheels(-DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED); }
static void mv_strafe_left(void)  { set_wheels(-DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED); }
static void mv_strafe_right(void) { set_wheels( DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED); }
static void mv_rotate_left(void)  { set_wheels(-DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED); }
static void mv_rotate_right(void) { set_wheels( DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED); }
static void mv_stop(void)         { set_wheels(0, 0, 0, 0); }

// アナログスティック設定
#define STICK_MAX        512   // Bluepad32 のスティック範囲は約 -512..511
#define STICK_DEADZONE   100   // 中立ドリフト対策 (実測ドリフト ~50 に余裕)
#define ANALOG_MAX_SPEED 80    // 0..100 のうちスティック全倒し時の速度

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int deadzone(int v) { return (v > -STICK_DEADZONE && v < STICK_DEADZONE) ? 0 : v; }

// ゲームパッド状態 -> メカナム走行。
//   左スティック : 傾けた向きへ平行移動 (上=前進 / 下=後退 / 左右=ストレイフ、斜め可)
//   右スティックX: その場旋回 (右=CW / 左=CCW)
//   スティック中立時は D-Pad / バンパー(LB=左旋回,RB=右旋回) でも操作可。
//
// 検証済み離散符号行列から導いた合成式 (vx=前後, vy=右ストレイフ, w=CW旋回):
//   FL=vx+vy+w  FR=vx+vy-w  BL=vx-vy+w  BR=vx-vy-w
static void mecanum_drive_from_gamepad(const uni_gamepad_t* gp) {
    int lx = deadzone(gp->axis_x);    // 左スティックX: 右=+  -> 右ストレイフ
    int ly = deadzone(gp->axis_y);    // 左スティックY: 上=-  -> 前進
    int rx = deadzone(gp->axis_rx);   // 右スティックX: 右=+  -> 右旋回(CW)

    if (lx != 0 || ly != 0 || rx != 0) {
        int vx = ( ly) * ANALOG_MAX_SPEED / STICK_MAX;  // 前後 (実機に合わせ符号反転: 上=前進)
        int vy = ( lx) * ANALOG_MAX_SPEED / STICK_MAX;  // 左右平行移動 (右=右) ※正しいので据え置き
        int w  = (-rx) * ANALOG_MAX_SPEED / STICK_MAX;  // 旋回 (実機に合わせ符号反転)
        set_wheels(clampi(vx + vy + w, -100, 100),
                   clampi(vx + vy - w, -100, 100),
                   clampi(vx - vy + w, -100, 100),
                   clampi(vx - vy - w, -100, 100));
        return;
    }

    // フォールバック: スティック中立時は D-Pad / バンパー
    uint8_t dpad = gp->dpad;
    if      (dpad & DPAD_UP)              mv_forward();
    else if (dpad & DPAD_DOWN)            mv_backward();
    else if (dpad & DPAD_LEFT)            mv_strafe_left();
    else if (dpad & DPAD_RIGHT)           mv_strafe_right();
    else if (gp->buttons & BUTTON_SHOULDER_L) mv_rotate_left();
    else if (gp->buttons & BUTTON_SHOULDER_R) mv_rotate_right();
    else                                  mv_stop();
}

//
// Platform Overrides
//
static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("my_platform: init()\n");

    // メカナムモータPWM初期化 + 停止
    mecanum_motors_init();
    mv_stop();

#if 0
    uni_gamepad_mappings_t mappings = GAMEPAD_DEFAULT_MAPPINGS;

    // Inverted axis with inverted Y in RY.
    mappings.axis_x = UNI_GAMEPAD_MAPPINGS_AXIS_RX;
    mappings.axis_y = UNI_GAMEPAD_MAPPINGS_AXIS_RY;
    mappings.axis_ry_inverted = true;
    mappings.axis_rx = UNI_GAMEPAD_MAPPINGS_AXIS_X;
    mappings.axis_ry = UNI_GAMEPAD_MAPPINGS_AXIS_Y;

    // Invert A & B
    mappings.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_B;
    mappings.button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_A;

    uni_gamepad_set_mappings(&mappings);
#endif
}

static void my_platform_on_init_complete(void) {
    logi("my_platform: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread

    // Start scanning and autoconnect to supported controllers.
    uni_bt_start_scanning_and_autoconnect_unsafe();

    // Based on runtime condition, you can delete or list the stored BT keys.
    if (1)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();

    // Turn off LED once init is done.
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    //    uni_bt_service_set_enabled(true);

    uni_property_dump_all();
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    // You can filter discovered devices here. Return any value different from UNI_ERROR_SUCCESS;
    // @param addr: the Bluetooth address
    // @param name: could be NULL, could be zero-length, or might contain the name.
    // @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
    // @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.

    // As an example, if you want to filter out keyboards, do:
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        logi("Ignoring keyboard\n");
        return UNI_ERROR_IGNORE_DEVICE;
    }

    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("my_platform: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("my_platform: device disconnected: %p\n", d);
    // フェイルセーフ: コントローラが切れたら必ず停止する
    mv_stop();
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("my_platform: device ready: %p\n", d);

    // You can reject the connection by returning an error.
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    static uni_controller_t prev = {0};
    uni_gamepad_t* gp;

    // 変化があった時だけログを出す(毎フレームのスパムを防ぐ)。
    if (memcmp(&prev, ctl, sizeof(*ctl)) != 0) {
        prev = *ctl;
        logi("(%p) id=%d ", d, uni_hid_device_get_idx_for_instance(d));
        uni_controller_dump(ctl);
    }

    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            gp = &ctl->gamepad;
            // D-Pad/バンパー -> メカナム走行。
            //   ↑=前進 ↓=後退 ←=左ストレイフ →=右ストレイフ
            //   LB=左旋回 RB=右旋回 何も無し=停止
            mecanum_drive_from_gamepad(gp);
            break;
        case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
            // Do something
            uni_balance_board_dump(&ctl->balance_board);
            break;
        case UNI_CONTROLLER_CLASS_MOUSE:
            // Do something
            uni_mouse_dump(&ctl->mouse);
            break;
        case UNI_CONTROLLER_CLASS_KEYBOARD:
            // Do something
            uni_keyboard_dump(&ctl->keyboard);
            break;
        default:
            loge("Unsupported controller class: %d\n", ctl->klass);
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
            // Optional: do something when "system" button gets pressed.
            trigger_event_on_gamepad((uni_hid_device_t*)data);
            break;

        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            // When the "bt scanning" is on / off. Could be triggered by different events
            // Useful to notify the user
            logi("my_platform_on_oob_event: Bluetooth enabled: %d\n", (bool)(data));
            break;

        default:
            logi("my_platform_on_oob_event: unsupported event: 0x%04x\n", event);
    }
}

//
// Helpers
//
static void trigger_event_on_gamepad(uni_hid_device_t* d) {
    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 50 /* duration ms */, 128 /* weak magnitude */,
                                          40 /* strong magnitude */);
    }

    if (d->report_parser.set_player_leds != NULL) {
        static uint8_t led = 0;
        led += 1;
        led &= 0xf;
        d->report_parser.set_player_leds(d, led);
    }

    if (d->report_parser.set_lightbar_color != NULL) {
        static uint8_t red = 0x10;
        static uint8_t green = 0x20;
        static uint8_t blue = 0x40;

        red += 0x10;
        green -= 0x20;
        blue += 0x40;
        d->report_parser.set_lightbar_color(d, red, green, blue);
    }
}

//
// Entry Point
//
struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "My Platform",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}