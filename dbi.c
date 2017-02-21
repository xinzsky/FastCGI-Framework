/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <tcutil.h>
#include "xmalloc.h"
#include "comn.h"
#include "conf.h"
#include "dbi.h"
#include "ttql.h"
#include "seql.h"
#include "myql.h"
#include "log.h"
#include "dal.h"
#include "rand.h"
#include "utils.h"

static void dbi_register_tt(dbi_t *dbi);
static void dbi_register_sphinx(dbi_t *dbi);
static void dbi_register_my(dbi_t *dbi);

int dbi_init(const char *confpath,const char *sectname,dbiset_t *dbiset,stmtset_t *stmtset)
{
  char section[128];
  stmt_t stmt;
  char *p,*dbin, **dbins,*arg,**args,*stn,**stns;
  int i,j,dbinum,argnum,stnum;

  const char *dbi[] = {
   "TT", "Sphinx", "MySQL", "MQ", "MC", "PC", "EP"
  };

  const int idbi[] = {
    DBI_TT, DBI_SPH, DBI_MY, DBI_MQ, DBI_MC, DBI_PC, DBI_EP
  };

  const char *type[] = {
    "read","write","count","fcount"
  };

  const int itype[] = {
    STMT_READ, STMT_WRITE, STMT_COUNT, STMT_FCOUNT
  };
  
  conf_t stmtc[] = {
    {section, "Type", CONF_STR, 0, NULL, (void *)type,(void *)COUNTOF(type),(void *)itype,&stmt.type},
    {section, "DBI",  CONF_STR, 0, NULL, (void *)dbi, (void *)COUNTOF(dbi), (void *)idbi,&stmt.dbi},
    {section, "DAL",  CONF_STR, 1, NULL, NULL,  NULL, NULL, &stmt.dalconf},
    {section, "RID",  CONF_STR, 1, NULL, NULL,  NULL, NULL, &stmt.rid},
    {section, "Stmt", CONF_STR, 0, NULL, NULL,  NULL, NULL, &stmt.stmt},
    {section, "Args", CONF_MULTI,1,NULL, (void *)&args,(void *)" ",(void *)&argnum,&arg}
  };
  void *cfgdata;
  
  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"Can't open configure file: %s.",confpath);
    return -1;
  }

  //dbi set 
  dbin = getconfmulti(cfgdata,sectname,"DBI",0,NULL," ",&dbins,&dbinum);
  dbiset->dbinum = dbinum;
  dbiset->dbis = (dbi_t *)xcalloc(dbiset->dbinum,sizeof(dbi_t));
  for(i=0;i<dbinum;i++)
  {
    if(strcasecmp(dbins[i],"TT") == 0)
      dbi_register_tt(dbiset->dbis+i);
    else if(strcasecmp(dbins[i],"sph") == 0 || strcasecmp(dbins[i],"sphinx") == 0)
      dbi_register_sphinx(dbiset->dbis+i);
    else if(strcasecmp(dbins[i],"my") == 0 || strcasecmp(dbins[i],"mysql") == 0)
      dbi_register_my(dbiset->dbis+i);
    else {
      write_log(LLERROR,"DBI [DBISet:DBI:%s] unsupport.",dbins[i]);
      return -1;
    }
  }
  xfree(dbins);
  xfree(dbin);

  // statment set
  stn = getconfmulti(cfgdata,sectname,"Stmts",0,NULL," ",&stns,&stnum);
  stmtset->stmtnum = stnum;
  stmtset->stmts = (stmt_t *)xcalloc(stnum,sizeof(stmt_t));
  for(i=0;i<stnum;i++) 
  {
    stmtset->stmts[i].name = xstrdup(stns[i]);
    snprintf(section,sizeof(section),"stmt:%s",stns[i]);
    memset(&stmt,0,sizeof(stmt_t));
    get_conf(cfgdata,stmtc,COUNTOF(stmtc));
    stmtset->stmts[i].type = stmt.type;
    stmtset->stmts[i].dbi = stmt.dbi;
    stmtset->stmts[i].dalconf = stmt.dalconf;
    stmtset->stmts[i].rid = stmt.rid;
    stmtset->stmts[i].stmt = stmt.stmt;
    if(arg) {
      stmtset->stmts[i].argnum = argnum;
      stmtset->stmts[i].args = (char **)xcalloc(argnum,sizeof(char *));
      stmtset->stmts[i].argtypes = (char **)xcalloc(argnum,sizeof(char *));
      for(j=0;j<argnum;j++) {
        stmtset->stmts[i].args[j] = xstrdup(args[j]);
        p = strchr(stmtset->stmts[i].args[j],':');
        if(p) {
          *p++ = 0;
          stmtset->stmts[i].argtypes[j] = p;
        }
      }

      xfree(arg);
      xfree(args);
    }
  }
  xfree(stn);
  xfree(stns);

  free_conf(cfgdata);

  for(i=0;i<stmtset->stmtnum;i++) {
    for(j=0;j<dbiset->dbinum;j++) {
      if(stmtset->stmts[i].dbi == dbiset->dbis[j].type) {
        stmtset->stmts[i].db = dbiset->dbis+j;
        break;
      }
    }

    if(j==dbiset->dbinum) {
      write_log(LLERROR,"[%s] dbi isn't implement.",stmtset->stmts[i].name);
      return -1;
    }

    pthread_mutex_init(&stmtset->stmts[i].mutex,NULL);
    stmtset->stmts[i].id = -1;

    if(stmtset->stmts[i].dalconf && !stmtset->stmts[i].rid) {
      write_log(LLERROR,"[%s] rid need set in dal mode.",stmtset->stmts[i].name);
      return -1;
    }
  }

  return 0;
}

void dbi_free(dbiset_t *dbiset,stmtset_t *stmtset)
{
  int i,j;

  xfree(dbiset->dbis);

  for(i=0;i<stmtset->stmtnum;i++)
  {
    xfree(stmtset->stmts[i].name);
    xfree(stmtset->stmts[i].dalconf);
    xfree(stmtset->stmts[i].rid);
    xfree(stmtset->stmts[i].stmt);
    for(j=0;j<stmtset->stmts[i].argnum;j++)
      xfree(stmtset->stmts[i].args[j]);
    xfree(stmtset->stmts[i].args);
    xfree(stmtset->stmts[i].argtypes);
    pthread_mutex_destroy(&stmtset->stmts[i].mutex);
  }
  xfree(stmtset->stmts);
}

static void dbi_register_tt(dbi_t *dbi)
{
  dbi->type           = DBI_TT;
  dbi->global_init    = NULL;
  dbi->global_free    = NULL;
  dbi->init           = ttquery_init;
  dbi->free           = ttquery_free;
  dbi->prepare        = ttquery_prepare;
  dbi->bind           = ttquery_bind;
  dbi->exec           = ttquery_exec;
  dbi->exec_multi     = ttquery_exec_para;
  dbi->gettime        = NULL;
  dbi->getsearchtime  = NULL;
  dbi->getresfrom     = NULL;
  dbi->getrestype     = ttquery_getrestype;
  dbi->getresbool     = ttquery_getresbool;
  dbi->getresint      = ttquery_getresint;
  dbi->getresdouble   = ttquery_getresdouble;
  dbi->getmaxretnum   = ttquery_getmaxretnum;
  dbi->getrestotal    = ttquery_getrestotal;
  dbi->getresnum      = ttquery_getresnum;
  dbi->getcolnum      = ttquery_getcolnum;
  dbi->getcolname     = ttquery_getcolname;
  dbi->getid          = NULL;
  dbi->getrelevance   = NULL;
  dbi->getwords       = NULL;
  dbi->fetch          = ttquery_fetch;
  dbi->fetch_byname   = ttquery_fetch_byname;
  dbi->freestmt       = ttquery_freestmt;
  dbi->freeopt        = TTSTMT_FREE_RESSET;
}

static void dbi_register_my(dbi_t *dbi)
{
  dbi->type           = DBI_MY;
  dbi->global_init    = myquery_global_init;
  dbi->global_free    = myquery_global_free;
  dbi->init           = myquery_init;
  dbi->free           = myquery_free;
  dbi->prepare        = myquery_prepare;
  dbi->bind           = myquery_bind;
  dbi->exec           = myquery_exec;
  dbi->exec_multi     = myquery_exec_multi;
  dbi->gettime        = NULL;
  dbi->getsearchtime  = NULL;
  dbi->getresfrom     = NULL;
  dbi->getrestype     = myquery_getrestype;
  dbi->getresbool     = NULL;
  dbi->getresint      = myquery_getresvalue;
  dbi->getresdouble   = NULL;
  dbi->getmaxretnum   = NULL;
  dbi->getrestotal    = myquery_getrestotal;
  dbi->getresnum      = myquery_getresnum;
  dbi->getcolnum      = myquery_getcolnum;
  dbi->getcolname     = myquery_getcolname;
  dbi->getid          = NULL;
  dbi->getrelevance   = NULL;
  dbi->getwords       = NULL;
  dbi->fetch          = myquery_fetch;
  dbi->fetch_byname   = myquery_fetch_byname;
  dbi->freestmt       = myquery_freestmt;
  dbi->freeopt        = MYSTMT_FREE_RESSET;
}

static void dbi_register_sphinx(dbi_t *dbi)
{
  dbi->type           = DBI_SPH;
  dbi->global_init    = sequery_global_init;
  dbi->global_free    = sequery_global_free;
  dbi->init           = sequery_init;
  dbi->free           = sequery_free;
  dbi->prepare        = sequery_prepare;
  dbi->bind           = sequery_bind;
  dbi->exec           = sequery_exec;
  dbi->exec_multi     = sequery_exec_multi;
  dbi->gettime        = sequery_gettime;
  dbi->getsearchtime  = sequery_getsearchtime;
  dbi->getresfrom     = sequery_getresfrom;
  dbi->getrestype     = sequery_getrestype;
  dbi->getresbool     = NULL;
  dbi->getresint      = NULL;
  dbi->getresdouble   = NULL;
  dbi->getmaxretnum   = sequery_getmaxretnum;
  dbi->getrestotal    = sequery_getrestotal;
  dbi->getresnum      = sequery_getresnum;
  dbi->getcolnum      = sequery_getcolnum;
  dbi->getcolname     = sequery_getcolname;
  dbi->getid          = sequery_getid;
  dbi->getrelevance   = sequery_getrelevance;
  dbi->getwords       = sequery_getwords;
  dbi->fetch          = sequery_fetch;
  dbi->fetch_byname   = sequery_fetch_byname;
  dbi->freestmt       = sequery_freestmt;
  dbi->freeopt        = SESTMT_FREE_RESSET;
}

int dbi_global_init(dbiset_t *dbiset,dbi_glob_t *glob,int globnum)
{
  int i,j;

  for(i=0;i<dbiset->dbinum;i++) {
    if(!dbiset->dbis[i].global_init)
      continue;

    for(j=0;j<globnum;j++) {
      if(dbiset->dbis[i].type == glob[j].type) { // glob[j].cfg可以为空
        glob[j].gdata = (*dbiset->dbis[i].global_init)(glob[j].cfg);
        if(!glob[j].gdata)
          return -1;
        break;
      }
    }

    if(j == dbiset->dbinum)
      return -1;
  }
  
  return 0;
}

int dbi_global_free(dbiset_t *dbiset,dbi_glob_t *glob,int globnum)
{
  int i,j;

  for(i=0;i<dbiset->dbinum;i++) {
    if(!dbiset->dbis[i].global_free)
      continue;

    for(j=0;j<globnum;j++) {
      if(dbiset->dbis[i].type == glob[j].type) {
        if(!glob[j].gdata) return -1;
        (*dbiset->dbis[i].global_free)(glob[j].gdata);
        break;
      }
    }

    if(j == dbiset->dbinum)
      return -1;
  }

  return 0;
}

// @return: dbi_env_t pointer array,need free.
dbi_env_t *dbi_env_create(dbiset_t *dbiset,dbi_glob_t *glob,int globnum)
{
  dbi_env_t *envs;
  int i,j;

  envs = (dbi_env_t *)xcalloc(dbiset->dbinum,sizeof(dbi_env_t));
  for(i=0;i<dbiset->dbinum;i++) {
    envs[i].type = dbiset->dbis[i].type;
    if(!dbiset->dbis[i].init) 
      goto EXIT;
    
    for(j=0;j<globnum;j++)
      if(dbiset->dbis[i].type == glob[j].type)
        break;
      
    if(j == globnum) 
      envs[i].env = (*dbiset->dbis[i].init)(NULL);
    else
      envs[i].env = (*dbiset->dbis[i].init)(glob[j].gdata);
    
    if(!envs[i].env) 
      goto EXIT;
  }
  
  return envs;

EXIT:
  xfree(envs);
  return NULL;
}

int dbi_env_destory(dbiset_t *dbiset,dbi_env_t *envs)
{
  int i;

  for(i=0;i<dbiset->dbinum;i++) {
    if(!dbiset->dbis[i].free || !envs[i].env) 
      return -1;

    (*dbiset->dbis[i].free)(envs[i].env);
  }

  xfree(envs);
  return 0;
}

// 每一个线程都有一份DAL,因为DAL是可写的，不能被多个线程共享。
dbi_dal_t *dbi_dal_create(stmtset_t *stmtset)
{
  int i,j;
  dbi_dal_t *dal;

  dal = (dbi_dal_t *)xcalloc(1,sizeof(dbi_dal_t));
  dal->dals = (void **)xcalloc(stmtset->stmtnum,sizeof(void *));
  dal->num = stmtset->stmtnum;

  for(i=0;i<stmtset->stmtnum;i++) {
    if(stmtset->stmts[i].dalconf) {
      for(j=0;j<i;j++) {
        if(stmtset->stmts[j].dalconf && strcasecmp(stmtset->stmts[i].dalconf,stmtset->stmts[j].dalconf) == 0) {
          dal->dals[i] = dal->dals[j];
          break;
        }
      }

      if(j==i) {
        dal->dals[i] = dal_new(stmtset->stmts[i].dalconf);
        if(!dal->dals[i]) {
          write_log_r(LLERROR,"[%s] dal_new() error.",stmtset->stmts[i].name);
          return NULL;
        }
      }
    }
  }

  return dal;
}

void dbi_dal_destory(dbi_dal_t *dal)
{
  int i,j;
  void *t;
  
  for(i=0;i<dal->num;i++) {
    if(!dal->dals[i]) continue;
    t = dal->dals[i];
    dal_del(dal->dals[i]);
    dal->dals[i] = NULL;

    for(j=i+1;j<dal->num;j++) {
      if(dal->dals[j] && dal->dals[j] == t)
        dal->dals[j] = NULL;
    }
  }

  xfree(dal->dals);
  xfree(dal);
}

int dbi_stmt_prepare(dbi_env_t *envs,int envnum,stmtset_t *stmtset)
{
  int i,j,stmtid;
  void *env;

  for(i=0;i<stmtset->stmtnum;i++) {
    for(j=0;j<envnum;j++) {
      if(stmtset->stmts[i].dbi == envs[j].type) {
        env = envs[j].env;
        break;
      }
    }

    if(j == envnum)
      return -1;
    
    if(stmtset->stmts[i].db->prepare) {
      stmtid = (*stmtset->stmts[i].db->prepare)(env,stmtset->stmts[i].stmt);
      if(stmtid < 0) 
        return -1;
      
      pthread_mutex_lock(&stmtset->stmts[i].mutex);
      if(stmtset->stmts[i].id < 0)
        stmtset->stmts[i].id = stmtid;
      else if(stmtset->stmts[i].id != stmtid) {
        pthread_mutex_unlock(&stmtset->stmts[i].mutex);
        return -1;
      }
      pthread_mutex_unlock(&stmtset->stmts[i].mutex);
    } 
    else
      return -1;
  }

  return 0;
}

void dbi_stmt_pushid(stmtset_t *stmtset,TCMAP *sysvars)
{
  int i;
  char stmtname[256],stmtid[256];
  
  for(i=0;i<stmtset->stmtnum;i++) {
    snprintf(stmtid,sizeof(stmtid),"%d",stmtset->stmts[i].id);
    snprintf(stmtname,sizeof(stmtname),"@%s",stmtset->stmts[i].name);
    tcmapput2(sysvars,stmtname,stmtid);
  }
}

// 用于复合语句中子句的参数绑定。返回参数数组和参数个数，语句执行完后需要释放。
char **dbi_stmt_bind(dbi_env_t *envset,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                             char **stmtnames,int stmtnum,TCMAP *sysvars,int *argnum)
{
  int i,j,argpos,arglen,serverid,access;
  stmt_t *stmt,**stmts;
  void *dal,**dals;
  void *env,**envs;
  char **argvals;
  const char *cp;
  char *p,*port,server[1024],name[1024],value[1024];

  stmts = (stmt_t **)xcalloc(stmtnum,sizeof(stmt_t *));
  dals = (void **)xcalloc(stmtnum,sizeof(void *));
  for(i=0;i<stmtnum;i++) {
    for(j=0;j<stmtset->stmtnum;j++) {
      if(strcasecmp(stmtnames[i],stmtset->stmts[j].name) == 0) {
        stmts[i] = stmtset->stmts + j;
        dals[i] = dalset->dals[j];
        break;
      }
    }
    if(j == stmtset->stmtnum) {
      write_log_r(LLERROR,"bind stmt#%s inexist.", stmtnames[i]);
      xfree(stmts);
      xfree(dals);
      *argnum = -1;
      return NULL;
    }
  }

  envs = (void **)xcalloc(stmtnum,sizeof(void *));
  for(i=0;i<stmtnum;i++) {
    for(j=0;j<envnum;j++) {
      if(stmts[i]->dbi == envset[j].type) {
        envs[i] = envset[j].env;
        break;
      }
    }
    if(j == envnum) {
      write_log_r(LLERROR,"bind stmt#%s dbi inexist.",stmts[i]->name);
      xfree(stmts);
      xfree(dals);
      xfree(envs);
      *argnum = -1;
      return NULL;
    }
  }
  
  *argnum = 0;
  for(i=0;i<stmtnum;i++)
    *argnum += stmts[i]->argnum;
  if(*argnum == 0) {
    xfree(stmts);
    xfree(dals);
    xfree(envs);
    return NULL;
  }
  argvals = (char **)xcalloc(*argnum,sizeof(char *));

  argpos = 0;
  for(i=0;i<stmtnum;i++) {
    stmt = stmts[i];
    dal = dals[i];
    env = envs[i];
    
    serverid = -1;
    if(dal) {
      if(strcasecmp(stmt->rid,"@sphrand") == 0) { // used for sphinx
        unsigned int randnum;
        char rands[32];
        _AutoSrand();
        randnum = _Rand();
        randnum = randnum % 10000000;
        snprintf(rands,sizeof(rands),"%u",randnum);
        tcmapput2(sysvars,"@sphrand",rands);
      } 
      cp = tcmapget2(sysvars,stmt->rid);
      if(!cp) {
        write_log_r(LLERROR,"Statement: %s get rid[%s] error for DAL.",stmt->name,stmt->rid);
        goto FREE;
      }
      if(stmt->type == STMT_READ)
        access = DAL_ACCESS_READ;
      else if(stmt->type == STMT_WRITE || stmt->type == STMT_COUNT || stmt->type == STMT_FCOUNT)
        access = DAL_ACCESS_WRITE;
      serverid = dal_getserver(dal,cp,strlen(cp),access,server);
      if(serverid < 0) {
        write_log_r(LLERROR,"Statement: %s get server error by DAL.",stmt->name);
        goto FREE;
      } else {
        p = strchr(server,':');
        *p++ = 0;
        port = p;
        p = strchr(port,':');
        *p = 0;
        tcmapput2(sysvars,"@host",server);
        tcmapput2(sysvars,"@port",port);
        snprintf(name,sizeof(name),"@%s#serverid",stmt->name);
        snprintf(value,sizeof(value),"%d",serverid);
        tcmapput2(sysvars,name,value);
      }
    }

    for(j=0;j<stmt->argnum;j++) {
      arglen = 0;
      cp = tcmapget2(sysvars,stmt->args[j]);
      if(cp) {
        argvals[argpos] = xstrdup(cp);
        arglen = strlen(argvals[argpos]);
      }
      if((*stmt->db->bind)(env,stmt->id,j,argvals+argpos,arglen,stmt->argtypes[j]) < 0) {
        write_log_r(LLERROR,"Statement: %s bind error.",stmt->name);
        goto FREE;
      }
      argpos++;
    }

    if(dal) {
      tcmapout2(sysvars,"@host");
      tcmapout2(sysvars,"@port");
    }
  }

  xfree(stmts);
  xfree(dals);
  xfree(envs);
  return argvals;

FREE:
  for(i=0;i<*argnum;i++)
    xfree(argvals[i]);
  xfree(argvals);
  xfree(stmts);
  xfree(dals);
  xfree(envs);
  *argnum = -1;
  return NULL;
}

void dbi_stmt_freeargs(char **argvals,int argnum)
{
  int i;
  for(i=0;i<argnum;i++)
    xfree(argvals[i]);
  xfree(argvals);
}

int dbi_stmt_exec(dbi_env_t *envs,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                          const char *divname,char *stmtname,TCMAP *sysvars)
{
  int i,j,argnum,serverid;
  char **argvals;
  const char *cp;
  char name[1024],value[1024];
  void *env,*dal;
  stmt_t *stmt;

  // stmt & DAL
  for(i=0;i<stmtset->stmtnum;i++) {
    if(strcasecmp(stmtname,stmtset->stmts[i].name) == 0) {
      stmt = stmtset->stmts+i;
      dal = dalset->dals[i];
      break;
    }
  }
  if(i == stmtset->stmtnum) {
    write_log_r(LLERROR,"div#%s stmt#%s inexist.",divname,stmtname);
    return -1;
  }

  // enviroment
  for(i=0;i<envnum;i++) {
    if(stmt->dbi == envs[i].type) {
      env = envs[i].env;
      break;
    }
  }
  if(i == envnum) {
    write_log_r(LLERROR,"div#%s stmt#%s dbi inexist.",divname,stmt->name);
    return -1;
  }
  
  // bind parameters
  if(stmt->argnum) {
    argvals = dbi_stmt_bind(envs,envnum,stmtset,dalset,&stmtname,1,sysvars,&argnum);
    if(!argvals)
      return -1;
  }

  // set statement type
  snprintf(name,sizeof(name),"%s#%s#type",divname,stmt->name);
  switch(stmt->type) {
    case STMT_READ:  tcmapput2(sysvars,name,"read"); break;
    case STMT_WRITE: tcmapput2(sysvars,name,"write"); break;
    case STMT_COUNT: 
    case STMT_FCOUNT:
      tcmapput2(sysvars,name,"count");
      break;
  }

  // execute statement
  if((*stmt->db->exec)(env,stmt->id) < 0) {
    snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
    tcmapput2(sysvars,name,"down");
    write_log_r(LLERROR,"Statement: %s execute error in div#%s.",stmt->name,divname);
    snprintf(name,sizeof(name),"@%s#serverid",stmt->name);
    cp = tcmapget2(sysvars,name);
    if(cp && dal) {
      serverid = atoi(cp);
      if(serverid >= 0)
        dal_setfailserver(dal,serverid);
    }
    goto END;
  }

  // result process
  switch(stmt->type)
  {
    int count,resnum,restotal,maxretnum,time,colnum,from;
    double fcount;
    const char *colname,*colvalue;
    
    case STMT_READ:
      if(stmt->db->getresnum) {
        resnum = (*stmt->db->getresnum)(env,stmt->id);
        snprintf(name,sizeof(name),"%s#%s#resnum",divname,stmt->name);
        snprintf(value,sizeof(value),"%d",resnum);
        tcmapput2(sysvars,name,value);
      }
      
      if(stmt->db->gettime) {
        time = (*stmt->db->gettime)(env,stmt->id);
        snprintf(value,sizeof(value),"%d.%03d",time/1000, time%1000);
        snprintf(name,sizeof(name),"%s#%s#time",divname,stmt->name);
        tcmapput2(sysvars,name,value);
      }

      if(stmt->db->getsearchtime) {
        time = (*stmt->db->getsearchtime)(env,stmt->id);
        snprintf(value,sizeof(value),"%d.%03d",time/1000, time%1000);
        snprintf(name,sizeof(name),"%s#%s#searchtime",divname,stmt->name);
        tcmapput2(sysvars,name,value);
      }

      if(stmt->db->getresfrom) {
        const char *resfrom[] = {"SEARCHD","CACHE","BATCH QUERY","CACHE FOR BATCH QUERY"};
        from = (*stmt->db->getresfrom)(env,stmt->id);
        //snprintf(value,sizeof(value),"%d",from);
        snprintf(name,sizeof(name),"%s#%s#resfrom",divname,stmt->name);
        tcmapput2(sysvars,name,resfrom[from]);
      }

      if(stmt->db->getmaxretnum) {
        maxretnum = (*stmt->db->getmaxretnum)(env,stmt->id);
        snprintf(value,sizeof(value),"%d",maxretnum);
        snprintf(name,sizeof(name),"%s#%s#maxretnum",divname,stmt->name);
        tcmapput2(sysvars,name,value);
      }

      if(stmt->db->getrestotal) {
        restotal = (*stmt->db->getrestotal)(env,stmt->id);
        //snprintf(value,sizeof(value),"%d",restotal);
        format_thousands_separator((long)restotal,value,sizeof(value));
        snprintf(name,sizeof(name),"%s#%s#restotal",divname,stmt->name);
        tcmapput2(sysvars,name,value);
      }

      if(stmt->db->getwords) {
        int wordnum;
        cp = (*stmt->db->getwords)(env,stmt->id,&wordnum);
        if(cp) {
          snprintf(value,sizeof(value),"%d",wordnum);
          snprintf(name,sizeof(name),"%s#%s#wordnum",divname,stmt->name);
          tcmapput2(sysvars,name,value);
          snprintf(name,sizeof(name),"%s#%s#words",divname,stmt->name);
          tcmapput2(sysvars,name,cp);
        }
      }

      colnum = (*stmt->db->getcolnum)(env,stmt->id);
      for(i=0;i<resnum;i++) {
        for(j=0;j<colnum;j++) {
          colname = (*stmt->db->getcolname)(env,stmt->id,j);
          colvalue = (*stmt->db->fetch_byname)(env,stmt->id,i,colname,NULL);
          if(colvalue) {
            snprintf(name,sizeof(name),"%s#%s#%d#%s",divname,stmt->name,i,colname);
            tcmapput2(sysvars,name,colvalue);
          }
        }
      }

      snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
      tcmapput2(sysvars,name,"record");
        
      break;
    case STMT_WRITE:
      if((*stmt->db->getresbool)(env,stmt->id)) {
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        tcmapput2(sysvars,name,"ok");
      }
      else {
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        tcmapput2(sysvars,name,"error");
        write_log_r(LLERROR,"Statement: %s execute failure.",stmt->name);
      }
      break;
    case STMT_COUNT:
      if((count = (*stmt->db->getresint)(env,stmt->id)) >= 0) {
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        snprintf(value,sizeof(value),"%d",count);
        tcmapput2(sysvars,name,value);
      } else {
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        tcmapput2(sysvars,name,"error_count");
        write_log_r(LLERROR,"Statement: %s execute failure.",stmt->name);
      }
      break;
    case STMT_FCOUNT:
      fcount = (*stmt->db->getresdouble)(env,stmt->id);
      snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
      snprintf(value,sizeof(value),"%f",fcount);
      tcmapput2(sysvars,name,value);
      break;
    default:
      snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
      tcmapput2(sysvars,name,"error_count"); // error_count在page生成时不会处理
      write_log_r(LLERROR,"Statement: %s type unsupport.",stmt->name);
      break;
  }

  (*stmt->db->freestmt)(env,stmt->id,stmt->db->freeopt);

END:
  if(stmt->argnum)
    dbi_stmt_freeargs(argvals,argnum);
  return 0;
}

// 同一类数据源的多语句并行执行
int dbi_stmt_exec_multi(dbi_env_t *envs,int envnum,stmtset_t *stmtset,dbi_dal_t *dalset,
                                   char **divnames,char **stmtnames,int stmtnum,TCMAP *sysvars)
{
  int i,j,k,res,dbi,*sid,argnum,serverid;
  const char *cp;
  char **argvals,name[1024],value[1024];
  void *env,**dals;
  stmt_t **stmts;

  argnum = 0;
  stmts = (stmt_t **)xcalloc(stmtnum,sizeof(stmt_t *));
  dals = (void **)xcalloc(stmtnum,sizeof(void *));
  for(i=0;i<stmtnum;i++) {
    for(j=0;j<stmtset->stmtnum;j++) {
      if(strcasecmp(stmtnames[i],stmtset->stmts[j].name) == 0) {
        stmts[i] = stmtset->stmts + j;
        dals[i] = dalset->dals[j];
        argnum += stmts[i]->argnum;
        break;
      }
    }
    if(j == stmtset->stmtnum) {
      write_log_r(LLERROR,"div#%s stmt#%s inexist.",divnames[i],stmtnames[i]);
      goto FREE;
    }
  }

  // 检查各个语句dbi是否一致
  dbi = stmts[0]->dbi;
  for(i=1;i<stmtnum;i++) {
    if(stmts[i]->dbi != dbi) {
      write_log_r(LLERROR,"multi-query statement dbi inconsistency.");
      goto FREE;
    }
  }

  // enviroment
  for(i=0;i<envnum;i++) {
    if(dbi == envs[i].type) {
      env = envs[i].env;
      break;
    }
  }
  if(i == envnum) {
    write_log_r(LLERROR,"multi-query statement dbi inexist.");
    goto FREE;
  }

  // bind parameters
  if(argnum) {
    argvals = dbi_stmt_bind(envs,envnum,stmtset,dalset,stmtnames,stmtnum,sysvars,&argnum);
    if(!argvals) 
      goto FREE;
  }

  // set statement type
  for(i=0;i<stmtnum;i++) {
    snprintf(name,sizeof(name),"%s#%s#type",divnames[i],stmts[i]->name);
    switch(stmts[i]->type) {
      case STMT_READ:  tcmapput2(sysvars,name,"read"); break;
      case STMT_WRITE: tcmapput2(sysvars,name,"write"); break;
      case STMT_COUNT: 
      case STMT_FCOUNT:
        tcmapput2(sysvars,name,"count");
        break;
    }
  }

  // para execute statement
  sid = (int *)xcalloc(stmtnum,sizeof(int));
  for(i=0;i<stmtnum;i++) 
    sid[i] = stmts[i]->id;

  res = (*stmts[0]->db->exec_multi)(env,sid,stmtnum);
  if(res < 0) { // 全部执行错误
    for(i=0;i<stmtnum;i++)
      sid[i] = -1;
  }
  
  for(i=0;i<stmtnum;i++) {
    if(sid[i] < 0) { // 该语句执行错误
      snprintf(name,sizeof(name),"%s#%s",divnames[i],stmts[i]->name);
      tcmapput2(sysvars,name,"down");
      write_log_r(LLERROR,"Statement: %s execute error in div#%s.",stmts[i]->name,divnames[i]);
      snprintf(name,sizeof(name),"@%s#serverid",stmts[i]->name);
      cp = tcmapget2(sysvars,name);
      if(cp && dals[i]) {
        serverid = atoi(cp);
        if(serverid >= 0)
          dal_setfailserver(dals[i],serverid);
      }
    }
  }
  
  // result process
  for(k=0;k<stmtnum;k++) {
    const char *divname;
    stmt_t *stmt;
    if(sid[k] < 0) continue;
    stmt = stmts[k];
    divname = divnames[k];
    switch(stmt->type)
    {
      int count,resnum,restotal,maxretnum,time,colnum,from;
      double fcount;
      const char *colname,*colvalue;
      
      case STMT_READ:
        if(stmt->db->getresnum) {
          resnum = (*stmt->db->getresnum)(env,stmt->id);
          snprintf(value,sizeof(value),"%d",resnum);
          snprintf(name,sizeof(name),"%s#%s#resnum",divname,stmt->name);
          tcmapput2(sysvars,name,value);
        }
        
        if(stmt->db->gettime) {
          time = (*stmt->db->gettime)(env,stmt->id);
          //snprintf(value,sizeof(value),"%d",time);
          snprintf(value,sizeof(value),"%d.%03d",time/1000, time%1000);
          snprintf(name,sizeof(name),"%s#%s#time",divname,stmt->name);
          tcmapput2(sysvars,name,value);
        }

        if(stmt->db->getsearchtime) {
          time = (*stmt->db->getsearchtime)(env,stmt->id);
          //snprintf(value,sizeof(value),"%d",time);
          snprintf(value,sizeof(value),"%d.%03d",time/1000, time%1000);
          snprintf(name,sizeof(name),"%s#%s#searchtime",divname,stmt->name);
          tcmapput2(sysvars,name,value);
        }

        if(stmt->db->getresfrom) {
          const char *resfrom[] = {"SEARCHD","CACHE","BATCH QUERY","CACHE FOR BATCH QUERY"};
          from = (*stmt->db->getresfrom)(env,stmt->id);
          //snprintf(value,sizeof(value),"%d",from);
          snprintf(name,sizeof(name),"%s#%s#resfrom",divname,stmt->name);
          tcmapput2(sysvars,name,resfrom[from]);
        }

        if(stmt->db->getmaxretnum) {
          maxretnum = (*stmt->db->getmaxretnum)(env,stmt->id);
          snprintf(value,sizeof(value),"%d",maxretnum);
          snprintf(name,sizeof(name),"%s#%s#maxretnum",divname,stmt->name);
          tcmapput2(sysvars,name,value);
        }

        if(stmt->db->getrestotal) {
          restotal = (*stmt->db->getrestotal)(env,stmt->id);
          //snprintf(value,sizeof(value),"%d",restotal);
          format_thousands_separator((long)restotal,value,sizeof(value));
          snprintf(name,sizeof(name),"%s#%s#restotal",divname,stmt->name);
          tcmapput2(sysvars,name,value);
        }

        if(stmt->db->getwords) {
          int wordnum;
          cp = (*stmt->db->getwords)(env,stmt->id,&wordnum);
          if(cp) {
            snprintf(value,sizeof(value),"%d",wordnum);
            snprintf(name,sizeof(name),"%s#%s#wordnum",divname,stmt->name);
            tcmapput2(sysvars,name,value);
            snprintf(name,sizeof(name),"%s#%s#words",divname,stmt->name);
            tcmapput2(sysvars,name,cp);
          }
        }

        colnum = (*stmt->db->getcolnum)(env,stmt->id);
        for(i=0;i<resnum;i++) {
          for(j=0;j<colnum;j++) {
            colname = (*stmt->db->getcolname)(env,stmt->id,j);
            colvalue = (*stmt->db->fetch_byname)(env,stmt->id,i,colname,NULL);
            if(colvalue) {
              snprintf(name,sizeof(name),"%s#%s#%d#%s",divname,stmt->name,i,colname);
              tcmapput2(sysvars,name,colvalue);
            }
          }
        }

        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        tcmapput2(sysvars,name,"record");
          
        break;
      case STMT_WRITE:
        if((*stmt->db->getresbool)(env,stmt->id)) {
          snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
          tcmapput2(sysvars,name,"ok");
        }
        else {
          snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
          tcmapput2(sysvars,name,"error");
          write_log_r(LLERROR,"Statement: %s execute failure.",stmt->name);
        }
        break;
      case STMT_COUNT:
        if((count = (*stmt->db->getresint)(env,stmt->id)) >= 0) {
          snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
          snprintf(value,sizeof(value),"%d",count);
          tcmapput2(sysvars,name,value);
        } else {
          snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
          tcmapput2(sysvars,name,"error_count");
          write_log_r(LLERROR,"Statement: %s execute failure.",stmt->name);
        }
        break;
      case STMT_FCOUNT:
        fcount = (*stmt->db->getresdouble)(env,stmt->id);
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        snprintf(value,sizeof(value),"%f",fcount);
        tcmapput2(sysvars,name,value);
        break;
      default:
        snprintf(name,sizeof(name),"%s#%s",divname,stmt->name);
        tcmapput2(sysvars,name,"error_count");
        write_log_r(LLERROR,"Statement: %s type unsupport.",stmt->name);
        break;
    }

    (*stmt->db->freestmt)(env,stmt->id,stmt->db->freeopt);
  }

  if(argnum) 
    dbi_stmt_freeargs(argvals,argnum);
  xfree(sid);
  xfree(stmts);
  xfree(dals);
  return 0;

FREE:
  xfree(stmts);
  xfree(dals);
  return -1;
}

#ifdef TEST_DBI

void initlog(char *logpath);

int main()
{
  int envnum;
  const char *cp;
  dbiset_t dbiset;
  stmtset_t stmtset;
  dbi_glob_t dbi_glob[1];
  se_cfg_t  secfg;
  dbi_env_t *envs;
  dbi_dal_t *dal;
  TCMAP *sysvars;
  char *divnames[3];
  char *stmtnames[2];

  initlog("dbi.log");
  
  if(dbi_init("../etc/wbe.conf","db",&dbiset,&stmtset) < 0) {
    printf("dbi_init() error.\n");
    return -1;
  }

  secfg.dicpath = "/root/bluefire/src/sw/dic/sw.xdb";
  secfg.rulepath = "/root/bluefire/src/sw/etc/rules.utf8.ini";
  secfg.segmode = 0;
  secfg.ignore = 0;
  secfg.limit = 0;
  secfg.around = 0;
  secfg.cachehost = "localhost";
  secfg.cacheport = 10000;
  
  dbi_glob[0].type = DBI_SPH;
  dbi_glob[0].cfg = &secfg;
  dbi_glob[0].gdata = NULL;
  if(dbi_global_init(&dbiset,dbi_glob,COUNTOF(dbi_glob)) < 0) {
    printf("dbi_global_init() error.\n");
    return -1;
  }

  envs = dbi_env_create(&dbiset,dbi_glob,COUNTOF(dbi_glob));
  if(!envs) {
    printf("dbi_env_create() error.\n");
    return -1;
  }
  envnum = dbiset.dbinum;

  dal = dbi_dal_create(&stmtset);
  if(!dal) {
    printf("dbi_dal_create() error.\n");
    return -1;
  }
  
  if(dbi_stmt_prepare(envs,envnum,&stmtset) < 0) {
    printf("dbi_stmt_prepare() error.\n");
    return -1;
  }

  sysvars = tcmapnew();
  tcmapput2(sysvars,"rid","\"836598971\"");
  tcmapput2(sysvars,"query1","H21022138");
  tcmapput2(sysvars,"query2","H10920011");
  if(dbi_stmt_exec(envs,envnum,&stmtset,dal,"main",stmtset.stmts->name,sysvars) < 0) {
    printf("dbi_stmt_exec() error.\n");
    return -1;
  }

  divnames[0] = "sr1";
  divnames[1] = "sr2";
  stmtnames[0] = stmtset.stmts[1].name;
  stmtnames[1] = stmtset.stmts[2].name;
  if(dbi_stmt_exec_multi(envs,envnum,&stmtset,divnames,stmtnames,2,sysvars) < 0) {
     printf("dbi_stmt_exec_multi() error.\n");
     return -1;
  }

  tcmapiterinit(sysvars);
  while((cp = tcmapiternext2(sysvars))) {
    printf("%s: %s\n",cp,tcmapget2(sysvars,cp));
  }

  tcmapdel(sysvars);
  dbi_dal_destory(dal);
  dbi_env_destory(&dbiset,envs);
  dbi_global_free(&dbiset,dbi_glob,COUNTOF(dbi_glob));
  dbi_free(&dbiset,&stmtset);
  free_log();
  return 0;
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

#endif

