/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef SECURITY_H
#define SECURITY_H


#ifdef __cplusplus
extern "C"
{
#endif

  void *xss_init(const char *tags,int maxtextsize);
  const char *xss_clean(void *xssdata,const char *html,int htmllen,int *sp);
  void xss_free(void *xssdata);
  char *xss_filter(const char *tags, const char * html, int htmllen, int maxtextsize);


#ifdef __cplusplus
}
#endif

#endif

