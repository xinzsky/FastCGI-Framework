/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <tcrdb.h>
#include "conf.h"
#include "xmalloc.h"
#include "url.h"
#include "urlparse.h"
#include "xstring.h"
#include "comn.h"
#include "crc32.h"
#include "log.h"

#define PAGE_ADJUST_MODE   "pn/rn+1"
#define MAX_STORE_COL      11

typedef struct {
  char *name;     // 引用来源网站,eg.baidu
  int dnum;       // 引用来源网站域名,可以有多个,eg. baidu.com 
  char **domains; 
  char *domain;
  char *action;   // 引用URL's action, eg. s, search, q, ...
  char *word;     // 引用URL查询关键词参数, eg. wd, q, w,...
  char *pn;       // 引用URL页号或分页起始数, eg. pn, start,...
  char *pnadj;    // 页号调整模式，只支持一种: pn/rn+1
  int  defpn;     // 页号参数不存在时的默认页号
  char *rn;       // 每页结果数参数
  int  defrn;     // 每页结果数参数不存在时的默认值
  char *code;     // 查询关键词的编码参数
  char *defcode;  // 查询关键词的编码参数不存在时默认编码: utf-8,gb18030
  int otnum;      // 其他查询参数: bs oq ...
  char *other;
  char **others;
}source_t;

typedef struct {
  int srcnum;
  source_t *src;
  char *dbhost;
  int   dbport;
  char *tocode;    // default: utf-8
  char *tocode2;   // default: utf-8//IGNORE
  char *fromcode;  // default: gb18030
}cfg_t;

typedef struct {
  TCRDB *dbc;
  iconv_t cd;
  TCMAP *cols;
}global_t;        // in multi-thread, every thread has one.

static cfg_t rc;  // only read in multi-thread.

int referer_init(const char *confpath)
{ 
  char *srcname,**srcnames;
  int  i,j,srcnum;
  char section[128];
  source_t s;
  void *cfgdata;
  const char *ccode[] = {"utf-8","gb18030"};
  
  conf_t global[] = {
    {"Referer",    "Sources",      CONF_MULTI, 0,  NULL, (void *)&srcnames,(void *)" ",(void *)&srcnum,&srcname},
    {"Referer",    "DBHost",       CONF_STR,   1,  NULL,  NULL, NULL, NULL, &rc.dbhost},
    {"Referer",    "DBPort",       CONF_INT,   1,  NULL, (void *)0,(void *)65535,NULL,&rc.dbport},
    {"Referer",    "ToCode",       CONF_STR,   1,  "utf-8",(void *)ccode,(void *)COUNTOF(ccode), NULL, &rc.tocode},
    {"Referer",    "FromCode",     CONF_STR,   1,  "gb18030",(void *)ccode,(void *)COUNTOF(ccode),NULL,&rc.fromcode}
  };

  conf_t src[] = {
    {section,      "Domains",      CONF_MULTI, 0,  NULL, (void *)&s.domains,(void *)" ",(void *)&s.dnum,&s.domain},
    {section,      "Action",       CONF_STR,   0,  NULL,  NULL,  NULL,  NULL, &s.action},
    {section,      "Word",         CONF_STR,   0,  NULL,  NULL,  NULL,  NULL, &s.word},
    {section,      "PageNum",      CONF_STR,   0,  NULL,  NULL,  NULL,  NULL, &s.pn},
    {section,      "PageAdjust",   CONF_STR,   1,  NULL,  NULL,  NULL,  NULL, &s.pnadj},
    {section,      "DefaultPageNum",CONF_INT,  0,  NULL,  NULL,  NULL,  NULL, &s.defpn},
    {section,      "ResultNum",    CONF_STR,   1,  NULL,  NULL,  NULL,  NULL, &s.rn},
    {section,      "DefaultResultNum",CONF_INT,0,  NULL,  NULL,  NULL,  NULL, &s.defrn},
    {section,      "WordCode",     CONF_STR,   1,  NULL,  NULL,  NULL,  NULL, &s.code},
    {section,      "DefaultCode",  CONF_STR,   0,  NULL,  (void *)ccode,(void *)COUNTOF(ccode), NULL, &s.defcode},
    {section,      "Others",       CONF_MULTI, 1,  NULL, (void *)&s.others,(void *)" ",(void *)&s.otnum,&s.other}
  };

  memset(&rc,0,sizeof(cfg_t));
  
  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"can't open configure file: %s.",confpath);
    return -1;
  }

  get_conf(cfgdata,global,COUNTOF(global));

  if(rc.tocode) {
    rc.tocode2 = (char *)xcalloc(1,strlen(rc.tocode)+strlen("//IGNORE")+1);
    strcpy(rc.tocode2,rc.tocode);
    strcat(rc.tocode2,"//IGNORE");
  }

  rc.srcnum = srcnum;
  rc.src = (source_t *)xcalloc(rc.srcnum,sizeof(source_t));
  for(i=0;i<rc.srcnum;i++)
  {
    snprintf(section,sizeof(section),"source:%s",srcnames[i]);
    memset(&s,0,sizeof(source_t));
    get_conf(cfgdata,src,COUNTOF(src));
    memcpy(rc.src+i,&s,sizeof(source_t));
    rc.src[i].name = xstrdup(srcnames[i]);
    for(j=0;j<rc.src[i].dnum;j++)
      strtolower_ascii(rc.src[i].domains[j]);
  }

  xfree(srcname);
  xfree(srcnames);
  free_conf(cfgdata);
  return 0;
}

void referer_free(void)
{
  int i;

  for(i=0;i<rc.srcnum;i++)
  {
    xfree(rc.src[i].name);
    xfree(rc.src[i].domains);
    xfree(rc.src[i].domain);
    xfree(rc.src[i].action);
    xfree(rc.src[i].word);
    xfree(rc.src[i].pn);
    xfree(rc.src[i].pnadj);
    xfree(rc.src[i].rn);
    xfree(rc.src[i].code);
    xfree(rc.src[i].defcode);
    xfree(rc.src[i].others);
    xfree(rc.src[i].other);
  }
  xfree(rc.src);
  xfree(rc.dbhost);
  xfree(rc.tocode);
  xfree(rc.tocode2);
  xfree(rc.fromcode);
}

void *referer_new(void)
{
  global_t *g;

  g = (global_t *)xcalloc(1,sizeof(global_t));
  if(rc.dbhost) {
    g->dbc = tcrdbnew();
    if(!tcrdbopen(g->dbc,rc.dbhost,rc.dbport)) {
      tcrdbdel(g->dbc);
      xfree(g);
      return NULL;
    }

    g->cols = tcmapnew2(MAX_STORE_COL);
  }

  if((g->cd = iconv_open(rc.tocode2,rc.fromcode)) < 0)
  {
    tcrdbclose(g->dbc);
    tcrdbdel(g->dbc);
    tcmapdel(g->cols);
    xfree(g);
    return NULL;
  }

  return g;
}

void referer_del(void *refdata)
{
  global_t *g = (global_t *)refdata;

  if(g->dbc) {
    tcrdbclose(g->dbc);
    tcrdbdel(g->dbc);
  }

  if(g->cols) tcmapdel(g->cols);
  iconv_close(g->cd);
  xfree(g);
}

char *referer_parse(void *refdata,const char *referer,char *wordbuf,int bufsize)
{
  int i,j,ecode;
  char *word;
  struct url *u;
  void *qryp;
  global_t *g = (global_t *)refdata;
  
  if(!refdata || !referer || !referer[0]) 
    return NULL;
  
  u = url_parse(referer,&ecode);
  if(!u) {
    write_log_r(LLERROR,"referer url parse error: %s",referer);
    return NULL;
  }

  for(i=0;i<rc.srcnum;i++)
  {
    for(j=0;j<rc.src[i].dnum;j++)
      if(strendwithignorecase(rc.src[i].domains[j],u->host,strlen(u->host)))
        break;

    if(j!=rc.src[i].dnum)
      break;
  }

  word = NULL;
  if(i != rc.srcnum && rc.src[i].action && u->file && u->query
    && strcasecmp(rc.src[i].action,u->file) == 0)
  {
    const char *wd,*code,*pn,*rn;
    char pns[64],pkbuf[64],buf[1024];
    int wdlen,ipn,irn,keysize,len,res;
    unsigned int pkey;

    qryp = url_qryparse_init();
    url_qryparse(qryp,u->query,1);
    
    wd = url_qryparse_get(qryp,rc.src[i].word);
    if(!wd) {
      write_log_r(LLWARN,"referer url word inexist: %s",referer);
      goto EXIT;
    }
    wdlen = strlen(wd);
    
    if(rc.src[i].code) 
    {
      code = url_qryparse_get(qryp,rc.src[i].code);
      if(!code) 
        code = rc.src[i].defcode;
      else if(strcasecmp(code,"gb2312") == 0 || strcasecmp(code,"gbk") == 0)
        code = "gb18030";
      else if(strcasecmp(code,"utf8") == 0)
        code = "utf-8";
    } 
    else
      code = rc.src[i].defcode;

    if(strcasecmp(code,rc.fromcode) != 0 && strcasecmp(code,rc.tocode) != 0) {
      write_log_r(LLWARN,"referer url code error: %s",referer);
      goto EXIT;
    }

    if(strcasecmp(code,rc.fromcode) == 0)
    {
      size_t inlen, outlen;
      char *inbuf, *outbuf;
  
      inbuf = (char *)wd;
      inlen = wdlen;
      memset(wordbuf,0,bufsize);
      outbuf = wordbuf;
      outlen = bufsize;
    
      if(iconv(g->cd,&inbuf,&inlen,&outbuf,&outlen) != 0) {
        write_log_r(LLERROR,"referer url word code converse error: %s",referer);
        goto EXIT;
      }
      iconv(g->cd,NULL,NULL,NULL,NULL); //set conversion state to the initial state
    } else {
      if(wdlen >= bufsize) {
        write_log_r(LLERROR,"referer url word too long: %s",referer);
        goto EXIT;
      }
      strcpy(wordbuf,wd);
    }
  
    pn = url_qryparse_get(qryp,rc.src[i].pn);
    if(pn) 
      ipn = atoi(pn);
    else   
      ipn = rc.src[i].defpn;

    if(rc.src[i].pnadj && strcasecmp(rc.src[i].pnadj,PAGE_ADJUST_MODE) == 0) {
      if(rc.src[i].rn) {
        rn = url_qryparse_get(qryp,rc.src[i].rn);
        if(rn)  irn = atoi(rn);
        else    irn = rc.src[i].defrn;
      } else
        irn = rc.src[i].defrn;

      if(irn <= 0) irn = rc.src[i].defrn;
      ipn = ipn/irn+1;
    }
    
    url_qryparse_free(qryp);
   
    //store into db
    if(rc.dbhost) 
    {
      snprintf(pns,sizeof(pns),"%d",ipn);
      len = snprintf(buf,sizeof(buf),"%s%s%s",rc.src[i].name,wordbuf,pns);
      if(len <= 0 || len >= sizeof(buf)) {
        write_log_r(LLERROR,"referer url data too long: %s",referer);
        goto EXIT;
      }
      pkey = CRC32n((unsigned char *)buf,len);
      keysize = snprintf(pkbuf,sizeof(pkbuf),"%u",pkey);

      tcmapput2(g->cols,"src",rc.src[i].name);
      tcmapput2(g->cols,"word",wordbuf);
      tcmapput2(g->cols,"page",pns);
      //tcmapput(g->cols,"_num",4,&_num,sizeof(int));
      if(!tcrdbtblputkeep(g->dbc,pkbuf,keysize,g->cols)) {
        if(tcrdbecode(g->dbc) == TTEKEEP) 
          res = tcrdbaddint(g->dbc,pkbuf,keysize,1);
        else {
          // db error.
          write_log_r(LLFATAL,"put referer url word into db error,pkey=%s.",pkbuf);
        }
      } else 
        res = tcrdbaddint(g->dbc,pkbuf,keysize,1);
    
      tcmapclear(g->cols);
    }
    
    word = wordbuf;
  }
  
  url_free(u);
  return word;
  
EXIT:
  url_qryparse_free(qryp);
  url_free(u);
  return NULL;
}

#ifdef TEST_REFERER

int main()
{
  void *refdata;
  char buf[1024],*word;
  char wordbuf[1024];
  
  if(referer_init("../etc/wbe.conf") < 0) {
    printf("referer_init() error.\n");
    return -1;
  }

  if(!(refdata = referer_new())) {
    printf("referer_new() error.\n");
    return -1;
  }

  
  while(fgets(buf,sizeof(buf),stdin)) {
      trim(buf);
      if(!buf[0]) continue;
      if(strcasecmp(buf,"exit") == 0) break;

      word = referer_parse(refdata,buf,wordbuf,1024);
      if(word)
        printf("referer word: %s\n",word);
      else
        printf("referer parse error.\n");
  }
  
  referer_del(refdata);
  referer_free();
  return 0;
}

#endif

