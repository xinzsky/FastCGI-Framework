/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include "comn.h"
#include "opt.h"
#include "xmalloc.h"
#include "cgic.h"
#include "fcgix.h"

struct _opts
{
  int   threadnum;     // -t, 可选, 默认为单线程
  int   debug;         // -D, 可选, 默认为0
};


struct _threadargs 
{
  CGI_T *cgi;
};

void getcmdopts(int argc,char *argv[],struct _opts *opts);
void freecmdopts(struct _opts *opts);
void *worker_init(void *globargs);
void worker_main(int workerid,void *request,void *threadargs);
void worker_free(void *threadargs);

int main(int argc, char *argv[])
{
  struct _opts opts;
  int workid;

  memset(&opts,0,sizeof(struct _opts));
  if(argc > 1)
    getcmdopts(argc,argv,&opts);

  if(opts.debug) {
    sleep(10); // 用于FCGI模式运行时调试: gdb -p pid
  }

  workid = fcgi_run(opts.threadnum,NULL,NULL,(void *)worker_init,(void *)worker_main,(void *)worker_free);
  if(workid > 0) 
    printf("create worker thread failure.\n");

  if(argc > 1) 
    freecmdopts(&opts);
  return 0;
}

void *worker_init(void *globargs)
{
  struct _threadargs *targs;

  targs = (struct _threadargs *)xcalloc(1,sizeof(struct _threadargs));
  targs->cgi = cgiNew();
  return targs;
}

void worker_main(int workerid,void *request,void *threadargs)
{
  int result,needspace;
  char buf[1024];
  struct _threadargs *targs = (struct _threadargs *)threadargs;
  
  result = cgiInit(targs->cgi,request);
  if(result < 0) {         // 400 Bad Request
    cgiHeaderStatus(targs->cgi,400,"Bad Request");
    return;
  }

  result = cgiFormStringSpaceNeeded(targs->cgi,"url",&needspace);
  if(result != cgiFormSuccess) goto ret400;
  if((needspace-1) >= sizeof(buf)) goto ret400;

  result = cgiFormStringNoNewlines(targs->cgi,"url",buf,sizeof(buf));
  if(result != cgiFormSuccess) 
    goto ret400;
    
  cgiHeaderLocation(targs->cgi,buf);
  cgiFree(targs->cgi);
  return;

ret400:
  cgiHeaderStatus(targs->cgi,400,"Bad Request");
  cgiFree(targs->cgi);
  return;
}

void worker_free(void *threadargs)
{
  struct _threadargs *targs = (struct _threadargs *)threadargs;
  
  cgiDel(targs->cgi);
  xfree(targs);
}

void printhelp(char **argv)
{
  printf("Usage: %s [options]\n", getprogramname(argv));
  printf("\
  optional: \n\
         -t, --thread=num\n\
         -D, --debug\n\
         -?, -h, --help\n");
   exit(1);
}

void getcmdopts(int argc,char *argv[],struct _opts *opts)
{
  int optnum;

  opt_t optset[] = {
    {"thread",'t',HASARG, ARG_INT,   1, "1",  (void *)0,(void *)0,NULL,&opts->threadnum},
    {"debug", 'D',NOARG,  ARG_INT,   1, NULL,  NULL, NULL, NULL, &opts->debug},
    {"help", '?', NOARG,  ARG_HELP,  1, NULL, NULL, NULL, NULL, NULL},
    {"help", 'h', NOARG,  ARG_HELP,  1, NULL, NULL, NULL, NULL, NULL}
  };

  optnum = COUNTOF(optset);
  memset(opts,0,sizeof(struct _opts));
  getopts(argc,argv,optset,optnum,printhelp);
}

void freecmdopts(struct _opts *opts)
{
}



