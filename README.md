# Nginx mmap geo module
   ngx_geo_mod 是一个地理位置信息查询模块。其根据客户端IP查询出对应的省份，城市，ISP信息，写入请求头。
   
   本模块包含一个将文本格式的地理位置信息编译成二进制数据的编译器。模块运行时直接使用二进制数据，并且采用mmap方式加载文件，不需要任何解析过程。

# Table of Contents
* [完整示例](#synopsis)
* [Nginx兼容性](#compatibility)
* [模块编译](#installation)
* [编译GEO编译器](#compile-the-geo-compiler)
* [模块指令](#directives)
    * [geodata_file](#geodata_file)
    * [mm_geo](#mm_geo)
    * [ip_from_url](#ip_from_url)
    * [ip_from_head](#ip_from_head)
    * [proxies](#proxies)
    * [proxies_recursive](#proxies_recursive)
* [获取IP的方式](#gets-the-ip-order)
* [变量的使用](#variable-usage)
* [编译器的使用](#compile-geo-data-file)
    * [数据文件格式](#geo-data-file-format)
    * [编译数据文件](#compile-data-file)
    * [使用geot测试二进制GEO文件](#test-binary-geo-data-file-by-geot)

# Synopsis
```nginx
http {
    include       mime.types;
    default_type  application/octet-stream;

    # set the data file path
    geodata_file /root/work/GitHub/ngx_geo_mod/top20.geo;
    # Get the IP address from the URL, only for testing.
    ip_from_url on;
    # Get the IP address from the request header, set to on when front-end have proxy.
    ip_from_head on;
    # Set the trusted proxy address. when the ip_from_head is true to used
    proxies 127.0.0.1/24;
    proxies_recursive on;

    # used variables in access log.
    log_format  main  '$remote_addr@$http_x_province $http_x_city '
                      '$http_x_isp [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"';
    access_log  logs/access.log  main;

    # Use the geo 
	mm_geo on;
    server {
        listen       80;
        server_name  localhost;
        location /area {
            # used variables in module
        	echo "      ip: $http_x_ip";
        	echo "province: $http_x_province";
        	echo "    city: $http_x_city";
        	echo "     isp: $http_x_isp";
        }

        location /not_used {
            # do not use the geo
            mm_geo off;
            echo "$http_x_province";
        }
    }
}

```
# Compatibility
本模块兼容以下版本nginx:
* 1.7.x (last tested: 1.7.4)
* 1.6.x (last tested: 1.6.1)
* 1.4.x (last tested: 1.4.7)
* 1.2.x (last tested: 1.2.9)
* 1.0.x (last tested: 1.0.15)

# Installation
```shell
# echo-nginx-module只是测试时需要使用,本模块并不依赖它。
cd nginx-1.x.x
./configure --add-module=path/to/ngx_geo_mod \
            --add-module=path/to/echo-nginx-module
make
make install
```

# Compile The GEO Compiler
```shell
cd path/to/ngx_geo_mod
make
>gcc -g geodata_compiler.c array.c -o geoc
>gcc -D_TOOLS_ -g geodata.c -o geot
>gcc -g -Werror geodata.c -fPIC -shared -o libgeo.so
```
* geoc是GEO编译器
* geot是GEO数据文件测试程序
* libgeo.so 是动态库，可以在各种程序中调用该模块。

# Directives
* [geodata_file](#geodata_file)
* [mm_geo](#mm_geo)
* [ip_from_url](#ip_from_url)
* [ip_from_head](#ip_from_head)
* [proxies](#proxies)
* [proxies_recursive](#proxies_recursive)

geodata_file
----------
**syntax:** *geodata_file &lt;path to binary geodata file&gt;*

**default:** *--*

**context:** *http*

指定二进制的GEO数据文件路径。该数据文件使用geoc编译生成。生成方法参考[编译器的使用](#compile-geo-data-file)

mm_geo
----------
**syntax:** *mm_geo &lt;on | off&gt;*

**default:** *off*

**context:** *http*,*server*,*location*,*location if*

打开或者关闭geo模块。打开GEO模块后，HTTP请求头中会添加4个自定义头：
* x-province *省份信息*
* x-city *城市信息*
* x-isp *ISP信息*
* x-ip *IP*

请求头可以在nginx各种模块中使用。使用方法见[变量的使用](#variable-usage)。

ip_from_url
----------
**syntax:** *ip_from_url &lt;on | off&gt;*

**default:** *off*

**context:** *http*

设置是否可以从HTTP请求参数中获取IP信息，该指令主要用于测试。
打开后，可以通过get参数*ip*指定IP信息，如： `http://server/url?ip=192.168.1.40 `


ip_from_head
----------
**syntax:** *ip_from_head &lt;on | off&gt;*

**default:** *off*

**context:** *http*

设置是可以从HTTP请求头获取IP地址信息，当设置成on时，模块会获取X-Real-IP或X-Forwarded-For中的IP地址。当nginx前端是代理时可使用该选项。

proxies
----------
**syntax:** *proxies &lt;address | CIDR&gt;*

**default:** *--*

**context:** *http*

设置受信任的代理地址。当需要从请求头X-Forwarded-For中获取IP地址，并且nginx版本大于1.3.13时需要使用该指令。

proxies_recursive
----------
**syntax:** *proxies_recursive &lt;on | off&gt;*

**default:** *off*

**context:** *http*

If recursive search is disabled, the original client address that matches one of the trusted addresses is replaced by the last address sent in the request header field defined by the real_ip_header directive.

If recursive search is enabled, the original client address that matches one of the trusted addresses is replaced by the last non-trusted address sent in the request header field.

# Gets The IP Order
GEO模块获取IP地址的的顺序如下：
* 如果ip_from_url开启了，从请求GET参数ip中获取。
* 如果ip_from_head开启了：
    * 先从请求头X-Real-IP中获取，如果没有，从请求头X-Forwarded-For中获取。
* 如果以上两个选项都未开启，使用socket中的实际IP地址。

# Variable Usage
mm_geo指令设置成on后，如果查找到相应的GEO信息，会添加4个自定义信息到请求头中。请求头见指令[mm_geo](#mm_geo)。这些请求头可以跟其它请求头一样在nginx各模块中引用。访问方式是：$http_请求头 。
* 在access log中使用：

```nginx
log_format  main  '$http_x_province $http_x_city $http_x_isp xxxx';
```

* 在echo模块中使用

```nginx
location /area {
    # used variables in module
	echo "      ip: $http_x_ip";
	echo "province: $http_x_province";
	echo "    city: $http_x_city";
	echo "     isp: $http_x_isp";
}
```

# Compile Geo Data File
##### Geo Data File Format
使用编译器编译GEO前，先准备好相应的文本格式数据文件。文件格式如下：
```
################# comment ##################
1.2.3.0  1.2.3.255   BeiJing   BeiJing Unicom
1.2.4.0  1.2.4.255   BeiJing   BeiJing Telecom
1.2.5.0  1.3.5.255   BeiJing   BeiJing Mobile
2.1.1.0  2.1.2.255   HuBei Wuhan Unicom
```
* 以#开头的行为注释，编译器会直接忽略掉。
* 数据一共5列，分别为：IP段开始值，开始段结束值，省份，城市，ISP。列之间以一个或多个空格(或Tab)分割。
* 数据必须是按IP排好序的。否则编译时会出错。注意：如果IP段有包含关系，同样会判定为未排序的，会导致编译错误。

##### Compile Data File
```shell
./geoc geodata.txt
--------- Compile geodata.txt OK
--------- Output: geodata.geo
```
编译好之后，就可以拿geodata.geo去mm_geo模块中使用了。

##### Test Binary GEO Data File By geot
```shell
./geot geodata.geo
#运行之后程序会提示输入IP，输入一个IP后，会显示查询结果。
Input a ip:1.2.4.1
> [1.2.4.1] -----------  [1.2.4.0-1.2.4.255 BeiJing BeiJing Telecom]
```

Authors
=======

* liuxiaojie (刘小杰)  <jie123108@163.com>

[Back to TOC](#table-of-contents)

Copyright & License
===================

This module is licenced under the BSD license.

Copyright (C) 2014, by liuxiaojie (刘小杰)  <jie123108@163.com>

All rights reserved.
