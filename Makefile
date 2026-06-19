.POSIX:
.SUFFIXES:
.SUFFIXES: .c .o

OPTS=-DWITH_ANYASCII

# Pass DEBUG=1, NATIVE=1, etc. to make if you're too
# lazy to manually set the CFLAGS and LDFLAGS
DEBUG=0
NATIVE=0
LTO=1
# LLVM=1 uses clang and lld instead of whatever's default --
# builds with LLVM tend to be more optimized
LLVM=0

CC_LLVM0=$(CC)
CC_LLVM1=clang
CC_USED=$(CC_LLVM$(LLVM))
AWK=awk

PKGCONF_MODULES=luajit libenet libisal "$$(test "x$$(uname -s)" = "xLinux" && printf '%s\n' 'libseccomp')"
LIBS=`pkg-config --libs $(PKGCONF_MODULES)` -lm
CPPFLAGS=`pkg-config --cflags $(PKGCONF_MODULES)` $(OPTS)

CFLAGS_NATIVE0=
CFLAGS_NATIVE1=-march=native
CFLAGS_DEBUG0=-O3 $(CFLAGS_LTO$(LTO))
CFLAGS_DEBUG1=-g
CFLAGS_LTO0=
CFLAGS_LTO1=-flto
CFLAGS=-Wall -Wextra -Wno-type-limits $(CFLAGS_DEBUG$(DEBUG)) $(CFLAGS_NATIVE$(NATIVE))

LDFLAGS_DEBUG0=-s $(LDFLAGS_LTO$(LTO))
LDFLAGS_DEBUG1=
LDFLAGS_LLVM0=
LDFLAGS_LLVM1=-fuse-ld=lld
LDFLAGS_LTO0=
LDFLAGS_LTO1=-flto
LDFLAGS=$(LIBS) $(LDFLAGS_DEBUG$(DEBUG)) $(LDFLAGS_LLVM$(LLVM))
LDFLAGSSTATIC=-Wl,-Bstatic -static-libgcc $(LDFLAGS) -Wl,-Bdynamic '-Wl,--export-dynamic-symbol=lua_*' '-Wl,--export-dynamic-symbol=luaL_*' -fvisibility=hidden

OBJECTS=src/budgetvxl.o src/cull.o src/demoncore.o src/funcs_event.o \
	src/funcs_packetrecv.o src/funcs_send.o src/lua.o src/main.o \
	src/masterlist.o src/sandbox.o src/textcodec.o src/pvx/src/vxl.o

all: server exec/libunixsock.so

server: $(OBJECTS)
	$(CC_USED) -o server $(OBJECTS) $(LDFLAGS)

src/budgetvxl.o: src/budgetvxl.c
src/cull.o: src/cull.c src/bitmask.h
src/demoncore.o: src/demoncore.c src/demoncore.h src/bitmask.h src/protocol.h src/state.h src/masterlist.h
src/funcs_event.o: src/funcs_event.c src/state.h src/protocol.h src/bitmask.h src/masterlist.h src/demoncore.h
src/funcs_packetrecv.o: src/funcs_packetrecv.c src/state.h src/protocol.h src/bitmask.h src/masterlist.h src/demoncore.h
src/funcs_send.o: src/funcs_send.c src/state.h src/protocol.h src/bitmask.h src/masterlist.h src/budgetvxl.h
src/lua.o: src/lua.c src/state.h src/protocol.h src/bitmask.h src/masterlist.h src/demoncore.h src/budgetvxl.h src/luaawk.h
src/main.o: src/main.c src/cull.h src/demoncore.h src/bitmask.h src/protocol.h src/state.h src/masterlist.h src/pvx/src/vxl.h src/sandbox.h
src/masterlist.o: src/masterlist.c src/masterlist.h
src/sandbox.o: src/sandbox.c
src/textcodec.o: src/textcodec.c src/state.h src/protocol.h src/bitmask.h src/masterlist.h src/textcodec_utf8.h src/textcodec_cp437.h
.c.o:
	$(CC_USED) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

# Not really static, musl doesn't like dlopen with static
# See https://www.openwall.com/lists/musl/2021/09/24/6
# You could definitely make a truly static build if you
# don't bother loading anything in the exec dir, though.
serverstatic: $(OBJECTS)
	$(CC_USED) $(CFLAGS) $(CPPFLAGS) -o serverstatic $(OBJECTS) $(LDFLAGSSTATIC)

serverstatic-crust: $(OBJECTS)
	$(CC_USED) $(CFLAGS) $(CPPFLAGS) -DNO_DEFAULT_SANDBOX -DWITH_LIBSECCOMP -o serverstatic $(OBJECTS) $(LDFLAGSSTATIC)

exec/libunixsock.so: src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c
	# TODO: remove getaddrinfo malloc from unixsock tcp
	mkdir -p exec
	$(CC_USED) $(CFLAGS) $(CPPFLAGS) --shared -o exec/libunixsock.so src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c -Wl,--exclude-libs,ALL $(LDFLAGS)

dist.tar.gz: serverstatic exec/libunixsock.so config.lua exec scripts dirty
	rm -fR dist/
	mkdir -p dist/exec dist/scripts dist/maps dist/rw
	cp serverstatic dist/server
	cp /lib/ld-musl-x86_64.so.1 dist/
	patchelf --set-interpreter './ld-musl-x86_64.so.1' dist/server
	ln config.lua dist/
	ln exec/* dist/exec/
	find scripts -maxdepth 1 -type f -exec ln {} dist/scripts/ \;
	bsdtar cf - dist | libdeflate-gzip -c12 - > dist.tar.gz
	rm -fR dist

src/luaawk.h: gen_lua_binding.awk src/state.h
	$(AWK) -f ./gen_lua_binding.awk src/state.h > src/luaawk.h

clean:
	rm -f ./server exec/libunixsock.so src/*.o src/pvx/src/*.o

dirty:

.PHONY: clean dirty
