-- stdio_console.lua -- Do something with stdin/stdout
local mod = {before={},after={}};

-- TODO: sometimes it thinks there's input on stdin when there is none and gets stuck -- reopen as nonblocking?
local ffi = require("ffi");

-- Wrap the cdef in pcall in an attempt to avoid "attempt to redefine 'linenoiseState'"
pcall(function()ffi.cdef[[
extern char *linenoiseEditMore;

struct linenoiseState {
    int in_completion;  /* The user pressed TAB and we are now in completion
                         * mode, so input is handled by completeLine(). */
    size_t completion_idx; /* Index of next completion to propose. */
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t oldrows;     /* Rows used by last refrehsed line (multiline mode) */
    int history_index;  /* The history index we are currently editing. */
};

typedef struct linenoiseCompletions {
  size_t len;
  char **cvec;
} linenoiseCompletions;

/* Non blocking API. */
int linenoiseEditStart(struct linenoiseState *l, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt);
char *linenoiseEditFeed(struct linenoiseState *l);
void linenoiseEditStop(struct linenoiseState *l);
void linenoiseHide(struct linenoiseState *l);
void linenoiseShow(struct linenoiseState *l);

/* Completion API. */
typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
typedef char*(linenoiseHintsCallback)(const char *, int *color, int *bold);
typedef void(linenoiseFreeHintsCallback)(void *);
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *);
void linenoiseSetHintsCallback(linenoiseHintsCallback *);
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *);
void linenoiseAddCompletion(linenoiseCompletions *, const char *);

/* History API. */
int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);

/* Other utilities. */
void linenoiseClearScreen(void);
void linenoiseSetMultiLine(int ml);
void linenoiseMaskModeEnable(void);
void linenoiseMaskModeDisable(void);
]];end)

local ln = ffi.load("./exec/liblinenoise.so");
local ls;
local buf;
local buflen = 1024;
local editing = false;
local unloading = false;

function mod.on_load()
	-- TODO: more durable way of doing this
	if (grant_cap) then
		grant_cap(32, "all");
	end
	ls = ffi.new("struct linenoiseState[1]");
	buf = ffi.new("char[?]", buflen);
	ln.linenoiseEditStart(ls, 0, 2, buf, buflen, "> ");
	editing = true;
	unloading = false;
end

function mod.on_unload()
	if (editing) then
		ln.linenoiseEditStop(ls);
		editing = false;
	end
	unloading = true;
	-- TODO: cleanup random buffers?
end

function mod.before.on_shutdown()
	if (editing) then
		ln.linenoiseEditStop(ls);
	end
	editing = false;
end

function mod.after.before_log()
	if (editing) then
		ln.linenoiseHide(ls);
	end
end

function mod.after.after_log()
	if (editing) then
		ln.linenoiseShow(ls);
	end
end

function mod.log(...)
	if (editing) then
		ln.linenoiseHide(ls);
		next_call("log", mod.log)(...);
		ln.linenoiseShow(ls);
	else
		next_call("log", mod.log)(...);
	end
end

function mod.send_chat(pid, msg, type, from)
	if (pid == 32) then
		log("%s", msg);
		return;
	end
	next_call("send_chat", mod.send_chat)(pid, msg, type, from);
end

function mod.get_name(pid)
	if (pid == 32) then
		-- TODO: should this be configurable? should i make lots of random trash configurable?
		return "console";
	end
	return next_call("get_name", mod.get_name)(pid);
end

local function handle_line(line)
	-- TODO: handle '/' at start?
	if (#line ~= 0) then
		handle_command(32, line, true);
	end
end

-- TODO: poll with enet fd and stdin, then call func if stdin POLLIN?
-- TODO: prevent linenoise from polluting stdout with \n
function mod.after.tick()
	while (input_on_stdin()) do
		local line = ln.linenoiseEditFeed(ls);

		if (line ~= ln.linenoiseEditMore) then
			ln.linenoiseEditStop(ls);
			editing = false;

			if (line == nil) then
				os.exit();
			end

			ln.linenoiseHistoryAdd(line);
			status, err = pcall(handle_line, ffi.string(line));
			if (not unloading) then
				ln.linenoiseEditStart(ls, 0, 2, buf, buflen, "> ");
				editing = true;
			end
			if (not status) then
				-- TODO: need to show, hide on error
				error(err);
			end
		end
	end
end

return mod;
