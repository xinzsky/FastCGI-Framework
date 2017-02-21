/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef  FORM_H
#define  FORM_H

#include "tcutil.h"

enum {
  FORM_VAR_INT = 1,
  FORM_VAR_FLOAT,
  FORM_VAR_STRING,
  FORM_VAR_STRING_NONL,     // string no newlines
  FORM_VAR_STRING_MULTI,    // multiple the same name string variable 
  FORM_VAR_SELECT_SINGLE,   // select single 
  FORM_VAR_SELECT_MULTIPLE, // select multiple
  FORM_VAR_SUBMIT,
  FORM_VAR_CHECKBOX_SINGLE,
  FORM_VAR_CHECKBOX_MULTIPLE,
  FORM_VAR_RADIO,
  FORM_VAR_UPLOAD
};

enum {
  FORM_VAR_HTMLENCODE = 1,
  FORM_VAR_URLENCODE,
  FORM_VAR_BASE64ENCODE,
  FORM_VAR_BASE64DECODE,
  FORM_VAR_MD5,
  FORM_VAR_XSS
};

typedef struct _formvar
{
  char *name;
  int type;
  union {int imin;double fmin;}min;
  union {int imax;double fmax;}max;
  char **choices;
  int choicenum;
  int isnull;
  char *defval;
  int maxlen;
  char *delimiter;      // used for multi-string

  char *relvar;         // 相关变量,used for int,float
  struct _formvar *rvar;

  // used for string variable.
  int encode;           // url,html,base64
  int handnum;
  int *handler;         // 对解析出来的字符串变量值进行处理
  char **args;
}formvar_t;

typedef struct {
  int varnum;
  formvar_t *vars;
}formvarset_t;

typedef struct
{
  char *name;
  int varnum;
  char **varname;
  formvar_t **vars;
  char *method;    // get, post, both
  char *enctype;   // application/x-www-form-urlencoded,multipart/form-data,both
  int postlimit;   // 0: no limit.
  char *proccfg;   // 表单处理配置文件
  char *procsect;  // 表单处理配置文件section
  void *fpargs;    
}form_t;

typedef struct
{
  int formnum;
  form_t *forms;
}formset_t;

typedef struct 
{
  char *varname;
  char *filename;
  char *contentype;
  int filesize;
  char *filebuf;
}upload_file_t;

#ifdef __cplusplus
extern "C" {
#endif

  int form_init(const char *confpath,const char *sectname,formvarset_t *formvarset,formset_t *formset);
  void form_free(formvarset_t *formvarset,formset_t *formset);
  int form_parse(CGI_T *cgi,form_t *form,TCMAP *sysvars);
  int form_upload(CGI_T *cgi,upload_file_t *file);
  int form_check(form_t *form,const char *method,const char *enctype,int contentlen);
  
#ifdef __cplusplus
}
#endif

#endif


