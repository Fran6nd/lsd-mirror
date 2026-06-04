-- lib_db.lua -- Nicer interface to a SQLite3 database
local mod = {};
local sql = require "lsqlite3";

mod.OK = sql.OK;
mod.DONE = sql.DONE;
mod.ROW = sql.ROW;

local codemap = {
	[sql.OK]="OK",
	[sql.ERROR]="ERROR",
	[sql.INTERNAL]="INTERNAL",
	[sql.PERM]="PERM",
	[sql.ABORT]="ABORT",
	[sql.BUSY]="BUSY",
	[sql.LOCKED]="LOCKED",
	[sql.NOMEM]="NOMEM",
	[sql.READONLY]="READONLY",
	[sql.INTERRUPT]="INTERRUPT",
	[sql.IOERR]="IOERR",
	[sql.CORRUPT]="CORRUPT",
	[sql.NOTFOUND]="NOTFOUND",
	[sql.FULL]="FULL",
	[sql.CANTOPEN]="CANTOPEN",
	[sql.PROTOCOL]="PROTOCOL",
	[sql.EMPTY]="EMPTY",
	[sql.SCHEMA]="SCHEMA",
	[sql.TOOBIG]="TOOBIG",
	[sql.CONSTRAINT]="CONSTRAINT",
	[sql.MISMATCH]="MISMATCH",
	[sql.MISUSE]="MISUSE",
	[sql.NOLFS]="NOLFS",
	[sql.FORMAT]="FORMAT",
	[sql.RANGE]="RANGE",
	[sql.NOTADB]="NOTADB",
	[sql.ROW]="ROW",
	[sql.DONE]="DONE"
};
local function fmt_code(code)
	return codemap[code] or code;
end

local function init_stmt(db, name, stmtsql)
	local stmt, code = db.con:prepare(stmtsql);
	assert(stmt ~= nil, "db.con:prepare: "..name..": "..db.con:errmsg()..": "..fmt_code(code));

	db.stmt[name] = stmt;
	return stmt;
end

function mod.open(path, creat)
	local con, code, msg = sql.open(path);
	if (con == nil) then
		-- msg may be nil if con ~= nil; illegal to cat it to a string
		error("sql.open: "..msg);
	end

	con:busy_timeout(100);

	code = con:exec([[
		PRAGMA journal_mode = WAL;
		PRAGMA temp_store = memory;
	]]..(creat or ""));

	if (code ~= sql.OK) then
		con:close();
		error("con:exec: "..db.con:errmsg()..": "..fmt_code(code));
	end

	-- Try to avoid memory errors by preparing statements up front
	local db = {con=con, stmt={}};
	init_stmt(db, "begin", "BEGIN;");
	init_stmt(db, "commit", "COMMIT;");
	init_stmt(db, "rollback", "ROLLBACK;");

	return db;
end

function mod.close(db)
	if (db == nil) then
		return;
	end

	db.con:close_vm();
	db.con:close();
end

function mod.transact(db, func)
	db.stmt.begin:reset();
	local code = db.stmt.begin:step();
	assert(code == sql.DONE, "begin:step: "..fmt_code(code));

	local status, err = pcall(func);

	if (not status) then
		db.stmt.rollback:reset();
		db.stmt.rollback:step();
		error(err, 2);
	end

	db.stmt.commit:reset();
	code = db.stmt.commit:step();
	if (code ~= sql.DONE) then
		db.stmt.rollback:reset();
		db.stmt.rollback:step();
		error("db.con:exec: "..fmt_code(code));
	end
end

function mod.exec(db, str)
	local code = db.con:exec(str);

	if (code ~= sql.OK) then
		error("db.con:exec: "..db.con:errmsg()..": "..fmt_code(code));
	end

	return code;
end

function mod.finalize(db, name)
	db.stmt[name]:finalize();
end

function mod.prepare_0ret(db, name, stmtsql)
	local stmt = init_stmt(db, name, stmtsql);

	return function(...)
		local code;

		stmt:reset();
		code = stmt:bind_values(...);
		assert(code == sql.OK, name..":bind_values: "..fmt_code(code));

		code = stmt:step();
		assert(code == sql.DONE or code == sql.CONSTRAINT, name..":step: "..fmt_code(code));

		return db.con:changes();
	end
end

function mod.prepare_1ret(db, name, stmtsql)
	local stmt = init_stmt(db, name, stmtsql);

	return function(...)
		local code;

		stmt:reset();
		code = stmt:bind_values(...);
		assert(code == sql.OK, name..":bind_values: "..fmt_code(code));

		code = stmt:step();

		if (code == sql.DONE) then
			return;
		end

		assert(code == sql.ROW, name..":step: "..fmt_code(code));
		return stmt:get_values();
	end
end

function mod.prepare_xret(db, name, stmtsql)
	local stmt = init_stmt(db, name, stmtsql);

	return function(...)
		local code;

		stmt:reset();
		code = stmt:bind_values(...);
		assert(code == sql.OK, name..":bind_values: "..fmt_code(code));

		return stmt:urows();
	end
end

function mod.init_schema(db, schema, upgrade)
	local checkexists = mod.prepare_1ret(db, "checkexists", "SELECT EXISTS(SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ?);");
	upgrade = upgrade or {};

	mod.transact(db, function()
		mod.exec(db, schema);

		for tbl,upgrsql in pairs(upgrade) do
			if (checkexists(tbl)[1] == 1) then
				mod.finalize(db, "checkexists");
				mod.exec(db, upgrsql);
				return;
			end
		end

		mod.finalize(db, "checkexists");
	end);
end

return mod;
