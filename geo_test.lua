local geo = require("geo")


local ok, geo_ctx = geo.init("./top20.txt.geo")
if not ok then
	print("geo.init(%s) failed! err:", geo_ctx)
	return
end

for i = 1, 1000 do 
	io.write("输入IP:")
	local ip = io.read()
	if ip == "exit" then
		break
	end
	local ok, province, city, isp = geo.find(geo_ctx, ip)
	if ok then
		print(string.format(" ip[%s] -------- [%s %s %s] ", ip, province, city, isp))
	else
		print(string.format(" ip[%s] %s", ip, province))
	end
end