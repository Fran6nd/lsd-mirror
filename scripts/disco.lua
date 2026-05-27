-- disco.lua -- Dominate the enemy team with epileptic seizures after winning
local mod = init_mod();
local disco_timer = nil;
local disco_end_timer = nil;
local disco_palette_idx = 1;

getcfg("disco_gameend_time", 10);
getcfg("disco_frequency", 0.25);
getcfg("disco_palette", {
	{r=235, g=64, b=0},
	{r=216, g=94, b=231},
	{r=43, g=72, b=228},
	{r=255, g=255, b=255},
	{r=220, g=223, b=12},
	{r=128, g=232, b=121}
});

local function start_disco()
	disco_timer = get_time() + disco_frequency;

	disco_palette_idx = 1;
	send_fog(PID_BROADCAST, disco_palette[disco_palette_idx]);
end

local function end_disco()
	disco_timer = nil;
	send_fog(PID_BROADCAST, get_fog());

	if (disco_end_timer) then
		disco_end_timer = nil;
		mod.early.next.on_game_end();
	end
end

local cmd = {name="disco", caps="disco", fakepid=true, desc="Toggle the disco party."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	if (disco_timer) then
		end_disco();
		return;
	end

	start_disco();
end
register_command(cmd, mod);

function mod.on_unload()
	if (disco_timer) then
		end_disco();
	end
end

function mod.early.on_game_end()
	if (disco_gameend_time > 0) then
		disco_end_timer = get_time() + disco_gameend_time;
		start_disco();
		return;
	end

	mod.early.next.on_game_end();
end

function mod.after.tick()
	local now = get_time();

	if (disco_end_timer and now >= disco_end_timer) then
		end_disco();
		return;
	end

	if (disco_timer and now >= disco_timer) then
		disco_timer = disco_timer + disco_frequency;

		disco_palette_idx = disco_palette_idx + 1;
		if (disco_palette_idx > #disco_palette) then
			disco_palette_idx = 1;
		end

		send_fog(PID_BROADCAST, disco_palette[disco_palette_idx]);
	end
end

return mod;
