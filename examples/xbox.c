// examples/xbox.c
//
// Xbox コントローラ -> Raspberry Pi(libevdev) -> USB-CDC serial -> Pico
// (Freenove FNK0089) -> メカナム4輪 を動かすサンプル。
//
// 使い方:
//   ./bin/xbox [シリアルデバイス]
//   引数省略時は /dev/ttyACM0 (無ければ /dev/ttyACM1) を使用する。
//
// 操作:
//   D-Pad 上/下   -> 前進 / 後退
//   D-Pad 左/右   -> 左ストレイフ / 右ストレイフ (平行移動、回転なし)
//   LB / RB       -> その場で左旋回 / 右旋回
//   何も押さない  -> 停止
//   Xbox ボタン   -> 終了

#include <stdio.h>
#include <xboxcontrollerlib.h>

int main(int argc, char *argv[]) {

    // argv[1] があればそれをシリアルデバイスとして使う。
    // 無ければ NULL を渡し、ライブラリ側の既定 (/dev/ttyACM0 -> /dev/ttyACM1) に任せる。
    const char *serial = (argc > 1) ? argv[1] : NULL;
    XBoxControllerSetSerial(serial);

    if (XBoxControllerConnect() != 0) {
        fprintf(stderr,
            "Xbox controller not found -- pair it and check /dev/input/event*\n"
            "  1. Bluetooth でコントローラをペアリングしたか確認してください。\n"
            "  2. `ls /dev/input/event*` にデバイスが現れるか確認してください。\n"
            "  3. Pico の USB が Pi につながり %s が存在するか確認してください。\n",
            (serial != NULL) ? serial : "/dev/ttyACM0");
        return 1;
    }

    // 操作説明バナー
    printf("========================================\n");
    printf(" XBoxControllerDevice -- Mecanum Car\n");
    printf("========================================\n");
    printf(" D-Pad 上/下  : 前後\n");
    printf(" D-Pad 左/右  : 平行移動 (ストレイフ)\n");
    printf(" LB / RB      : 旋回 (左 / 右)\n");
    printf(" Xbox ボタン  : 終了\n");
    printf("----------------------------------------\n");
    printf(" 接続情報:\n");
    XBoxControllerStructInfoPrint();
    printf("========================================\n");
    fflush(stdout);

    // メインループ (Xbox ボタンで抜ける)
    int rc = XBoxControllerRun();

    XBoxControllerDisConnect();

    return (rc == 0) ? 0 : 1;
}
