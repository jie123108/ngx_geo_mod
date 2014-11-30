# coding=utf8

import sys
import re 
import socket 
import struct

def ip2long(ipstr): 
    return struct.unpack("!I", socket.inet_aton(ipstr))[0]

def long2ip(ip): 
    return socket.inet_ntoa(struct.pack("!I", ip))

if len(sys.argv) != 2:
	print("Usage: python " + sys.argv[0] + " 纯真IP库文件.txt\n".decode("utf8"))
	sys.exit(1)

filename = sys.argv[1]

unproc_province_citys = {}
provinces = {}
unproc_citys = {}
citys = {}



def province_split(province_city):
	indexstr = ["省", "内蒙古", "广西", "新疆", "西藏", "黑龙江","宁夏", "市"]
	for s in indexstr:
		index = province_city.find(s)
		if index >= 0:
			province = province_city[:index+len(s)]
			if s == "市":
				city = province
			else:
				city = province_city[index+len(s):]
			return True, province, city
	return False, None, None

def city_split(orig_city):
	indexstr = ["市", "地区", "州", "县", "乌鲁木齐"]
	for s in indexstr:
		index = orig_city.find(s)
		if index >= 0:
			city = orig_city[:index+len(s)]
			return True, city
	return False, None

#是某一个ISP的子网段。
def is_subnet(isp):
	indexstr = ["网吧", "会所", "宾馆", "学校", "小学", "大学", "中学", "高中", "学院", "公司"]
	for s in indexstr:
		index = isp.find(s)
		if index >= 0:
			return True
	return False

def is_samenet(isp, pre_isp):
	return isp.find(pre_isp) >= 0 or pre_isp.find(isp) >= 0

datas = []

file = open(filename, 'r')
try:
	prev_node = None
	while 1:
		line = file.readline()
		if not line:
			break
		line = line.strip()
		if line.startswith("#") or line == "":
			#print "comment:", line
			continue
		arr = re.split("\s*", line)
		if len(arr) != 4:
			# 纯真库中使用这个分割后，就不符合这个规范。
			#print "Invalid Line:", line
			continue
 
		ip_start = arr[0]
		ip_end = arr[1]
		province_city = arr[2]
		isp = arr[3]
		
		ok, province, orig_city = province_split(province_city)
		if not ok:
			unproc_province_citys[province_city] = 1
			continue
		provinces[province] = 1
		ok, city = city_split(orig_city)
		if not ok:
			unproc_citys[orig_city] = 1
			continue
		citys[city] = 1
		i_ip_start = ip2long(ip_start)
		i_ip_end = ip2long(ip_end)

		#print "The info:", province, city, isp, ",ip_start:", i_ip_start
		same = (prev_node != None and prev_node["end"] + 1 == i_ip_start
				and prev_node["province"] == province
				and prev_node["city"] == city
				and (prev_node["isp"] == isp or is_subnet(isp) or is_samenet(isp, prev_node["isp"])))
		if same:
			prev_node["end"] = i_ip_end
		else:
			prev_node = {"begin":i_ip_start, "end":i_ip_end, 
						"province":province, "city":city, "isp":isp}
			#print "pre_node: ", prev_node
			datas.append(prev_node)
finally:
	 file.close( )

'''
print "############## province ###################"
for p in provinces.keys():
	print "province: ", p

print "############## city ########################"
for c in citys.keys():
	print "city: ", c

print "##############  unproc province city ############"
for pc in unproc_province_citys.keys():
	print "province_city: ", pc

print "##############  unproc city ############"
for pc in unproc_citys.keys():
	print "city: ", pc
'''

for node in datas:
	print "%s\t%s\t%s\t%s\t%s" % (long2ip(node["begin"]), long2ip(node["end"]), node["province"], node["city"], node["isp"])