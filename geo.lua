
ffi = require("ffi")

local _M = {}

function find_shared_obj(cpath, so_name)
    --ngx.log(ngx.INFO, "cpath:", cpath, ",so_name:", so_name)
    for k, v in string.gmatch(cpath, "[^;]+") do
        local so_path = string.match(k, "(.*/)")
        --ngx.log(ngx.INFO, "so_path:", so_path)

        if so_path then
            -- "so_path" could be nil. e.g, the dir path component is "."
            so_path = so_path .. so_name

            -- Don't get me wrong, the only way to know if a file exist is
            -- trying to open it.
            local f = io.open(so_path)
            if f ~= nil then
                io.close(f)
                return so_path
            end
        end
    end
end

local so_path = find_shared_obj(package.cpath, "libgeo.so")
local G = ffi.load(so_path)
function _M.init(filename)
	ffi.cdef[[
	typedef struct {
		uint32_t ip_begin;
		uint32_t ip_end;
		const char* province;
		const char* city;
		const char* isp;
	}geo_result_t;

	typedef struct {
		char* ptr;
		int size;
	}geo_ctx_t;

	geo_ctx_t* geo_new();
	int geo_init(geo_ctx_t* geo_ctx, const char* geodatafile);
	void geo_destroy(geo_ctx_t* geo_ctx);
	int geo_find2(geo_ctx_t* geo_ctx, uint32_t ip, geo_result_t* result);
	int geo_find(geo_ctx_t* geo_ctx, const char* ip, geo_result_t* result);
	]]
	local geo_ctx = G.geo_new()
	local ret = tonumber(G.geo_init(geo_ctx, filename))
	if ret == 0 then
		return true, geo_ctx
	else
		local errmsg = nil
		if ret == -1 then
			errmsg = "open geo data file failed"
		elseif ret == -2 then
			errmsg = "no memory"
		elseif ret == -3 then
			errmsg = "mmap failed!"
		elseif ret == -4 then
			errmsg = "geo data file invalid!"
		else
			errmsg = "unknow"
		end
		return false, errmsg
	end
end

function _M.find(geo_ctx, ip)
	local result = ffi.new("geo_result_t[1]")
	local ret = 0
	if type(ip) == "string" then
		ret = tonumber(G.geo_find(geo_ctx, ip, result))
	elseif type(ip) == "number" then
		ret = tonumber(G.geo_find2(geo_ctx, ip, result))
	else
		return false, "input ip's type invalid! type:" .. type(ip)
	end

	if ret == 0 then
		local r = result[0]
		return true, ffi.string(r.province), ffi.string(r.city), 
			ffi.string(r.isp), tonumber(r.ip_begin), tonumber(r.ip_end)
	else
		return false, "can not find ip"
	end
end

function _M.destroy(geo_ctx)
	G.geo_destroy(geo_ctx)
end

return _M