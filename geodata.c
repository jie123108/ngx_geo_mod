#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include "geodata.h"

static inline uint32_t ip2long(const char *ip,int len) {
    uint32_t ip_long=0;
    uint8_t ip_len= len > 0 ? len: strlen(ip);
	
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

geo_ctx_t* geo_new()
{
	return (geo_ctx_t*)calloc(1, sizeof(geo_ctx_t));
}

int geo_init(geo_ctx_t* geo_ctx, const char* geodatafile)
{
	int ret = 0;
	int fd = open(geodatafile, O_RDONLY);
	if(fd <= 0){
		printf("ERROR: open file(%s) failed! %d err:%s\n", errno, strerror(errno));
		return -1;
	}

	geo_ctx->size = lseek(fd, 0, SEEK_END);
	void * shm = mmap(NULL, geo_ctx->size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if(shm == MAP_FAILED){
		if(errno == ENOMEM){
			ret = -2;
			printf("mmap failed no memory!\n");
		}else{
			ret = -3;
			printf("mmap failed! %d err:%s\n", errno, strerror(errno));
		}
	}else{
		geo_head_t* geo_head = (geo_head_t*)shm;
		if(geo_head->magic != GEODATA_MAGIC){
			ret = -4;
			printf("read invalid geo magic: 0x%04x\n", geo_head->magic);
			munmap(shm, geo_ctx->size);
		}else{
			geo_ctx->ptr = (char*)shm;
			ret = 0;
		}
	}
	
	return ret;
}


void geo_destroy(geo_ctx_t* geo_ctx)
{
	if(geo_ctx){
		if(geo_ctx->ptr != NULL){
			int ret = munmap(geo_ctx->ptr, geo_ctx->size);
			if(ret != 0){
				printf("munmap failed! %d err:%s\n",errno, strerror(errno));
			}
		}
		free(geo_ctx);
	}
}

int geo_find2(geo_ctx_t* geo_ctx, uint32_t ip, geo_result_t* result)
{
	assert(geo_ctx->ptr != NULL);
	geo_head_t* geo_head = (geo_head_t*)geo_ctx->ptr;
	geo_item_t* items = (geo_item_t*)(geo_ctx->ptr + geo_head->geo_item_offset);

	int size = geo_head->geo_item_count;
	if(size < 1){
		return -1;
	}
	int High = size - 1;
	int Low = 0;
	int M = size/2;

#define IP_EQ(ip, node) (ip>=node.ip_begin && ip <=node.ip_end)
	geo_item_t* find_item = NULL;
	do{
		if(IP_EQ(ip, items[M]))
		{
			find_item = &items[M];
			break;
		}
		while(Low<=High)
		{
			if(High-Low<=1)
			{
				if(IP_EQ(ip, items[Low]))
				{
					find_item = &items[Low];
					break;
				}
				else if(IP_EQ(ip, items[High]))
				{
					find_item = &items[High];
					break;
				}
				else
				{
					break;
				}
			}
			
			if(ip<items[M].ip_begin)
			{	
				High = M-1;
			}
			else if(ip>items[M].ip_end)
			{
				Low = M+1;
			}
			else if(IP_EQ(ip, items[M]))
			{
				find_item = &items[M];
				break;
			}

			M = (Low + High)/2;
		}
	}while(0);
	if(find_item == NULL){
		return -1;
	}

	result->ip_begin = find_item->ip_begin;
	result->ip_end = find_item->ip_end;
	const_index_t* indexs = (const_index_t*)(geo_ctx->ptr + sizeof(geo_head_t));
	const char* buf = geo_ctx->ptr + geo_head->const_table_offset;
	result->province = cvalue(indexs, buf, find_item->province);
	result->city= cvalue(indexs, buf, find_item->city);
	result->isp = cvalue(indexs, buf, find_item->isp);
	
	return 0;
}

int geo_find(geo_ctx_t* geo_ctx, const char* ip, geo_result_t* result)
{
	uint32_t long_ip = ip2long(ip, strlen(ip));
	return geo_find2(geo_ctx, long_ip, result);
}


#ifdef _TOOLS_

static inline const char* long2ip(uint32_t ip_long){
	struct in_addr addr;
	addr.s_addr = htonl(ip_long);
	return inet_ntoa(addr);
}

const char* long2ip2(uint32_t ip_long, char* szbuf){
	const char* p = long2ip(ip_long);
	strcpy(szbuf, p);
	return szbuf;
}

int main(int argc, char* argv[])
{
	if(argc < 2){
		printf("Usage: %s filename\n", argv[0]);
		exit(1);
	}

	const char* filename = argv[1];

	geo_ctx_t* geo_ctx = geo_new();
	int ret = geo_init(geo_ctx, filename);
	if(ret != 0){
		printf("geo_init(%s) failed! ret=%d\n", filename, ret);
		exit(2);
	}
	geo_result_t result;
	while(1){
		char input[64];
		memset(input,0,sizeof(input));
		printf("Input a ip:");
		scanf("%s", input);
		if(strcmp(input,"exit")==0) break;

		memset(&result, 0, sizeof(result));
		ret = geo_find(geo_ctx, input, &result);
		if(ret == 0){
			char ip_begin[32];
			char ip_end[32];
			memset(ip_begin,0, sizeof(ip_begin));
			memset(ip_end,0, sizeof(ip_end));
			
			printf("[%s] -----------  [%s-%s %s %s %s]\n",  
				input, long2ip2(result.ip_begin, ip_begin),
				long2ip2(result.ip_end, ip_end),
				result.province, result.city, result.isp);
		}else{
			printf("can not find ip: %s\n", input);
		}
		
	}

	geo_destroy(geo_ctx);
}
#endif
