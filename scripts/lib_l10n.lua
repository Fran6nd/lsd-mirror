-- lib_l10n.lua -- Allow scripts to send chat in multiple languages

-- TODO: hook openspades fancy version packet
function get_lang(pid)
	return "en";
end
server.get_lang = get_lang;

-- TODO: support e.g. pt-BR, pt
function l10n_send_chat(pid, tab)
	for i in piditer(pid) do
		local msg = tab[get_lang(i)];
		if (msg == nil) then
			-- TODO: determine most preferable fallback for a given language
			msg = tab["en"];
		end

		send_chat(i, msg, 2, 0);
	end
end
