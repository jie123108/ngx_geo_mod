# Nginx mmap geo module
ngx_geo_mod is a geographic location information module. According to the client IP query it the provinces, cities, ISP information, write to http request header.(Baidu Translate)
   
   This module contains a compiler that compiled text format geo data into binary data. This module use of this binary data, and the use of mmap to load the file to memory, does not require any analytical process.

# Table of Contents
* [synopsis](#synopsis)
* [Nginx Compatibility](#compatibility)
* [Installation](#installation)
* [Compile The Geo Compiler](#compile-the-geo-compiler)
* [Directives](#directives)
    * [geodata_file](#geodata_file)
    * [mm_geo](#mm_geo)
    * [ip_from_url](#ip_from_url)
    * [ip_from_head](#ip_from_head)
    * [proxies](#proxies)
    * [proxies_recursive](#proxies_recursive)
* [Gets The IP Order](#gets-the-ip-order)
* [Variable Usage](#variable-usage)
* [Compile Geo Data File](#compile-geo-data-file)
    * [Geo Data File Format](#geo-data-file-format)
    * [Compile Geo Data File](#compile-data-file)
    * [Test Geo Data File By geot](#test-binary-geo-data-file-by-geot)

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
This module is compatible with the following version of nginx:
* 1.7.x (last tested: 1.7.4)
* 1.6.x (last tested: 1.6.1)
* 1.4.x (last tested: 1.4.7)
* 1.2.x (last tested: 1.2.9)
* 1.0.x (last tested: 1.0.15)

# Installation
```shell
# echo-nginx-module is for testing only,  this module does not depend on it.
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
* geoc: is the geo data compiler.
* geot: is test program for the geo data.
* libgeo.so is dynamic library.

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

Specified The GEO data file path. The data file using geoc compiled.[Compile Geo Data File](#compile-geo-data-file)

mm_geo
----------
**syntax:** *mm_geo &lt;on | off&gt;*

**default:** *off*

**context:** *http*,*server*,*location*,*location if*

Open or close the geo module. If open the GEO module, the HTTP request header will add 4 custom head:
* x-province *province info*
* x-city *city info*
* x-isp *ISP info*
* x-ip *IP*

Those request heads can by used in other modules, See also: [Variable Usage](#variable-usage)。

ip_from_url
----------
**syntax:** *ip_from_url &lt;on | off&gt;*

**default:** *off*

**context:** *http*

If set to on, module can be get IP info by request args,  the instruction is mainly used for testing.
if set to on, Can set the IP info by http args *ip*, such as： `http://server/url?ip=192.168.1.40 `


ip_from_head
----------
**syntax:** *ip_from_head &lt;on | off&gt;*

**default:** *off*

**context:** *http*

If set to on, module can be get IP info by Request Header “X-Real-IP” or “X-Forwarded-For”, When the nginx front end have a agent can use this option.


proxies
----------
**syntax:** *proxies &lt;address | CIDR&gt;*

**default:** *--*

**context:** *http*
Defines trusted addresses. When a request comes from a trusted address, an address from the “X-Forwarded-For” request header field will be used instead.

proxies_recursive
----------
**syntax:** *proxies_recursive &lt;on | off&gt;*

**default:** *off*

**context:** *http*

If recursive search is disabled, the original client address that matches one of the trusted addresses is replaced by the last address sent in the request header field defined by the real_ip_header directive.

If recursive search is enabled, the original client address that matches one of the trusted addresses is replaced by the last non-trusted address sent in the request header field.

# Gets The IP Order
The GEO module to obtain IP address in the order as follows:
* if ip_from_url set to on, get IP info from http get args “ip”。
* if ip_from_head set to on,get IP info from request head “X-Real-IP”，if have not, from request head “X-Forwarded-For”.
* Otherwise, the use IP of socket.

# Variable Usage
Instruction mm_geo set to on, If find the geo info, will add 4 geo info to the request headers。Those request headers can use in other module, Used for $http_HEADER-NAME.

* Used in access log:

```nginx
log_format  main  '$http_x_province $http_x_city $http_x_isp xxxx';
```

* Used in echo module:

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
Before using the compiler GEO, prepared text format data file. The file format is as follows:
```
################# comment ##################
1.2.3.0  1.2.3.255   BeiJing   BeiJing Unicom
1.2.4.0  1.2.4.255   BeiJing   BeiJing Telecom
1.2.5.0  1.3.5.255   BeiJing   BeiJing Mobile
2.1.1.0  2.1.2.255   HuBei Wuhan Unicom
```
* # comments
* Data have 5 column，Respectively for：IP-begin，IP-end，Province，City，ISP。Between the columns are divided by one or more spaces(or Tab)。
* IP data must be sorted by ip. Otherwise you will get a compiler error. Note: a  IP segment can not contains anther IP segment.

##### Compile Data File
```shell
./geoc geodata.txt
--------- Compile geodata.txt OK
--------- Output: geodata.geo
```
After compilation, datafile.geo can be used for mm_geo module.

##### Test Binary GEO Data File By geot
```shell
./geot geodata.geo
Input a ip:1.2.4.1
> [1.2.4.1] -----------  [1.2.4.0-1.2.4.255 BeiJing BeiJing Telecom]
```

Authors
=======

* liuxiaojie (刘小杰)  <jie123108@gmail.com>

[Back to TOC](#table-of-contents)

Copyright & License
===================

This module is licenced under the BSD license.

Copyright (C) 2014, by liuxiaojie (刘小杰)  <jie123108@163.com>

All rights reserved.
