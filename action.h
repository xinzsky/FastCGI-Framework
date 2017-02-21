/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef ACTION_H
#define ACTION_H

#include "cgic.h"
#include "tcutil.h"
#include "dbi.h"

// http://xxx/[baseuri]/module/controller/action?...
typedef struct {
  char *name;      
  char *baseuri;   
  char *module;    // 模块名，可以为空
  char *controller;// 控制器名，可以为空。
  char *formname;  // input，可以为空，比如动态生成首页...
  char *pagename;  // output
  int   refparse;  // 该action是否需要解析referer
  int   log;       // 是否记录日志
  int   logrt;     // 是否实时刷新日志
  char *redirect;  // 用户提交表单错误则重定向到首页
  pg_page_t *page;
  form_t *form;
}action_t;

typedef struct {
  int actnum;
  action_t *acts;
  int refparse;
}actionset_t;


extern pg_divset_t   divset;
extern pg_pageset_t  pageset;
extern stmtset_t     stmtset;
extern dbiset_t      dbiset;
extern formvarset_t  formvarset;
extern formset_t     formset;
extern actionset_t   actset;


#ifdef __cplusplus
extern "C" {
#endif

  int action_init(const char *confpath,const char *sectname,actionset_t *actset);
  void action_free(actionset_t *actset);
  int action_check(void *checkargs,const char * cgiScriptName,const char *method,const char *enctype,int contentlen);
  int action_perform(CGI_T *cgi,TCMAP *sysvars,dbi_env_t * envs,int envnum,dbi_dal_t *dals,void *refdata,char *pagebuf,int bufsize);

#ifdef __cplusplus
}
#endif

#endif

