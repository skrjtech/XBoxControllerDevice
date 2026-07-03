# Docker で Pico W ファームをビルド & 書き込み

Pico W (Freenove FNK0089) 用 Bluepad32 ファームを Docker でコンパイルし、
picotool で書き込むための手順です。ホストは Raspberry Pi (aarch64, Debian trixie)、
`docker` グループ所属で sudo 不要。

- イメージ名: `xbox-pico-fw`（`build` / `flash` で共有）
- **書き込み元はイメージ内蔵の `/out/firmware.uf2`**。`docker compose build` した時点で
  コンパイル済み・焼き込み済みなので、`flash` は `build` の実行(ホスト `./out`)に依存しません。

## 1. ビルド（=コンパイル検証）

```sh
docker compose build              # ここでファームがコンパイルされる (通ればコンパイル成功)
# 任意: 生成 uf2 をホストに取り出したいとき
mkdir -p out                      # ./out をユーザ所有で先に用意 (root所有回避)
docker compose run --rm build     # /out/firmware.uf2 -> ./out/firmware.uf2 にコピー
```

`docker build` 時に `cmake` でリンクまで行うため、build が通ればコンパイルは成功です。

## 2. 書き込み

Pico を BOOTSEL に落としてから picotool で書き込みます。ホストヘルパが簡単です。

```sh
./docker-flash.sh                 # 既定 /dev/ttyACM0
./docker-flash.sh /dev/ttyACM1    # ポート指定も可
```

`docker-flash.sh` の流れ:

1. `/dev/ttyACM0` を 1200bps で開いて閉じる → pico stdio_usb が BOOTSEL リセット
2. BOOTSEL の USB デバイス `2e8a:0003` の出現を最大10秒待つ
3. `docker compose run --rm flash` → `picotool load -x /out/firmware.uf2`（イメージ内蔵 uf2）

手動で BOOTSEL に入れてある場合（ボタン押しながら USB 接続）は直接:

```sh
docker compose run --rm flash
```

## USB 権限について（privileged を使わない）

- `flash` は `privileged` の代わりに **`device_cgroup_rules: ["c 189:* rmw"]`**
  （USB キャラクタデバイス major 189 のみ許可）+ `/dev/bus/usb:/dev/bus/usb` マウントで、
  コンテナ内 picotool が libusb 経由で BOOTSEL の Pico にアクセスします（最小権限）。
- **フォールバック**: 環境によって libusb が届かない場合は、`docker-compose.yml` の
  `flash` から `device_cgroup_rules` と `volumes` を消して `privileged: true` にする。
- **注意**: デスクトップ環境等でホストが BOOTSEL の `RPI-RP2` を自動マウントする場合は、
  書き込み前に `udisksctl unmount -b /dev/sdX1` 等で外してから実行してください。

## トラブルシュート

| 症状 | 対処 |
|------|------|
| `docker-flash.sh` が「BOOTSEL デバイスが見つかりません」 | 手動 BOOTSEL(ボタン押しながら挿し直し)後に再実行 |
| `picotool` が `No accessible RP2040/RP2350 devices` | Pico が BOOTSEL でない/既に書込済で再起動した。BOOTSEL に入れて再実行 |
| `xbox-pico-fw` が pull できないと言われる | 先に `docker compose build` する（`pull_policy: never`＝ローカル専用） |
| libusb が Pico に届かない | 上記フォールバックで `privileged: true` にする／`RPI-RP2` を umount |
| ビルドが失敗する | `docker compose build --no-cache`。リンクで `__retarget_lock` が出たら Dockerfile の注記(公式Arm toolchain)を参照 |

## 補足

- `-x` オプションで書き込み後に自動リブート（アプリ実行）します。
- 書き込み後、Pico をペアリングモードにして Xbox コントローラを接続してください
  （手順は同ディレクトリの `README.md`）。
