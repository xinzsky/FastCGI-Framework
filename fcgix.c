/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcgi_config.h>
#include <fcgiapp.h>

struct _fcgiargs
{
  void *globargs;
  void *(*worker_init)(void *globargs);
  void (*worker_main)(int workerid,void *request,void *threadargs);
  void (*worker_free)(void *threadargs);
  int workernum;
  int workerid;
};

// fcgi工作线程函数
static void *fcgi_worker(void *args)
{
  int rc;
  FCGX_Request request;
  struct _fcgiargs *fcgi = (struct _fcgiargs *)args;
  void *targs;

  if(fcgi->worker_init) {
    if((targs = (*fcgi->worker_init)(fcgi->globargs)) == NULL)
      return NULL;
  }
  
  FCGX_InitRequest(&request, 0, 0);
 
  for(;;)
  {
    static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&accept_mutex);
    rc = FCGX_Accept_r(&request);
    pthread_mutex_unlock(&accept_mutex);
    if (rc < 0)
      break;

    (*fcgi->worker_main)(fcgi->workerid,(void *)&request,targs);

    FCGX_Finish_r(&request);
  }

  if(fcgi->worker_free)
    (*fcgi->worker_free)(targs);
  
  return NULL;
}


// @usrargs @workerinit @workerfree可以为NULL.
// @return: error: -1,ok: 0, > 0: 创建线程失败
int fcgi_run(int workernum,void *globargs,const char *webserveraddrlist,
                 void *workerinit,void *workermain,void *workerfree)
{
  int i;
  pthread_t *tids;
  struct _fcgiargs fcgiargs;

  if(!workermain) return -1; 
  if(workernum <= 0) workernum = 1;
  
  tids = (pthread_t *)calloc(workernum,sizeof(pthread_t));
  if(!tids) return -1;

  fcgiargs.globargs = globargs;
  fcgiargs.worker_init = workerinit;
  fcgiargs.worker_main = workermain;
  fcgiargs.worker_free = workerfree;
  fcgiargs.workernum = workernum;

  if(webserveraddrlist)
    setenv("FCGI_WEB_SERVER_ADDRS",webserveraddrlist,1);
  
  FCGX_Init();
  
  for (i = 1; i < workernum; i++)
  {
    fcgiargs.workerid = i;
    if(pthread_create(tids+i, NULL, fcgi_worker, (void *)&fcgiargs)) {
      free(tids);
      return i;
    }
  }

  fcgiargs.workerid = 0;
  fcgi_worker((void *)&fcgiargs);

  for(i = 1; i < workernum; i++) 
    pthread_join(tids[i],NULL);
  
  free(tids);
  return 0;
}

