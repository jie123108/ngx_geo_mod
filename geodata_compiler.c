#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "geodata.h"
#include "array.h"

const char* long2ip(uint32_t ip_long){
	struct in_addr addr;
	addr.s_addr = htonl(ip_long);
	return inet_ntoa(addr);
}

uint32_t ip2long(const char *ip,int len) {
    uint32_t ip_long=0;
    int ip_len= len > 0 ? len: (int)strlen(ip);
	
    uint32_t ip_sec=0;
    int8_t ip_level=3;
    uint8_t i,n;
    for (i=0;i<=ip_len;i++) {
        if (i!=ip_len && ip[i]!='.' && (ip[i]<48 || ip[i]>57)) {
            continue;
        }
        if (ip[i]=='.' || i==ip_len) {
            /*too many .*/
            if (ip_level==-1) {
                return 0;
            }
            for (n=0;n<ip_level;n++) {
                ip_sec*=256;
            }
            ip_long+=ip_sec;
            if (i==ip_len) {
                break;
            }
            ip_level--;
            ip_sec=0;
        } else {
            /*char '0' == int 48*/
            ip_sec=ip_sec*10+(ip[i]-48);
        }
    }
    return ip_long;
}

#define LOG_BUF_LEN 2048

void ReleasePrint(const char* LEVEL, const char* funcName, 
			const char* fileName, int line,  const char* format,  ...){
	va_list   args;
	va_start(args,   format); 
	char vsp_buf[LOG_BUF_LEN];	
	memset(vsp_buf, 0, LOG_BUF_LEN);
	int pos = 0; 
	pos = snprintf(vsp_buf + pos, LOG_BUF_LEN-pos, "%s:%s[%s:%d] ",
			LEVEL, funcName, fileName, line); 
	pos += vsnprintf(vsp_buf +pos ,  LOG_BUF_LEN-pos, format , args);
	if(pos < LOG_BUF_LEN-1){ 
 		pos += snprintf(vsp_buf + pos, LOG_BUF_LEN-pos, "\n");
	}else{
		vsp_buf[LOG_BUF_LEN-2] = '\n';
		vsp_buf[LOG_BUF_LEN-1] = 0;
		pos = LOG_BUF_LEN;
	}
	fprintf(stderr, "%.*s", pos, vsp_buf);
	
	va_end(args); 
}

#define LOG_ERROR(format, args...) \
        ReleasePrint("ERROR", __FUNCTION__, __FILE__, __LINE__, format, ##args)

char* trim(char* s, int c){
    register int i = strlen(s);
    while(i > 0 && s[i - 1] == c)
        s[--i] = 0;

	i = 0;
	while(s[i] != 0 && s[i] == c)i++;

	return &s[i];
}

typedef struct {
	char* buf;
	int size;
	int pos;
}buf_t;

buf_t* buf_init(size_t init_size){
	buf_t* buf = calloc(1, sizeof(buf_t));
	buf->buf = calloc(init_size, 1);
	buf->size = init_size;
	buf->pos = 0;
	return buf;
}

void buf_destroy(buf_t* buf)
{
	if(buf){
		if(buf->buf){
			free(buf->buf);
		}
		free(buf);
	}
}

int buf_append(buf_t* buf, const char* str, int len)
{
	int pre_pos = buf->pos;
	int rest = buf->size - buf->pos;
	if(rest < len+1){
		buf->buf = realloc(buf->buf, buf->size+1024);
		buf->size += 1024;
	}
	buf->pos += sprintf(buf->buf + buf->pos, "%.*s", len, str);
	buf->buf[buf->pos++] = 0;
	return (int)buf->pos-pre_pos;
}

int get_const_index(array_t* arr_indexs, buf_t* buf, 
			const char* const_value, int const_len)
{
	const_index_t* indexs = (const_index_t*)arr_indexs->elts;
	int index_cnt = arr_indexs->nelts;
	int i;
	for(i=0;i<index_cnt;i++){
		if(indexs[i].len == const_len+1
			&& strncmp(const_value, buf->buf + indexs[i].begin, const_len)==0){
			return i;
		}
	}
	const_index_t* new_index = (const_index_t*)array_push(arr_indexs);
	new_index->begin = buf->pos;
	new_index->len = const_len+1;
	int new_len = buf_append(buf, const_value, const_len);
	assert(new_len == const_len+1);
	assert(i == arr_indexs->nelts-1);
	
	return i;
}

/**
 * 将文本格式的IP数据编译成二进制的。
 * 分割符号为\t, 文件格式为UTF8
 * 文本格式为：ip-begin	ip-end 省份 市 ISP
 * 根据filename生成名output的二进制数据。
 * 只有当目标文件不存在，或者filename文件的修改时间大于output时，才会重新编译。
 * 当force参数大于0时，总是强制编译。
 */
int gendata_compile(const char* filename, const char* output, int force)
{
	FILE* fp = fopen(filename, "r");
	if(fp == NULL){
		LOG_ERROR("Read file [%s] failed!", filename);
		return -1;
	}

	int n = 0;
	char szbegin[16];
	char szend[16];
	char szprovince[128];
	char szcity[128];
	char szisp[128];
	uint32_t ip_begin = 0;
	uint32_t ip_end = 0;

	array_t* arr_const_indexs = array_create(1024, sizeof(const_index_t));
	array_t* arr_items = array_create(1024, sizeof(geo_item_t));
	buf_t* const_bufs = buf_init(1024*16);
	if(arr_const_indexs==NULL || arr_items == NULL || const_bufs == NULL){
		LOG_ERROR("malloc failed!");
		exit(1);
	}

	geo_item_t* geo_item = NULL;
	int curline = 0;
	do{
		char line[256];
		memset(line,0,sizeof(line));
		char* ret = fgets(line, sizeof(line), fp);
		if(ret == NULL) break;
		curline++;
		//LOG_DEBUG("Read line [%s]", line);
		ret = trim(ret, '\n');
		ret = trim(ret, '\r');
		ret = trim(ret, ' ');
		if(strlen(ret)==0) continue;
		if(ret[0] == '#') continue;
		
		memzero(szbegin,sizeof(szbegin));
		memzero(szend,sizeof(szend));
		memzero(szprovince,sizeof(szprovince));
		memzero(szcity,sizeof(szcity));
		memzero(szisp,sizeof(szisp));
		n = sscanf(line, "%s %s %s %s %s", szbegin,szend,szprovince,szcity,szisp);
		if(n != 5){
			LOG_ERROR("Invalid Ip line %d [%s] n=%d", curline, line, n);
			continue;
		}
		ip_begin = ip2long(szbegin, strlen(szbegin));
		ip_end = ip2long(szend, strlen(szend));
		
		if(ip_end < ip_begin){
			LOG_ERROR("Invalid Ip line %d [%s], ip_end[%u] < ip_begin[%u]", 
							curline, line, ip_end, ip_begin);
			continue;
		}
		//IP数据必须是按升序排好序的。
		if(geo_item != NULL && ip_begin <= geo_item->ip_end){
			LOG_ERROR("geo data file [%s] must be in ascending order.", filename);
			LOG_ERROR("in line %d: %s", curline, line);
			exit(1);
		}

		geo_item = (geo_item_t*)array_push(arr_items);
		geo_item->ip_begin = ip_begin;
		geo_item->ip_end = ip_end;
		geo_item->province = get_const_index(arr_const_indexs, 
						const_bufs, szprovince, strlen(szprovince));
		geo_item->city = get_const_index(arr_const_indexs,
						const_bufs, szcity, strlen(szcity));
		geo_item->isp = get_const_index(arr_const_indexs,
						const_bufs, szisp, strlen(szisp));
	}while(1);

	fclose(fp);

	int fd = 0;
	int ret = 0;
	do{
		fd = open(output, O_RDWR|O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if(fd == -1)
		{
			LOG_ERROR("open file[%s] failed!", output);
			ret = -1;
			break;
		}
		
		geo_head_t geo_head;
		memzero(&geo_head, sizeof(geo_head));
		geo_head.magic = GEODATA_MAGIC;
		geo_head.const_count = arr_const_indexs->nelts;
		geo_head.const_table_offset = sizeof(geo_head) 
					+ arr_const_indexs->nelts * sizeof(const_index_t);
		geo_head.geo_item_count = arr_items->nelts;
		geo_head.geo_item_offset = geo_head.const_table_offset + const_bufs->pos;

		int geo_head_size = sizeof(geo_head);
		int const_table_size = arr_const_indexs->nelts * sizeof(const_index_t);
		int const_pool_size = const_bufs->pos;
		int items_size = arr_items->nelts * sizeof(geo_item_t);
		geo_head.filesize = geo_head_size + const_table_size + const_pool_size + items_size;
		
		
		ret = write(fd, &geo_head, geo_head_size);
		if(ret != geo_head_size){
			LOG_ERROR("write geo_head to [%s] failed!", output);
			ret = -1;
			break;
		}

		ret = write(fd, arr_const_indexs->elts, const_table_size);
		if(ret != const_table_size){
			LOG_ERROR("write const_index to [%s] failed!", output);
			ret = -1;
			break;
		}

		const_pool_size = const_bufs->pos;
		ret = write(fd, const_bufs->buf, const_pool_size);
		if(ret != const_pool_size){
			LOG_ERROR("write const_bufs to [%s] failed!", output);
			ret = -1;
			break;
		}

		ret = write(fd, arr_items->elts, items_size);
		if(ret != items_size){
			LOG_ERROR("write const_index to [%s] failed!", output);
			ret = -1;
			break;
		}
		ret = 0;
		printf("--------- Compile %s OK\n", filename);
		printf("--------- Output: %s\n", output);
		printf("| PART | GEO_HEAD | CONST_TABLE | CONST_POOL | GEO_ITEMS |\n");
		printf("----------------------------------------------------------\n");
		printf("| SIZE | %8d | %6d(%d) | %10d | %4d(%d) |\n",
				geo_head_size,const_table_size,arr_const_indexs->nelts, 
				const_pool_size,  items_size,arr_items->nelts);

	}while(0);

	array_destroy(arr_const_indexs);
	array_destroy(arr_items);
	buf_destroy(const_bufs);
	if(fd > 0){
		close(fd);
	}
	
	return ret;
}


int main(int argc, char* argv[])
{
	if(argc < 2){
		printf("Usage: %s filename\n", argv[0]);
		exit(1);
	}

	const char* filename = argv[1];
	
	char output[256];
	memset(output, 0, sizeof(output));
	sprintf(output, "%s", filename);
	char* dot_index = rindex(output, '.');
	if(dot_index == NULL){
		strcat(output, ".geo");
	}else{
		sprintf(dot_index, ".geo");
	}
	gendata_compile(filename, output, 1);
}
