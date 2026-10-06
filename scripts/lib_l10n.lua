-- lib_l10n.lua -- Allow scripts to send chat in multiple languages
local mod = init_mod();

getcfg("l10n_default_lang", "en");
getcfg("l10n_log_lang", "en");

function mod.impl.l10n_get_lang(pid)
	return l10n_default_lang;
end

function mod.l10n_get_lang(pid)
	if (is_fakepid(pid)) then
		return mod.next.l10n_get_lang(pid);
	end

	local extlang = get_client_ext_language(pid);

	if (#extlang == 0) then
		return mod.next.l10n_get_lang(pid);
	end

	if (#extlang == 5) then
		return string.lower(string.sub(extlang, 1, 2)).."_"..string.upper(string.sub(extlang, 4, 5));
	end

	return string.lower(extlang);
end

local lang_fallback = {
	en_US={"en"}
};

local function interp(str, tab)
	return string.gsub(str, "%%%b()", function(mtch) return tab[string.sub(mtch, 3, -2)]; end);
end

function mod.impl.l10n_get_str_lang(lang, msgtab, interptab)
	local msg = msgtab[lang];
	if (msg == nil and lang_fallback[lang] ~= nil) then
		for _,lang in pairs(lang_fallback[lang]) do
			msg = msgtab[lang];
			if (msg ~= nil) then
				break;
			end
		end
	end

	if (msg == nil) then
		msg = msgtab[l10n_default_lang];
	end

	if (msg == nil) then
		msg = msgtab["en"];
	end

	return interp(msg, interptab);
end

-- Try to only pass a singular, non-broadcast PID.
function mod.impl.l10n_get_str_pid(pid, msgtab, interptab)
	return l10n_get_str_lang(l10n_get_lang(pid), msgtab, interptab);
end

-- TODO: make BROADCAST_ plids unsigned?
function mod.impl.l10n_send_chat(pid, msgtab, interptab)
	if (is_fakepid(pid)) then
		server_msg(pid, l10n_get_str_pid(pid, msgtab, interptab));
		return;
	end
	-- TODO: support invalid PIDs?
	for i in piditer(pid) do
		server_msg(i, l10n_get_str_pid(i, msgtab, interptab));
	end
end

function mod.impl.l10n_log(msgtab, interptab)
	log("%s", l10n_get_str_lang(l10n_log_lang, msgtab, interptab));
end

return mod;
