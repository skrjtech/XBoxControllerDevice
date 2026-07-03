#!/usr/bin/env bash
# docker-flash.sh — 稼働中の Pico を BOOTSEL に落として Docker(picotool) で書き込む。
#   1) 稼働中ファームを 1200bps で開いて閉じる -> pico stdio_usb が BOOTSEL リセット
#   2) BOOTSEL の USB デバイス (2e8a:0003) の出現を待つ
#   3) docker compose run --rm flash (イメージ内蔵 /out/firmware.uf2 を書き込み)
#
# 前提: 先に `docker compose build`(または初回 run で自動ビルド)でイメージを作っておくこと。
#       書き込み元はイメージ内蔵 uf2 なので ./out は不要。
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="${1:-/dev/ttyACM0}"

echo "==> BOOTSEL 誘発: $PORT を 1200bps でオープン/クローズ"
if [ -e "$PORT" ]; then
  python3 - "$PORT" <<'PY' || echo "   (1200bps タッチ失敗: 既に BOOTSEL かもしれません)"
import os, sys, time, termios
port = sys.argv[1]
# CDC-ACM の baud 変更には書き込み権限が要るため O_RDWR で開く (raw fd)。
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    a = termios.tcgetattr(fd)
    # a = [iflag, oflag, cflag, lflag, ispeed, ospeed, cc]
    a[4] = termios.B1200        # ispeed
    a[5] = termios.B1200        # ospeed = 1200 -> close で pico stdio_usb が BOOTSEL リセット
    a[2] |= termios.HUPCL       # close 時に DTR を落とす (念のため)
    termios.tcsetattr(fd, termios.TCSANOW, a)
    time.sleep(0.3)
finally:
    os.close(fd)
PY
else
  echo "   $PORT が無い -> 既に BOOTSEL か、手動 BOOTSEL (ボタン押しながら USB 接続) が必要です"
fi

echo "==> BOOTSEL デバイス (USB 2e8a:0003) の出現を待機 (最大10秒)"
found=0
for _ in $(seq 1 20); do
  # picotool が実際に使うのは USB 経路。mass storage の RPI-RP2 ラベルは補助表示のみ。
  if lsusb 2>/dev/null | grep -qi "2e8a:0003"; then
    found=1
    break
  fi
  sleep 0.5
done

if [ "$found" -ne 1 ]; then
  echo "!! BOOTSEL デバイスが見つかりません。"
  echo "   Pico の BOOTSEL ボタンを押しながら USB を挿し直し、再度実行してください。"
  exit 1
fi
sleep 0.3   # USB 列挙の完了を待ってから picotool を呼ぶ

echo "==> 書き込み開始 (docker compose run --rm flash)"
cd "$HERE"
docker compose run --rm flash
echo "==> 完了 (Pico はアプリを実行中のはず)"
