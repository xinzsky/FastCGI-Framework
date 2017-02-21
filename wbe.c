/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include "comn.h"
#include "xmalloc.h"
#include "xstring.h"
#include "utils.h"
#include "conf.h"
#include "opt.h"
#include "log.h"
#include "sig.h"
#include "seql.h"
#include "myql.h"
#include "cgic.h"
#include "fcgix.h"
#include "session.h"
#include "referer.h"
#include "page.h"
#include "dbi.h"
#include "form.h"
#include "action.h"

#define HOME_ENV            "WBE_HOME"
#define DEFAULT_POST_LIMIT  "10485760"      // 10M
#define MAX_PAGE_SIZE       (4*1024*1024) // 4M

struct _opts
{
  char *homepath;      // -h, 可选, 默认在$HOME_ENV
  int   threadnum;     // -t, 可选, 默认为单线程
  char *confpath;      // -c, 可选, 默认在$HOME_ENV/etc/wbe.conf
  char *logfile;       // -l, 可选, 默认为$HOME_ENV/log/wbe.log
  int   debug;         // -D, 可选, 默认为0
};

struct _cfg 
{
  char *webaddrlist;
  se_cfg_t se;
  struct my_cfg my;
};

struct _globargs {
  dbi_glob_t *glob;
  int globnum;
};

// 每一个工作线程都有一份的数据
struct _threadargs
{
  CGI_T *cgi;
  TCMAP *sysvars;
  dbi_env_t *envs;
  int envnum;
  dbi_dal_t *dals;
  char *pagebuf;
  int bufsize;
  void *refdata;
};

  // Globals
  dbiset_t      dbiset;
  stmtset_t     stmtset;
  formvarset_t  formvarset;
  formset_t     formset;
  pg_divset_t   divset;
  pg_pageset_t  pageset;
  actionset_t   actset;

  // Prototypes
  void getcmdopts(int argc,char *argv[],struct _opts *opts);
  void freecmdopts(struct _opts *opts);
  void getcfg(struct _cfg *cfg,const char *confpath);
  void freecfg(struct _cfg *cfg);
  void initlog(char *logpath);
  void initsig(void);
  void *worker_init(void *globargs);
  void worker_main(int workerid,void *request,void *threadargs);
  void worker_free(void *threadargs);

int main(int argc, char *argv[])
{
  struct _opts opts;
  struct _cfg  cfg;
  struct _globargs globargs; 
  dbi_glob_t dbi_glob[2];
  int workid;
  char home[MAX_PATH_LEN+1];
  char confpath[MAX_PATH_LEN+1];
  char path[MAX_PATH_LEN+1];

  memset(&opts,0,sizeof(struct _opts));
  if(argc > 1)
    getcmdopts(argc,argv,&opts);

  if(opts.debug) {
    sleep(10); // 用于FCGI模式运行时调试: gdb -p pid
  }
  
  if(opts.homepath)
    strcpy(home,opts.homepath);
  else if(gethomepath(HOME_ENV,home,MAX_PATH_LEN+1) <= 0) {
    printf("HOME Environment set error.\n");
    return -1;
  }

  if(opts.logfile)
    initlog(opts.logfile);
  else {
    snprintf(path,sizeof(path),"%s/log/wbe.log",home);
    initlog(path);
  }

  if(opts.confpath)
    snprintf(confpath,sizeof(confpath),"%s",opts.confpath);
  else
    snprintf(confpath,sizeof(confpath),"%s/etc/wbe.conf",home);
  
  getcfg(&cfg,confpath);
  
  if(dbi_init(confpath,"DB",&dbiset,&stmtset) < 0) return -1;
  if(form_init(confpath,"Web",&formvarset,&formset) < 0) return -1;
  if(page_init(confpath,"Web",&divset,&pageset,home) < 0) return -1;
  if(action_init(confpath,"Web",&actset) < 0) return -1;

  dbi_glob[0].type = DBI_SPH;
  dbi_glob[0].cfg = &cfg.se;
  dbi_glob[0].gdata = NULL;
  dbi_glob[1].type = DBI_MY;
  dbi_glob[1].cfg = &cfg.my;
  dbi_glob[1].gdata = NULL;
  if(dbi_global_init(&dbiset,dbi_glob,COUNTOF(dbi_glob)) < 0)
    return -1;

  memset(&globargs,0,sizeof(struct _globargs));
  globargs.glob = dbi_glob;
  globargs.globnum = COUNTOF(dbi_glob);

  workid = fcgi_run(opts.threadnum,&globargs,cfg.webaddrlist,\
                    (void *)worker_init,(void *)worker_main,(void *)worker_free);
  if(workid > 0)
    write_log(LLERROR,"create worker thread failure.");
  
  dbi_global_free(&dbiset,dbi_glob,COUNTOF(dbi_glob));
  action_free(&actset);
  page_free(&divset,&pageset);
  form_free(&formvarset,&formset);
  dbi_free(&dbiset,&stmtset);
  freecfg(&cfg);
  if(argc > 1) freecmdopts(&opts);
  free_log();
  return 0;
}

void *worker_init(void *globargs)
{
  struct _globargs *gargs = (struct _globargs *)globargs;
  struct _threadargs *targs;

  targs = (struct _threadargs *)xcalloc(1,sizeof(struct _threadargs));
  targs->cgi = cgiNew();
  targs->sysvars = tcmapnew();
  targs->pagebuf = (char *)xmalloc(MAX_PAGE_SIZE+1);
  targs->bufsize = MAX_PAGE_SIZE+1;
  targs->envnum = dbiset.dbinum;
  targs->envs = dbi_env_create(&dbiset,gargs->glob,gargs->globnum);
  if(!targs->envs) {
    write_log_r(LLDEBUG,"dbi_env_create() error.");
    return NULL;
  }
  
  targs->dals = dbi_dal_create(&stmtset);
  if(!targs->dals) {
    write_log_r(LLDEBUG,"dbi_dal_create() error.");
    return NULL;
  }
  
  if(dbi_stmt_prepare(targs->envs,targs->envnum,&stmtset) < 0) {
    write_log_r(LLERROR,"dbi statement prepare error.");
    return NULL;
  }
  
  if(actset.refparse) {
    if(!(targs->refdata = referer_new())) {
      write_log_r(LLERROR,"create referer global data error.");
      return NULL;
    }
  }

  return targs;
}

void worker_main(int workerid,void *request,void *threadargs)
{
  struct _threadargs *targs = (struct _threadargs *)threadargs;
  int res,pagesize;
  const char *cp;

  tcmapclear(targs->sysvars);

  res = cgiInit(targs->cgi,request);
  if(res < 0) {         // 400 Bad Request
    write_log_r(LLERROR,"cgiInit() error.");
    cgiHeaderStatus(targs->cgi,400,"Bad Request");
    return;
  }

  res = action_check(targs->sysvars,targs->cgi->cgiScriptName,targs->cgi->cgiRequestMethod,targs->cgi->cgiContentType,targs->cgi->cgiContentLength);
  if(res > 0) {        // 上传文件过大返回413
    write_log_r(LLERROR,"post data too large.");
    cgiHeaderStatus(targs->cgi,413,"Request Entity Too Large");
    cgiFree(targs->cgi);
    return;
  } else if(res < 0) { // action检查错误
    cgiHeaderStatus(targs->cgi,400,"Bad Request");
    cgiFree(targs->cgi);
    return;
  }

  if((pagesize = action_perform(targs->cgi,targs->sysvars,targs->envs,targs->envnum, \
                                targs->dals,targs->refdata,targs->pagebuf,targs->bufsize)) < 0) 
  {
    if(pagesize == -2) { // 用户输入错误返回302重定向到首页
      cp = tcmapget2(targs->sysvars,"@redirect");
      if(cp) 
        cgiHeaderLocation(targs->cgi,(char *)cp);
      else
        cgiHeaderStatus(targs->cgi,400,"Bad Request");
    } else {             // 内部错误返回503
      cgiHeaderStatus(targs->cgi,503,"Service Unavailable");
    }
  } else {               // 执行成功返回200
    cgiHeaderContentLength(targs->cgi,pagesize);
    cgiHeaderContentType(targs->cgi,"text/html","UTF-8");
    cgiOutputContent(targs->cgi,targs->pagebuf,pagesize);
  }
  
  cgiFree(targs->cgi);
  return;
}

void worker_free(void *threadargs)
{
  struct _threadargs *targs = (struct _threadargs *)threadargs;
  
  dbi_env_destory(&dbiset,targs->envs);
  dbi_dal_destory(targs->dals);
  xfree(targs->pagebuf);
  tcmapdel(targs->sysvars);
  cgiDel(targs->cgi);
  if(actset.refparse)
    referer_del(targs->refdata);
  xfree(targs);
}


void initlog(char *logpath)
{
  char *pc;
  char logfile[MAX_PATH_LEN+1];
  char logdir[MAX_PATH_LEN+1];

  strcpy(logdir,logpath);
  pc = strrchr(logdir, '/'); //注意: /xxx.log情况
  if (pc)
  {
    strcpy(logfile, pc + 1);
    if (pc != logdir)
      *pc = 0;
    else
      *(pc + 1) = 0;
  }
  else  //xxx.log
  {
    strcpy(logfile, logdir);
    logdir[0] = '.'; //当前目录
    logdir[1] = 0;
  }

  if (init_log(LLDEBUG, (500*1024*1024),logdir,logfile) < 0)
  {
    printf("initialize log failure.\n");
    exit(1);
  }
}

void initsig(void)
{
  if(signalsetupexit(SIGUSR1) < 0)
  {
    write_log(LLERROR,"unable to set SIGUSR1: %s", strerror(errno));
    exit(1);
  }
  
  if(signalblock(SIGUSR1) < 0)
  {
    write_log(LLERROR,"can't block SIGUSR1");
    exit(1);
  }
}


void printhelp(char **argv)
{
  printf("Usage: %s [options]\n", getprogramname(argv));
  printf("\
  optional: \n\
         -h, --home=dir\n\
         -c, --conf=path\n\
         -l, --log=path\n\
         -t, --thread=num\n\
         -D, --debug\n\
         -?, --help\n");
   exit(1);
}

void getcmdopts(int argc,char *argv[],struct _opts *opts)
{
  int optnum;

  opt_t optset[] = {
    {"home", 'h', HASARG, ARG_DIR,   1, NULL, NULL, NULL, NULL, &opts->homepath},
    {"conf", 'c', HASARG, ARG_STRING,1, NULL, NULL, NULL, NULL, &opts->confpath},
    {"log",  'l', HASARG, ARG_STRING,1, NULL, NULL, NULL, NULL, &opts->logfile},
    {"thread",'t',HASARG, ARG_INT,   1, "1",  (void *)0,(void *)0,NULL,&opts->threadnum},
    {"debug", 'D',NOARG,  ARG_INT,   1, NULL,  NULL, NULL, NULL, &opts->debug},
    {"help", '?', NOARG,  ARG_HELP,  1, NULL, NULL, NULL, NULL, NULL}
  };

  optnum = COUNTOF(optset);
  memset(opts,0,sizeof(struct _opts));
  getopts(argc,argv,optset,optnum,printhelp);
}

void freecmdopts(struct _opts *opts)
{
  xfree(opts->homepath);
  xfree(opts->confpath);
  xfree(opts->logfile);
}

void getcfg(struct _cfg *cfg,const char *confpath)
{
  void *cfgdata;
  conf_t items[] = {
    {"Global", "WebServer.AddrList", CONF_STR, 1, NULL,  NULL, NULL, NULL, &cfg->webaddrlist},
    {"Global", "Segwords.DicPath",   CONF_STR, 0, NULL,  NULL, NULL, NULL, &cfg->se.dicpath},
    {"Global", "Segwords.RulePath",  CONF_STR, 1, NULL,  NULL, NULL, NULL, &cfg->se.rulepath},
    {"Global", "Segwords.Unihan",    CONF_STR, 1, NULL,  NULL, NULL, NULL, &cfg->se.unihan},
    {"Global", "Segwords.SegMode",   CONF_INT, 1, "0",(void *)0,(void *)3,NULL, &cfg->se.segmode},
    {"Global", "Segwords.Ignore",    CONF_INT, 1, "0",(void *)0,(void *)1,NULL, &cfg->se.ignore},
    {"Global", "Snippet.Around",     CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.around},
    {"Global", "Snippet.Limit",      CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.limit},
    {"Global", "Snippet.WeightOrder",CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.weight_order},
    {"Global", "Snippet.LimitPassages",CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.limit_passages},
    {"Global", "Snippet.rcs",        CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.rcs},
    {"Global", "Snippet.wsp",        CONF_INT, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.wsp},
    {"Global", "Snippet.Index",      CONF_STR, 1, NULL, NULL, NULL, NULL, &cfg->se.snipopt.index},
    {"Global", "Search.Cache.Host",  CONF_STR, 1, NULL, NULL, NULL, NULL, &cfg->se.cachehost},
    {"Global", "Search.Cache.Port",  CONF_INT, 1, NULL,(void *)1,(void *)65535,NULL, &cfg->se.cacheport},
    {"Global", "MySQL.Config.File",  CONF_STR, 1, NULL, NULL, NULL, NULL, &cfg->my.file},
    {"Global", "MySQL.Config.Section",CONF_STR, 1, NULL, NULL, NULL, NULL, &cfg->my.section}
  };

  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"Can't open configure file: %s.",confpath);
    exit(1);
  }

  memset(cfg,0,sizeof(struct _cfg));
  get_conf(cfgdata,items,COUNTOF(items));
  free_conf(cfgdata);
}

void freecfg(struct _cfg *cfg)
{
  xfree(cfg->webaddrlist);
  xfree(cfg->se.dicpath);
  xfree(cfg->se.rulepath);
  xfree(cfg->se.unihan);
  xfree(cfg->se.cachehost);
  xfree(cfg->se.snipopt.index);
  xfree(cfg->my.file);
  xfree(cfg->my.section);
}

