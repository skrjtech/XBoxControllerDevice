#ifndef __SERIAL_H__
#define __SERIAL_H__

// シリアル通信 (USB-CDC) ヘルパー
// Pico の micro-USB が Raspberry Pi につながっているので /dev/ttyACM0 が既定。
// 115200 8N1, raw, non-blocking (VMIN=0 VTIME=0)。

// dev を指定して開く。dev が NULL の場合は "/dev/ttyACM0" -> "/dev/ttyACM1" の順に試す。
// 成功で fd(>=0)、失敗で -1 を返す。
int SerialOpen(const char *dev);

// コマンド文字 cmd と '\n' を書き込む。成功で 0、失敗で -1 を返す。
int SerialWriteCmd(int fd, char cmd);

// シリアルポートを閉じる (fd < 0 は無視)。
void SerialClose(int fd);

#endif // __SERIAL_H__
