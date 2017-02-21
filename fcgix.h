/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef FCGIX_H
#define FCGIX_H

#ifdef __cplusplus
extern "C" {
#endif

int fcgi_run(int workernum,void *globargs,const char *webserveraddrlist,
                 void *workerinit,void *workermain,void *workerfree);

#ifdef __cplusplus
}
#endif

#endif

