-- wire_dbg.lua -- Display trash sent on the wire
local mod = init_mod();

getcfg("wire_dbg_len", 16);
getcfg("wire_dbg_ignore", {});
getcfg("wire_dbg_ignore_in", {[0]=true, [1]=true});
getcfg("wire_dbg_ignore_out", {[2]=true});

local function tohex(data)
	local hex = "";

	for i=1,wire_dbg_len do
		if (i > #data) then
			break;
		end

		if (i % 4 == 1) then
			hex = hex.." ";
		end

		hex = hex..string.format("%02x", string.byte(data, i));
	end

	if (#data > wire_dbg_len) then
		hex = hex.."...";
	end

	return hex;
end

-- If not not inp, assume out
local function log_packet(pid, fmt, data, inp)
	if (
		#data == 0 or
		(
			not wire_dbg_ignore[string.byte(data, 1)] and
			(inp or not wire_dbg_ignore_out[string.byte(data, 1)]) and
			(not inp or not wire_dbg_ignore_in[string.byte(data, 1)])
		)
	) then
		log(fmt, pid<0 or pid>=MAX_PLAYERS and "b" or " ", #data, tohex(data));
	end
end

function mod.before.on_any_packet(pid, data)
	log_packet(pid, "recv %s : %5u:%s", data, true);
end

function mod.before.send_packet(pid, data)
	log_packet(pid, "send %s : %5u:%s", data);
end

function mod.before.send_packet_unreliable(pid, data)
	log_packet(pid, "send %su: %5u:%s", data);
end

return mod;
