.POSIX:

CC=clang
AWK=awk

PKGCONF_MODULES=luajit libenet libisal "$$(test "x$$(uname -s)" = "xLinux" && printf '%s\n' 'libseccomp')"
LIBS=`pkg-config --libs $(PKGCONF_MODULES)` -lm
CPPFLAGS=`pkg-config --cflags $(PKGCONF_MODULES)`

CFLAGS=-Wall -Wextra -s -O3 -flto -fuse-ld=lld $(CPPFLAGS)
CFLAGSNATIVE=-Wall -Wextra -s -O3 -flto -march=native -fuse-ld=lld $(CPPFLAGS)
CFLAGSG=-Wall -Wextra -g $(CPPFLAGS)
LDFLAGS=$(LIBS)

server: src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h exec/libunixsock.so
	$(CC) $(CFLAGS) -o server src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)

# Not really static, musl doesn't like dlopen with static
# See https://www.openwall.com/lists/musl/2021/09/24/6
# You could definitely make a truly static build if you
# don't bother loading anything in the exec dir, though.
serverstatic: src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h exec/libunixsock.so
	$(CC) $(CFLAGS) -o serverstatic src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o \
		-Wl,-Bstatic -static-libgcc $(LDFLAGS) -Wl,-Bdynamic '-Wl,--export-dynamic-symbol=lua_*' '-Wl,--export-dynamic-symbol=luaL_*' -fvisibility=hidden
serverstatic-crust: src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h exec/libunixsock.so
	$(CC) $(CFLAGS) -DNO_DEFAULT_SANDBOX -DWITH_LIBSECCOMP -o serverstatic src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o \
		-Wl,-Bstatic -static-libgcc $(LDFLAGS) -Wl,-Bdynamic '-Wl,--export-dynamic-symbol=lua_*' '-Wl,--export-dynamic-symbol=luaL_*' -fvisibility=hidden
servernative: src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h exec/libunixsock.so
	$(CC) $(CFLAGSNATIVE) -o servernative src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)
serverg: src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h exec/libunixsock.so
	$(CC) $(CFLAGSG) -g -o serverg src/main.c src/funcs_packetrecv.c src/funcs_event.c src/funcs_send.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)

exec/libunixsock.so: src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c
	# TODO: remove getaddrinfo malloc from unixsock tcp
	mkdir -p exec
	$(CC) $(CFLAGS) --shared -o exec/libunixsock.so src/exec/sha1.c src/exec/websockets.c src/exec/b64.c src/exec/unixsock.c -Wl,--exclude-libs,ALL

dist.tar.gz: serverstatic aloha.lua exec scripts maps dirty
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

dirty:
.PHONY: dirty

src/luaawk.h: gen_lua_binding.awk src/state.h
	$(AWK) -f ./gen_lua_binding.awk src/state.h > src/luaawk.h
