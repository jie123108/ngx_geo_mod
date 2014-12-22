
ffi = require("ffi")

ffi.cdef[[
static const int GEODATA_MAGIC = 0x9E00;

static const int O_RDONLY=0;
static const int SEEK_END=2;
static const int PROT_READ=1;
static const int MAP_SHARED=1;
static const int MAP_FAILED=-1;

struct in_addr {
    uint32_t s_addr;
};

int inet_aton(const char *cp, struct in_addr *inp);
uint32_t ntohl(uint32_t netlong);
char *inet_ntoa(struct in_addr in);
uint32_t htonl(uint32_t hostlong);

char *strerror(int errnum);
int open(const char *pathname, int flags);
int close(int fd);
int64_t lseek(int fd, int64_t offset, int whence);
char *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset);
int munmap(void *addr, size_t length);

typedef struct geo_head{
	uint32_t magic;
	uint32_t const_count; //常量数量
	uint32_t const_table_offset;
	uint32_t geo_item_count;
	uint32_t geo_item_offset;
	uint32_t filesize; //geo文件大小。
	char rest[8]; //保留字段。
}geo_head_t;
static const int GEO_HEAD_LEN = 32;

/**
 * 常量索引
 */
typedef struct {
	int begin;  	//begin 是相对于常量区的偏移。
	int len;		//常量串长度，包括一个\0
}const_index_t;

typedef struct {
	uint32_t ip_begin;
	uint32_t ip_end;
	uint32_t province; 
	uint32_t city;
	uint32_t isp;
}geo_item_t;
]]

local C = ffi.C

function ip2long(ip)
    local inp = ffi.new("struct in_addr[1]")
    if C.inet_aton(ip, inp) ~= 0 then
        return tonumber(C.ntohl(inp[0].s_addr))
    end
    return nil
end

function long2ip(long)
    if type(long) ~= "number" then
        return nil
    end
    local addr = ffi.new("struct in_addr")
    addr.s_addr = C.htonl(long)
    return ffi.string(C.inet_ntoa(addr))
end

function geo_init(geodatafile)
	local fd = tonumber(ffi.C.open(geodatafile, ffi.C.O_RDONLY))
	if fd <= 0 then
		local err = ffi.string(ffi.C.strerror(ffi.errno))
		print("open [", geodatafile, "] failed! err:", err)
		return nil
	end
	local _GEO = {}
	_GEO.size = tonumber(ffi.C.lseek(fd, 0, ffi.C.SEEK_END))

	local shm = ffi.C.mmap(nil, _GEO.size, ffi.C.PROT_READ, ffi.C.MAP_SHARED, fd, 0);

	ffi.C.close(fd);
	if shm == ffi.C.MAP_FAILED then
		local err = ffi.string(ffi.C.strerror(ffi.errno))
		print("mmap(", geodatafile, ", size=", _GEO.size, ") failed! err:", err)
		return nil
	end
	print("mmap successed!")
	_GEO.shm = shm
	return _GEO
end


function cvalue(indexs,buf, index) 
	return ffi.string(buf + indexs[index].begin, indexs[index].len)
end


function geo_find(geo, ip)
	local head = ffi.cast("geo_head_t*", geo.shm)
	
	if geo == nil or ip == nil or ip == "" then
		return nil
	end
	if type(ip) == 'string' then
		ip = ip2long(ip)
		if ip == 0 then
			return nil
		end
	end
	
	local items = ffi.cast("geo_item_t*", geo.shm + head.geo_item_offset)
	if head.filesize ~= geo.size then
		geo.size = head.filesize
	end

	if head.geo_item_count < 1 then
		return nil
	end

	local find_item = nil
	local High = head.geo_item_count-1
	local Low = 0
	local M = nil
	while Low<=High	do
	    M = math.modf((Low + High)/2)
	    --print("Low-M-High:", Low, M,High)
	    if ip<items[M].ip_begin then
	    	--print("111:", items[M].ip_begin)
			High = M-1; 
		elseif ip>items[M].ip_end then 
			--print("222:", items[M].ip_begin)
			Low = M+1; 
		elseif ip>=items[M].ip_begin and ip<=items[M].ip_end then
			--print("333:", items[M].ip_begin)
			find_item = items[M]
			break
		end
	end
	if find_item == nil then
		return nil
	end
	
	local indexs = ffi.cast("const_index_t*", geo.shm + ffi.C.GEO_HEAD_LEN)
	local buf = ffi.cast("const char*", geo.shm + head.const_table_offset)
	local province = cvalue(indexs, buf, find_item.province)
	local city = cvalue(indexs, buf, find_item.city)
	local isp = cvalue(indexs, buf, find_item.isp)

	return province, city, isp, find_item.ip_begin, find_item.ip_end
end

geo = geo_init("./ip.geo")


for i = 1, 1000 do 
	io.write("输入IP:")
	local ip = io.read()

	if ip == "exit" then
		break
	end
	if ip ~= nil then 
		local province, city, isp, ip_begin, ip_end = geo_find(geo, ip)
		if province then
			print(string.format(" ip[%s] -------- [%s %s %s %s %s] ",  ip, long2ip(ip_begin), long2ip(ip_end), province, city, isp))
		else
			print(string.format(" ip[%s] %s", ip, province))
		end
	end
end
