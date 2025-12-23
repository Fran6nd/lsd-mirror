server: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/pvx/vxl.o
	clang -Wall -Wextra -g -o server src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/pvx/vxl.o -lenet -lisal -lluajit-5.1 -lm -lseccomp
serverspeed: src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/pvx/vxl.o
	clang -Wall -Wextra -s -O3 -flto -march=native -fuse-ld=lld -o serverspeed src/main.c src/sandbox.c src/lua.c src/demoncore.c src/budgetvxl.c src/cull.c src/pvx/vxl.o -lenet -lisal -lluajit-5.1 -lm -lseccomp
