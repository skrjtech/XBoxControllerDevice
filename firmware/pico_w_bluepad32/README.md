# pico_w_bluepad32 — Xbox コントローラで走るメカナムカー (Pico W 自走)

Raspberry Pi Pico W 自身が Bluetooth ホストになり、**Xbox ワイヤレスコントローラと直接
BLE 接続**して Freenove FNK0089 メカナムカーを操縦するファームウェアです。
Raspberry Pi は**開発・書き込み時のみ**使用し、走行時は不要（車体バッテリーで自走）。

[Bluepad32](https://github.com/ricardoquesada/bluepad32) の `examples/pico_w`
カスタムプラットフォームをベースに、`src/my_platform.c` にモータ駆動と操作マッピングを実装しています。

## 操作方法

| 入力 | 動作 |
|------|------|
| **左スティック** | 全方向移動（上=前進 / 下=後退 / 左右=平行移動、斜め可、倒し量で速度可変） |
| **右スティック 左右** | その場旋回（左=CCW / 右=CW） |
| 左スティック＋右スティック同時 | 走りながら方向転換（カーブ） |
| D-Pad / LB・RB | スティック中立時のフォールバック（↑↓=前後, ←→=ストレイフ, LB/RB=旋回） |
| コントローラ切断 | フェイルセーフで全モータ停止 |

メカナム合成式（検証済みの離散符号行列から導出）: `FL=vx+vy+w, FR=vx+vy-w, BL=vx-vy+w, BR=vx-vy-w`
（`vx`=前後, `vy`=右ストレイフ, `w`=CW旋回）

## モータ配線 (Freenove FNK0089)

| モータ | 位置 | 前進ピン | 後退ピン |
|--------|------|---------|---------|
| M1 FL | 前左 | GP18 | GP19 |
| M2 BL | 後左 | GP21 | GP20 |
| M3 FR | 前右 | GP7  | GP6  |
| M4 BR | 後右 | GP9  | GP8  |

※「前進ピン」はペアの若い番号とは限らない（FL 以外は上位ピンが前進）。実機検証済み。

## ビルド手順（すべて root 不要・ホーム内で完結）

前提: `cmake` `git` `ninja`/`make` `python3` は導入済みとする。

### 1. ARM GNU ツールチェーン（arm-none-eabi）
Pico SDK は公式 Arm GNU ツールチェーン前提（arduino 同梱 gcc はリンクで
`__retarget_lock_*` 未解決になるため不可）。root 不要でホームに展開:
```bash
cd /tmp
curl -fL -o armgnu.tar.xz \
  https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-aarch64-arm-none-eabi.tar.xz
mkdir -p ~/.local/opt && tar -C ~/.local/opt -xf armgnu.tar.xz
export PATH=$HOME/.local/opt/arm-gnu-toolchain-13.3.rel1-aarch64-arm-none-eabi/bin:$PATH
```
（ホストが 64bit ARM の Raspberry Pi OS の場合。x86_64 なら該当アーキのtarを使う）

### 2. Pico SDK 2.1.1
```bash
git clone --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk && git submodule update --init      # cyw43 / btstack / lwip / tinyusb / mbedtls
export PICO_SDK_PATH=$HOME/pico-sdk
```

### 3. Bluepad32
```bash
git clone --recursive https://github.com/ricardoquesada/bluepad32.git ~/bluepad32
export BLUEPAD32_ROOT=$HOME/bluepad32
```

### 4. このファームをビルド
```bash
cd firmware/pico_w_bluepad32
mkdir -p build && cd build
cmake -DPICO_BOARD=pico_w ..
make -j4
# => build/bluepad32_picow_example_app.uf2
```
`BLUEPAD32_ROOT` を設定していれば、このディレクトリはリポジトリ内のままでビルドできます。
（設定しない場合は `~/bluepad32/examples/pico_w/` にこのディレクトリの中身を置いてビルド）

## 書き込み（picotool・root 不要）

Pico W の USB ノードは `plugdev` グループなので、そのユーザなら `sudo` 不要で書けます。

```bash
PT=~/.arduino15/packages/rp2040/tools/pqt-picotool/*/picotool   # or 別途 picotool
# 既に別ファームが動いている場合は 1200bps タッチで BOOTSEL へ:
python3 - <<'EOF'
import os,termios,time
fd=os.open('/dev/ttyACM0',os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd); a[4]=a[5]=termios.B1200; a[2]|=termios.HUPCL
termios.tcsetattr(fd,termios.TCSANOW,a); time.sleep(0.3); os.close(fd)
EOF
# もしくは BOOTSEL ボタンを押しながら USB 接続
$PT load build/bluepad32_picow_example_app.uf2
$PT reboot -f
```

## ペアリング

1. Pico W をバッテリー/USB で起動（ファームが BLE スキャン開始）
2. Xbox コントローラをペアリングモードに: **Xbox ボタンで起動 → 上面のペアリングボタン長押し**
   （ロゴが速く点滅）
3. 数秒で Pico W が自動接続 → ロゴ点灯

対応確認済み: 純正 Xbox Wireless Controller（VID 0x045e / PID 0x02fd, Model 1708, fw 4.8）。
BLE 対応の Xbox コントローラが必要（古い USB 専用モデルは不可）。

> 注: `src/my_platform.c` の `on_init_complete` は起動ごとに保存済みBTキーを削除する
> （`uni_bt_del_keys_unsafe()`）ため、電源投入ごとに再ペアリングが必要。電源オフでも
> 自動再接続させたい場合は、その行を `uni_bt_list_keys_unsafe()` 側に切り替える。

## デバッグ

シリアルログを頭から見たい場合は `CMakeLists.txt` の
`PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS` のブロックを有効化し、`src/sdkconfig.h` の
`CONFIG_BLUEPAD32_LOG_LEVEL` を 3 に上げて再ビルド。DTR をアサートして
`/dev/ttyACM0` を 115200 で読む（`cat` では DTR 未アサートで出力が出ないことがある）。
