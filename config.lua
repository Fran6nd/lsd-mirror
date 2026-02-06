-- Of course, all of this can be overridden by scripts, right?
masterlist_name = "[LS2] democracy24"
team_name = {"bread", "cowboys"}
team_color = {
	{r=0, g=32, b=255},
	{r=255, g=32, b=0}
}
fog = {r=32, g=64, b=128}
max_score = 24

load "pid_tables"
load "commands"
load "command_exec"
load "command_modutils"
load "command_cmds"
load "command_kill"
load "command_say"
load "command_caps"

--load "motd"
motd = [[
HIIIIIIIIIIIIIIIII!
THIS IS A MOTD
WHO COULD HAVE EXPECTED THAT
IT'S MADE IN LUA TOO
have i burned your ears off yet?
]]

--load "tip_spam"
tips = {
	"This is a worthless tip.",
	"Did you learn something new today?",
	"Use /APOC to die.",
	"Press the L key to change teams (unless it's , or .)",
	"TODO: client-conditional tips",
	"Block color won't change? Try the arrow keys and E.",
	"This is not Build and Shoot. This is ACE OF SPADES.",
	"Some day I'll add a /tutor"
}
tip_frequency = 5*60

--load "team_block_colors"
load "masterlist"
load "map_queue"
load "map_meta"
load "mapscripts"
load "fall_damage"
--load "babel"
load "noclip"
load "jp"
load "trashheap"
load "caps"
register(maptime);
load "stdio_console"
--load "sock_console"
load "lib_sock"
load "sockcon2"
load "websock_console"
--load "purgatory"
