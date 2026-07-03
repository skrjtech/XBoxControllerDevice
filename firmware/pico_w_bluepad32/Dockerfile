# Pico W (Freenove FNK0089) 用 Bluepad32 ファームを
#   builder ステージ : フルツールチェーンでコンパイル (docker build がコンパイル検証)
#   runtime ステージ : picotool + 生成 uf2 だけの軽量イメージ (書き込み用/成果物取出用)
# マルチステージにより最終イメージを小さく保つ (SDK/ツールチェーンは builder に隔離)。
# ベース: Debian trixie (aarch64 マルチアーチOK)。

# ============================================================================
# builder: コンパイル
# ============================================================================
FROM debian:trixie-slim AS builder

# gcc-arm-none-eabi + libnewlib + libstdc++-arm-none-eabi-{dev,newlib} で pico-sdk 2.1.1 を
# 正常ビルド (Arduino同梱gccで出る __retarget_lock の multiple definition を回避)。
# pico-sdk/bluepad32 は C++ も使う(project C CXX ASM)ため arm-none-eabi 用 libstdc++
# ヘッダ(cstdlib 等)が必須。picotool は pico-sdk の find_package(picotool 2.1.1) を満たすため同梱。
# 検証済み(2026-07): gcc-arm-none-eabi 15:14.2.rel1-1 / libnewlib 4.5.0 / picotool 2.1.1+dfsg-1。
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake \
        git \
        ninja-build \
        build-essential \
        gcc-arm-none-eabi \
        libnewlib-arm-none-eabi \
        libstdc++-arm-none-eabi-dev \
        libstdc++-arm-none-eabi-newlib \
        python3 \
        libusb-1.0-0 \
        picotool \
        pkg-config \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Pico SDK 2.1.1 (tag を shallow clone し submodule 取得)
ENV PICO_SDK_PATH=/opt/pico-sdk
RUN git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk \
    && git -C /opt/pico-sdk submodule update --init

# Bluepad32 (指定 commit を checkout してから submodule を再帰取得)
# ※ full clone にすること (--depth 1 だと任意 commit を fetch できず checkout が失敗する)。
ENV BLUEPAD32_ROOT=/opt/bluepad32
RUN git clone https://github.com/ricardoquesada/bluepad32.git /opt/bluepad32 \
    && git -C /opt/bluepad32 checkout e9b755faabc240585da42e6d26164bb2cdd064d3 \
    && git -C /opt/bluepad32 submodule update --init --recursive

# firmware ソース (COPY は src / CMakeLists.txt / pico_sdk_import.cmake のみ)
COPY src /work/app/src
COPY CMakeLists.txt /work/app/CMakeLists.txt
COPY pico_sdk_import.cmake /work/app/pico_sdk_import.cmake

# ビルド -> 成果物 uf2 を /out/firmware.uf2 に配置
RUN cmake -DPICO_BOARD=pico_w -S /work/app -B /work/build \
    && cmake --build /work/build -j \
    && mkdir -p /out \
    && cp /work/build/bluepad32_picow_example_app.uf2 /out/firmware.uf2

# ============================================================================
# runtime: 書き込み用の軽量イメージ (picotool + uf2 のみ)
# ============================================================================
FROM debian:trixie-slim AS runtime

# 書き込みに必要な picotool と libusb だけ (SDK/ツールチェーンは含めない)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        picotool \
        libusb-1.0-0 \
    && rm -rf /var/lib/apt/lists/*

# builder で作った uf2 を持ち込む
COPY --from=builder /out/firmware.uf2 /out/firmware.uf2

WORKDIR /out
