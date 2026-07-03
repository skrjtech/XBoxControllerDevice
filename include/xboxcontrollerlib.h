#ifndef __XBOXCONTROLLERLIB_H__
#define __XBOXCONTROLLERLIB_H__

struct xboxinfo {
    struct {
        int id;
        char name[256];
        struct libevdev *dev;
    } device;
    struct {
        struct {
            int Y;      // 308
            int B;      // 305
            int A;      // 304
            int X;      // 307
            int VIEW;   // 158
            int MENU;   // 315
            int XBOX;   // 172
            int LB;     // 310
            int RB;     // 311
        } BUTTON;
        struct {
            int LT;   // 10
            int RT;   // 9
        } BACKBUTTON;
        struct {
            int UP;     // 17 -1 ~ 0
            int RIGHT;  // 16  0 ~ 1
            int LEFT;   // 16 -1 ~ 0
            int DOWN;   // 17  0 ~ 1
        } PAD;
        struct {
            int X;    // 0
            int Y;    // 1
        } JOYSTICKLEFT;
        struct {
            int X;    // 2
            int Y;    // 5
        } JOYSTICKRIGHT;
    } controller;
};

// シリアルポート(Picoへのリンク)のデバイスパスを設定する。
// XBoxControllerRun() の前に呼ぶこと。NULL または未設定の場合、Run() は
// 既定で "/dev/ttyACM0"、それが無ければ "/dev/ttyACM1" を試す。
// path は内部にコピーされる。
void XBoxControllerSetSerial(const char *path);

int XBoxControllerConnect(void);
int XBoxControllerReConnect(void);
int XBoxControllerDisConnect(void);

int XBoxControllerRun(void);
int XBoxControllerGetStructInfo(struct xboxinfo *info);
void XBoxControllerStructInfoPrint(void);

#endif // __XBOXCONTROLLERLIB_H__
