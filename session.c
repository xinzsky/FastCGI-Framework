/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <tcutil.h>
#include "ttepcache.h"
#include "rand.h"
#include "md5.h"
#include "sha1.h"
#include "cgic.h"
#include "xmalloc.h"
#include "conf.h"
#include "utils.h"
#include "xstring.h"
#include "comn.h"

#define SESSION_ID_MINLEN      22
#define SESSION_ID_MAXLEN      40
#define SESSION_VAR_ID        "_sid_"
#define SESSION_VAR_ADDR      "_addr_"    // user address

enum {
	HASH_FUNC_MD5,      // 128bits
	HASH_FUNC_SHA1      // 160bits
};

typedef struct {
  char *name;  // reserved for extend
  char *host;  // default: localhost
  int   port;  // default: 10000
}sess_db_t;

typedef struct {
  int lifetime; // defalut: 0, set zero is session or memory cookie. 
  char *path;	  // default: /
  char *domain;	// default: .gekang.com
  int secure;	  // default: 0
  int httponly; // default: 1
}sess_cookie_t;

typedef struct {
  sess_db_t db;       // expire persistent cache
  char  *sessname;    // GKSESSID
  int   sesssize;     // session object size
	char  *referer;     // external referer check
	int  verifyaddr;    // verify user address
	int  dynamicsid;    // alter session id for each access
	int  keepoldsid;    // old sid isn't valid?
  int  maxlifetime;   // session expire = now + maxlifetime
  int  hashfunc;      // md5(128b) sha1(160b)
  int  hashbits;      // 4,5,6
  char *entropyfile;  // /dev/urandom
  int  entropylength; // 16
  int  sidincookie;   // yes
  int  sidinurl;      // no
  sess_cookie_t cookie;
  int cachelimiter;   
  int cacheexpire;
}sess_global_t;

// used for multi-thread.
static sess_global_t SG;

int session_init(const char *confpath)
{ 
  void *cfgdata;
  char *p;
  char *hashfunc[] = {"md5","sha1"};
  int hashf[] = {HASH_FUNC_MD5,HASH_FUNC_SHA1};
  char *entropyfile[] = {"/dev/urandom", "/dev/random"};
  char *cachelimiter[] = {"none","nocache","private","public"};
  int cachel[] = {CACHE_TYPE_NONE,CACHE_TYPE_NOCACHE,CACHE_TYPE_PRIVATE,CACHE_TYPE_PUBLIC};
  conf_t cfg[] = {
    {"session", "EPCache",  CONF_STR, 1, "localhost:10000", NULL, NULL,   NULL, &SG.db.host},
    {"session", "DBName",   CONF_STR, 1, NULL,              NULL, NULL,   NULL, &SG.db.name},
    {"session", "LifeTime", CONF_INT, 1, "86400",           (void *)0,(void *)0,NULL,&SG.maxlifetime},
    {"session", "Size",     CONF_INT, 1, "16",              NULL, NULL,   NULL, &SG.sesssize},
    {"session", "SessName", CONF_STR, 1, "GKSESSID",        NULL, NULL,   NULL, &SG.sessname},
    {"session", "HashFunc", CONF_STR, 1, "md5",             (void *)hashfunc,(void *)COUNTOF(hashfunc),(void *)hashf,&SG.hashfunc},
    {"session", "HashBits", CONF_INT, 1, "4",               (void *)4,(void *)6,NULL,&SG.hashbits},
    {"session", "EntropyFile",CONF_STR,1,"/dev/urandom",    (void *)entropyfile,(void *)COUNTOF(entropyfile),NULL,&SG.entropyfile},
    {"session", "EntropyLength",CONF_INT,1,"16",            (void *)4,(void *)1024,NULL, &SG.entropylength},
    {"session", "CheckReferer",CONF_STR, 1, NULL,            NULL, NULL,NULL, &SG.referer},
    {"session", "VerifyAddr",  CONF_INT, 1, "0",             (void *)0,(void *)1,NULL,&SG.verifyaddr},
    {"session", "DynamicSID",  CONF_INT, 1, "0",             (void *)0,(void *)1,NULL,&SG.dynamicsid},
    {"session", "KeepOldSID",  CONF_INT, 1, "0",             (void *)0,(void *)1,NULL,&SG.keepoldsid},
    {"session", "SidInCookie", CONF_INT, 1, "1",             (void *)0,(void *)1,NULL,&SG.sidincookie},
    {"session", "SidInURL",    CONF_INT, 1, "0",             (void *)0,(void *)1,NULL,&SG.sidinurl},
    {"session", "CookieLifeTime",CONF_INT,1,"0",             (void *)0,(void *)0,NULL,&SG.cookie.lifetime},
    {"session", "CookiePath",CONF_STR,    1,"/",              NULL,  NULL,  NULL, &SG.cookie.path},
    {"session", "CookieDomain",CONF_STR,  1,".gekang.com",    NULL,NULL,NULL, &SG.cookie.domain},
    {"session", "CookieSecure",CONF_INT,  1,"0",              (void *)0,(void *)1,NULL, &SG.cookie.secure},
    {"session", "CookieHttpOnly",CONF_INT,1,"1",              (void *)0,(void *)1,NULL, &SG.cookie.httponly},
    {"session", "BrowserCacheLimiter",  CONF_STR,1, "none",   (void *)cachelimiter,(void *)COUNTOF(cachelimiter),(void *)cachel,&SG.cachelimiter},
    {"session", "BrowserCacheExpire",   CONF_INT,1, "0",      (void *)0,(void *)0,NULL, &SG.cacheexpire}
  };

  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0)  return -1;
  get_conf(cfgdata,cfg,COUNTOF(cfg));
  free_conf(cfgdata);

  //connect to session cache
  p = strchr(SG.db.host,':');
  if(p) {
    *p++ = 0;
    SG.db.port = atoi(p);
  } else
    SG.db.port = 10000;
  
  return 0;
}

void session_free(void)
{
  xfree(SG.db.host);
  xfree(SG.db.name);
  xfree(SG.sessname);
  xfree(SG.referer);
  xfree(SG.entropyfile);
  xfree(SG.cookie.path);
  xfree(SG.cookie.domain);
}

// each thead call once.
void *session_new(void) 
{
  void *db;

  db = epcache_init(SG.db.host,SG.db.port);
  if(!db) 
    return NULL;

  return db;
}

void session_del(void *db)
{
  epcache_free(db);
}

// create session object
void *session_create(void)
{
  TCMAP *sess;
    
  sess = tcmapnew2(SG.sesssize);
  return sess;
}

// destroy session object
void session_destroy(void *session)
{
  TCMAP *sess = (TCMAP *)session;

  tcmapdel(sess);
}

int session_var_register(void *session,const char *name,const char *value)
{
  TCMAP *sess = (TCMAP *)session;

  if(!name || !name[0] || !value)
    return -1;

  tcmapput2(sess,name,value); // overwrite the same "name" variable
  return 0;
}

// @return: error: -1, inexist: 0, ok: 1
int session_var_unregister(void *session,const char *name)
{
  TCMAP *sess = (TCMAP *)session;

  if(!name || !name[0]) 
    return -1;

  return tcmapout2(sess,name);
}

// @return: error: -1, inexist: 0, exist: 1
int session_var_isregister(void *session,const char *name)
{
  TCMAP *sess = (TCMAP *)session;

  if(!name || !name[0]) 
    return -1;
  
  if(tcmapget2(sess,name))
    return 1;
  else
    return 0;
}

const char *session_var_get(void *session,const char *name)
{
  TCMAP *sess = (TCMAP *)session;

  if(!name || !name[0]) 
    return NULL;

  return tcmapget2(sess,name);
}

// 清空session数据容器中所有数据
void session_var_unset(void *session)
{
  TCMAP *sess = (TCMAP *)session;

  tcmapclear(sess);
}

// traversal session object
void session_var_iterinit(void *session)
{
  TCMAP *sess = (TCMAP *)session;

  tcmapiterinit(sess);
}

// @return next variable name,not value.
// The order of iteration is assured to be the same as the stored order.
const char *session_var_iternext(void *session)
{
   TCMAP *sess = (TCMAP *)session;

   return tcmapiternext2(sess);
}

int session_put(void *db,char *sid,int sidlen,void *session,int lifetime)
{
  TCMAP *sess = (TCMAP *)session;
  void *packet;
  int sp,r;

  packet = tcmapdump(sess,&sp);
  r = epcache_put(db,sid,sidlen,packet,sp,lifetime);
  xfree(packet);
  return r;
}

// @attention: need call "tcmapdel" free return value.
void *session_get(void *db,char *sid,int sidlen)
{
  int sp;
  void *packet;
  TCMAP *sess;
  
  packet = epcache_get(db,sid,sidlen,&sp);  // session inexist or occure error ?
  if(!packet)
    return NULL;
  sess = tcmapload(packet,sp);
  xfree(packet);
  return sess;
}

// remove a session from db.
int session_out(void *db,char *sid,int sidlen)
{
  return epcache_out(db,sid,sidlen);
}

const char *session_name(void)
{
  return SG.sessname;
}

const char *session_id(void *session)
{
  TCMAP *sess = (TCMAP *)session;

  return tcmapget2(sess,SESSION_VAR_ID);
}

int session_create_sid(const char *remote_addr,const char *remote_port,char *sidbuf,int bufsize)
{
  struct timeval tv;
  char buf[2048],digest[20];
  int len,pos,digestlen;

  if(bufsize <= SESSION_ID_MINLEN)
    return -1;
  
	gettimeofday(&tv, NULL);
  /* maximum 15+5+19+19+10 bytes */
	len = snprintf(buf, sizeof(buf), "%.15s%s%ld%ld%0.8F", 
                 remote_addr,remote_port,
                 tv.tv_sec, (long int)tv.tv_usec, 
                 combined_lcg() * 10);
  if(len <= 0 || len >= sizeof(buf))
    return -1;
  pos = len;
  
  if(SG.entropylength > 0 && SG.entropyfile)
  {
    if(SG.entropylength > sizeof(buf) - pos)
     len = entropy_rand(SG.entropyfile,buf+pos,sizeof(buf)-pos);
    else
     len = entropy_rand(SG.entropyfile,buf+pos,SG.entropylength);

    if(len < 0)
      return -1;
    pos += len;
  }

  if(SG.hashfunc == HASH_FUNC_MD5) {
    md5((unsigned char *)buf,pos,(unsigned char *)digest);
    digestlen = 16;
  } else if(SG.hashfunc == HASH_FUNC_SHA1) {
    sha1((unsigned char *)buf,pos,(unsigned char *)digest);
    digestlen = 20;
  } else {
    return -1;
  }

  if(SG.hashbits != 4 && SG.hashbits != 5 && SG.hashbits != 6)
    SG.hashbits = 4;

  binencode(digest,digestlen,sidbuf,SG.hashbits);
  return strlen(sidbuf);
}

// @return a session object, need free.
void *session_start(void *db,CGI_T *cgi)
{
  char sid[1024];
  void *session;
  int sidlen;
  const char *remoteaddr,*remoteport,*useraddr;

  remoteaddr = cgi->cgiRemoteAddr;
  remoteport = cgi->cgiRemotePort;

  // extern referer check
  if(SG.referer) {
    char *ref;
    ref = cgi->cgiReferrer;
    if(ref[0] && !stristr(ref,SG.referer))
      goto CREATE_NEWSESS;
  }

  // check cookie hasn't sid?
  if(cgiCookieString(cgi,SG.sessname,sid,sizeof(sid)) != cgiFormSuccess) {
    sid[0] = 0;
    // check url hasn't sid?
    if(cgiFormString(cgi,SG.sessname,sid,sizeof(sid)) != cgiFormSuccess)
      goto CREATE_NEWSESS;
  }
  
  // check sid 
  sidlen = strlen(sid);
  if(sidlen < SESSION_ID_MINLEN || sidlen > SESSION_ID_MAXLEN)
    goto CREATE_NEWSESS;

  // Get session object from db.
  if((session = session_get(db,sid,sidlen)))
  {
    if(SG.verifyaddr) {
      useraddr = session_var_get(session,SESSION_VAR_ADDR);
      if(remoteaddr[0] && useraddr && strcasecmp(remoteaddr,useraddr) != 0)
        goto CREATE_NEWSESS;
    }
    
    return session;
  }

CREATE_NEWSESS:
  session_create_sid(remoteaddr,remoteport,sid,sizeof(sid));
  session = session_create();
  session_var_register(session,SESSION_VAR_ID,sid);
  if(SG.verifyaddr && remoteaddr[0])
    session_var_register(session,SESSION_VAR_ADDR,remoteaddr);
  return session;
}

// @cookie_lifetime < 0表示使用配置文件默认过期时间。
int session_end(void *db,CGI_T *cgi,void *session,int cookie_lifetime)
{
  char *sid;
  char sidbuf[1024];

  if(SG.dynamicsid) {
      const char *remoteaddr,*remoteport;
      remoteaddr = cgi->cgiRemoteAddr;
      remoteport = cgi->cgiRemotePort;
      session_create_sid(remoteaddr,remoteport,sidbuf,sizeof(sidbuf));
      session_var_register(session,SESSION_VAR_ID,sidbuf);
      sid = sidbuf;
   } else if(!(sid = (char *)session_id(session))) 
      return -1;

  if(session_put(db,sid,strlen(sid),session,SG.maxlifetime) < 0)
    return -1;

  if(SG.sidincookie)
    cgiHeaderCookieSetString(cgi,SG.sessname,(char *)sid,
                             cookie_lifetime<0?SG.cookie.lifetime:cookie_lifetime,
                             SG.cookie.path,SG.cookie.domain,
                             SG.cookie.secure,SG.cookie.httponly);
  
  cgiHeaderCacheControl(cgi,SG.cachelimiter,SG.cacheexpire);

  if(SG.dynamicsid && !SG.keepoldsid)
    session_out(db,sid,strlen(sid));
  
  session_destroy(session);
  return 0;
}

int session_logout(void *db,CGI_T *cgi,int cookie_lifetime)
{
  char sid[1024];
  int sidlen,sidincookie;

  // check cookie hasn't sid?
  sidincookie = 0;
  if(cgiCookieString(cgi,SG.sessname,sid,sizeof(sid)) != cgiFormSuccess) {
    sid[0] = 0;
    
    // check url hasn't sid?
    if(cgiFormString(cgi,SG.sessname,sid,sizeof(sid)) != cgiFormSuccess)
      return 0;
  }
  else
    sidincookie = 1;
  
  // check sid 
  sidlen = strlen(sid);
  if(sidlen < SESSION_ID_MINLEN || sidlen > SESSION_ID_MAXLEN)
    goto REMOVE_USER_COOKIE;
  
  // remove session from db.
  session_out(db,sid,sidlen);

  // remove remote user cookie
  REMOVE_USER_COOKIE:
  if(sidincookie && (cookie_lifetime<0?SG.cookie.lifetime:cookie_lifetime))
  {
    cgiHeaderCookieSetString(cgi,SG.sessname,(char *)sid,-86400,
                           SG.cookie.path,SG.cookie.domain,
                           SG.cookie.secure,SG.cookie.httponly);
  }

  return 0;
}

#ifdef TEST_SESSION
#include "page.h"
#include "dbi.h"
#include "form.h"
#include "action.h"

dbiset_t      dbiset;
stmtset_t     stmtset;
formvarset_t  formvarset;
formset_t     formset;
pg_divset_t   divset;
pg_pageset_t  pageset;
actionset_t   actset;

int main() 
{
  char sidbuf[1024];
  
  if(session_init("../etc/wbe.conf") < 0) {
    printf("session_init() error.\n");
    return -1;
  }

  session_create_sid("localhost","1234",sidbuf,sizeof(sidbuf));
  printf("GKSESSID=%s\n",sidbuf);
  
  session_free();
  return 0;
}

#endif

