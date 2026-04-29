BEGIN {
	term = "";
	printf "<!doctype html><html lang=en><meta charset=utf-8><title>LSd Lua Api</title><meta name=viewport content=\"width=device-width,initial-scale=1\"><meta name=color-scheme content=\"dark light\"><style>body{margin:6rem 3rem;overflow-y:scroll;line-height:1.4}main{margin:0 auto;max-width:35rem;columns:14rem}h1{text-align:center;column-span:all;+h2{margin-top:0}}li{overflow-wrap:anywhere}footer{text-align:center;column-span:all;margin:1.5rem 0}</style><main><article><h1>LSd Lua Api</h1>";
}

/^# / {
	split($0, args, "[ ~]");
	sub(".html$", "", args[3]);
	sect = args[3];
	funct = args[2];

	if (!seen[sect]) {
		seen[sect] = 1;

		uppersect = toupper(substr(sect, 1, 1)) substr(sect, 2);
		while (match(uppersect, "_"))
			uppersect = substr(uppersect, 1, RSTART-1) " " toupper(substr(uppersect, RSTART+1, 1)) substr(uppersect, RSTART+2);

		printf term"<h2><a href=%s.html>%s</a></h2><ul>", sect, uppersect;
		term = "</ul>"
	}

	printf "<li>%s", funct;
}

END {
	printf term"<footer><small>This work is marked <a rel=noreferrer href=https://creativecommons.org/publicdomain/zero/1.0/>CC0 1.0 Universal</a>. Public domain.</small></footer></article></main>";
}
