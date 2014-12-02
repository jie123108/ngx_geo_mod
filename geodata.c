#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "geodata.h"

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

geo_ctx_t* geo_new()
{
	return (geo_ctx_t*)calloc(1, sizeof(geo_ctx_t));
}

#define idx_is_valid(i, c) (i.begin >=0 && i.begin < c && (i.begin+i.len) >=0 && (i.begin+i.len)<=c)
#define item_is_valid(i, c) (i.province<c && i.city < c && i.isp < c)
int geo_verify(geo_ctx_t* geo_ctx)
{
	geo_head_t* geo_head = (geo_head_t*)geo_ctx->ptr;
	if(geo_head->magic != GEODATA_MAGIC){
		printf("ERROR: invalid magic: 0x%04x\n", geo_head->magic);
		return -1;
	}
	unsigned int const_count = (geo_head->const_table_offset-sizeof(geo_head_t))/sizeof(const_index_t);
	if(const_count != geo_head->const_count){
		printf("ERROR: geo_head.const_count: %u != calculate const_count: %u\n",
						geo_head->const_count, const_count);
		return -1;
	}
	unsigned int i;
	const_index_t* indexs = (const_index_t*)(geo_ctx->ptr + sizeof(geo_head_t));
	int const_pool_size = geo_head->geo_item_offset-geo_head->const_table_offset;
	for(i=0;i<const_count;i++){
		if(!idx_is_valid(indexs[i], const_pool_size)){
			printf("ERROR: indexs[%d].begin:%d, len:%d not in constant pool: 0-%d\n",
				i, indexs[i].begin, indexs[i].len, const_pool_size);
			return -1;
		}			
	}

	unsigned int geo_item_count = (geo_ctx->size - geo_head->geo_item_offset)/sizeof(geo_item_t);
	if(geo_item_count != geo_head->geo_item_count){
		printf("ERROR: geo_head.geo_item_count: %u != calculate geo_item_count: %u\n",
						geo_head->geo_item_count, geo_item_count);
		return -1;
	}

	geo_item_t* items = (geo_item_t*)(geo_ctx->ptr + geo_head->geo_item_offset);
	for(i=0;i<geo_head->geo_item_count;i++){
		if(!item_is_valid(items[i], const_count)){
			printf("ERROR: geo_item[%d].province:%d, city:%d, isp:%d index great then const_count: %d\n",
				i, items[i].province, items[i].city, items[i].isp, const_count);
			return -1;
		}
	}
	
	return 0;
}

// TODO: file verify..
int geo_init(geo_ctx_t* geo_ctx, const char* geodatafile)
{
	int ret = 0;
	int fd = open(geodatafile, O_RDONLY);
	if(fd <= 0){
		printf("ERROR: open file(%s) failed! %d err:%s\n", 
					geodatafile, errno, strerror(errno));
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
		geo_ctx->ptr = (char*)shm;
		if(geo_head->magic != GEODATA_MAGIC){
			ret = -4;
			printf("read invalid geo magic: 0x%04x\n", geo_head->magic);
			munmap(shm, geo_ctx->size);
			geo_ctx->ptr = NULL;
		}else if(geo_verify(geo_ctx)!=0){
			ret = -4;
			printf("invalid geo file: %s\n", geodatafile);
			munmap(shm, geo_ctx->size);
			geo_ctx->ptr = NULL;
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
	result->province_len = clength(indexs, find_item->province)-1;
	result->city= cvalue(indexs, buf, find_item->city);
	result->city_len = clength(indexs, find_item->city)-1;
	result->isp = cvalue(indexs, buf, find_item->isp);
	result->isp_len = clength(indexs, find_item->isp)-1;
	
	return 0;
}

int geo_find(geo_ctx_t* geo_ctx, const char* ip, geo_result_t* result)
{
	uint32_t long_ip = ip2long(ip, strlen(ip));
	return geo_find2(geo_ctx, long_ip, result);
}


#ifdef _TOOLS_

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
		if(strcmp(input,"exit")==0){
			break;
		}else if(strcmp(input, "info")==0){
			
		}

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
