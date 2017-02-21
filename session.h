/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef SESSION_H
#define SESSION_H

#include "cgic.h"

#ifdef __cplusplus
extern "C"
{
#endif

  int session_init(const char *confpath);
  void session_free(void);
  void *session_new(void);
  void session_del(void *db);
  
  void *session_create(void);
  void session_destroy(void *session);
  int session_put(void *db,char *sid,int sidlen,void *session,int lifetime);
  void *session_get(void *db,char *sid,int sidlen);
  int session_out(void *db,char *sid,int sidlen);

  int session_var_register(void *session,const char *name,const char *value);
  int session_var_unregister(void *session,const char *name);
  int session_var_isregister(void *session,const char *name);
  const char *session_var_get(void *session,const char *name);
  void session_var_unset(void *session);
  void session_var_iterinit(void *session);
  const char *session_var_iternext(void *session);
  
  const char *session_name(void);
  const char *session_id(void *session);
  
  int session_create_sid(const char *remote_addr,const char *remote_port,char *sidbuf,int bufsize);
  void *session_start(void *db,CGI_T *cgi);
  int session_end(void *db,CGI_T *cgi,void *session,int cookie_lifetime);
  int session_logout(void *db,CGI_T *cgi,int cookie_lifetime);
  
#ifdef __cplusplus
}
#endif

#endif

