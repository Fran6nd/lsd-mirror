.POSIX:

AWK=awk
GZIP=libdeflate-gzip -12
BROTLI=brotli -Z

all: pages out/singlepage.html gzip brotli

gzip: pages out/singlepage.html
	$(GZIP) -kf out/*.html

brotli: pages out/singlepage.html
	$(BROTLI) -kf out/*.html

pages: out/index.html index
	cd src; for x in *; do sed -f ../index "$$x" | $(AWK) -f ../stapler.awk -v "TITLE=$$x" > "../out/$$x.html"; done

out/singlepage.html: pages
	# TODO: strip out old pages that haven't been make clean'd
	# TODO: tr -d doesn't play nicely with <pre>
	printf '%s\n' out/lua_misc.html out/index.html out/*.html | sed '2!{/\/index.html$$/d};/\/singlepage.html$$/d' | xargs sed '1!s|.*<main>||;1s|<title>.*</title>|<title>LSd Lua Api – single-page</title>|;1s|<main>.*|<main>|;1s|<style>|&h1{text-align:center}|;$$!s|<footer>.*|</article>|;s|<a href=[a-z_]*\.html|<a href=|g;s|<a href=>\([^<]*\)</a>|\1|g' | tr -d '\n' > out/singlepage.html

out/index.html: index
	mkdir -p out
	$(AWK) -f toc.awk index | sed -f index > out/index.html

index: dirty
	cd src; { for x in *; do printf 'FILE\t%s\n' "$$x" | cat - "$$x"; done; } | sed -f ../index.sed > ../index

clean:
	rm -fR index out

apicov: index
	sed '/^s:<a href=#/!d;s/^s:<a href=#//;s/>.*//' index | sort > apicovtmp_impl
	sed '/^#define LUA_CALLS /!d;s/[^{]*{/{/;s/{"\([^"]*\)",[^,]*,/\1\n/g;s/\n$$//' ../src/luaawk.h | sort > apicovtmp_c
	sed '/^\t{"/!{/lua_setglobal(l, "/!d};s/[^"]*"//;s/".*//' ../src/lua.c | sort > apicovtmp_lua
	printf '%s\n' 'C:'; comm -13 apicovtmp_impl apicovtmp_c | sed 's/^/\t/'
	printf '%s\n' 'Lua:'; comm -13 apicovtmp_impl apicovtmp_lua | sed 's/^/\t/'
	wc -l apicovtmp_impl
	rm -f apicovtmp_impl apicovtmp_c apicovtmp_lua

blame:
	{ cd "$(DIR)"; printf '%s\t%s\t%s\n' "$$(git remote get-url origin)" "$(COMMIT)" "$(NAME)"; git show "$(COMMIT)"; } | sed -f blame.sed

dirty:
.PHONY: apicov blame clean dirty
.SILENT: apicov blame
