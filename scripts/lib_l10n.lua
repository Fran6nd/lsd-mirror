-- lib_l10n.lua -- Allow scripts to send chat in multiple languages

-- TODO: hook openspades fancy version packet
function get_lang(pid)
	return "en";
end
server.get_lang = get_lang;

local function interp(str, tab)
	return string.gsub(str, "%%%b()", function(mtch) return tab[string.sub(mtch, 3, -2)]; end);
end

function l10n_get_str_lang(lang, msgtab, interptab)
	local msg = msgtab[lang];
	if (msg == nil) then
		-- TODO: determine most preferable fallback for a given language
		return msgtab["en"];
	end

	return msg;
end

-- Try to only pass a singular, non-broadcast PID.
function l10n_get_str_pid(pid, msgtab, interptab)
	return l10n_get_str_lang(get_lang(i), msgtab, interptab);
end

-- TODO: support e.g. pt-BR, pt
function l10n_send_chat(pid, msgtab, interptab)
	for i in piditer(pid) do
		send_chat(i, l10n_get_str_pid(pid, msgtab, interptab), 2, 0);
	end
end
