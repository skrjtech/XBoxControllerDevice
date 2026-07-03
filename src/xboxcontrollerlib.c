#include <stddef.h>
#include <stdlib.h>   // malloc, free
#include <dirent.h>   // opendir, readdir, closedir
#include <fcntl.h>    // O_RDONLY, O_NONBLOCK
#include <string.h>   // strncmp, strcmp, strstr, strncpy, memcpy, memset, strerror
#include <errno.h>    // errno
#include <unistd.h>   // open, close, usleep
#include <stdio.h>    // fprintf, printf
#include <time.h>     // clock_gettime
#include <libevdev/libevdev.h>  // libevdevの関数と型

#include "xboxcontrollerlibP.h"
#include "serial.h"

// ---- evdev コード定数 (検証済み) ----
#define XBOX_KEY_A     304
#define XBOX_KEY_B     305
#define XBOX_KEY_X     307
#define XBOX_KEY_Y     308
#define XBOX_KEY_VIEW  158
#define XBOX_KEY_MENU  315
#define XBOX_KEY_XBOX  172
#define XBOX_KEY_LB    310
#define XBOX_KEY_RB    311

#define XBOX_ABS_HAT_X 16   // -1 左 / +1 右
#define XBOX_ABS_HAT_Y 17   // -1 上 / +1 下

#define XBOX_HAT_UP    (-1)
#define XBOX_HAT_DOWN  ( 1)
#define XBOX_HAT_LEFT  (-1)
#define XBOX_HAT_RIGHT ( 1)

// 制御ループのパラメータ
#define HEARTBEAT_MS   100   // 状態変化がなくても ~100ms 毎に再送
#define POLL_SLEEP_US  5000  // 5ms ポーリング

static struct xboxinfo *info = NULL;

// シリアルデバイスパス (XBoxControllerSetSerial で設定)。空文字列なら既定を使う。
static char serial_path[256] = "";

// ---------------------------------------------------------------------------
// 内部: コントローラ構造体の初期化
// ---------------------------------------------------------------------------
static void ContollerInfoInit(void) {
    info->controller.BUTTON.Y = 0;
    info->controller.BUTTON.B = 0;
    info->controller.BUTTON.A = 0;
    info->controller.BUTTON.X = 0;
    info->controller.BUTTON.VIEW = 0;
    info->controller.BUTTON.MENU = 0;
    info->controller.BUTTON.XBOX = 0;
    info->controller.BUTTON.LB = 0;
    info->controller.BUTTON.RB = 0;
    info->controller.BACKBUTTON.LT = 0;
    info->controller.BACKBUTTON.RT = 0;
    info->controller.PAD.UP = 0;
    info->controller.PAD.RIGHT = 0;
    info->controller.PAD.LEFT = 0;
    info->controller.PAD.DOWN = 0;
    info->controller.JOYSTICKLEFT.X = 0;
    info->controller.JOYSTICKLEFT.Y = 0;
    info->controller.JOYSTICKRIGHT.X = 0;
    info->controller.JOYSTICKRIGHT.Y = 0;
}

// ---------------------------------------------------------------------------
// 内部: Xboxコントローラを /dev/input/event* から探す
//   完全一致 "Xbox Wireless Controller" を優先し、無ければ名前に "Xbox" を
//   含むデバイスも受け入れる。マッチしない fd/dev はきちんと解放する。
// ---------------------------------------------------------------------------
static int FindXBoxController(void) {
    DIR *dp;
    struct dirent *entry;
    char path[300];  // "/dev/input/" + d_name。切り詰め警告を避けるため余裕を持つ。

    dp = opendir("/dev/input");
    if (dp == NULL) {
        fprintf(stderr, "Failed to open /dev/input: %s\n", strerror(errno));
        return -1;
    }

    int found_fd = -1;
    struct libevdev *found_dev = NULL;
    char found_path[256] = "";
    int exact = 0;  // 完全一致を掴んでいるか

    while ((entry = readdir(dp)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            // 権限や一時的な理由で開けないデバイスはスキップ
            continue;
        }

        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }

        const char *name = libevdev_get_name(dev);
        if (name != NULL) {
            int is_exact = (strcmp(name, "Xbox Wireless Controller") == 0);
            int is_match = is_exact || (strstr(name, "Xbox") != NULL);

            if (is_match && (found_dev == NULL || (is_exact && !exact))) {
                // より良い候補が見つかった。以前の候補があれば解放する。
                if (found_dev != NULL) {
                    libevdev_free(found_dev);
                    close(found_fd);
                }
                found_fd = fd;
                found_dev = dev;
                strncpy(found_path, path, sizeof(found_path) - 1);
                found_path[sizeof(found_path) - 1] = '\0';
                exact = is_exact;

                if (is_exact) {
                    // 完全一致が最優先。これ以上探さない。
                    break;
                }
                // 部分一致は保持したまま、完全一致を探し続ける。
                continue;
            }
        }

        // この候補は不要。リークしないよう解放する。
        libevdev_free(dev);
        close(fd);
    }

    closedir(dp);

    if (found_dev == NULL) {
        return -1;  // Xboxコントローラーが見つからなかった
    }

    info->device.id = found_fd;
    info->device.dev = found_dev;
    strncpy(info->device.name, found_path, sizeof(info->device.name) - 1);
    info->device.name[sizeof(info->device.name) - 1] = '\0';
    return 0;
}

// ---------------------------------------------------------------------------
// 内部: 初期化
// ---------------------------------------------------------------------------
static int XBoxControllerInit(void) {
    info = (struct xboxinfo *)malloc(sizeof(struct xboxinfo));
    if (info == NULL) {
        fprintf(stderr, "malloc error\n");
        return -1;
    }
    memset(info, 0, sizeof(struct xboxinfo));
    info->device.id = -1;
    info->device.dev = NULL;

    ContollerInfoInit();

    if (FindXBoxController() != 0) {
        fprintf(stderr, "Xbox controller not found\n");
        free(info);
        info = NULL;
        return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// 内部: 破棄 (NULL安全)
// ---------------------------------------------------------------------------
static int XBoxControllerDestroy(void) {
    if (info == NULL) {
        return 0;
    }
    if (info->device.dev != NULL) {
        libevdev_free(info->device.dev);
        info->device.dev = NULL;
    }
    if (info->device.id >= 0) {
        close(info->device.id);
        info->device.id = -1;
    }
    free(info);
    info = NULL;
    return 0;
}

// ---------------------------------------------------------------------------
// 内部: 1つの evdev イベントを状態へ反映する
// ---------------------------------------------------------------------------
static void ApplyEvent(const struct input_event *ev) {
    if (ev->type == EV_KEY) {
        int v = (ev->value != 0) ? 1 : 0;  // 1=押下(またはリピート), 0=離す
        switch (ev->code) {
            case XBOX_KEY_A:    info->controller.BUTTON.A = v;    break;
            case XBOX_KEY_B:    info->controller.BUTTON.B = v;    break;
            case XBOX_KEY_X:    info->controller.BUTTON.X = v;    break;
            case XBOX_KEY_Y:    info->controller.BUTTON.Y = v;    break;
            case XBOX_KEY_VIEW: info->controller.BUTTON.VIEW = v; break;
            case XBOX_KEY_MENU: info->controller.BUTTON.MENU = v; break;
            case XBOX_KEY_XBOX: info->controller.BUTTON.XBOX = v; break;
            case XBOX_KEY_LB:   info->controller.BUTTON.LB = v;   break;
            case XBOX_KEY_RB:   info->controller.BUTTON.RB = v;   break;
            default: break;
        }
    } else if (ev->type == EV_ABS) {
        switch (ev->code) {
            case XBOX_ABS_HAT_X:  // D-Pad 左右
                info->controller.PAD.LEFT  = (ev->value == XBOX_HAT_LEFT)  ? 1 : 0;
                info->controller.PAD.RIGHT = (ev->value == XBOX_HAT_RIGHT) ? 1 : 0;
                break;
            case XBOX_ABS_HAT_Y:  // D-Pad 上下
                info->controller.PAD.UP   = (ev->value == XBOX_HAT_UP)   ? 1 : 0;
                info->controller.PAD.DOWN = (ev->value == XBOX_HAT_DOWN) ? 1 : 0;
                break;
            case ABS_X:  info->controller.JOYSTICKLEFT.X  = ev->value; break;
            case ABS_Y:  info->controller.JOYSTICKLEFT.Y  = ev->value; break;
            case ABS_RX: info->controller.JOYSTICKRIGHT.X = ev->value; break;
            case ABS_RY: info->controller.JOYSTICKRIGHT.Y = ev->value; break;
            case ABS_Z:  info->controller.BACKBUTTON.LT   = ev->value; break;
            case ABS_RZ: info->controller.BACKBUTTON.RT   = ev->value; break;
            default: break;
        }
    }
}

// ---------------------------------------------------------------------------
// 内部: 溜まっている evdev イベントを全部処理して状態を更新 (non-blocking)
//   カーネルバッファのオーバーラン時は SYN_DROPPED により
//   LIBEVDEV_READ_STATUS_SYNC が返るので、SYNC フラグで最新状態へ再同期する。
//   これを怠るとボタン離しや D-Pad の中立復帰イベントを取りこぼし、
//   ロボットが誤った状態で走り続けることがある。
// ---------------------------------------------------------------------------
static void XBoxControllerHandleEvent(void) {
    if (info == NULL || info->device.dev == NULL) {
        return;
    }

    struct input_event ev;
    int rc;

    // SUCCESS または SYNC の間はループ、-EAGAIN (これ以上なし) 等で終了する。
    while ((rc = libevdev_next_event(info->device.dev,
                                     LIBEVDEV_READ_FLAG_NORMAL, &ev))
           == LIBEVDEV_READ_STATUS_SUCCESS
           || rc == LIBEVDEV_READ_STATUS_SYNC) {

        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // ドロップ発生。SYNC フラグで再同期イベントを最後まで適用する。
            while (libevdev_next_event(info->device.dev,
                                       LIBEVDEV_READ_FLAG_SYNC, &ev)
                   == LIBEVDEV_READ_STATUS_SYNC) {
                ApplyEvent(&ev);
            }
            continue;
        }

        ApplyEvent(&ev);
    }
}

// ---------------------------------------------------------------------------
// 内部: 現在の状態を1つのコマンド文字に変換する
//   縦優先: UP=>'F', DOWN=>'B'; 次に LEFT=>'L', RIGHT=>'R';
//   D-Pad が中立なら LB=>'Q', RB=>'E'; それ以外は 'S'。
// ---------------------------------------------------------------------------
static char XBoxControllerDeriveCommand(void) {
    if (info == NULL) {
        return 'S';
    }
    // 縦軸を最優先
    if (info->controller.PAD.UP)   return 'F';
    if (info->controller.PAD.DOWN) return 'B';
    // 次に横軸 (平行移動)
    if (info->controller.PAD.LEFT)  return 'L';
    if (info->controller.PAD.RIGHT) return 'R';
    // D-Pad が中立のときだけバンパー(回転)
    if (info->controller.BUTTON.LB) return 'Q';
    if (info->controller.BUTTON.RB) return 'E';
    return 'S';
}

// ---------------------------------------------------------------------------
// 公開: シリアルパス設定
// ---------------------------------------------------------------------------
void XBoxControllerSetSerial(const char *path) {
    if (path == NULL) {
        serial_path[0] = '\0';
        return;
    }
    strncpy(serial_path, path, sizeof(serial_path) - 1);
    serial_path[sizeof(serial_path) - 1] = '\0';
}

// ---------------------------------------------------------------------------
// 公開: 接続 / 再接続 / 切断
// ---------------------------------------------------------------------------
int XBoxControllerConnect(void) {
    return XBoxControllerInit();
}

int XBoxControllerReConnect(void) {
    XBoxControllerDestroy();
    return XBoxControllerInit();
}

int XBoxControllerDisConnect(void) {
    return XBoxControllerDestroy();
}

// ---------------------------------------------------------------------------
// 公開: 構造体情報の取得 / 表示
// ---------------------------------------------------------------------------
int XBoxControllerGetStructInfo(struct xboxinfo *out) {
    if (info == NULL || out == NULL) {
        return -1;
    }
    memcpy(out, info, sizeof(struct xboxinfo));
    return 0;
}

void XBoxControllerStructInfoPrint(void) {
    if (info == NULL) {
        printf("XBox controller: (not connected)\n");
        return;
    }
    printf("==== XBox Controller State ====\n");
    printf("device: %s (fd=%d)\n", info->device.name, info->device.id);
    printf("BUTTON: A=%d B=%d X=%d Y=%d VIEW=%d MENU=%d XBOX=%d LB=%d RB=%d\n",
           info->controller.BUTTON.A, info->controller.BUTTON.B,
           info->controller.BUTTON.X, info->controller.BUTTON.Y,
           info->controller.BUTTON.VIEW, info->controller.BUTTON.MENU,
           info->controller.BUTTON.XBOX, info->controller.BUTTON.LB,
           info->controller.BUTTON.RB);
    printf("BACK:   LT=%d RT=%d\n",
           info->controller.BACKBUTTON.LT, info->controller.BACKBUTTON.RT);
    printf("PAD:    UP=%d DOWN=%d LEFT=%d RIGHT=%d\n",
           info->controller.PAD.UP, info->controller.PAD.DOWN,
           info->controller.PAD.LEFT, info->controller.PAD.RIGHT);
    printf("STICK L: X=%d Y=%d   R: X=%d Y=%d\n",
           info->controller.JOYSTICKLEFT.X, info->controller.JOYSTICKLEFT.Y,
           info->controller.JOYSTICKRIGHT.X, info->controller.JOYSTICKRIGHT.Y);
    printf("===============================\n");
}

// ---------------------------------------------------------------------------
// 内部: 経過ミリ秒
// ---------------------------------------------------------------------------
static long elapsed_ms(const struct timespec *from, const struct timespec *to) {
    return (to->tv_sec - from->tv_sec) * 1000L
         + (to->tv_nsec - from->tv_nsec) / 1000000L;
}

// ---------------------------------------------------------------------------
// 公開: 制御ループ
//   毎周: イベント処理 -> コマンド算出 -> 変化 or ~100ms 経過でシリアル送信。
//   XBOX(ガイド)ボタンが押されたら 'S' を送って 0 を返す。
// ---------------------------------------------------------------------------
int XBoxControllerRun(void) {
    if (info == NULL) {
        fprintf(stderr, "XBoxControllerRun: not connected\n");
        return -1;
    }

    const char *dev = (serial_path[0] != '\0') ? serial_path : NULL;
    int sfd = SerialOpen(dev);
    if (sfd < 0) {
        fprintf(stderr, "XBoxControllerRun: failed to open serial\n");
        return -1;
    }

    char last_cmd = '\0';
    struct timespec last_send;
    clock_gettime(CLOCK_MONOTONIC, &last_send);

    for (;;) {
        XBoxControllerHandleEvent();

        // XBOX(ガイド)ボタンで終了
        if (info->controller.BUTTON.XBOX) {
            SerialWriteCmd(sfd, 'S');
            SerialClose(sfd);
            return 0;
        }

        char cmd = XBoxControllerDeriveCommand();

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (cmd != last_cmd || elapsed_ms(&last_send, &now) >= HEARTBEAT_MS) {
            SerialWriteCmd(sfd, cmd);
            last_cmd = cmd;
            last_send = now;
        }

        usleep(POLL_SLEEP_US);
    }
}
