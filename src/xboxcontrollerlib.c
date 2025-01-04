#include <stddef.h>
#include <malloc.h>
#include <dirent.h>    // opendir, readdir, closedir
#include <fcntl.h>     // O_RDONLY, O_NONBLOCK
#include <string.h>    // strncmp, strcmp, strncpy, strerror
#include <errno.h>     // errno
#include <unistd.h>    // open, close
#include <stdio.h>     // fprintf
#include <pthread.h>
#include <libevdev/libevdev.h>  // libevdevの関数と型

#include "xboxcontrollerlibP.h"

static struct xboxinfo *info = NULL;

static int XBoxControllerInit(void) {
    
    int ret = 0;

    info = (struct xboxinfo *)malloc(sizeof(struct xboxinfo));
    if (info == NULL) {
        fprintf(stderr, "malloc error\n");
        return -1;
    }



}

static void ContollerInfoInit(void) {
    info->controller.BUTTON.Y = 0;
    info->controller.BUTTON.B = 0;
    info->controller.BUTTON.A = 0;
    info->controller.BUTTON.X = 0;
    info->controller.BUTTON.VIEW = 0;
    info->controller.BUTTON.MENU = 0;
    info->controller.BUTTON.XBOX = 0;
    info->controller.BUTTON.LB = 0;
    info->controller.BACKBUTTON.LT = 0;
    info->controller.BUTTON.RB = 0;
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

static int FindXBoxController(void) {

    DIR *dp;
    struct dirent *entry;
    char path[256];
    int index;

    dp = opendir("/dev/input");
    if (dp == NULL) {
        fprintf(stderr, "Failed to open /dev/input: %s\n", strerror(errno));
        return -1;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            info->device.id = open(path, O_RDONLY | O_NONBLOCK);
            if (info->device.id < 0) {
                fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
                continue;
            }

            int rc = libevdev_new_from_fd(info->device.id, &info->device.dev);
            if (rc < 0) {
                fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
                close(info->device.id);
                continue;
            }

            if (strcmp(libevdev_get_name(info->device.dev), "Xbox Wireless Controller") == 0) {
                strncpy(info->device.name, path, sizeof(info->device.name));  // パスをresultにコピー
                closedir(dp);
                return 0;
            }

            libevdev_free(info->device.dev);
            close(info->device.id);
        }
    }

    closedir(dp);
    return -1;  // Xboxコントローラーが見つからなかった場合
}
