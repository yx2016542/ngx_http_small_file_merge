/*
 Copyright(C) 20180123 dgh Holding Limited
 jiajie8301@163.com
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_flag_t enable;
    ngx_uint_t files_merge_number;
    ngx_flag_t ignore_file_error;
    ngx_str_t  delimiter;
    ngx_hash_t types;
    ngx_array_t *types_keys;
}ngx_http_small_file_merge_loc_conf_t;


static ngx_int_t ngx_http_small_file_merge_save_path(ngx_http_request_t *r, ngx_array_t *uris, size_t max, ngx_str_t *path, u_char *start, size_t length);
static ngx_int_t ngx_http_small_file_merge_init(ngx_conf_t *cf);
static void *ngx_http_small_file_merge_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_small_file_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_str_t ngx_http_small_file_merge_default_types[] = {
    ngx_string("application/octet-stream"),
    ngx_null_string
};

static ngx_command_t ngx_http_small_file_merge_commands[] = {
    {ngx_string("file_merge"),
     NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
     ngx_conf_set_flag_slot,
     NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_small_file_merge_loc_conf_t, enable),
     NULL},

     {ngx_string("file_merge_number"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_small_file_merge_loc_conf_t, files_merge_number),
      NULL},
    
     {ngx_string("files_types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_types_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_small_file_merge_loc_conf_t, types_keys),
      &ngx_http_small_file_merge_default_types[0]},

     {ngx_string("ignore_file_error"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_small_file_merge_loc_conf_t, ignore_file_error),
      NULL},

      ngx_null_command
};

static ngx_http_module_t ngx_http_small_file_merge_module_ctx = {
    NULL,                                       /*preconfiguration*/
    ngx_http_small_file_merge_init,             /*postconfiguration*/

    NULL,                                       /*create main configuration*/
    NULL,                                       /*init main configuration*/

    NULL,                                       /*create server configuration*/
    NULL,                                       /*merge server configuration*/

    ngx_http_small_file_merge_create_loc_conf,  /*create location configuration*/
    ngx_http_small_file_merge_loc_conf          /*merge location configuration*/
};

ngx_module_t ngx_http_small_file_merge_module = {
    NGX_MODULE_V1, 
    &ngx_http_small_file_merge_module_ctx,  /*module context*/
    ngx_http_small_file_merge_commands,     /*module directives*/
    NGX_HTTP_MODULE,                  /*module type*/
    NULL,                             /*init master*/
    NULL,                             /*init module*/
    NULL,                             /*init process*/
    NULL,                             /*init thread*/
    NULL,                             /*exit thread*/
    NULL,                             /*exit process*/
    NULL,                             /*exit master*/
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_small_file_merge_handler(ngx_http_request_t *r)
{
    size_t root, length;
    time_t last_modified;
    u_char tmp_path[1024] = {0};
    u_char *end, *start, *e, *last, *ptr;
    ngx_int_t  rc;
    ngx_str_t *uri, *file_name, path, value;
    ngx_buf_t *b;
    ngx_uint_t i, level;
    ngx_array_t uris;
    ngx_chain_t out, **last_out, *cl;
    ngx_open_file_info_t of;
    ngx_http_core_loc_conf_t *ccf;
    ngx_http_small_file_merge_loc_conf_t *clcf;
    
    ngx_log_t *log = NULL;
    log = r->connection->log;

    ngx_log_error(NGX_LOG_INFO, log, 0, "get uri:%V, args:%V", &r->uri, &r->args);

    if(!(r->method &(NGX_HTTP_GET|NGX_HTTP_HEAD))){
        ngx_log_error(NGX_LOG_ERR, log, 0, "method is not get or head");
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_small_file_merge_module);
    if(!clcf->enable){
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);
    if(rc != NGX_OK){
        ngx_log_error(NGX_LOG_ERR, log, 0, "discard is error");
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if(last == NULL){
        ngx_log_error(NGX_LOG_ERR, log, 0, "map to uri is failed, path:%V", &path);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ptr = strrchr(path.data, '.');
    if (ptr == NULL){
        ngx_log_error(NGX_LOG_ALERT, log, 0, "not file full path");

        path.len = last - path.data;
    }else{

        ptr = strrchr(path.data, '/');
        path.len = ptr - path.data + 1; 
    }
    
    ngx_log_error(NGX_LOG_ERR, log, 0, "http concat root:\"%V\", path.len:%d", &path, path.len);

    ccf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

#if (NGX_SUPPRESS_WARN)
    ngx_memzero(&uris, sizeof(ngx_array_t));
#endif

    if(ngx_array_init(&uris, r->pool, 10, sizeof(ngx_str_t)) != NGX_OK){
        ngx_log_error(NGX_LOG_ERR, log, 0, "array init is failed");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if(ngx_http_arg(r, (u_char*)"file_list", 9, &value) == NGX_OK){
        ngx_log_error(NGX_LOG_ALERT, log, 0, "get file_list:%V", &value);
    }

#if 1
    if(ptr != NULL){
        end = ptr + 1;
        length = ngx_strlen(end);
        
        ngx_log_error(NGX_LOG_ALERT, log, 0, "get ext ts:%s, length:%d", end, length);     

        rc = ngx_http_small_file_merge_save_path(r, &uris, clcf->files_merge_number, &path, end, length);
    }
#endif

    ngx_log_error(NGX_LOG_ALERT, log, 0, "***********get first uri is success, path:%V*******************", &path);
    
    e = value.data + value.len;

    for(end = value.data, start = end; end != e; end++){
        ngx_memset(tmp_path, '\0', 1024);

        if(*end == ':'){
            length = end - start;
            ngx_snprintf(tmp_path, length, "%s", start);
           
            ngx_log_error(NGX_LOG_ALERT, log, 0, "get file_path:%s", tmp_path);

            rc = ngx_http_small_file_merge_save_path(r, &uris, clcf->files_merge_number, &path, start, length);
            if(rc != NGX_OK){
                ngx_log_error(NGX_LOG_ERR, log, 0, "http file add_path is failed, dot flag, rc = %d", rc);
            }
            
            start = end+1;
        }else{
            continue;        
        }    
    }        

    length = end - start;
    ngx_snprintf(tmp_path, length, "%s", start);    
    ngx_log_error(NGX_LOG_ALERT, log, 0, "get file path:%s", tmp_path);    

    rc = ngx_http_small_file_merge_save_path(r, &uris, clcf->files_merge_number, &path, start, length);
    if(rc != NGX_OK){
        ngx_log_error(NGX_LOG_ERR, log, 0, "http file add_path is failed, dot flag, rc = %d", rc);
    }

    last_modified = 0;
    last_out = NULL;
    b = NULL; 
    length = 0;
    uri = uris.elts;

    for(i = 0; i < uris.nelts; i++){
        file_name = uri + i;
       
        ngx_log_error(NGX_LOG_ALERT, log, 0, "get file_path from array, path:%V", file_name);

        r->headers_out.content_type.len = 0;
        if(ngx_http_set_content_type(r) != NGX_OK){
            ngx_log_error(NGX_LOG_ALERT, log, 0, "set content type is not ok");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->headers_out.content_type_lowcase = NULL;

        ngx_memzero(&of, sizeof(ngx_open_file_info_t));

        of.read_ahead = ccf->read_ahead;
        of.directio = ccf->directio;
        of.valid = ccf->open_file_cache_valid;
        of.min_uses = ccf->open_file_cache_min_uses;
        of.errors = ccf->open_file_cache_errors;
        of.events = ccf->open_file_cache_events;

        if(ngx_open_cached_file(ccf->open_file_cache, file_name, &of, r->pool) != NGX_OK)
        {
            switch(of.err){
                case 0:
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;

                case NGX_ENOENT:
                case NGX_ENOTDIR:
                case NGX_ENAMETOOLONG:
                     level = NGX_LOG_ERR;
                     rc = NGX_HTTP_NOT_FOUND;
                     break;

                case NGX_EACCES:
                     level = NGX_LOG_ERR;
                     rc = NGX_HTTP_FORBIDDEN;
                     break;

                default:
                     level = NGX_LOG_CRIT;
                     rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
                     break;        
            }

            if(rc != NGX_HTTP_NOT_FOUND || ccf->log_not_found){
                ngx_log_error(level, log, of.err,
                    "%s\"%V\" failed", of.failed, file_name);
            }

            if(clcf->ignore_file_error 
                && (rc == NGX_HTTP_NOT_FOUND || rc == NGX_HTTP_FORBIDDEN))
            {
                continue;
            }

            return rc;
        } 

        if(!of.is_file){
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno, "\"%V\" is not a regular file", file_name);
            if(clcf->ignore_file_error){
                continue;
            }

            return NGX_HTTP_NOT_FOUND;
        }
    
        if(of.size == 0){
            continue;
        }

        length += of.size;
        if(last_out == NULL){
            last_modified = of.mtime;
        }else{
            if(of.mtime > last_modified){
                last_modified = of.mtime;
            }
        }

        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if(b == NULL){
            ngx_log_error(NGX_LOG_ERR, log, 0, "pcalloc ngx_buf is failed");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
        if(b->file == NULL){
            ngx_log_error(NGX_LOG_ERR, log, 0, "pcalloc ngx_file is failed");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->file_pos = 0; 
        b->file_last = of.size;
        
        b->in_file = b->file_last ?1:0;
        b->file->fd = of.fd;
        b->file->name = *file_name;
        b->file->log = r->connection->log;
        
        b->file->directio = of.is_directio;
        
        if(last_out == NULL){
            ngx_log_error(NGX_LOG_ALERT, log, 0, "******************last out is NULL*************");
            out.buf = b;
            last_out = &out.next;
            out.next = NULL;
        }else {

            ngx_log_error(NGX_LOG_ALERT, log, 0, "*****************last out is not NULL*************");

            cl = ngx_alloc_chain_link(r->pool);
            if(cl == NULL){
                ngx_log_error(NGX_LOG_ERR, log, 0, "alloc chain link is failed, last_out is not null, failed");
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            cl->buf = b;
            *last_out = cl;
            last_out = &cl->next;
            cl->next = NULL;
        }

        if(i + 1 == uris.nelts){
            continue;
        }

    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = length;
    r->headers_out.last_modified_time = last_modified;

    rc = ngx_http_send_header(r);
    if(rc == NGX_ERROR || rc > NGX_OK || r->header_only){
        ngx_log_error(NGX_LOG_ERR, log, 0, "send header is failed");
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

static ngx_int_t 
ngx_http_small_file_merge_save_path(ngx_http_request_t *r, ngx_array_t *uris, size_t max, ngx_str_t *path, u_char *start, size_t length)
{
    u_char *d;
    ngx_str_t *uri, args;
    ngx_uint_t flags;

    if(start == NULL || length == 0){
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "client sent zero file chunk filename");

        return NGX_HTTP_BAD_REQUEST;
    }

    if(uris->nelts >= max){
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "client sent too many file chunk filenames");
        return NGX_HTTP_BAD_REQUEST;
    }

    uri = ngx_array_push(uris);
    if(uri == NULL){
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "array push is failed, uri is null");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    uri->len = path->len + length;
    uri->data = ngx_pnalloc(r->pool, uri->len + 1);
    if(uri->data == NULL){
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "nalloc uri data is error, is failed");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    d = ngx_cpymem(uri->data, path->data, path->len);

    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "*******save path:%s, start:%s, length:%d", uri->data, start, length); 

    d = ngx_cpymem(d, start, length);
    *d = '\0';

    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "*******http file chunk add file:\"%s\"", uri->data);

    return NGX_OK;
}

static void *
ngx_http_small_file_merge_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_small_file_merge_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_small_file_merge_loc_conf_t));
    if(conf == NULL){
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->ignore_file_error = NGX_CONF_UNSET;
    conf->files_merge_number = NGX_CONF_UNSET_UINT;

    return conf;
}

static char *
ngx_http_small_file_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_small_file_merge_loc_conf_t *prev = parent;
    ngx_http_small_file_merge_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_str_value(conf->delimiter, prev->delimiter, "");
    ngx_conf_merge_value(conf->ignore_file_error, prev->ignore_file_error, 0);
    ngx_conf_merge_uint_value(conf->files_merge_number, prev->files_merge_number, 10);

    if(ngx_http_merge_types(cf, &conf->types_keys, &conf->types, &prev->types_keys, &prev->types, ngx_http_small_file_merge_default_types) != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static ngx_int_t 
ngx_http_small_file_merge_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if(h == NULL){
        return NGX_ERROR;
    }

    *h = ngx_http_small_file_merge_handler;

    return NGX_OK;
}

