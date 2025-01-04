#ifndef __XBOXCONTROLLERLIBP_H__
#define __XBOXCONTROLLERLIBP_H__

#include "../include/xboxcontrollerlib.h"

static struct xboxinfo *info = NULL;

static int XBoxControllerInit(void);
static int XBoxControllerDestroy(void);
static void XBoxControllerHandleEvent(void);

static void ContollerInfoInit(void);
static int FindXBoxController(void);


#endif // __XBOXCONTROLLERLIBP_H__