# Pico W (Freenove FNK0089) 用 Bluepad32 ファームを
# ビルド(コンパイル検証)+ picotool 書き込みまで行う単一イメージ。
# ベース: Debian trixie (aarch64 マルチアーチOK)。
FROM debian:trixie-slim

# --- 依存 (Debian trixie apt) ---
# gcc-arm-none-eabi + libnewlib-arm-none-eabi で pico-sdk 2.1.1 を正常ビルド
# (Arduino同梱gccで出る __retarget_lock の multiple definition を回避)。
# picotool は flash サービスが同イメージを使うため & pico-sdk の find_package(picotool 2.1.1)
# を満たすため同梱 (Debian の picotool 2.1.1+dfsg-1 がバージョン一致)。
# 検証済みバージョン(2026-07): gcc-arm-none-eabi 15:14.2.rel1-1 / libnewlib 4.5.0 /
#   picotool 2.1.1+dfsg-1。厳密再現が必要なら snapshot.debian.org でこれらを固定する。
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake \
        git \
        ninja-build \
        build-essential \
        gcc-arm-none-eabi \
        libnewlib-arm-none-eabi \
        python3 \
        libusb-1.0-0 \
        picotool \
        pkg-config \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# --- Pico SDK 2.1.1 ---
# tag 2.1.1 を shallow clone し、必要な submodule を取得。
ENV PICO_SDK_PATH=/opt/pico-sdk
RUN git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk \
    && git -C /opt/pico-sdk submodule update --init

# --- Bluepad32 ---
# 指定 commit を checkout してから submodule を再帰取得。
# ※ full clone にすること (--depth 1 だと任意 commit を fetch できず checkout が失敗する)。
ENV BLUEPAD32_ROOT=/opt/bluepad32
RUN git clone https://github.com/ricardoquesada/bluepad32.git /opt/bluepad32 \
    && git -C /opt/bluepad32 checkout e9b755faabc240585da42e6d26164bb2cdd064d3 \
    && git -C /opt/bluepad32 submodule update --init --recursive

# --- firmware ソース ---
# COPY 対象は src / CMakeLists.txt / pico_sdk_import.cmake のみ (.dockerignore で他を除外)。
COPY src /work/app/src
COPY CMakeLists.txt /work/app/CMakeLists.txt
COPY pico_sdk_import.cmake /work/app/pico_sdk_import.cmake

# --- ビルド (docker build がコンパイル検証を兼ねる) ---
# 成果物 uf2 を /out/firmware.uf2 に配置。
RUN cmake -DPICO_BOARD=pico_w -S /work/app -B /work/build \
    && cmake --build /work/build -j \
    && mkdir -p /out \
    && cp /work/build/bluepad32_picow_example_app.uf2 /out/firmware.uf2

WORKDIR /out
