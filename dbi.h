/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#ifndef DBI_H
#define DBI_H

#include "tcutil.h"

enum {
  DBI_TT,     // tokyo tryant
  DBI_SPH,    // sphinx
  DBI_MY,     // mysql
  DBI_MQ,     // message queue
  DBI_MC,     // memcached
  DBI_PC,     // persistent cache
  DBI_EP      // expire persistent cache
};

typedef struct {
  int type;
  void *(*global_init)(void *);
  void  (*global_free)(void *);
  void *(*init)(void *);
  void  (*free)(void *);
  int (*prepare)(void *,const char *);
  int (*bind)(void *,int,int,char **,int,const char*);
  int (*exec)(void * ,int);
  int (*exec_multi)(void *,int *,int);
  int (*gettime)(void *,int);
  int (*getsearchtime)(void *,int);
  int (*getresfrom)(void *,int);
  int (*getrestype)(void *,int);
  int (*getresbool)(void *,int);
  int (*getresint)(void *,int);
  double (*getresdouble)(void *,int);
  int (*getmaxretnum)(void*,int);
  int (*getrestotal)(void*,int);
  int (*getresnum)(void*,int);
  int (*getcolnum)(void *,int);
  const char *(*getcolname)(void *,int,int);
  const char *(*getid)(void *,int,int);
  const char *(*getrelevance)(void *,int,int);
  const char *(*getwords)(void *,int,int *);
  const char *(*fetch)(void *,int,int,int,int *);
  const char *(*fetch_byname)(void *,int,int,const char *,int *);
  int  (*freestmt)(void *,int,int);
  int freeopt;
}dbi_t;

typedef struct {
  int dbinum;
  dbi_t *dbis;
}dbiset_t;

enum {
  STMT_READ = 1, // record
  STMT_WRITE,    // write, delete, sync, vanish: bool
  STMT_COUNT,    // int 
  STMT_FCOUNT    // Float
};

enum {
  STMT_RESULT_RECORD = 1,
  STMT_RESULT_BOOL,
  STMT_RESULT_INT,
  STMT_RESULT_FLOAT
};

typedef struct {
  char *name;           // 语句名
  int   type;           // read, write, count, fcount
  int   dbi;            // database interface
  dbi_t *db;            // database driver
  char  *dalconf;       // DAL配置文件路径
  char  *rid;           // 记录ID参数对应的系统变量,可以为空，用于DAL. sphinx: @sphrand
  char *stmt;           // 语句
  int   argnum;         // 语句参数个数 
  char **args;          // 语句参数
  char **argtypes;      // 语句参数类型, used for mysql
  int   id;             // 语句ID
  pthread_mutex_t mutex;// 用于stmtid互斥修改
}stmt_t;

typedef struct {
  int stmtnum;
  stmt_t *stmts;
}stmtset_t;
//////////////////////////////////

typedef struct {
  int   type;         // tt or sphinx or my ...
  void *cfg;          // input
  void *gdata;        // output
}dbi_glob_t;

typedef struct {
  int type;
  void *env;
}dbi_env_t; 

typedef struct {
  int num;       // 语句集中有多少个语句就有多少个DAL
  void **dals;
}dbi_dal_t;

#ifdef __cplusplus
extern "C" {
#endif

  int dbi_init(const char *confpath,const char *sectname,dbiset_t *dbiset,stmtset_t *stmtset);
  void dbi_free(dbiset_t *dbiset,stmtset_t *stmtset);
  int dbi_global_init(dbiset_t *dbiset,dbi_glob_t *glob,int globnum);
  int dbi_global_free(dbiset_t *dbiset,dbi_glob_t *glob,int globnum);
  dbi_env_t *dbi_env_create(dbiset_t *dbiset,dbi_glob_t *glob,int globnum);
  int dbi_env_destory(dbiset_t *dbiset,dbi_env_t *envs);
  dbi_dal_t *dbi_dal_create(stmtset_t *stmtset);
  void dbi_dal_destory(dbi_dal_t *dal);
  int dbi_stmt_prepare(dbi_env_t *envs,int envnum,stmtset_t *stmtset);
  void dbi_stmt_pushid(stmtset_t *stmtset,TCMAP *sysvars);
  char **dbi_stmt_bind(dbi_env_t *envset,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                             char **stmtnames,int stmtnum,TCMAP *sysvars,int *argnum);
  void dbi_stmt_freeargs(char **argvals,int argnum);
  int dbi_stmt_exec(dbi_env_t *envs,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                            const char *divname,char *stmtname,TCMAP *sysvars);
  int dbi_stmt_exec_multi(dbi_env_t *envs,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                            char **divnames,char **stmtnames,int stmtnum,TCMAP *sysvars);
#ifdef __cplusplus
}
#endif

#endif

