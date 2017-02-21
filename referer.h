/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef  REFERER_H
#define  REFERER_H

#ifdef __cplusplus
extern "C"
{
#endif

  int referer_init(const char *confpath);
  void referer_free(void);
  void *referer_new(void);
  void referer_del(void *refdata);
  char *referer_parse(void *refdata,const char *referer,char *wordbuf,int bufsize);


#ifdef __cplusplus
}
#endif

#endif


