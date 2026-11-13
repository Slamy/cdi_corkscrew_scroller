CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -pedantic
CDI_DEFINES ?=

.PHONY: test encoder cdi-object cdi disc transfer clean
test: build/scroll_test
	./build/scroll_test
	python3 tests/test_xa_to_acm.py

build/scroll_test: src/scroll.c src/scroll.h tests/scroll_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/scroll.c tests/scroll_test.c -o $@

# Keep the third-party checkout clean. Some FFmpeg versions require an
# explicit declaration of av_free(), supplied here via compiler flags.
encoder:
	@test -f third_party/psxavenc/meson.build || { echo 'Run: git submodule update --init --recursive' >&2; exit 1; }
	@if [ ! -f build/psxavenc/build.ninja ]; then CCACHE_DISABLE=1 meson setup build/psxavenc third_party/psxavenc --buildtype=release -Dc_args='-include libavutil/mem.h'; fi
	CCACHE_DISABLE=1 meson compile -C build/psxavenc

# Wine drive D: must point to Nobelia's cdi-sdk, as in its build instructions.
cdi-object:
	mkdir -p build
	WINEPATH=D:/DOS/BIN wine D:/DOS/BIN/xcc.exe -eas=build -tp=68K,sc -v=D:/OS9/CDI/defs -bc -r -O=2 -d=CDI_ASM_COLUMNS=1 src/scroll.c
	WINEPATH=D:/DOS/BIN wine D:/DOS/BIN/xcc.exe -eas=build -tp=68K,sc -v=D:/OS9/CDI/defs -bc -r -O=2 $(CDI_DEFINES) src/main.c
	WINEPATH=D:/DOS/BIN wine D:/DOS/BIN/xcc.exe -eas=build -tp=68K,sc -v=D:/OS9/CDI/defs -bc -r -O=2 src/sprites.a

cdi: cdi-object
	WINEPATH=D:/DOS/BIN wine D:/DOS/BIN/l68.exe -z=link.txt -o=build/scroller -n=cdi_scroller
	WINEPATH=D:/DOS/BIN wine D:/DOS/BIN/fixmod.exe -uo=0.0 build/scroller

disc: cdi
	./make_image.sh

transfer:
	./transfer_mister.sh

clean:
	rm -rf build
