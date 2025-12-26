-- lib_totp.lua -- not a burner copied the SHA-1 implementation off of wikipedia
local mod = {};

local h = {
	0x67452301,
	0xefcdab89,
	0x98badcfe,
	0x10325476,
	0xc3d2e1f0
}

local function int32tobytes(val)
	local out = {};
	for i=0,3 do
		out[i+1] = bit.band(bit.rshift(val, (3-i)*8), 0xff);
	end
	return string.char(unpack(out));
end

local function int64tobytes(val)
	-- bitops only work on 32 bits, thus the crusty division
	return int32tobytes(math.floor(val / 4294967296)) .. int32tobytes(val);
end

local function bytestoint(val)
	local out = 0;
	for i=0,#val-1 do
		out = bit.bor(out, bit.lshift(string.byte(val, #val-i), i*8));
	end
	return out;
end

-- TODO: should i use number arrays instead of lstrings?
local function sha1(data)
	local len = #data*8;
	local words = {};
	local ha = {};
	data = data .. '\x80' .. string.rep('\0', 64-(#data+1+8)%64) .. int64tobytes(len);
	local off = 0;

	local hb = {};
	for i=1,5 do
		hb[i] = h[i];
	end
	for off=1,#data,64 do
		for i=0,16 do
			words[i] = bytestoint(string.sub(data, off+i*4, off+3+i*4));
		end

		for i=16,79 do
			words[i] = bit.rol(bit.bxor(bit.bxor(bit.bxor(words[i-3], words[i-8]), words[i-14]), words[i-16]), 1);
		end

		for i=1,5 do
			ha[i] = hb[i];
		end

		for i=0,79 do
			if (i < 20) then
				ha[6] = bit.bor(bit.band(ha[2], ha[3]), bit.band(bit.bnot(ha[2]), ha[4]));
				ha[7] = 0x5a827999;
			elseif (i < 40) then
				ha[6] = bit.bxor(bit.bxor(ha[2], ha[3]), ha[4]);
				ha[7] = 0x6ed9eba1;
			elseif (i < 60) then
				ha[6] = bit.bor(bit.bor(bit.band(ha[2], ha[3]), bit.band(ha[2], ha[4])), bit.band(ha[3], ha[4]));
				ha[7] = 0x8f1bbcdc;
			else
				ha[6] = bit.bxor(bit.bxor(ha[2], ha[3]), ha[4]);
				ha[7] = 0xca62c1d6;
			end

			local tmp = bit.rol(ha[1], 5) + ha[5] + ha[6] + ha[7] + words[i];
			ha[5] = ha[4];
			ha[4] = ha[3];
			ha[3] = bit.rol(ha[2], 30);
			ha[2] = ha[1];
			ha[1] = tmp;
		end

		for i=1,5 do
			hb[i] = bit.tobit(ha[i] + hb[i]);
		end
	end

	local out = "";
	for i=1,5 do
		out = out .. int32tobytes(hb[i]);
		end

	return out;
end

function mod.hmac_sha1(key, msg)
	local ipad = "";
	local opad = "";

	if (#key > 64) then
		key = sha1(key);
	end

	if (#key < 64) then
		key = key .. string.rep('\0', 64-#key);
	end

	for i=1,#key do
		ipad = ipad .. string.char(bit.bxor(string.byte(key, i), 0x36));
		opad = opad .. string.char(bit.bxor(string.byte(key, i), 0x5c));
	end

	return sha1(opad .. sha1(ipad .. msg));
end

-- key is normally represented in base32, but should be passed in here unencoded
-- Probably you want to pass in os.time()/30 as counter
function mod.gen_code(key, counter, hmac)
	local ac = hmac(key, int64tobytes(math.floor(counter)));
	local off = bit.band(string.byte(ac, -1), 0xf);
	local num = bit.band(bytestoint(string.sub(ac, off+1, off+4)), 0x7fffffff) % 1000000;
	return string.format("%06u", num);
end

return mod;
