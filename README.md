# XBoxControllerDevice

Xbox ワイヤレスコントローラで **Freenove FNK0089**（Raspberry Pi Pico W 用
メカナムホイールカー）を操縦するプロジェクト。

## 2つの構成

| | A. Pico W 自走（推奨・動作確認済み） | B. Raspberry Pi 経由 |
|---|---|---|
| 接続 | Xbox コン → **Bluetooth** → Pico W → モータ | Xbox コン → Pi(libevdev) → USB-CDC → Pico → モータ |
| 走行時に Pi | **不要**（車体バッテリーで自走） | 必要（Pi が中継） |
| 操作 | 左スティックで全方向移動＋右スティックで旋回（同時可） | D-Pad で前後/ストレイフ、LB/RB で旋回 |
| 実装 | [`firmware/pico_w_bluepad32/`](firmware/pico_w_bluepad32/)（Bluepad32/C） | 下記＋[`firmware/pico_mecanum/`](firmware/pico_mecanum/)（Arduino/C） |

**A（Pico W が Xbox コントローラと直接 BLE 接続して自走）** が実機で動作確認済みの
本命構成です。ビルド・書き込み・ペアリング手順は
[`firmware/pico_w_bluepad32/README.md`](firmware/pico_w_bluepad32/README.md) を参照。

以下は **B（Raspberry Pi 経由）** の構成の説明です。

## 概要（構成 B: Raspberry Pi 経由）

Xbox コントローラ → Raspberry Pi（libevdev）→ USB-CDC シリアル →
Pico（Freenove FNK0089）→ メカナム4輪。

Raspberry Pi が Xbox Wireless Controller の入力を evdev 経由で読み取り、
1文字のコマンド（`F`/`B`/`L`/`R`/`Q`/`E`/`S`）を Pico の native USB-CDC へ
115200 8N1 で送信する。Pico 側ファームウェア（`firmware/pico_mecanum`）が
コマンドを解釈し、4つの DC モータをメカナム走行の符号どおりに駆動する。

## システム構成図

```
 +---------------------+       Bluetooth        +------------------------+
 | Xbox Wireless       |  ===================>   | Raspberry Pi           |
 | Controller          |     (evdev / HID)       |  bin/xbox              |
 +---------------------+                         |  libxboxcontrollerlib  |
                                                 |  + libevdev            |
                                                 +-----------+------------+
                                                             |
                              USB-CDC (micro-USB ケーブル)   |
                              115200 8N1  /dev/ttyACM0       |
                                                             v
 +---------------------+                         +------------------------+
 | Mecanum 4 wheels    | <====================== | Raspberry Pi Pico W     |
 | FL  FR              |     H-bridge / PWM      |  firmware/pico_mecanum  |
 | BL  BR              |                         |  (Freenove FNK0089)     |
 +---------------------+                         +------------------------+
```

## 必要環境

- Raspberry Pi 側
  - `libevdev-dev`（ビルド・実行に必須）

    ```bash
    sudo apt-get install -y libevdev-dev
    ```

  - `gcc` / `make`
  - Bluetooth でペアリング済みの Xbox Wireless Controller
- Pico 側
  - Arduino IDE
  - **arduino-pico core（earlephilhower）** … `firmware/pico_mecanum/README.md` 参照

## 配線 / 接続

- **リンク:** Pico 自身の micro-USB を Raspberry Pi に接続する（native USB-CDC）。
  Pico(Arduino) 側では `Serial`、Pi 側では `/dev/ttyACM0`（無ければ
  `/dev/ttyACM1`）として現れる。115200 baud, 8N1。
- **モータピン（Freenove FNK0089）:** 各モータは H ブリッジで、2本のうち
  「forward pin」に PWM・「reverse pin」に LOW を出すと前進する。forward pin は
  必ずしも小さい方の GPIO ではない点に注意。

  | Motor | 位置        | forward pin | reverse pin |
  |-------|-------------|-------------|-------------|
  | M1 FL | Front-Left  | GP18        | GP19        |
  | M2 BL | Back-Left   | GP21        | GP20        |
  | M3 FR | Front-Right | GP7         | GP6         |
  | M4 BR | Back-Right  | GP9         | GP8         |

## ビルド手順

### Raspberry Pi（ホスト側）

```bash
sudo apt-get install -y libevdev-dev   # 一度だけ
make                                   # bin/xbox を生成
```

### Pico（ファームウェア）

Arduino IDE で `firmware/pico_mecanum/pico_mecanum.ino` を開き、
ボードに **Raspberry Pi Pico W** を選んで書き込む。詳細な手順（arduino-pico
core のインストール等）は `firmware/pico_mecanum/README.md` を参照。

## 操作方法

```bash
make run                       # /dev/ttyACM0 -> /dev/ttyACM1 を自動で試す
make run ARGS=/dev/ttyACM1
# または直接:
./bin/xbox [シリアルデバイス]
```

| 入力            | 動作                              |
|-----------------|-----------------------------------|
| D-Pad 上        | 前進                              |
| D-Pad 下        | 後退                              |
| D-Pad 左        | 左ストレイフ（平行移動、回転なし）|
| D-Pad 右        | 右ストレイフ                      |
| LB              | 左旋回（その場で CCW）            |
| RB              | 右旋回（その場で CW）             |
| 何も押さない    | 停止                              |
| Xbox ボタン     | 終了                              |

- 上下（前後）が左右（ストレイフ）より優先される。
- 旋回（LB/RB）は D-Pad が中立のときだけ有効。
- D-Pad 制御の速度は固定で 60（0..100 のうち）。

## プロトコル

Pi → Pico: 1文字の ASCII コマンド + `\n`、115200 8N1（USB-CDC）。

| バイト | 動作                                |
|--------|-------------------------------------|
| `F`    | 前進                                |
| `B`    | 後退                                |
| `L`    | 左ストレイフ（平行移動）            |
| `R`    | 右ストレイフ                        |
| `Q`    | 左旋回 / CCW（その場旋回）          |
| `E`    | 右旋回 / CW（その場旋回）           |
| `S`    | 停止                                |

- Pi は状態が変わるたびにコマンドを送信し、さらに現在のコマンドを
  約 100 ms ごとにハートビートとして送り続ける。
- **フェイルセーフ:** 有効なコマンドが 500 ms 届かないと Pico は全モータを停止する。

## トラブルシュート

- **`Xbox controller not found`**
  - Bluetooth でコントローラがペアリング/接続されているか確認する。
  - `ls /dev/input/event*` にデバイスが現れるか、`sudo` 権限が必要でないか確認する。
- **`/dev/ttyACM0` が無い / 車が動かない**
  - Pico の micro-USB が Pi につながっているか、`ls -l /dev/ttyACM*` を確認する。
  - デバイスが `/dev/ttyACM1` の場合は `make run ARGS=/dev/ttyACM1` を使う。
  - ファームウェアが書き込まれているか（`firmware/pico_mecanum`）確認する。
- **少し動いてすぐ止まる**
  - 500 ms フェイルセーフの動作。ハートビートが届いていない（ケーブル/権限）
    可能性がある。手動テストは `firmware/pico_mecanum/README.md` を参照。
- **`libevdev/libevdev.h: No such file`**
  - `sudo apt-get install -y libevdev-dev` を実行する。
