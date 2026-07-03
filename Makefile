# XBoxControllerDevice -- Makefile
#
# ライブラリ (src/*.c) をビルドし、examples/xbox.c とリンクして bin/xbox を生成する。
#
# 事前準備 (Raspberry Pi 側で一度だけ):
#     sudo apt-get install -y libevdev-dev
#
# 主なターゲット:
#     make        (= all)  ライブラリ + サンプルをビルドし bin/xbox を生成
#     make run             bin/xbox を実行 (必要ならビルド)
#     make clean           obj/ bin/ を削除

CC      = gcc

# libevdev のヘッダは /usr/include/libevdev-1.0/ 以下にあり、既定の include パスに
# 無いため pkg-config で解決する。pkg-config が無い/未登録の場合はハードコードに
# フォールバックする ( sudo apt-get install -y libevdev-dev で導入される )。
LIBEVDEV_CFLAGS := $(shell pkg-config --cflags libevdev 2>/dev/null || echo -I/usr/include/libevdev-1.0)
LIBEVDEV_LIBS   := $(shell pkg-config --libs   libevdev 2>/dev/null || echo -levdev)

CFLAGS  = -Wall -Iinclude $(LIBEVDEV_CFLAGS)
LDFLAGS =
LDLIBS  = $(LIBEVDEV_LIBS) -lpthread

SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin
EXAMPLE = examples/xbox.c
BIN     = $(BINDIR)/xbox

# ライブラリのソース (src/ 配下すべて) をワイルドカードで収集
LIBSRC = $(wildcard $(SRCDIR)/*.c)
LIBOBJ = $(LIBSRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

.PHONY: all clean run

all: $(BIN)

# サンプルをライブラリのオブジェクトとリンクして実行ファイルを生成
$(BIN): $(EXAMPLE) $(LIBOBJ)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLE) $(LIBOBJ) $(LDFLAGS) $(LDLIBS)

# src/*.c -> obj/*.o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# bin/xbox を実行 (引数でシリアルデバイスを渡せる: make run ARGS=/dev/ttyACM1)
run: $(BIN)
	./$(BIN) $(ARGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)
