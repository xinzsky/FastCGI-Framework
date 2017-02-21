/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef  SNAPSHOT_H
#define  SNAPSHOT_H

#ifdef __cplusplus
extern "C"
{
#endif

int snap_makehighlightpos(const char *page,int pagsize,
                                       const char *keywords,const char *url,
                                       char *newpage,int *newpagesize,
                                       char *keywordsbar,int *barsize);

#ifdef __cplusplus
}
#endif

#endif



