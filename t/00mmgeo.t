# vi:filetype=perl

use lib 'lib';
use lib 't';
use Test::Nginx::Socket;
use Cwd qw(cwd);
my $pwd = cwd();

repeat_each(1);
plan tests => repeat_each() * 2 * blocks() ;

our $http_config = <<"_EOF_";
    geodata_file $pwd/t/geo_data.geo;
    ip_from_url on;
    ip_from_head on;
    mm_geo on;
    proxies 127.0.0.1;
_EOF_

our $location = <<"_EOF_";
        location /area {
            echo "\$http_x_ip,\$http_x_province,\$http_x_city,\$http_x_isp";
        }
_EOF_

our $location_server_geo_off = <<"_EOF_";
        mm_geo off;
        location /area {
            echo "\$http_x_ip,\$http_x_province,\$http_x_city,\$http_x_isp";
        }

        location /geo_on {
            mm_geo on;
            echo "\$http_x_ip,\$http_x_province,\$http_x_city,\$http_x_isp";
        }
_EOF_


no_root_location();
no_shuffle();
run_tests();


__DATA__

=== Test 1: geo simple
--- http_config eval: $::http_config
--- config eval: $::location
--- timeout: 16
--- request
GET /area
--- error_code: 200
--- response_body
127.0.0.1,WLAN,WLAN,WLAN

=== Test 2: ip from url
--- http_config eval: $::http_config
--- config eval: $::location
--- timeout: 16
--- request
GET /area?ip=3.4.5.6
--- error_code: 200
--- response_body
3.4.5.6,GuangDong,ShenZhen,Unicom

=== Test 3: ip from head(x_real_ip)
--- http_config eval: $::http_config
--- config eval: $::location
--- timeout: 16
--- request
GET /area
--- more_headers
X-Real-IP: 1.2.3.4
--- error_code: 200
--- response_body
1.2.3.4,BeiJing,BeiJing,Unicom

=== Test 4: ip from head(x_forwarded_for)
--- http_config eval: $::http_config
--- config eval: $::location
--- timeout: 16
--- request
GET /area
--- more_headers
X-Forwarded-For: 1.2.3.4
--- error_code: 200
--- response_body
1.2.3.4,BeiJing,BeiJing,Unicom

=== Test 5: server geo off
--- http_config eval: $::http_config
--- config eval: $::location_server_geo_off
--- timeout: 16
--- request
GET /area
--- more_headers
X-Forwarded-For: 1.2.3.4
--- error_code: 200
--- response_body
,,,

=== Test 6: location geo on
--- http_config eval: $::http_config
--- config eval: $::location_server_geo_off
--- timeout: 16
--- request
GET /geo_on?ip=1.2.3.4
--- error_code: 200
--- response_body
1.2.3.4,BeiJing,BeiJing,Unicom

=== Test 7: ip not found
--- http_config eval: $::http_config
--- config eval: $::location
--- timeout: 16
--- request
GET /area?ip=240.240.240.240
--- error_code: 200
--- response_body
240.240.240.240,,,unknow
