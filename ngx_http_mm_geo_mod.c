/*************************************************
 * Author: jie123108@163.com
 * Copyright: jie123108
 *************************************************/
#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <time.h>
#include <string.h>
#include "geodata.h"

#define XFWD_NEW_VER 1003013

typedef struct {
	ngx_flag_t ip_from_url; //用于测试，表示可以从url中取得IP参数。
	ngx_flag_t ip_from_head; //表示从head中取地址，用于前端有代理时。(x_real_ip,x_forwarded_for)
	ngx_str_t geodata_file;  //IP文件。(二进制)
	geo_ctx_t* geo_ctx;
#if (nginx_version>XFWD_NEW_VER)
    ngx_array_t       *proxies;     /* array of ngx_cidr_t */
    ngx_flag_t         proxies_recursive;
#endif
} ngx_http_mm_geo_main_conf_t;

typedef struct {
   ngx_flag_t geo;        /* on | off */
} ngx_http_mm_geo_loc_conf_t;

#if nginx_version>XFWD_NEW_VER
static char *ngx_http_mm_geo_proxies(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
#endif

static void ngx_http_mm_geo_cleanup(void *data);

static char *
ngx_http_mm_geo_datafile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_mm_geo_create_main_conf(ngx_conf_t *cf);
static char* ngx_http_mm_geo_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_mm_geo_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_mm_geo_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

static ngx_int_t ngx_http_mm_geo_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_mm_geo_commands[] = {
	{ngx_string("ip_from_url"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_main_conf_t, ip_from_url),
      NULL},
	{ngx_string("ip_from_head"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_main_conf_t, ip_from_head),
      NULL},
	{ngx_string("geodata_file"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_mm_geo_datafile,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_main_conf_t, geodata_file),
      NULL},

	{ngx_string("mm_geo"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_SIF_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_loc_conf_t, geo),
      NULL},
      
#if nginx_version>XFWD_NEW_VER  
    { ngx_string("proxies"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_mm_geo_proxies,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_main_conf_t, proxies),
      NULL },

    { ngx_string("proxies_recursive"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_mm_geo_main_conf_t, proxies_recursive),
      NULL },
#endif
	
      ngx_null_command
};


static ngx_http_module_t  ngx_http_mm_geo_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_mm_geo_init,                     /* postconfiguration */

    ngx_http_mm_geo_create_main_conf,         /* create main configuration */
    ngx_http_mm_geo_init_main_conf,         /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_mm_geo_create_loc_conf,          /* create location configuration */
    ngx_http_mm_geo_merge_loc_conf            /* merge location configuration */
};


ngx_module_t  ngx_http_mm_geo_module = {
    NGX_MODULE_V1,
    &ngx_http_mm_geo_module_ctx,              /* module context */
    ngx_http_mm_geo_commands,                 /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,       							/* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,       							/* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

inline  ngx_int_t ngx_http_add_header_in(ngx_http_request_t *r, 
								ngx_str_t* key, ngx_str_t *value)
{
    ngx_table_elt_t  *h;
	ngx_uint_t hash;
	unsigned n;
	u_char ch;
    if (value->len) {
        h = (ngx_table_elt_t*)ngx_list_push(&r->headers_in.headers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        h->key = *key;
        h->value = *value;
        h->lowcase_key = (u_char*)ngx_pcalloc(r->pool, key->len+1);
        
        hash = 0;
        for (n = 0; n < key->len; n++) {
            ch = key->data[n];

            if (ch >= 'A' && ch <= 'Z') {
                ch |= 0x20;

            } else if (ch == '-') {
                ch = '_';
            }

            hash = ngx_hash(hash, ch);
            h->lowcase_key[n] = ch;
        }
        h->hash = hash; //ngx_hash_key(key->data, key->len);

    }

    return NGX_OK;
}


ngx_uint_t ngx_http_get_remote_ip(ngx_http_request_t *r){
	ngx_http_mm_geo_main_conf_t *conf = NULL;
	conf = (ngx_http_mm_geo_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_mm_geo_module);
	uint32_t ip = 0;
	do{
		ngx_str_t* real_ip;
		
		if(conf->ip_from_url){
			//用于测试，从URL中获取IP信息。
			ngx_str_t szip = ngx_null_string;
			if(ngx_http_arg(r, (u_char*)"ip", 2, &szip)==NGX_OK){
				ip = ip2long((const char*)szip.data, szip.len);
				//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "### ip from url ");
				break;
			}
		}

		if(conf->ip_from_head){
			if(r->headers_in.x_real_ip != NULL){
				real_ip = &r->headers_in.x_real_ip->value;
				ip = ip2long((const char*)real_ip->data, real_ip->len);
				//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "### ip from x_real_ip ");
				break;
			}

#if nginx_version>XFWD_NEW_VER
			ngx_array_t* xfwd = &r->headers_in.x_forwarded_for; 
		    if (xfwd->nelts > 0 && conf->proxies != NULL) {
				ngx_addr_t addr;
				memset(&addr,0,sizeof(addr));
			    addr.sockaddr = r->connection->sockaddr;
			    addr.socklen = r->connection->socklen;
			    
		        (void) ngx_http_get_forwarded_addr(r, &addr, xfwd, NULL,
		                                           conf->proxies, conf->proxies_recursive);
		        struct sockaddr_in * sin = (struct sockaddr_in *) addr.sockaddr;
				ip =  ntohl(sin->sin_addr.s_addr);
				//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "### ip from x_forwarded_for ");
				break;
		    }
#else
			if(r->headers_in.x_forwarded_for != NULL){
				real_ip = &r->headers_in.x_forwarded_for->value;
				unsigned i=0;
				for(i=0;i<real_ip->len;i++){
					if(real_ip->data[i] == (u_char)','){
						break;
					}
				}
				real_ip->len = i;				
				ip = ip2long((const char*)real_ip->data, real_ip->len);
				//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "### ip from x_forwarded_for ");
				break;
			}
#endif
		}
		struct sockaddr_in * sin = (struct sockaddr_in *)r->connection->sockaddr;
		ip =  ntohl(sin->sin_addr.s_addr);
		//ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "### ip from socket ");
	}while(0);

	return ip;
}

ngx_int_t ngx_http_mm_geo_handler(ngx_http_request_t *r)
{
	static ngx_str_t x_province_key = ngx_string("x-province");
	static ngx_str_t x_city_key = ngx_string("x-city");
	static ngx_str_t x_isp_key = ngx_string("x-isp");
	static ngx_str_t x_ip_key = ngx_string("x-ip");
	
    ngx_http_mm_geo_loc_conf_t* glcf = ngx_http_get_module_loc_conf(r, ngx_http_mm_geo_module);
	if (glcf == NULL || glcf->geo <= 0) {
        return NGX_OK;
    }

	ngx_http_mm_geo_main_conf_t   *gmcf;
    gmcf = ngx_http_get_module_main_conf(r, ngx_http_mm_geo_module);

	uint32_t remote_ip = ngx_http_get_remote_ip(r);
	ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "remote_ip: %uD", remote_ip);
	char* szip = (char*)ngx_pcalloc(r->pool, 32);
	ngx_memzero(szip, 32);
	const char* sip = long2ip(remote_ip);
	strncpy(szip, sip, 32);

	geo_result_t result;
	memset(&result,0,sizeof(result));
	int ret = geo_find2(gmcf->geo_ctx, remote_ip, &result);
	if(ret == 0){
		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
					"Find ip [%s] info [%s.%s %s]", szip, result.province, result.city, result.isp);
		ngx_str_t x_province_value = {result.province_len, (u_char*)result.province};
		ngx_str_t x_city_value = {result.city_len, (u_char*)result.city};
		ngx_str_t x_isp_value = {result.isp_len, (u_char*)result.isp};
		ngx_str_t x_ip_value = {strlen(szip), (u_char*)szip};		
		ngx_http_add_header_in(r, &x_province_key, &x_province_value);
		ngx_http_add_header_in(r, &x_city_key, &x_city_value);
		ngx_http_add_header_in(r, &x_isp_key, &x_isp_value);			
		ngx_http_add_header_in(r, &x_ip_key, &x_ip_value);			
	}else{
		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
					"Ip [%s] not found!", szip);
		static ngx_str_t x_isp_value = ngx_string("unknow");
		ngx_str_t x_ip_value = {strlen(szip), (u_char*)szip};		
		ngx_http_add_header_in(r, &x_isp_key, &x_isp_value);	
		ngx_http_add_header_in(r, &x_ip_key, &x_ip_value);
	}
	
    return NGX_OK;
}


static void* ngx_http_mm_geo_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_mm_geo_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mm_geo_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

	conf->ip_from_url = NGX_CONF_UNSET;
	conf->ip_from_head = NGX_CONF_UNSET;
#if nginx_version>XFWD_NEW_VER  
	conf->proxies = NGX_CONF_UNSET_PTR;
	conf->proxies_recursive = NGX_CONF_UNSET;
#endif

	ngx_pool_cleanup_t     *cln;
    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_http_mm_geo_cleanup;
    cln->data = conf;


    return conf;
}

static char*   ngx_http_mm_geo_init_main_conf(ngx_conf_t *cf, void *conf)
{
	ngx_http_mm_geo_main_conf_t  *gmcf = conf;
	static ngx_str_t def_file = ngx_string("conf/geodata.geo");
	if(gmcf->ip_from_url == NGX_CONF_UNSET)gmcf->ip_from_url = 0;
	if(gmcf->ip_from_head == NGX_CONF_UNSET)gmcf->ip_from_head = 0;
	if(gmcf->geodata_file.data == NULL) gmcf->geodata_file = def_file;
	#if nginx_version>XFWD_NEW_VER 
		if(gmcf->proxies_recursive == NGX_CONF_UNSET) gmcf->proxies_recursive = 1;
	#endif

	return NGX_CONF_OK;
}

static void *
ngx_http_mm_geo_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_mm_geo_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mm_geo_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
	conf->geo = NGX_CONF_UNSET; 
	
    return conf;
}
    

static char * ngx_http_mm_geo_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_mm_geo_loc_conf_t *prev = parent;
    ngx_http_mm_geo_loc_conf_t *conf = child;

	ngx_conf_merge_value(conf->geo, prev->geo, 0);
    
    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_mm_geo_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt        *h;
	ngx_http_mm_geo_main_conf_t   *gmcf;
	ngx_http_core_main_conf_t  *cmcf;

	gmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_mm_geo_module);
	cf->cycle->conf_ctx[ngx_http_mm_geo_module.index] = (void***)gmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_mm_geo_handler;
	
    return NGX_OK;
}



static char *
ngx_http_mm_geo_datafile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_mm_geo_main_conf_t  *gcf = conf;

    ngx_str_t  *value;

    if (gcf->geo_ctx) {
        return "is duplicate";
    }

    value = cf->args->elts;
	ngx_str_t  geodata_file = value[1];
    gcf->geo_ctx = geo_new();

	if(ngx_conf_full_name(cf->cycle, &geodata_file, 0)==NGX_ERROR){
		return "conf error";
	}	

	char geodatafile[256];
	memset(geodatafile, 0, sizeof(geodatafile));
	ngx_sprintf((u_char*)geodatafile, "%V", &geodata_file);
	
	int ret = geo_init(gcf->geo_ctx, geodatafile);

    if (ret != 0) {
		char errmsg[64];
		memset(errmsg, 0, sizeof(errmsg));
		switch(ret){
		case -1: sprintf(errmsg, "open file failed!");break;
		case -2: sprintf(errmsg, "mmap failed! no memory");break;
		case -3: sprintf(errmsg, "mmap failed! %s", strerror(errno));break;
		case -4: sprintf(errmsg, "invalid geo file"); break;
		default: sprintf(errmsg, "unknow [%d]", ret);
		}
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "geo_init(\"%V\") failed! errno:%d err:%s", 
                           &geodata_file, ret, errmsg
                           );

        return NGX_CONF_ERROR;
    }

	return NGX_CONF_OK;
}

static void ngx_http_mm_geo_cleanup(void *data)
{
    ngx_http_mm_geo_main_conf_t  *gcf = data;

    if (gcf->geo_ctx) {
        geo_destroy(gcf->geo_ctx);
		gcf->geo_ctx = NULL;
    }
}

#if nginx_version>XFWD_NEW_VER  
static char *ngx_http_mm_geo_proxies(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	char  *p = (char*)conf;
    ngx_int_t                rc;
    ngx_str_t               *value;
    ngx_cidr_t              *cidr;

    value = (ngx_str_t*)cf->args->elts;
    
	ngx_array_t      **a;
	a = (ngx_array_t **) (p + cmd->offset);

	if (*a == NGX_CONF_UNSET_PTR) {
		*a = ngx_array_create(cf->pool, 2, sizeof(ngx_cidr_t));
		if (*a == NULL) {
		    return (char*)NGX_CONF_ERROR;
		}
	}

	cidr = (ngx_cidr_t*)ngx_array_push(*a);
	if (cidr == NULL) {
		return (char*)NGX_CONF_ERROR;
	}

#if (NGX_HAVE_UNIX_DOMAIN)

    if (ngx_strcmp(value[1].data, "unix:") == 0) {
         cidr->family = AF_UNIX;
         return (char *)NGX_CONF_OK;
    }

#endif

    rc = ngx_ptocidr(&value[1], cidr);

    if (rc == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"",
                           &value[1]);
        return (char *)NGX_CONF_ERROR;
    }

    if (rc == NGX_DONE) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "low address bits of %V are meaningless", &value[1]);
    }

    return (char *)NGX_CONF_OK;
}
#endif

