# XBoxControllerDevice

Xbox ワイヤレスコントローラで **Freenove FNK0089**（Raspberry Pi Pico W 用
メカナムホイールカー）を操縦するプロジェクト。

コントローラを **Pico W に直結して自走させる構成（推奨・動作確認済み）** と、
**Raspberry Pi を中継させる構成** の 2 通りを収録しています。

## ディレクトリ構成

```
XBoxControllerDevice/
├── firmware/
│   ├── pico_w_bluepad32/     ★推奨: Pico W 自走ファーム (Bluepad32 / C / Pico SDK)
│   │   ├── src/my_platform.c   モータ駆動 + スティック操作マッピング (本体)
│   │   ├── CMakeLists.txt / pico_sdk_import.cmake / src/*.h
│   │   ├── Dockerfile / docker-compose.yml / docker-flash.sh   Docker ビルド&書き込み
│   │   ├── README.md           ビルド/書き込み/ペアリング手順
│   │   └── DOCKER.md           Docker 手順
│   └── pico_mecanum/         構成B用 Pico ファーム (Arduino / C++)
│       ├── pico_mecanum.ino
│       └── README.md
├── include/ , src/ , examples/  構成B用 Raspberry Pi 側 libevdev ライブラリ (C)
├── Makefile                     構成B (Pi側) のビルド
└── README.md                    このファイル
```

## 2 つの構成

| | **A. Pico W 自走**（推奨・実機確認済み） | **B. Raspberry Pi 経由** |
|---|---|---|
| 接続 | Xbox コン → **Bluetooth(BLE)** → Pico W → モータ | Xbox コン → Pi(libevdev) → USB-CDC → Pico → モータ |
| 走行時に Pi | **不要**（車体バッテリーで自走） | 必要（Pi が中継） |
| 操作 | 左スティックで全方向移動 + 右スティックで旋回（同時=カーブ） | D-Pad で前後/ストレイフ、LB/RB で旋回 |
| Pico 実装 | [`firmware/pico_w_bluepad32/`](firmware/pico_w_bluepad32/)（Bluepad32/C） | [`firmware/pico_mecanum/`](firmware/pico_mecanum/)（Arduino/C++） |
| ホスト実装 | 不要 | `src/` `include/` `examples/` + `Makefile`（libevdev/C） |

---

## 構成 A: Pico W 自走（推奨）

Pico W 自身が Bluetooth ホストになり、Xbox コントローラと直接 BLE 接続してメカナムカーを
操縦します。Raspberry Pi は開発・書き込み時のみ使用。

- **操作**: 左スティック=全方向移動（上=前進/下=後退/左右=平行移動、斜め可・傾き量で速度）、
  右スティック左右=旋回、両方同時=走りながらカーブ。D-Pad/LB・RB は中立時フォールバック。
  コントローラ切断でフェイルセーフ停止。
- **ビルド/書き込み**: 手動（pico-sdk + Bluepad32）は
  [`firmware/pico_w_bluepad32/README.md`](firmware/pico_w_bluepad32/README.md)、
  **Docker（コンパイル〜書き込み）** は
  [`firmware/pico_w_bluepad32/DOCKER.md`](firmware/pico_w_bluepad32/DOCKER.md) を参照。

```sh
# Docker で: コンパイル -> BOOTSEL 誘発 -> 書き込み
cd firmware/pico_w_bluepad32
docker compose build       # ファームをコンパイル (通ればコンパイル成功)
./docker-flash.sh          # Pico を BOOTSEL に落として picotool で書き込み
```

- **ペアリング**: Pico 起動後、Xbox コントローラをペアリングモード（Xbox ボタンで起動→
  ペアリングボタン長押しで速点滅）にすると自動接続。BLE 対応 Xbox コントローラが必要
  （確認済み: 純正 Xbox Wireless Controller, 045e:02fd / Model 1708 / fw 4.8）。

---

## 構成 B: Raspberry Pi 経由

Xbox コントローラ → Raspberry Pi（libevdev）→ USB-CDC シリアル → Pico（Freenove FNK0089）
→ メカナム 4 輪。Pi が evdev で D-Pad/ボタンを読み、1 文字コマンド（`F`/`B`/`L`/`R`/`Q`/`E`/`S`）
を Pico の USB-CDC へ 115200 8N1 で送信し、Pico(Arduino) が駆動します。

```
 Xbox Controller ─(Bluetooth/USB evdev)→ Raspberry Pi ─USB-CDC /dev/ttyACM0→ Pico(Arduino) → 4 wheels
                                          bin/xbox + libevdev        F/B/L/R/Q/E/S + failsafe
```

### 必要環境 / ビルド（Pi 側）

```sh
sudo apt-get install -y libevdev-dev   # 一度だけ
make                                   # bin/xbox を生成 (pkg-config で libevdev を解決)
```

- libevdev を root なしでビルドしたい場合は `make LIBEVDEV_PREFIX=$HOME/.local/opt/libevdev`
  （`apt-get download` + `dpkg -x` で展開したツリーを指定）。

### Pico（構成B ファーム）

Arduino IDE で [`firmware/pico_mecanum/pico_mecanum.ino`](firmware/pico_mecanum/pico_mecanum.ino)
を開き、ボードに **Raspberry Pi Pico W** を選んで書き込む。詳細は
[`firmware/pico_mecanum/README.md`](firmware/pico_mecanum/README.md)。

### 操作 / プロトコル（構成B）

```sh
make run                        # /dev/ttyACM0 -> /dev/ttyACM1 を自動で試す
./bin/xbox [シリアルデバイス]
```

| 入力 | 動作 | | Pi→Pico バイト | 動作 |
|---|---|---|---|---|
| D-Pad 上/下 | 前進/後退 | | `F` / `B` | 前進 / 後退 |
| D-Pad 左/右 | 左/右ストレイフ | | `L` / `R` | 左/右ストレイフ |
| LB / RB | 左/右旋回(その場) | | `Q` / `E` | 左/右旋回(CCW/CW) |
| Xbox ボタン | 終了 | | `S` | 停止 |

- コマンドは状態変化時 + 約 100 ms ごとのハートビートで送信。
- **フェイルセーフ**: 有効なコマンドが 500 ms 届かないと Pico は全モータ停止。

---

## 検証済みハードウェア情報（両構成共通）

Freenove FNK0089 のモータは H ブリッジで、2 本のうち「forward pin」に PWM・「reverse pin」に
LOW を出すと前進します。**forward pin は必ずしも小さい方の GPIO ではありません**（実機検証済み）。

| Motor | 位置 | forward pin | reverse pin |
|-------|------|-------------|-------------|
| M1 FL | Front-Left  | GP18 | GP19 |
| M2 BL | Back-Left   | GP21 | GP20 |
| M3 FR | Front-Right | GP7  | GP6  |
| M4 BR | Back-Right  | GP9  | GP8  |

メカナム符号行列（各輪 +1=前転 / -1=後転、実機検証済み）と合成式:

```
forward (+,+,+,+)  backward (-,-,-,-)
strafe_left (FL-,FR-,BL+,BR+)  strafe_right (FL+,FR+,BL-,BR-)
rotate_left/CCW (FL-,FR+,BL-,BR+)  rotate_right/CW (FL+,FR-,BL+,BR-)
アナログ合成: FL=vx+vy+w  FR=vx+vy-w  BL=vx-vy+w  BR=vx-vy-w   (vx=前後, vy=右, w=CW)
```

## トラブルシュート

- **A: コントローラが繋がらない** → ペアリングモード（速点滅）にし直す。BLE 対応 Xbox
  モデルか確認（古い USB 専用モデルは不可）。
- **A: Docker の書き込みで BOOTSEL が見つからない** → Pico の BOOTSEL ボタンを押しながら
  USB 挿し直し。詳細は `firmware/pico_w_bluepad32/DOCKER.md`。
- **B: `Xbox controller not found`** → コントローラをペアリング/接続し `/dev/input/event*` を確認。
- **B: `/dev/ttyACM0` が無い / 動かない** → Pico の USB が Pi に繋がっているか、`ls /dev/ttyACM*` を確認。
- **B: `libevdev/libevdev.h: No such file`** → `sudo apt-get install -y libevdev-dev`
  （または `make LIBEVDEV_PREFIX=...`）。

## ライセンス

`LICENSE` を参照。Bluepad32 例由来ファイル（`firmware/pico_w_bluepad32/src/main.c` 等）は
Public Domain（Bluepad32 のヘッダ表記に従う）。
