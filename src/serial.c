#include <fcntl.h>      // open, O_RDWR, O_NOCTTY, O_NONBLOCK
#include <unistd.h>     // close, write
#include <termios.h>    // termios, cfmakeraw, cfsetispeed, tcsetattr
#include <string.h>     // memset, strerror
#include <errno.h>      // errno
#include <stdio.h>      // fprintf

#include "serial.h"

// 1つのデバイスパスを開いて termios を設定する内部ヘルパー。
static int serial_open_one(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "tcgetattr(%s) error: %s\n", dev, strerror(errno));
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);                 // raw モード (8bit, パリティなし, 制御なし)

    cfsetispeed(&tty, B115200);      // 115200 baud
    cfsetospeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;          // 8N1
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= (CLOCAL | CREAD); // モデム制御を無視 + 受信有効
    tty.c_cflag &= ~CRTSCTS;         // ハードウェアフロー制御なし

    tty.c_cc[VMIN] = 0;              // non-blocking read
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "tcsetattr(%s) error: %s\n", dev, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int SerialOpen(const char *dev) {
    if (dev != NULL) {
        int fd = serial_open_one(dev);
        if (fd < 0) {
            fprintf(stderr, "Failed to open serial %s: %s\n", dev, strerror(errno));
        }
        return fd;
    }

    // 既定: /dev/ttyACM0 -> /dev/ttyACM1 の順で試す。
    int fd = serial_open_one("/dev/ttyACM0");
    if (fd >= 0) {
        return fd;
    }
    fd = serial_open_one("/dev/ttyACM1");
    if (fd >= 0) {
        return fd;
    }

    fprintf(stderr, "Failed to open serial /dev/ttyACM0 or /dev/ttyACM1\n");
    return -1;
}

int SerialWriteCmd(int fd, char cmd) {
    if (fd < 0) {
        return -1;
    }
    char buf[2];
    buf[0] = cmd;
    buf[1] = '\n';

    ssize_t total = 0;
    while (total < 2) {
        ssize_t n = write(fd, buf + total, (size_t)(2 - total));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // non-blocking で書き込み待ち。少しだけ再試行する。
                continue;
            }
            fprintf(stderr, "serial write error: %s\n", strerror(errno));
            return -1;
        }
        total += n;
    }
    return 0;
}

void SerialClose(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
