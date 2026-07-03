#ifndef __XBOXCONTROLLERLIBP_H__
#define __XBOXCONTROLLERLIBP_H__

#include "../include/xboxcontrollerlib.h"

static int XBoxControllerInit(void);
static int XBoxControllerDestroy(void);
static void XBoxControllerHandleEvent(void);

static void ContollerInfoInit(void);
static int FindXBoxController(void);

// Run ループ内でコントローラ状態を1つのコマンド文字に変換する。
// 'F' 前進 / 'B' 後退 / 'L' 左平行移動 / 'R' 右平行移動 /
// 'Q' 左回転(CCW) / 'E' 右回転(CW) / 'S' 停止
static char XBoxControllerDeriveCommand(void);

#endif // __XBOXCONTROLLERLIBP_H__
