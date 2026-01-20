BEGIN {
	pad = "";
	lua_calls = "";
	lua_reg = "";
	print("#ifndef LS2_SERVER_LUAAWK_H");
	print("#define LS2_SERVER_LUAAWK_H\n");
}

END {
	printf("\nstatic void register_luaawk(lua_State *l, struct State *st) {%s}\n", lua_reg);
	printf("\n#define LUA_CALLS %s\n#endif\n", lua_calls);
}

# void register_functions(lua_State *l, struct State *st) {
# 	const struct luaL_Reg *func = funcs;
#
# 	while (func->name != NULL) {
# 		lua_pushcfunction(l, func->func);
# 		lua_setglobal(l, func->name);
#
# 		func++;
# 	}
#
# 	luaL_openlib(l, "server", funcs, 0);
#
# 	f = st->f;
# 	st->f.tick = ctick;
# 	st->f.set_color = cset_color;
# 	st->f.send_chat = csend_chat;

function extract_name(arg) {
	toklen = split(arg, tokens, " ");
	match(tokens[toklen], /[_a-zA-Z0-9]*$/);
	return substr(tokens[toklen], RSTART, RLENGTH);
}

/struct State \*st);/ {
	if (match($0, /send_state_/)) next;
	if (match($0, /ENetPacket/)) next;
	if (match($0, /void \(\*/)) {
		match($0, /\(\*[a-z_]*\)/);
		name = substr($0, RSTART+2, RLENGTH-3);

		match($0, /\)\([_a-zA-Z0-9, \*\[\]]*\)/);
		args = substr($0, RSTART+2, RLENGTH-3);
		argc = split(args, argv, ", ");

		lua_calls = lua_calls sprintf("{\"%s\",l%s},", name, name);
		lua_reg = lua_reg sprintf("st->f.%s=c%s;", name, name);

		printf(pad"static int l%s(lua_State *l) {\n", name);
		defer = "";
		# Ignore the last arg (struct State *st) here, we don't need it
		for (i=1;i<argc;i++) {
			# It's pretty, I know.
			if (match(argv[i], /^plid /))
				pullfunc = "%s = luaL_checknumber(l, %i);";
			else if (match(argv[i], /^fvec3 /))
				pullfunc = "%s = get_fvec3(l, %i);";
			else if (match(argv[i], /^ivec3 /))
				pullfunc = "%s = get_ivec3(l, %i);";
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

		printf("\n\tf.%s(", name);
		argsep = "";
		for (i=1;i<=argc;i++) {
			printf(argsep"%s", extract_name(argv[i]));
			argsep = ", ";
		}
		printf(");\n\treturn 0;\n}\n\n");

		printf("static void c%s(%s) {\n\t(void)st;\n\tlua_getglobal(l, \"%s\");\n\n", name, args, name);
		for (i=1;i<argc;i++) {
			if (match(argv[i], /^plid /))
				pushfunc = "lua_pushnumber(l, %s);";
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

		printf("\n\tif (lua_pcall(l, %i, 0, 0) != 0)\n\t\tCBAIL(\"%s: %%s\", luaL_checkstring(l, -1));\n}\n", argc-1, name);
		pad = "\n\n";
	}
}
