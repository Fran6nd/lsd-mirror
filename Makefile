.POSIX:

CC=clang
AWK=awk
CFLAGS=-Wall -Wextra -s -O3 -flto -fuse-ld=lld
CFLAGSNATIVE=-Wall -Wextra -s -O3 -flto -march=native -fuse-ld=lld
CFLAGSG=-Wall -Wextra -g
LIBS=-lenet -lisal -lluajit-5.1 -lm -lseccomp
LDFLAGS=$(LIBS)

server: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h
	$(CC) $(CFLAGS) -o server src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)

# Not really static, musl doesn't like dlopen with static
# See https://www.openwall.com/lists/musl/2021/09/24/6
# You could definitely make a truly static build if you
# don't bother loading anything in the exec dir, though.
serverstatic: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h
	$(CC) $(CFLAGS) -o serverstatic src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o \
		-Wl,-Bstatic -static-libgcc $(LDFLAGS) -Wl,-Bdynamic '-Wl,--export-dynamic-symbol=lua_*' '-Wl,--export-dynamic-symbol=luaL_*' -fvisibility=hidden
servernative: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h
	$(CC) $(CFLAGSNATIVE) -o servernative src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)
serverg: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o src/luaawk.h
	$(CC) $(CFLAGSG) -g -o serverg src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/masterlist.c src/pvx/vxl.o $(LDFLAGS)

dist.tar.gz: serverstatic aloha.lua exec scripts maps dirty
	mkdir -p dist/exec dist/scripts dist/maps
	ln serverstatic dist/server
	cp /lib/ld-musl-x86_64.so.1 dist/
	patchelf --set-interpreter './ld-musl-x86_64.so.1' dist/server
	printf '%s\n' 'log("You probably want to run this as ./server -c babel.lua");' > dist/config.lua
	ln aloha.lua dist/babel.lua
	ln exec/* dist/exec/
	ln scripts/* dist/scripts/
	ln maps/* dist/maps/
	bsdtar cf - dist | libdeflate-gzip -c12 - > dist.tar.gz
	rm -fR dist

dirty:
.PHONY: dirty

src/luaawk.h: generate_lua_api_binding.awk src/state.h
	$(AWK) -f ./generate_lua_api_binding.awk src/state.h > src/luaawk.h
