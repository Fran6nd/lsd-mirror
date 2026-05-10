BEGIN {
	pad = "";
	lua_calls = "";
	lua_reg = "";
	print("#ifndef LS2_SERVER_LUAAWK_H");
	print("#define LS2_SERVER_LUAAWK_H\n");
}

END {
	printf("\nstatic void register_luaawk(lua_State *l, struct State *st) {\n\t(void)l;\n%s}\n", lua_reg);
	printf("\n#define LUA_CALLS%s\n\n#endif\n", lua_calls);
}

function extract_name(arg) {
	toklen = split(arg, tokens, " ");
	match(tokens[toklen], /[_a-zA-Z0-9]*$/);
	return substr(tokens[toklen], RSTART, RLENGTH);
}

function largs(argv) {
	defer = "";
	# Ignore the last arg (struct State *st) here, we don't need it
	for (i=1;i<argc;i++) {
		# It's pretty, I know.
		if (match(argv[i], /^plid /))
			pullfunc = "%s = check_plid(l, %i);";
		else if (match(argv[i], /^bplid /))
			pullfunc = "%s = check_bplid(l, %i);";
		else if (match(argv[i], /^nplid /))
			pullfunc = "%s = check_nplid(l, %i);";
		else if (match(argv[i], /^teamid /))
			pullfunc = "%s = check_teamid(l, %i);";
		else if (match(argv[i], /^gteamid /))
			pullfunc = "%s = check_gteamid(l, %i);";
		else if (match(argv[i], /^clk /))
			pullfunc = "%s = check_clk(l, %i);";
		else if (match(argv[i], /^fvec3 /))
			pullfunc = "%s = get_fvec3(l, %i);";
		else if (match(argv[i], /^ivec3 /))
			pullfunc = "%s = get_ivec3(l, %i, 1);";
		else if (match(argv[i], /^color /)) {
			# This one's annoying. TODO: color -> struct already. . .
			printf("\t%s;\n", argv[i]);
			defer = defer sprintf("\tget_color2(l, %i, %s);\n", i, extract_name(argv[i]));
			continue;
		} else if (match(argv[i], /^size_t /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^unsigned /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^uint32_t /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^int /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^bint /))
			pullfunc = "%s = lua_toboolean(l, %i);";
		else if (match(argv[i], /^float /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^double /))
			pullfunc = "%s = luaL_checknumber(l, %i);";
		else if (match(argv[i], /^const char \*/))
			pullfunc = "%s = luaL_checkstring(l, %i);";
		else
			pullfunc = "#error mystery type";

		printf("\t"pullfunc"\n", argv[i], i);
	}

	# TODO: <= or just ==?
	if (argc <= 1)
		printf("\t(void)l;\n");

	if (defer != "") {
		printf("\n%s", defer);
	}
}

function do_func(ret, type) {
	match($0, /\(\*[a-z_]*\)/);
	name = substr($0, RSTART+2, RLENGTH-3);

	match($0, /\)\([][_a-zA-Z0-9, *]*\)/);
	args = substr($0, RSTART+2, RLENGTH-3);
	argc = split(args, argv, ", ");

	lua_calls = lua_calls sprintf(" \\\n\t{\"%s\", l%s},", name, name);
	lua_reg = lua_reg sprintf("\tst->f.%s = c%s;\n", name, name);

	printf(pad"static int l%s(lua_State *l) {\n", name);
	largs(argv);

	printf("\n\t%sf.%s(", ret ? "lua_pushnumber(l, " : "", name);
	argsep = "";
	for (i=1;i<=argc;i++) {
		printf(argsep"%s", extract_name(argv[i]));
		argsep = ", ";
	}
	printf(")%s;\n\treturn %i;\n}\n\n", ret ? ")" : "", ret);

	printf("static %s c%s(%s) {\n\t%s(void)st;\n\tlua_getglobal(l, \"%s\");\n\n", type, name, args, ret ? type" ret;\n\n\t" : "", name);

	for (i=1;i<argc;i++) {
		if (match(argv[i], /^plid /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^bplid /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^nplid /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^teamid /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^gteamid /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^clk /))
			pushfunc = "push_clk(l, %s);";
		else if (match(argv[i], /^fvec3 /))
			pushfunc = "push_fvec3(%s);";
		else if (match(argv[i], /^ivec3 /))
			pushfunc = "push_ivec3(%s);";
		else if (match(argv[i], /^color /))
			pushfunc = "push_color(%s);";
		else if (match(argv[i], /^size_t /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^unsigned /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^uint32_t /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^int /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^bint /))
			pushfunc = "lua_pushboolean(l, %s);";
		else if (match(argv[i], /^float /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^double /))
			pushfunc = "lua_pushnumber(l, %s);";
		else if (match(argv[i], /^const char \*/))
			pushfunc = "lua_pushstring(l, %s);";
		else
			pushfunc = "#error mystery type";

		printf("\t"pushfunc"\n", extract_name(argv[i]));
	}

	printf("\n\tif (lua_pcall(l, %i, %i, 0) != 0)\n\t\tCBAIL%s(\"%s: %%s\", luaL_checkstring(l, -1));\n%s}\n", argc-1, ret, ret ? "N1" : "", name, \
	       ret ? "\n\tif (!lua_isnumber(l, -1))\n\t\tCBAIL1N1(\""name": should return a number\");\n\n\tret = lua_tonumber(l, -1);\n\tlua_pop(l, 1);\n\treturn ret;\n" : "");

	pad = "\n\n";
}

# TODO: ffi interface?
# TODO: handle vecs
# TODO: find better things to return than -1 in CBAIL for functions with ret
/struct State \*st);/ {
	if (match($0, /send_state_/)) next;
	if (match($0, /ENetPacket/)) next;
	if (match($0, /send_packet/)) next;
	if (match($0, /on_[a-z]*_packet/)) next;
	if (match($0, /on_version/)) next;
	if (match($0, /_from_mem/)) next;
	# TODO: automate typing
	if (match($0, /void \(\*/)) do_func(0, "void");
	if (match($0, /int \(\*/)) do_func(1, "int");
	if (match($0, /clk \(\*/)) do_func(1, "clk");
	if (match($0, /uint32_t \(\*/)) do_func(1, "uint32_t");
	if (match($0, /size_t \(\*/)) do_func(1, "size_t");
}
