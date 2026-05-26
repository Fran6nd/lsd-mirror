.POSIX:
.SUFFIXES: .c .o

CC=clang
AWK=awk

PKGCONF_MODULES=luajit libenet libisal "$$(test "x$$(uname -s)" = "xLinux" && printf '%s\n' 'libseccomp')"
LIBS=`pkg-config --libs $(PKGCONF_MODULES)` -lm
OPTS=-DWITH_ANYASCII
CPPFLAGS=`pkg-config --cflags $(PKGCONF_MODULES)` $(OPTS)

CFLAGS=-Wall -Wextra -O3 -flto
CFLAGSNATIVE=-Wall -Wextra -O3 -flto -march=native
CFLAGSG=-Wall -Wextra -g

LDFLAGS=$(LIBS) -s -flto -fuse-ld=lld
LDFLAGSNATIVE=$(LIBS) -s -flto -fuse-ld=lld
LDFLAGSG=$(LIBS)
LDFLAGSSTATIC=-Wl,-Bstatic -static-libgcc $(LDFLAGS) -Wl,-Bdynamic '-Wl,--export-dynamic-symbol=lua_*' '-Wl,--export-dynamic-symbol=luaL_*' -fvisibility=hidden

OBJECTS=src/budgetvxl.o src/cull.o src/demoncore.o src/funcs_event.o src/funcs_packetrecv.o src/funcs_send.o src/lua.o src/main.o src/masterlist.o src/sandbox.o src/textcodec.o src/pvx/src/vxl.o
INCL=src/bitmask.h src/budgetvxl.h src/cull.h src/demoncore.h src/luaawk.h src/masterlist.h src/protocol.h src/sandbox.h src/state.h src/textcodec_cp437.h src/textcodec_utf8.h

all: server exec/libunixsock.so

server: $(OBJECTS) $(INCL)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o server $(OBJECTS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

# Not really static, musl doesn't like dlopen with static
# See https://www.openwall.com/lists/musl/2021/09/24/6
# You could definitely make a truly static build if you
# don't bother loading anything in the exec dir, though.
serverstatic: $(OBJECTS) $(INCL)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o serverstatic $(OBJECTS) $(LDFLAGSSTATIC)

serverstatic-crust: $(OBJECTS) $(INCL)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DNO_DEFAULT_SANDBOX -DWITH_LIBSECCOMP -o serverstatic $(OBJECTS) $(LDFLAGSSTATIC)

servernative: $(OBJECTS) $(INCL)
	$(CC) $(CFLAGSNATIVE) $(CPPFLAGS) -o servernative $(OBJECTS) $(LDFLAGSNATIVE)

serverg: $(OBJECTS) $(INCL)
	$(CC) $(CFLAGSG) $(CPPFLAGS) -g -o serverg $(OBJECTS) $(LDFLAGSG)

exec/libunixsock.so: src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c
	# TODO: remove getaddrinfo malloc from unixsock tcp
	mkdir -p exec
	$(CC) $(CFLAGS) $(CPPFLAGS) --shared -o exec/libunixsock.so src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c -Wl,--exclude-libs,ALL $(LDFLAGS)

dist.tar.gz: serverstatic exec/libunixsock.so aloha.lua exec scripts maps dirty
	rm -fR dist/
	mkdir -p dist/exec dist/scripts dist/maps
	cp serverstatic dist/server
	cp /lib/ld-musl-x86_64.so.1 dist/
	patchelf --set-interpreter './ld-musl-x86_64.so.1' dist/server
	printf '%s\n' 'log("You probably want to run this as ./server -c babel.lua");' > dist/config.lua
	ln aloha.lua dist/babel.lua
	ln exec/* dist/exec/
	ln scripts/* dist/scripts/
	find maps -maxdepth 1 -type f -exec ln {} dist/maps/ \;
	bsdtar cf - dist | libdeflate-gzip -c12 - > dist.tar.gz
	rm -fR dist

src/luaawk.h: gen_lua_binding.awk src/state.h
	$(AWK) -f ./gen_lua_binding.awk src/state.h > src/luaawk.h

clean:
	rm -f ./server exec/libunixsock.so src/*.o src/pvx/src/*.o

dirty:

.PHONY: clean dirty
