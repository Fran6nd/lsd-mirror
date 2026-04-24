BEGIN {
	autolink = 1;
	autofmt = 1;
	listjokes = 1;

	if (BLAMEDIR == "")
		BLAMEDIR = "/tmp/lsd-blame";

	uppertitle = toupper(substr(TITLE, 1, 1)) substr(TITLE, 2);
	while (match(uppertitle, "_"))
		uppertitle = substr(uppertitle, 1, RSTART-1) " " toupper(substr(uppertitle, RSTART+1, 1)) substr(uppertitle, RSTART+2);

	printf "<!doctype html><html lang=en><meta charset=utf-8>"\
		"<title>LSd Lua Api – %s</title>"\
		"<meta name=viewport content=\"width=device-width,initial-scale=1\"><meta name=color-scheme content=\"dark light\">"\
		"<style>"\
			"body{margin:6rem 3rem;overflow-y:scroll;line-height:1.4}"\
			"main{margin:0 auto;max-width:53ch}"\
			"h3{margin-bottom:.5rem}"\
			"p{+p{text-indent:1em}margin:0;text-align:justify;hyphens:auto}"\
			"footer{text-align:center;margin:1.5rem 0}"\
			".num{dt{text-align:right}display:grid;grid-template-columns:auto 1fr}"\
			"dl{dl{margin-left:3em;clear:left}line-height:1.7}"\
			"dt{font-weight:bold;float:left;clear:left;margin-right:1em}"\
			"dd{margin:0}"\
			"details{margin:.5rem 0}"\
			"pre{border:2px solid;border-left:none;border-right:none;margin:1rem calc(50%% - 40ch)1rem;padding:1rem 0 1rem;width:80ch;white-space:pre-wrap;word-break:break-all}"\
			"kbd{font:inherit}"\
			"code{hyphens:none}"\
			"ins,del{text-decoration:none}ins{color:#060}del{color:#a22}@media(prefers-color-scheme:dark){ins{color:#3e3}del{color:#f88}}"\
			"sup{line-height:1}"\
			"@media screen{"\
				".joke{background:linear-gradient(.2turn,#e22,#e2e,#22e,#2ee,#2e2,#ee2,#e22);background-clip:text;color:#0000}"\
			"}"\
			"p{samp{color:#25f}kbd{color:#2f5}}var{color:#f52}"\
		"</style><main><article>"\
		"<h2>%s</h2>", uppertitle, uppertitle;
}

/^<AUTOLINK>$/ {autolink = 1; next;}
/^<NOAUTOLINK>$/ {autolink = 0; next;}

/^<AUTOFMT>$/ {autofmt = 1; next;}
/^<NOAUTOFMT>$/ {autofmt = 0; cont = ""; next;}

/^<LISTJOKES>$/ {listjokes = 1; next;}
/^<NOLISTJOKES>$/ {listjokes = 0; next;}

/^<BLAME .*>$/ {
	gsub(/[\x00-\x1f!-\/:-@[-`{-~]/, "");
	split($0, blame, " ");

	name = blame[2];
	dir = BLAMEDIR"/"name;
	commit = blame[3];

	cmd = "{ cd '"dir"' && printf '%s\t%s\t%s\n' \"$(git remote get-url origin)\" '"commit"' '"name"' && git show '"commit"'; } | sed -f ../blame.sed";
	# TODO: licensefuckery
	cmd = "false";

	blamecont = "";
	while (cmd | getline) {
		cont = "";
		printf blamecont"%s", $0;
		blamecont = "\n";
	}

	close(cmd);
	next;
}

{
	gsub(/<!--.*-->/, "");
	if (autofmt) {
		gsub(/^[\t ]*/, "");
		gsub(/[\t ]*$/, "");
	}
}

/ ->$/ {
	cont = "";
	sub(/ ->$/, "");
	if (match($0, / [a-zA-Z0-9_]*\(/))
		id = substr($0, RSTART+1, RLENGTH-2);
	else
		id = $0;

	gsub(/[a-zA-Z0-9_] /, "&~");

	# That's an nbsp, not a regular space
	sub(/ ~/, " ");
	gsub(/ ~[a-zA-Z0-9_]*/, "<var>&</var>");
	# Also an nbsp
	gsub(/ ~/, " ");

	printf "<h3 id=%s><code>%s</code></h3><p>", id, $0;
	next;
}

/./ {if (autofmt) {
	if (/^<\/dl>/ || /^<\/ul>/ || /^<d[ltd]>/ || /^<ul>/ || /^<li>/ || /^<p>/ || /^<details>/)
		cont = "";

	# TODO: take advantage of index for determining function file name
	# TODO: actually, probably just move this entire block (sub TITLE
	# unless you like to brute-force capitalization) into index.sed. . .
	if (autolink) {
		gsub(" connected", " <a href=lua_playerget.html#is_connected>connected</a>");
		gsub("-connected", "-<a href=lua_playerget.html#is_connected>connected</a>");
		gsub(" joined", " <a href=lua_playerget.html#is_joined>joined</a>");
		gsub("-joined", "-<a href=lua_playerget.html#is_joined>joined</a>");
		gsub(" alive", " <a href=lua_playerget.html#is_alive>alive</a>");
		gsub("-alive", "-<a href=lua_playerget.html#is_alive>alive</a>");
		gsub(" airborne", " <a href=lua_playerget.html#is_airborne>airborne</a>");
                gsub("-airborne", "-<a href=lua_playerget.html#is_airborne>airborne</a>");
                gsub("DDA", "<a rel=noreferrer href=https://en.wikipedia.org/wiki/Digital_differential_analyzer_(graphics_algorithm)><abbr title=\"digital differential analyzer\">DDA</abbr></a>");
	}
	gsub("<a href="TITLE".html#", "<a href=#");

	if (listjokes) {
		gsub("pyspades", "<span class=joke>&</span>");
		# piqueserver is just a fancy way of saying "pyspades, but in a context where notafile/sbyte/utf screwed something up"
		gsub("piqueserver", "<span class=joke>&</span>");
		gsub("BetterSpades", "<span class=joke>&</span>");
	}

	gsub("&nbsp;", " ");
	gsub("&minus;", "−");
	gsub("&le;", "≤");
	gsub("&ge;", "≥");
	gsub("&ne;", "≠");
	gsub("---", "—");
	gsub("--", "–");
	gsub("``", "“");
	gsub("`", "‘");
	gsub("''", "”");
	gsub("'", "’");
	gsub("&apos;", "'");

	printf cont"%s", $0;
	cont = " ";
}}

{
	if (!autofmt)
		print $0;
}

END {
	printf "<footer><small>This work is marked <a rel=noreferrer href=https://creativecommons.org/publicdomain/zero/1.0/>CC0 1.0 Universal</a>. Public domain.</small></footer></article></main>";
}
