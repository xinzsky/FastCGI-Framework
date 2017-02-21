/* Copyright (c) 2011.1 Liu xing zhi. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcutil.h>
#include "comn.h"
#include "conf.h"
#include "url.h"
#include "xmalloc.h"
#include "xstring.h"
#include "utils.h"
#include "page.h"
#include "log.h"
#include "htmtmpl.h"
#include "snapshot.h"
#include "rand.h"
#include "charset.h"
#include "testif.h"

#define MAX_RAW_PAGE   (4*1024*1024)

int page_init(const char *confpath,const char *sectname,pg_divset_t *divset,pg_pageset_t *pageset,const char *home)
{
  pg_div_t div;
  pg_page_t page;
  char *p,*q,section[128];
  char *fn,**fns,*dn,**dns,*dir,*paging,*bt,**bts,*st,**sts,*qe,**qes;
  char *pn,**pns,*bf,**bfs,*af,**afs,*ts,**tss,*mqs,**mqss,*cla,**clas;
  int i,j,k,pnnum,divnum,fnnum,bfnum,afnum,tsnum,btnum,stnum,mqsnum,qenum,clanum;
  void *qp,*cfgdata;
  const char *r;
  
  const char *divtypes[] = {
  "main","rand","hot",
  "latest","relative","headword",
  "tags","other"};

  int divtype[] = {
    DIV_TYPE_MAIN,  DIV_TYPE_RAND,    DIV_TYPE_HOT,
    DIV_TYPE_LATEST,DIV_TYPE_RELATIVE,DIV_TYPE_HEADWORD,
    DIV_TYPE_TAGS,  DIV_TYPE_OTHER
  };

  const char *pagetypes[] = {
    "index",  "browse", "search",
    "content",  "snapshot"
  };

  int pagetype[] = {
    PAGE_TYPE_INDEX,  PAGE_TYPE_BROWSE, PAGE_TYPE_SEARCH,     
    PAGE_TYPE_CONTENT,PAGE_TYPE_SNAPSHOT  
  };

  const char *encodetypes[] = {
    "url","html","html2", "base64"
  };

  int encodetype[] = {
    ENCODE_TYPE_URL,ENCODE_TYPE_HTML, ENCODE_TYPE_HTML2, ENCODE_TYPE_BASE64
  };

  const char *fieldtypes[] = {
    "normal","recid","title","url","raw","frags"
  };

  int fieldtype[] = {
    FIELD_TYPE_NORMAL,FIELD_TYPE_RECID,FIELD_TYPE_TITLE,FIELD_TYPE_URL,FIELD_TYPE_RAW,FIELD_TYPE_MULTIFRAG
  };
 
  conf_t divc[] = {
    {section,  "Type",    CONF_STR,  0,  NULL,    (void *)divtypes, (void *)COUNTOF(divtypes),  (void *)divtype,  &div.type},
    {section,  "Title",   CONF_STR,  1,  NULL,    NULL,   NULL,   NULL, &div.title},
    {section,  "DBC",     CONF_MULTI,0,  NULL,    (void *)&sts,(void *)" ",(void *)&stnum,&st},
    {section,  "RandMod", CONF_INT,  1,  NULL,    NULL,   NULL,   NULL, &div.randmod},
    {section,  "BoolText",CONF_MULTI,1,  NULL,    (void *)&bts,(void *)";",(void *)&btnum,&bt},
    {section,  "QueryEmpty",CONF_MULTI,1,NULL,    (void *)&qes,(void *)";",(void *)&qenum,&qe},
    {section,  "TV_Title",CONF_STR,  1,  NULL,    NULL,   NULL,   NULL, &div.tv_title},
    {section,  "TB_Record",CONF_STR, 0,  NULL,    NULL,   NULL,   NULL, &div.record.tb_record},
    {section,  "TV_Error", CONF_STR, 1,  NULL,    NULL,   NULL,   NULL, &div.tv_error},
    {section,  "TB_Div",   CONF_STR,  0, NULL,    NULL,   NULL,   NULL, &div.tb_div},
    {section,  "Dir",      CONF_STR,  1, NULL,    NULL,   NULL,   NULL, &dir},
    {section,  "Paging",   CONF_STR,  1, NULL,    NULL,   NULL,   NULL, &paging},
    {section,  "Fields",   CONF_MULTI,0, NULL,   (void *)&fns,(void *)" ",(void *)&fnnum,&fn},
    {section,  "TV_HLPos", CONF_STR,  1, NULL,    NULL,   NULL,   NULL, &div.tv_hlpos},
    {section,  "HLWords",  CONF_STR,  1, NULL,    NULL,   NULL,   NULL, &div.hlwords}
  };

  conf_t pagec[] = {
    {section,  "Type",           CONF_STR,  0, NULL,   (void *)pagetypes,(void *)COUNTOF(pagetypes), (void *)pagetype, &page.type},
    {section,  "TV_Before",      CONF_MULTI,1, NULL,   (void *)&bfs,(void *)" ",(void *)&bfnum,&bf},
    {section,  "Divs",           CONF_MULTI,0, NULL,   (void *)&dns,(void *)" ",(void *)&divnum,&dn},
    {section,  "TV_After",       CONF_MULTI,1, NULL,   (void *)&afs,(void *)" ",(void *)&afnum,&af},
    {section,  "TemplateDir",    CONF_STR,  1, NULL,   NULL, NULL, NULL, &page.tptdir},
    {section,  "Templates",      CONF_MULTI,0, NULL,   (void *)&tss,(void *)" ",(void *)&tsnum,&ts},
    {section,  "DefaultTemplate",CONF_STR,  0, NULL,   NULL, NULL, NULL, &page.deftpt},
    {section,  "TemplateVar",    CONF_STR,  0, NULL,   NULL, NULL, NULL, &page.tptvar},
    {section,  "MQS",            CONF_MULTI,1, NULL,   (void *)&mqss,(void *)" ",(void *)&mqsnum,&mqs},
    {section,  "Clauses",        CONF_MULTI,1, NULL,   (void *)&clas,(void *)" ",(void *)&clanum,&cla}
  };
  
  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"Can't open configure file: %s.",confpath);
    return -1;
  }

  dn = getconfmulti(cfgdata,sectname,"Divs",0,NULL," ",&dns,&divnum);
  divset->divnum = divnum;
  divset->divs = (pg_div_t *)xcalloc(divset->divnum,sizeof(pg_div_t));
  
  for(i=0;i<divnum;i++)
  {
    snprintf(section,sizeof(section),"Div:%s",dns[i]);
    memset(&div,0,sizeof(pg_div_t));
    get_conf(cfgdata,divc,COUNTOF(divc));
    
    divset->divs[i].name = xstrdup(dns[i]);
    divset->divs[i].type = div.type;
    divset->divs[i].title = div.title;
    divset->divs[i].tv_title = div.tv_title;
    divset->divs[i].tv_error = div.tv_error;

    divset->divs[i].dbc.stmtnum = stnum;
    divset->divs[i].dbc.stmts = (char **)xcalloc(stnum,sizeof(char *));
    divset->divs[i].dbc.inmqs = (int *)xcalloc(stnum,sizeof(int));
    divset->divs[i].dbc.tvs = (char **)xcalloc(stnum,sizeof(char *));
    for(j=0;j<stnum;j++) {
      divset->divs[i].dbc.stmts[j] = xstrdup(sts[j]);
      p = strchr(divset->divs[i].dbc.stmts[j],':'); // statement:inmqs:tv
      if(p) {
        *p++ = 0;
        q = strchr(p,':');
        if(q) {
          *q++ = 0;
          if(*q) divset->divs[i].dbc.tvs[j] = q;
        }
        if(*p == '0')
          divset->divs[i].dbc.inmqs[j] = 0;
        else
          divset->divs[i].dbc.inmqs[j] = 1;
      }
    }
    xfree(st);
    xfree(sts);

    if(bt) {
      if(btnum != 2) {
        write_log(LLERROR,"[%s:BoolText] set error.",section); 
        return -1;
      }
      divset->divs[i].booltext = (char **)xcalloc(btnum,sizeof(char *));
      for(j=0;j<btnum;j++)
        divset->divs[i].booltext[j] = xstrdup(bts[j]);
      xfree(bt);
      xfree(bts);
    }

    if(qe) {
      if(qenum > 2) {
        write_log(LLERROR,"[%s:QueryEmpty] set error.",section); 
        return -1;
      }
      divset->divs[i].queryempty = (char **)xcalloc(qenum+1,sizeof(char *));
      for(j=0;j<qenum;j++)
        divset->divs[i].queryempty[j] = xstrdup(qes[j]);
      xfree(qe);
      xfree(qes);
    }
    
    divset->divs[i].randmod = div.randmod;
    divset->divs[i].tv_hlpos = div.tv_hlpos;
    divset->divs[i].hlwords = div.hlwords;
    divset->divs[i].tb_div = div.tb_div;
    divset->divs[i].record.fieldnum = fnnum;
    if(fnnum)
      divset->divs[i].record.field = (pg_field_t *)xcalloc(fnnum,sizeof(pg_field_t));
    divset->divs[i].record.tb_record = div.record.tb_record;
    
    if(dir) {
      qp = url_qryparse_init();
      url_qryparse2(qp,dir,1,'=',',');
      if((r=url_qryparse_get(qp,"title"))) divset->divs[i].record.dir.title = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_title"))) divset->divs[i].record.dir.tv_title = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_itemname"))) divset->divs[i].record.dir.tv_itemname = xstrdup(r);
      else {write_log(LLERROR,"[%s:Dir:tv_itemname] isn't set.",section); return -1;}
      if((r=url_qryparse_get(qp,"tv_itemvalue"))) divset->divs[i].record.dir.tv_itemvalue = xstrdup(r);
      else {write_log(LLERROR,"[%s:Dir:tv_itemvalue] isn't set.",section); return -1;}
      if((r=url_qryparse_get(qp,"tb_item"))) divset->divs[i].record.dir.tb_item = xstrdup(r);
      else {write_log(LLERROR,"[%s:Dir:tb_item] isn't set.",section); return -1;}
      if((r=url_qryparse_get(qp,"tb_dir"))) divset->divs[i].record.dir.tb_dir = xstrdup(r);
      else {write_log(LLERROR,"[%s:Dir:tb_dir] isn't set.",section); return -1;}
      url_qryparse_free(qp);
      xfree(dir);
    }

    if(paging) {
      qp = url_qryparse_init();
      url_qryparse2(qp,paging,1,'=',',');
      if((r=url_qryparse_get(qp,"rndef"))) divset->divs[i].paging.rndef = atoi(r);
      else divset->divs[i].paging.rndef = 10;
      if((r=url_qryparse_get(qp,"rnvar"))) divset->divs[i].paging.rnvar= xstrdup(r);
      else divset->divs[i].paging.rnvar = xstrdup("rn");
      if((r=url_qryparse_get(qp,"snvar"))) divset->divs[i].paging.snvar= xstrdup(r);
      else divset->divs[i].paging.snvar = xstrdup("sn");
      if((r=url_qryparse_get(qp,"maxretnum"))) divset->divs[i].paging.maxretnum = atoi(r);
      else divset->divs[i].paging.maxretnum = 1000;
      if((r=url_qryparse_get(qp,"urltype"))) divset->divs[i].paging.urltype = atoi(r);
      else divset->divs[i].paging.urltype = 1;
      if((r=url_qryparse_get(qp,"urlprefix"))) divset->divs[i].paging.urlprefix = xstrdup(r);
      else {write_log(LLERROR,"[%s:Paging:urlprefix] isn't set.",section); return -1;}
      if((r=url_qryparse_get(qp,"urlvars"))) 
      {
        char *uv,**uvs;
        int uvnum,uvlen;
        uvlen = strlen(r);
        uv = xmalloc(uvlen + 1);
        //trimid(r, uv, 1);
        strcpy(uv,r);
        strsqueeze(uv);
        uvnum = strfragnum(uv," ");
        uvs = (char **)xcalloc(uvnum, sizeof(char *));
        split(uv," ",uvs, uvnum);
        divset->divs[i].paging.urlvarnum = uvnum;
        divset->divs[i].paging.urlvars = (char **)xcalloc(uvnum,sizeof(char *));
        for(j=0;j<uvnum;j++)
          divset->divs[i].paging.urlvars[j] = xstrdup(uvs[j]);
        xfree(uvs);
        xfree(uv);
      }
      
      if((r=url_qryparse_get(qp,"linknum"))) divset->divs[i].paging.linknum =atoi(r);
      else divset->divs[i].paging.linknum = 10;
      
      if((r=url_qryparse_get(qp,"prevnext"))) {
        divset->divs[i].paging.prevnext[0] = xstrdup(r);
        p = strchr(divset->divs[i].paging.prevnext[0],':');
        if(!p) {
          write_log(LLERROR,"[%s:Paging:prevnext] set error.",section);
          return -1;
        } else {
          *p++ = 0;
          divset->divs[i].paging.prevnext[1] = p;
        }
      } else {
        write_log(LLERROR,"[%s:Paging:prevnext] isn't set.",section);
        return -1;
      }
      
      if((r=url_qryparse_get(qp,"firstlast"))) {
        divset->divs[i].paging.firstlast[0] = xstrdup(r);
        p = strchr(divset->divs[i].paging.firstlast[0],':');
        if(!p) {
          write_log(LLERROR,"[%s:Paging:firstlast] set error.",section);
          return -1;
        } else {
          *p++ = 0;
          divset->divs[i].paging.firstlast[1] = p;
        }
      } 
      
      if((r=url_qryparse_get(qp,"curattr"))) divset->divs[i].paging.curattr = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_paging"))) divset->divs[i].paging.tv_paging = xstrdup(r);
      else {write_log(LLERROR,"[%s:Paging:tv_paging] isn't set.",section); return -1;}
      if((r=url_qryparse_get(qp,"tb_paging"))) divset->divs[i].paging.tb_paging = xstrdup(r);
      else {write_log(LLERROR,"[%s:Paging:tb_paging] isn't set.",section); return -1;}
      url_qryparse_free(qp);
      xfree(paging);
    }

    for(j=0;j<fnnum;j++)
    {
      char *fvalue;
      divset->divs[i].record.field[j].name = xstrdup(fns[j]);
      fvalue = getconfstring(cfgdata,section,fns[j],0,NULL,NULL,NULL,0,NULL);
      qp = url_qryparse_init();
      url_qryparse2(qp,fvalue,1,'=',',');
      if(!(r=url_qryparse_get(qp,"type")))
        r = "normal";
      for(k=0;k<COUNTOF(fieldtypes);k++) {
        if(strcasecmp(r,fieldtypes[k]) == 0) {
          divset->divs[i].record.field[j].type = fieldtype[k];
          break;
        }
      }
      if(k == COUNTOF(fieldtypes)) {
        write_log(LLERROR,"[%s:%s:%s] set error.",section,fns[j],"type");
        return -1;
      }
      if((r=url_qryparse_get(qp,"if")))    divset->divs[i].record.field[j].ifout = xstrdup(r);
      if((r=url_qryparse_get(qp,"title"))) divset->divs[i].record.field[j].title = xstrdup(r);
      if((r=url_qryparse_get(qp,"isnav"))) divset->divs[i].record.field[j].isnav = atoi(r);
      if((r=url_qryparse_get(qp,"isnull"))) divset->divs[i].record.field[j].isnull = atoi(r);
      else divset->divs[i].record.field[j].isnull = 1;
      if((r=url_qryparse_get(qp,"defval"))) divset->divs[i].record.field[j].defval = xstrdup(r);
      if((r=url_qryparse_get(qp,"maxlen"))) divset->divs[i].record.field[j].maxlen = atoi(r);
      if((r=url_qryparse_get(qp,"encode"))) {
        for(k=0;k<COUNTOF(encodetypes);k++) {
          if(strcasecmp(r,encodetypes[k]) == 0) {
            divset->divs[i].record.field[j].encode  = encodetype[k];
            break;
          }
        }

        if(k == COUNTOF(encodetypes)) {
          write_log(LLERROR,"[%s:%s:%s] set error.",section,fns[j],"encode");
          return -1;
        }
      }
      if((r=url_qryparse_get(qp,"tv_name"))) divset->divs[i].record.field[j].tv_name = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_value"))) divset->divs[i].record.field[j].tv_value = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_title"))) divset->divs[i].record.field[j].tv_title = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_cut"))) divset->divs[i].record.field[j].tv_cut = xstrdup(r);
      if((r=url_qryparse_get(qp,"tv_encode"))) divset->divs[i].record.field[j].tv_encode = xstrdup(r);
      if((r=url_qryparse_get(qp,"tb_field"))) divset->divs[i].record.field[j].tb_field = xstrdup(r);
      if(  !divset->divs[i].record.field[j].tv_value 
        && !divset->divs[i].record.field[j].tv_cut 
        && !divset->divs[i].record.field[j].tv_encode) {
        write_log(LLERROR,"[%s:%s:tv_value/tv_cut/tv_encode] isn't set.",section,fns[j]); 
        return -1;
      }

      if(divset->divs[i].record.field[j].type == FIELD_TYPE_MULTIFRAG) {
        if((r=url_qryparse_get(qp,"delimiter"))) divset->divs[i].record.field[j].delimiter= xstrdup(r);
        else {write_log(LLERROR,"[%s:%s:delimiter] isn't set.",section,fns[j]); return -1;}
        if((r=url_qryparse_get(qp,"separator"))) divset->divs[i].record.field[j].separator = xstrdup(r);
        else {write_log(LLERROR,"[%s:%s:separator] isn't set.",section,fns[j]); return -1;}
        if((r=url_qryparse_get(qp,"fragnum"))) divset->divs[i].record.field[j].fragnum = atoi(r);
        if((r=url_qryparse_get(qp,"numinline"))) divset->divs[i].record.field[j].numinline = atoi(r);
        if((r=url_qryparse_get(qp,"linestart"))) divset->divs[i].record.field[j].linestart = xstrdup(r);
        if((r=url_qryparse_get(qp,"lineclose"))) divset->divs[i].record.field[j].lineclose = xstrdup(r);
        
        if((r=url_qryparse_get(qp,"tv_sep"))) divset->divs[i].record.field[j].tv_sep = xstrdup(r);
        if((r=url_qryparse_get(qp,"tv_linestart"))) divset->divs[i].record.field[j].tv_linestart = xstrdup(r);
        if((r=url_qryparse_get(qp,"tv_lineclose"))) divset->divs[i].record.field[j].tv_lineclose = xstrdup(r);
        if((r=url_qryparse_get(qp,"tb_frag"))) divset->divs[i].record.field[j].tb_frag = xstrdup(r);
        else {write_log(LLERROR,"[%s:%s:tb_frag] isn't set.",section,fns[j]); return -1;}
        if((r=url_qryparse_get(qp,"tb_value"))) divset->divs[i].record.field[j].tb_value = xstrdup(r);
        else {write_log(LLERROR,"[%s:%s:tb_value] isn't set.",section,fns[j]); return -1;}
      }
      
      url_qryparse_free(qp);
      xfree(fvalue);
    }

    xfree(fns);
    xfree(fn);
  }

  xfree(dns);
  xfree(dn);
  
  // pages configuration
  pn = getconfmulti(cfgdata,sectname,"Pages",0,NULL," ",&pns,&pnnum);
  pageset->pagenum = pnnum;
  pageset->pages = (pg_page_t *)xcalloc(pageset->pagenum,sizeof(pg_page_t));

  for(i=0;i<pnnum;i++)
  {
    snprintf(section,sizeof(section),"Page:%s",pns[i]);
    memset(&page,0,sizeof(pg_page_t));
    get_conf(cfgdata,pagec,COUNTOF(pagec));
    
    pageset->pages[i].name = xstrdup(pns[i]);
    pageset->pages[i].type = page.type;

    if(bf) {
      pageset->pages[i].tvbfnum = bfnum;
      pageset->pages[i].tv_before = (char **)xcalloc(bfnum,sizeof(char *));
      for(j=0;j<bfnum;j++) 
        pageset->pages[i].tv_before[j] = xstrdup(bfs[j]);
      xfree(bf);
      xfree(bfs);
    }
    
    pageset->pages[i].divnum = divnum;
    pageset->pages[i].divnames = (char **)xcalloc(divnum,sizeof(char *));
    for(j=0;j<divnum;j++)
      pageset->pages[i].divnames[j] = xstrdup(dns[j]);
    xfree(dn);
    xfree(dns);

    if(af) {
      pageset->pages[i].tvafnum = afnum;
      pageset->pages[i].tv_after = (char **)xcalloc(afnum,sizeof(char *));
      for(j=0;j<afnum;j++)
        pageset->pages[i].tv_after[j] = xstrdup(afs[j]);
      xfree(af);
      xfree(afs);
    }

    pageset->pages[i].tptdir = page.tptdir;
    pageset->pages[i].tptnum = tsnum;
    pageset->pages[i].tpts = (pg_tpt_t *)xcalloc(tsnum,sizeof(pg_tpt_t));
    pageset->pages[i].deftpt = page.deftpt;
    pageset->pages[i].tptvar = page.tptvar;
    for(j=0;j<tsnum;j++)
      pageset->pages[i].tpts[j].name = xstrdup(tss[j]);
    xfree(ts);
    xfree(tss);

    if(mqs) {
      pageset->pages[i].mqsnum = mqsnum;
      pageset->pages[i].mqs = (mqs_t *)xcalloc(mqsnum,sizeof(mqs_t));
      for(j=0;j<mqsnum;j++) 
        pageset->pages[i].mqs[j].stmtnum = strgetfrags(mqss[j],":",&pageset->pages[i].mqs[j].stmts);

      xfree(mqs);
      xfree(mqss);
    }

    if(cla) {
      pageset->pages[i].clausenum = clanum;
      pageset->pages[i].clauses = (char **)xcalloc(clanum,sizeof(char *));
      for(j=0;j<clanum;j++)
        pageset->pages[i].clauses[j] = xstrdup(clas[j]);
      
      xfree(cla);
      xfree(clas);
    }
  }

  xfree(pn);
  xfree(pns);
  free_conf(cfgdata);

  // Initialize ...
  for(i=0;i<pageset->pagenum;i++) {
    pageset->pages[i].divs = (pg_div_t **)xcalloc(pageset->pages[i].divnum,sizeof(pg_div_t *));
    for(j=0;j<pageset->pages[i].divnum;j++) {
      for(k=0;k<divset->divnum;k++) {
        if(strcasecmp(pageset->pages[i].divnames[j],divset->divs[k].name) == 0) {
          pageset->pages[i].divs[j] = divset->divs+k;
          break;
        }
      }

      if(k == divset->divnum) {
        write_log(LLERROR,"[%s:%s] div is inexist.",pageset->pages[i].name,pageset->pages[i].divnames[j]);
        return -1;
      }
    }
  }

  // check default template
  for(i=0;i<pageset->pagenum;i++) {
    for(j=0;j<pageset->pages[i].tptnum;j++) {
      if(strcasecmp(pageset->pages[i].deftpt,pageset->pages[i].tpts[j].name) == 0)
        break;
    }

    if(j == pageset->pages[i].tptnum) {
      write_log(LLERROR,"Default template [%s:%s] inexist.",pageset->pages[i].name,pageset->pages[i].deftpt);
      return -1;
    }
  }

  // Load page template ...
  for(i=0;i<pageset->pagenum;i++) {
    for(j=0;j<pageset->pages[i].tptnum;j++) {
      char path[1024];
      if(pageset->pages[i].tptdir)
        snprintf(path,sizeof(path),"%s/%s",pageset->pages[i].tptdir,pageset->pages[i].tpts[j].name);
      else
        snprintf(path,sizeof(path),"%s/etc/template/%s",home,pageset->pages[i].tpts[j].name);
      pageset->pages[i].tpts[j].content = loadfile(path,(long *)&pageset->pages[i].tpts[j].size);
      if(!pageset->pages[i].tpts[j].content) {
         write_log(LLERROR,"Load page template file: %s failure.",path);
         return -1;
      }
    }
  }

  for(i=0;i<pageset->pagenum;i++) {
    int l,m;
    for(j=0;j<pageset->pages[i].mqsnum;j++) {
      pageset->pages[i].mqs[j].divs = (char **)xcalloc(pageset->pages[i].mqs[j].stmtnum,sizeof(char *));
      for(k=0;k<pageset->pages[i].mqs[j].stmtnum;k++) {
        for(l=0;l<pageset->pages[i].divnum;l++) {
          for(m=0;m<pageset->pages[i].divs[l]->dbc.stmtnum;m++) {
            if(!pageset->pages[i].divs[l]->dbc.inmqs[m]) continue;
            if(strcasecmp(pageset->pages[i].mqs[j].stmts[k],pageset->pages[i].divs[l]->dbc.stmts[m]) == 0) {
              pageset->pages[i].mqs[j].divs[k] = pageset->pages[i].divs[l]->name;
              goto nextmqs_stmt;
            }
          }
        }
nextmqs_stmt:
        continue;
      }
    }
  }

  return 0;
}

void page_free(pg_divset_t *divset,pg_pageset_t *pageset)
{
  int i,j,k;

  for(i=0;i<divset->divnum;i++)
  {
    xfree(divset->divs[i].name);
    xfree(divset->divs[i].title);
    xfree(divset->divs[i].tv_title);
    for(j=0;j<divset->divs[i].record.fieldnum;j++)
    {
      xfree(divset->divs[i].record.field[j].name);
      xfree(divset->divs[i].record.field[j].ifout);
      xfree(divset->divs[i].record.field[j].title);
      xfree(divset->divs[i].record.field[j].defval);
      xfree(divset->divs[i].record.field[j].tv_name);
      xfree(divset->divs[i].record.field[j].tv_value);
      xfree(divset->divs[i].record.field[j].tv_title);
      xfree(divset->divs[i].record.field[j].tv_cut);
      xfree(divset->divs[i].record.field[j].tv_encode);
      xfree(divset->divs[i].record.field[j].tb_field);
      xfree(divset->divs[i].record.field[j].delimiter);
      xfree(divset->divs[i].record.field[j].separator);
      xfree(divset->divs[i].record.field[j].linestart);
      xfree(divset->divs[i].record.field[j].lineclose);
      xfree(divset->divs[i].record.field[j].tv_sep);
      xfree(divset->divs[i].record.field[j].tv_linestart);
      xfree(divset->divs[i].record.field[j].tv_lineclose);
      xfree(divset->divs[i].record.field[j].tb_frag);
      xfree(divset->divs[i].record.field[j].tb_value);
    }
    xfree(divset->divs[i].record.field);
    xfree(divset->divs[i].record.dir.title);
    xfree(divset->divs[i].record.dir.tv_title);
    xfree(divset->divs[i].record.dir.tv_itemname);
    xfree(divset->divs[i].record.dir.tv_itemvalue);
    xfree(divset->divs[i].record.dir.tb_item);
    xfree(divset->divs[i].record.dir.tb_dir);
    xfree(divset->divs[i].record.tb_record);
    xfree(divset->divs[i].paging.rnvar);
    xfree(divset->divs[i].paging.snvar);
    xfree(divset->divs[i].paging.urlprefix);
    for(j=0;j<divset->divs[i].paging.urlvarnum;j++)
      xfree(divset->divs[i].paging.urlvars[j]);
    xfree(divset->divs[i].paging.urlvars);
    xfree(divset->divs[i].paging.prevnext[0]);
    xfree(divset->divs[i].paging.firstlast[0]);
    xfree(divset->divs[i].paging.curattr);
    xfree(divset->divs[i].paging.tv_paging);
    xfree(divset->divs[i].paging.tb_paging);
    xfree(divset->divs[i].tv_error);
    for(j=0;j<divset->divs[i].dbc.stmtnum;j++)
      xfree(divset->divs[i].dbc.stmts[j]);
    xfree(divset->divs[i].dbc.stmts);
    xfree(divset->divs[i].dbc.inmqs);
    xfree(divset->divs[i].dbc.tvs);
    if(divset->divs[i].booltext) {
      for(j=0;j<2;j++) xfree(divset->divs[i].booltext[j]);
      xfree(divset->divs[i].booltext);
    }
    if(divset->divs[i].queryempty) {
      for(j=0;j<2;j++) xfree(divset->divs[i].queryempty[j]);
      xfree(divset->divs[i].queryempty);
    }
    xfree(divset->divs[i].tb_div);
    xfree(divset->divs[i].tv_hlpos);
    xfree(divset->divs[i].hlwords);
  }
  xfree(divset->divs);

  for(i=0;i<pageset->pagenum;i++)
  {
    xfree(pageset->pages[i].name);
    
    for(j=0;j<pageset->pages[i].tvbfnum;j++)
      xfree(pageset->pages[i].tv_before[j]);
    xfree(pageset->pages[i].tv_before);
    
    for(j=0;j<pageset->pages[i].divnum;j++)
      xfree(pageset->pages[i].divnames[j]);
    xfree(pageset->pages[i].divnames);
    xfree(pageset->pages[i].divs);
    
    for(j=0;j<pageset->pages[i].tvafnum;j++)
      xfree(pageset->pages[i].tv_after[j]);
    xfree(pageset->pages[i].tv_after);

    for(j=0;j<pageset->pages[i].tptnum;j++)
    {
      xfree(pageset->pages[i].tpts[j].name);
      xfree(pageset->pages[i].tpts[j].content);
    }
    xfree(pageset->pages[i].tpts);
    xfree(pageset->pages[i].deftpt);
    xfree(pageset->pages[i].tptvar);
    xfree(pageset->pages[i].tptdir);

    for(j=0;j<pageset->pages[i].mqsnum;j++) {
      for(k=0;k<pageset->pages[i].mqs[j].stmtnum;k++)
        xfree(pageset->pages[i].mqs[j].stmts[k]);
      xfree(pageset->pages[i].mqs[j].stmts);
      xfree(pageset->pages[i].mqs[j].divs);
    }
    xfree(pageset->pages[i].mqs);

    for(j=0;j<pageset->pages[i].clausenum;j++)
      xfree(pageset->pages[i].clauses[j]);
    xfree(pageset->pages[i].clauses);
  }
  xfree(pageset->pages);
}


#define PAGE_RESULT_NUM 10   // 页面默认结果数
#define PAGE_LINK_NUM   10
#define PAGE_CUR_ATTR   "class=\"on\""

/**  输出分页列表.
 **  参数说明:
 **  @restotal:  结果总数,最大返回结果数
 **  @start:     从哪一个结果开始,即skip
 **  @resnum:    每页显示结果数，即max
 **  @linknum:   分页链接数
 **  @prevnext:  "上一页" "下一页"
 **  @firstlast: 指明是否需要显示"第一页" "最后一页"
 **  @pageurl:   构造分页URL参数
 **  @snargs:    用于翻页的参数，必须有"%d"
 **  @curattr:   当前页显示属性，eg. <a href="/a" class="on">1</a> curattr=class="on"
*/

int page_makepaging(int restotal,int start,int resnum,
               int linknum,char **prevnext,char **firstlast,
               const char *pageurl,const char *snargs,const char *curattr,
               char *outbuf,int bufsize)
{
  int firstpage;   //翻页条的第一页
  int lastpage;    //翻页条的最后一页
  int curpage;     //翻页条的当前页
  int totalpage;   //返回结果页数
  int i,pos,len,sn;//sn: start number
  char url[1024];

  if(restotal <= 0 || !pageurl || !pageurl[0] || !outbuf || bufsize <= 0)
    return -1;

  if (start < 0 || start >= restotal) start = 0;
  if (resnum <= 0) resnum = PAGE_RESULT_NUM;
  if (linknum <= 0) linknum = PAGE_LINK_NUM;
  if (!curattr || !curattr[0]) curattr = PAGE_CUR_ATTR;

  totalpage = (restotal % resnum == 0) ? restotal/resnum : restotal/resnum + 1;
  curpage = (start == 0) ? 1 : (start - 1) / resnum + 2;  //like google
  firstpage = MAX(curpage - linknum , 1);
  lastpage = MIN(curpage + linknum -1, totalpage);

  if (lastpage - firstpage < linknum) lastpage = MIN(firstpage + linknum - 1, totalpage);
  if (lastpage - firstpage < linknum) firstpage = MAX(lastpage - linknum + 1, 1);

  if (totalpage == 1)   //如果只有一页，则不进行分页
  {
    outbuf[0] = 0;
    return 0;
  }

  pos = 0;
  if (curpage > 1) //此时有"上一页"和"第一页"链接
  {
    if (firstlast[0]) //显示"第一页"链接
    {
      snprintf(url,sizeof(url),snargs,0); 
      len = snprintf(outbuf + pos , bufsize - pos , "<a href=\"%s%s\">%s</a> ",pageurl,url,firstlast[0]);
      if(len <= 0 || len >= bufsize - pos)
        return -1;
      pos += len;
    }

    snprintf(url,sizeof(url),snargs,(start - resnum) >= 0 ? (start - resnum) : 0);
    len = snprintf(outbuf + pos , bufsize - pos , "<a href=\"%s%s\">%s</a> ",pageurl,url,prevnext[0]);
    if(len <= 0 || len >= bufsize - pos)
      return -1;
    pos += len;
  }

  for (i = firstpage; i <= lastpage; ++i)
  {
    if (i == curpage) //当前页
    {
      snprintf(url,sizeof(url),snargs,start);
      len = snprintf(outbuf + pos , bufsize - pos , "<a href=\"%s%s\" %s>%d</a> ",pageurl,url,curattr,i);
      if(len <= 0 || len >= bufsize - pos)
        return -1;
      pos += len;
    }
    else
    {
      sn = MAX((i - curpage) * resnum + start, 0);
      snprintf(url,sizeof(url),snargs,sn);
      len = snprintf(outbuf + pos , bufsize - pos ,"<a href=\"%s%s\">%d</a> ",pageurl,url, i);
      if(len <= 0 || len >= bufsize - pos)
        return -1;
      pos += len;
    }
  }

  if (curpage < totalpage) //此时有"下一页"和"最后一页"链接
  {
    snprintf(url,sizeof(url),snargs,start + resnum);
    len = snprintf(outbuf + pos , bufsize - pos , "<a href=\"%s%s\">%s</a> ",pageurl,url,prevnext[1]);
    if(len <= 0 || len >= bufsize - pos)
      return -1;
    pos += len;

    if (firstlast[1]) // 显示"最后一页"链接
    {
      snprintf(url,sizeof(url),snargs,restotal- ((restotal % resnum) ? (restotal % resnum) : resnum));
      len = snprintf(outbuf + pos , bufsize - pos , "<a href=\"%s%s\">%s</a> ",pageurl,url,firstlast[1]);
      if(len <= 0 || len >= bufsize - pos)
        return -1;
      pos += len;
    }
  }

  return pos;
}

int page_makefragment(void *htmtmpl,pg_field_t *field,const char *fieldvalue)
{
  int i,fragnum,fraglen,n;
  char *encode,*frag,**fragpp;

  frag = xstrdup(fieldvalue);
  fragnum = strfragnum(frag,field->delimiter);
  fragpp = (char **)xcalloc(fragnum, sizeof(char *));
  split(frag,field->delimiter, fragpp, fragnum);
  if(field->fragnum && fragnum > field->fragnum)
    fragnum = field->fragnum;

  n = 0;
  for (i=0;i<fragnum;i++) {
    fraglen = strlen(fragpp[i]);
    if(!fraglen) continue;
    n++;

    // 块中变量一旦赋值，在没有重新赋值之前是不会改变的。
    if(field->linestart && field->tv_linestart && field->numinline) {
      if(n % field->numinline == 1)
        HTSetVar(htmtmpl,field->tv_linestart,field->linestart);
      else
        HTSetVar(htmtmpl,field->tv_linestart,"");
    }

    HTSetVar(htmtmpl, field->tv_value, fragpp[i]);
    
    if(field->tv_encode) {
      switch (field->encode) {
        case ENCODE_TYPE_URL:
          encode = (char *)xcalloc(fraglen*3+1,sizeof(char));
          encode = urlencode(fragpp[i],encode);
          break;
        case ENCODE_TYPE_HTML:
          encode = (char *)xcalloc(fraglen*5+1,sizeof(char));
          htmlencode(fragpp[i],encode);
          break;
        case ENCODE_TYPE_BASE64:
          encode = (char *)xcalloc(fraglen*2,sizeof(char));
          base64encode4url(fragpp[i],fraglen,encode);
          break;
        default:
          write_log_r(LLERROR,"Field#%s encode error.",field->name);
          goto error;
      }

      HTSetVar(htmtmpl,field->tv_encode,encode);
      xfree(encode);
    }
  
    //判断是否需要设置片段分隔符: 最后一个片段以及每行末尾不需要设置分隔符
    if(field->tv_sep && field->separator) { 
      if(i+1 != fragnum && (!field->numinline || n % field->numinline != 0))
        HTSetVar(htmtmpl,field->tv_sep,field->separator);
      else
        HTSetVar(htmtmpl,field->tv_sep,"");
    }
      
    if(field->lineclose && field->tv_lineclose && field->numinline) {
      if(n % field->numinline == 0)
        HTSetVar(htmtmpl,field->tv_lineclose,field->lineclose);
      else
        HTSetVar(htmtmpl,field->tv_lineclose,"");
    }

    HTParse(htmtmpl,field->tb_frag,0);
  }

  HTParse(htmtmpl,field->tb_value,0);
  
  xfree(fragpp);
  xfree(frag);
  return 0;

error:
  xfree(fragpp);
  xfree(frag);
  return -1;
}

int page_makefield(void *htmtmpl,pg_field_t *field,const char *fieldvalue)
{
  int fieldlen,emptyfield,len;
  char *cut,*encode;
  const char *p;

  if(!fieldvalue || !fieldvalue[0])
    emptyfield = 1;
  else
    emptyfield = 0;

  cut = NULL;
  if(!emptyfield) {
    fieldlen = strlen(fieldvalue);
    if(field->maxlen && fieldlen > field->maxlen) {
      cut = xstrdup(fieldvalue);
      strcutforutf8(cut,field->maxlen-4);
      fieldlen = strlen(cut);
      memcpy(cut+fieldlen,"...\0",4);
    }
  } else if(!field->isnull) {
    write_log_r(LLERROR,"Field#%s can't empty,but field value is empty.",field->name);
    return -1;
  } else if(field->defval) {
      fieldvalue = field->defval;
  } else { // 字段值为空
      // 由于块中变量一旦赋值，在没有重新赋值之前是不会改变的。
      // 清空该字段对应模板变量取值，避免为前一个记录的相应字段值
      if(field->tv_title)  HTSetVar(htmtmpl,field->tv_title,"");
      if(field->tv_name)   HTSetVar(htmtmpl,field->tv_name,"");
      if(field->tv_value)  HTSetVar(htmtmpl,field->tv_value,"");
      if(field->tv_encode) HTSetVar(htmtmpl,field->tv_encode,"");
      if(field->tv_cut)    HTSetVar(htmtmpl,field->tv_cut,""); 
      return 0;
  }

  if (field->type == FIELD_TYPE_MULTIFRAG) {
    page_makefragment(htmtmpl,field,fieldvalue);
  } else {
    fieldlen = strlen(fieldvalue);
    if(field->tv_encode) {
      p = fieldvalue;
      if(cut) p = cut;
      len = strlen(p);
      switch (field->encode)
      {
        case ENCODE_TYPE_URL:
          encode = (char *)xcalloc(len*3+1,sizeof(char));
          encode = urlencode((char *)p,encode);
          break;
        case ENCODE_TYPE_HTML:
          encode = (char *)xcalloc(len*5+1,sizeof(char));
          htmlencode(p,encode);
          break;
        case ENCODE_TYPE_HTML2:
          encode = (char *)xcalloc(len*5+1,sizeof(char));
          htmlencode2(p,encode,"em");
          break;
        case ENCODE_TYPE_BASE64:
          encode = (char *)xcalloc(len*2,sizeof(char));
          base64encode4url(p,len,encode);
          break;
        default:
          write_log_r(LLERROR,"Field#%s encode error.",field->name);
          return -1;
      }

      HTSetVar(htmtmpl,field->tv_encode,encode);
      xfree(encode);
    }

    if(field->tv_value && fieldvalue)
      HTSetVar(htmtmpl,field->tv_value,(char *)fieldvalue);
  }

  if(field->isnav && field->tv_name)
    HTSetVar(htmtmpl,field->tv_name,field->name);

  if(field->title && field->tv_title)
    HTSetVar(htmtmpl,field->tv_title,field->title);

  if(field->tv_cut) {
    if(cut) { 
      HTSetVar(htmtmpl,field->tv_cut,cut); 
      xfree(cut);
    } else {
      HTSetVar(htmtmpl,field->tv_cut,(char *)fieldvalue);
    }
  }

  if(field->tb_field)
    HTParse(htmtmpl,field->tb_field,0);
  
  return 0;
}

int page_makerecord(void *htmtmpl,pg_record_t *record,char **fieldvalues)
{
  int i,j,testres;
  TCMAP *recmap = NULL;
  char nullval[1];

  //检查记录的所有字段是否都为空，如果都为空则不生成该记录。@20131022
  for(i=0;i<record->fieldnum;i++) {
    if(fieldvalues[i] && *fieldvalues[i])
      break;
  }
  
  if(i == record->fieldnum) //记录的所有字段都为空。@20131022
    return 0;

  // record fields
  nullval[0] = 0;
  for(i=0;i<record->fieldnum;i++)
  {
    if(record->field[i].ifout) {
      if(!recmap) {
        recmap = tcmapnew();
        for(j=0;j<record->fieldnum;j++) {
          if(fieldvalues[j])
            tcmapput2(recmap,record->field[j].name,fieldvalues[j]);
          else
            tcmapput2(recmap,record->field[j].name,nullval);
        }
      }
      testres = testif(recmap,fieldvalues[i],record->field[i].ifout);
      if(testres < 0) {
        if(recmap) tcmapdel(recmap);
        return -1;
      } else if(testres == 0)
        continue;
    }
    
    if(page_makefield(htmtmpl,record->field+i,fieldvalues[i]) < 0) {
      if(recmap) tcmapdel(recmap);
      return -1;
    }
  }

  if(recmap)
    tcmapdel(recmap);

  // record directory navigator
  if(record->dir.tb_dir) 
  {
    if(record->dir.title && record->dir.tv_title)
      HTSetVar(htmtmpl,record->dir.tv_title,record->dir.title);

    for(i=0;i<record->fieldnum;i++) 
    {
      if((fieldvalues[i] || record->field[i].defval) && record->field[i].isnav) 
      {
        HTSetVar(htmtmpl,record->dir.tv_itemname,record->field[i].name);
        HTSetVar(htmtmpl,record->dir.tv_itemvalue,record->field[i].title);
        HTParse(htmtmpl,record->dir.tb_item,0);
      }
    }
    
    HTParse(htmtmpl,record->dir.tb_dir,0);
  }

  HTParse(htmtmpl,record->tb_record,0);
  
  return 0;
}

int page_makepagingbar(void *htmtmpl,pg_div_t *div,TCMAP *sysvars,int stmtindex)
{
  int i,rn,sn,rt,pos,len;
  char name[1024],pageurl[1024],snargs[1024],paging[32*1024];
  char *vn,vo;
  const char *cp;
  
  cp = tcmapget2(sysvars,div->paging.rnvar);
  if(!cp)
    rn = div->paging.rndef;
  else
    rn = atoi(cp);

  cp = tcmapget2(sysvars,div->paging.snvar);
  if(!cp)
    sn = 0;
  else
    sn = atoi(cp);

  snprintf(name,sizeof(name),"%s#%s#maxretnum",div->name,div->dbc.stmts[stmtindex]);
  cp = tcmapget2(sysvars,name);
  if(!cp) {
    write_log_r(LLERROR,"%s#%s maxretnum is empty.",div->name,div->dbc.stmts[stmtindex]);
    return -1;
  } else
    rt = atoi(cp);

  if(rt > div->paging.maxretnum)
    rt = div->paging.maxretnum;

  strcpy(pageurl,div->paging.urlprefix);
  pos = strlen(pageurl);
  if(div->paging.urltype == URL_TYPE_STATIC) 
    pageurl[pos] = '/';
  else 
    pageurl[pos] = '?';

  pos++;

  for(i=0;i<div->paging.urlvarnum;i++) {
    cp = tcmapget2(sysvars,div->paging.urlvars[i]);
    vn = strchr(div->paging.urlvars[i],'#'); // 检查变量名中是否有'#',有则去掉'#'后面的内容
    if(vn) { vo = *vn; *vn = 0; }
    if(cp && div->paging.urltype == URL_TYPE_STATIC) {
      len = snprintf(pageurl+pos,sizeof(pageurl)-pos,"%s%c%s%c",div->paging.urlvars[i],STATIC_URL_DELIMITER,cp,STATIC_URL_SEPARATOR);
      pos += len;
    } else if(cp && div->paging.urltype == URL_TYPE_DYNAMIC) {
      len = snprintf(pageurl+pos,sizeof(pageurl)-pos,"%s%c%s%c",div->paging.urlvars[i],DYNAMIC_URL_DELIMITER,cp,DYNAMIC_URL_SEPARATOR);
      pos += len;
    }

    if(vn) *vn = vo;
  }

  pageurl[pos] = 0;

  // sn_%d/ or sn=%d
  if(div->paging.urltype == URL_TYPE_STATIC) 
    snprintf(snargs,sizeof(snargs),"%s%c%s/",div->paging.snvar,STATIC_URL_DELIMITER,"%d");
  else if(div->paging.urltype == URL_TYPE_DYNAMIC)
    snprintf(snargs,sizeof(snargs),"%s%c%s",div->paging.snvar,DYNAMIC_URL_DELIMITER,"%d");

  if(page_makepaging(rt,sn,rn,div->paging.linknum,div->paging.prevnext,div->paging.firstlast, \
                     pageurl,snargs,div->paging.curattr,paging,sizeof(paging)) < 0) {
    write_log_r(LLERROR,"%s#%s make paging error.",div->name,div->dbc.stmts[stmtindex]);
    return -1;
  }

  HTSetVar(htmtmpl,div->paging.tv_paging,paging);
  HTParse(htmtmpl,div->paging.tb_paging,0);
  return 0;
}

int page_makediv(void *htmtmpl,pg_div_t *div,TCMAP *sysvars)
{
  int i,j,k,res,resnum,rawindex,pagesize,newpagesize,barsize;
  const char *cp,*url;
  char name[1024],**fv,*newpage,*p,*pp[1];
  char keywordsbar[8*1024];

  // title
  if(div->title && div->tv_title)
    HTSetVar(htmtmpl,div->tv_title,div->title);

  // 版块对应的各个语句执行情况
  for(i=0;i<div->dbc.stmtnum;i++)
  {
    snprintf(name,sizeof(name),"%s#%s",div->name,div->dbc.stmts[i]);
    cp = tcmapget2(sysvars,name);
    if(!cp || strcasecmp(cp,"down") == 0) { // server down, MQS执行错误则cp==NULL
      write_log_r(LLERROR,"%s#%s server is down.",div->name,div->dbc.stmts[i]);
      snprintf(name,sizeof(name),"%s#%s#type",div->name,div->dbc.stmts[i]);
      cp = tcmapget2(sysvars,name);
      if(!cp || (cp && strcmp(cp,"read") == 0)) // MQS情况下cp == NULL
        return -1;
      //MQS或read语句执行错误时才返回错误，write、count语句执行错误仍然返回read结果
    } else if(strcasecmp(cp,"error") == 0) { // write error
      if(div->dbc.tvs[i]) HTSetVar(htmtmpl,div->dbc.tvs[i],div->booltext[1]);
      write_log_r(LLERROR,"%s#%s execute error.",div->name,div->dbc.stmts[i]);
    } else if(strcasecmp(cp,"ok") == 0) {    // write ok
      if(div->dbc.tvs[i]) HTSetVar(htmtmpl,div->dbc.tvs[i],div->booltext[0]);
    } else if(strcasecmp(cp,"error_count") == 0) { // count error
      // ...
    } else if(strcasecmp(cp,"record") == 0) { // read record
      snprintf(name,sizeof(name),"%s#%s#resnum",div->name,div->dbc.stmts[i]);
      cp = tcmapget2(sysvars,name);
      resnum = atoi(cp);
      if(!resnum) {               // query result empty
        if(div->queryempty[1]) {  // 需要进行变量赋值
          cp = tcmapget2(sysvars,div->queryempty[1]+1); // $var
          if(cp) {
            pp[0] = (char *)cp;
            p = strreplace(div->queryempty[0],strlen(div->queryempty[0]),div->queryempty+1,pp,1,CODETYPE_UTF8);
          } else {
            p = strfilter(div->queryempty[0],strlen(div->queryempty[0]),div->queryempty+1,1,CODETYPE_UTF8);
          }
          if(div->dbc.tvs[i]) HTSetVar(htmtmpl,div->dbc.tvs[i],p);
          xfree(p);
        } else if(div->dbc.tvs[i]) 
          HTSetVar(htmtmpl,div->dbc.tvs[i],div->queryempty[0]);
      } else { 
        fv = (char **)xcalloc(div->record.fieldnum,sizeof(char *));
      }
      
      for(j=0;j<resnum;j++) {
        rawindex = -1;
        newpage = NULL;
        for(k=0;k<div->record.fieldnum;k++) {
          snprintf(name,sizeof(name),"%s#%s#%d#%s",div->name,div->dbc.stmts[i],j,div->record.field[k].name);
          cp = tcmapget2(sysvars,name);
          fv[k] = (char *)cp;
          if(cp && div->type == DIV_TYPE_MAIN && div->record.field[k].type == FIELD_TYPE_TITLE) {
            tcmapput2(sysvars,"@title",(char *)cp);
          } else if(cp && div->type == DIV_TYPE_MAIN && div->record.field[k].type == FIELD_TYPE_URL) {
            tcmapput2(sysvars,"@url",(char *)cp);
          } else if(cp && div->type == DIV_TYPE_MAIN && div->record.field[k].type == FIELD_TYPE_RAW) {
            rawindex = k;
          }
        }

        // snapshot raw page
        if(rawindex >= 0 && div->tv_hlpos && div->hlwords) {
          cp = tcmapget2(sysvars,div->hlwords);
          if(!cp || !cp[0]) goto makerecord;
          if(!fv[rawindex] || !fv[rawindex][0])  goto makerecord;
          url = tcmapget2(sysvars,"@url");
          if(!url || !url[0]) goto makerecord;
          pagesize = strlen(fv[rawindex]);
          if(pagesize > MAX_RAW_PAGE)
            goto makerecord;
          newpage = (char *)xcalloc(MAX_RAW_PAGE+1,sizeof(char));
          newpagesize = MAX_RAW_PAGE;
          barsize = 8*1024;
          res = snap_makehighlightpos(fv[rawindex],pagesize,cp,url,newpage,&newpagesize,keywordsbar,&barsize);
          if(res < 0) {
            xfree(newpage);
            goto makerecord;
          }
          fv[rawindex] = newpage;
          HTSetVar(htmtmpl,div->tv_hlpos,keywordsbar);
        }
        
makerecord:
        if(page_makerecord(htmtmpl,&div->record,fv) < 0) {
          write_log_r(LLERROR,"%s#%s make record error.",div->name,div->dbc.stmts[i]);
          return -1;
        }
        xfree(newpage);
      }
      
      if(resnum) xfree(fv);

      // paging
      if(div->paging.tv_paging && div->paging.tb_paging && resnum) {
        if(page_makepagingbar(htmtmpl,div,sysvars,i) < 0)
          return -1;
      }
    } else { // count
      if(div->dbc.tvs[i])
        HTSetVar(htmtmpl,div->dbc.tvs[i],(char *)cp);
    } 
  }

  HTParse(htmtmpl,div->tb_div,0);
  return 0;
}

// 页面是由多个版块构成的，只要主体版块没有返回错误，则不返回错误。
int page_make(pg_page_t *page,char *pagebuf,int bufsize,TCMAP *sysvars)
{
  void *htmtmpl;
  char *tn;
  int i,r,t,bufpos;
  const char *cp;

  tn = NULL;
  if(page->tptvar)
    tn = (char *)tcmapget2(sysvars,page->tptvar);
  if(!tn) 
    tn = page->deftpt;
  for(i=0;i<page->tptnum;i++)
    if(strcasecmp(tn,page->tpts[i].name) == 0)
      break;
  if(i==page->tptnum) {
    write_log_r(LLWARN,"page#%s template: %s is unsupport.",page->name,tn);
    t = 0;
  } else
    t = i;

  htmtmpl = HTLoadFile(page->tpts[t].content,page->tpts[t].size);
  if(!htmtmpl) {
    write_log_r(LLERROR,"page#%s Load template: %s failure.",page->name,page->tpts[t].name);
    return -1;
  }

  for(i=0;i<page->divnum;i++) {
    if(page->divs[i]->type == DIV_TYPE_RAND) {
      unsigned int randnum;
      char rands[32];
      _AutoSrand();
      randnum = _Rand();
      if(page->divs[i]->randmod > 0)
       randnum = randnum % page->divs[i]->randmod;
      else 
       randnum = randnum % 10000000;
      snprintf(rands,sizeof(rands),"%u",randnum);
      tcmapput2(sysvars,"@rand",rands);
      break;
    }
  }

  for(i=0;i<page->tvbfnum;i++) {
    cp = tcmapget2(sysvars,page->tv_before[i]);
    if(cp)
      HTSetVar(htmtmpl,page->tv_before[i],(char *)cp);
  }

  switch (page->type)
  {
    case PAGE_TYPE_INDEX:
    case PAGE_TYPE_BROWSE:
    case PAGE_TYPE_SEARCH:
      for(i=0;i<page->divnum;i++) {
        r = page_makediv(htmtmpl,page->divs[i],sysvars);
        if(r < 0 && page->divs[i]->type == DIV_TYPE_MAIN) {
          write_log_r(LLERROR,"page#%s make main divsion#%s error.",page->name,page->divs[i]->name);
          return -1;
        }
      }
      break;
    case PAGE_TYPE_CONTENT:   // 先生成主体内容再生成其他版块内容
      for(i=0;i<page->divnum;i++)
        if(page->divs[i]->type == DIV_TYPE_MAIN) {
          r = page_makediv(htmtmpl,page->divs[i],sysvars);
          if(r < 0) {
            write_log_r(LLERROR,"page#%s make main divsion#%s error.",page->name,page->divs[i]->name);
            return -1;
          }
          break;
        }

      for(i=0;i<page->divnum;i++)
        if(page->divs[i]->type != DIV_TYPE_MAIN)
          page_makediv(htmtmpl,page->divs[i],sysvars);
      break;
    case PAGE_TYPE_SNAPSHOT:
      break;
  }

  for(i=0;i<page->tvafnum;i++) {
    cp = tcmapget2(sysvars,page->tv_after[i]);
    if(cp)
      HTSetVar(htmtmpl,page->tv_after[i],(char *)cp);
  }

  bufpos = HTFinish(htmtmpl,HT_OUTPUT_STRING,pagebuf,bufsize);
  return bufpos;
}

#ifdef TEST_PAGE

int main()
{
  pg_divset_t divset;
  pg_pageset_t pageset;
  
  if(page_init("../etc/wbe.conf","web",&divset,&pageset,"/root/bluefire/src/wbe") < 0) {
    printf("page_init() error.\n");
    return -1;
  }

  page_free(&divset,&pageset);
  return 0;
}

#endif

