/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcutil.h>
#include "comn.h"
#include "conf.h"
#include "log.h"
#include "xmalloc.h"
#include "xstring.h"
#include "utils.h"
#include "cgic.h"
#include "page.h"
#include "form.h"
#include "dbi.h"
#include "action.h"
#include "referer.h"

int action_init(const char *confpath,const char *sectname,actionset_t *actset)
{
  action_t act;
  char section[128];
  char *an,**ans;
  int i,j,anum,isrefparse;
  void *cfgdata;
  conf_t actc[] = {
    {section, "BaseUri", CONF_STR, 1, NULL,  NULL, NULL, NULL, &act.baseuri},
    {section, "Module",  CONF_STR, 1, NULL,  NULL, NULL, NULL, &act.module},
    {section, "Controller",CONF_STR, 1, NULL,  NULL, NULL, NULL, &act.controller},
    {section, "Form",    CONF_STR, 1, NULL,  NULL, NULL, NULL, &act.formname},
    {section, "Page",    CONF_STR, 0, NULL,  NULL, NULL, NULL, &act.pagename},
    {section, "Referer", CONF_INT, 1, NULL,  NULL, NULL, NULL, &act.refparse},
    {section, "Log",     CONF_INT, 1, NULL,  NULL, NULL, NULL, &act.log},
    {section, "LogRT",   CONF_INT, 1, NULL,  NULL, NULL, NULL, &act.logrt},
    {section, "Redirect",CONF_STR, 1, NULL,  NULL, NULL, NULL, &act.redirect}
  };
 
  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"Can't open configure file: %s.",confpath);
    return -1;
  }

  an = getconfmulti(cfgdata,sectname,"Actions",0,NULL," ",&ans,&anum);
  actset->actnum = anum;
  actset->acts = (action_t *)xcalloc(actset->actnum,sizeof(action_t));
  
  for(i=0;i<anum;i++)
  {
    actset->acts[i].name = xstrdup(ans[i]);
    snprintf(section,sizeof(section),"action:%s",ans[i]);
    memset(&act,0,sizeof(action_t));
    get_conf(cfgdata,actc,COUNTOF(actc));
    actset->acts[i].baseuri = act.baseuri;
    actset->acts[i].module = act.module;
    actset->acts[i].controller = act.controller;
    actset->acts[i].formname = act.formname;
    actset->acts[i].pagename = act.pagename;
    actset->acts[i].refparse = act.refparse;
    actset->acts[i].log = act.log;
    actset->acts[i].logrt = act.logrt;
    actset->acts[i].redirect = act.redirect;
  }

  xfree(an);
  xfree(ans);
  free_conf(cfgdata);

  isrefparse = 0;
  for(i=0;i<actset->actnum;i++) {
    for(j=0;j<pageset.pagenum;j++) {
      if(strcasecmp(actset->acts[i].pagename,pageset.pages[j].name) == 0) {
        actset->acts[i].page = pageset.pages+j;
        break;
      }
    }
    
    if(j == pageset.pagenum) {
      write_log(LLERROR,"[%s:page:%s] set error.",actset->acts[i].name,actset->acts[i].pagename);
      return -1;
    }

    if(actset->acts[i].formname) {
      for(j=0;j<formset.formnum;j++) {
        if(strcasecmp(actset->acts[i].formname,formset.forms[j].name) == 0) {
          actset->acts[i].form = formset.forms+j;
          break;
        }
      }

      if(j == formset.formnum) {
        write_log(LLERROR,"[%s:form:%s] set error.",actset->acts[i].name,actset->acts[i].formname);
        return -1;
      }
    }

    if(actset->acts[i].refparse)
      isrefparse = 1;
  }

  actset->refparse = isrefparse;
  if(actset->refparse && (referer_init(confpath) < 0)) {
    write_log(LLERROR,"referer init error.");
    return -1;
  }
  
  return 0;
}

void action_free(actionset_t *actset)
{
  int i;

  for(i=0;i<actset->actnum;i++)
  {
    xfree(actset->acts[i].baseuri);
    xfree(actset->acts[i].module);
    xfree(actset->acts[i].controller);
    xfree(actset->acts[i].name);
    xfree(actset->acts[i].pagename);
    xfree(actset->acts[i].formname);
    xfree(actset->acts[i].redirect);
  }

  if(actset->refparse)
    referer_free();
  xfree(actset->acts);
}

// @return: error: -1, ok: 0, too large: 1
int action_check(void *checkargs,const char * cgiScriptName,const char *method,const char *enctype,int contentlen)
{
  int i,r;
  action_t *act;
  char *script,*module,*controller,*action;
  TCMAP *sysvars = (TCMAP *)checkargs;
  char actindex[32];
  
  // http://xxx/baseuri/module/controller/action?...
  if(!cgiScriptName || !cgiScriptName[0]) {
    write_log_r(LLERROR,"ScriptName inexist.");
    return -1;
  } 

  script = (char *)cgiScriptName;
  action = strrchr(script,'/');
  if(!action || !*(action+1)) { // end of '/'
    write_log_r(LLERROR,"action error in ScriptName.");
    return -1;
  }
  controller = strrchrn(script,'/',2);
  module = NULL;  // todo...
  
  for(i=0;i<actset.actnum;i++) {
    if((controller && actset.acts[i].controller && strcasecmp(controller+1,actset.acts[i].controller) != 0)
    || (controller && !actset.acts[i].controller)
    || (!controller && actset.acts[i].controller))
      continue;
    
    if(strcasecmp(action+1,actset.acts[i].name) == 0) {
      act = actset.acts+i;
      break;
    }
  }

  if(i == actset.actnum) {
    if(controller)
      write_log_r(LLERROR,"controller & action: %s/%s is unsupport.",controller+1,action+1);
    else
      write_log_r(LLERROR,"action: %s is unsupport.",action+1);
    return -1;
  }

  r = form_check(act->form,method,enctype,contentlen);
  if(r == 0) {
    snprintf(actindex,sizeof(actindex),"%d",i);
    tcmapput2(sysvars,"@action",action+1);
    tcmapput2(sysvars,"@actionindex",actindex);
    if(controller)
      tcmapput2(sysvars,"@controller",controller+1);
  }

  return r;
}

int action_perform(CGI_T *cgi,TCMAP *sysvars,dbi_env_t *envs,int envnum,
                          dbi_dal_t *dals,void *refdata,char *pagebuf,int bufsize)
{
  int i,j,actindex,pagesize,argnum;
  action_t *act;
  const char *action;
  char *content,**argvals;
 
  action = tcmapget2(sysvars,"@actionindex");
  if(!action) {
    write_log_r(LLERROR,"action inexist.");
    return -1;
  }
  actindex = atoi(action);
  act = actset.acts + actindex;

  // log
  if(act->log) {
    content = NULL;
    if(strcasecmp(cgi->cgiRequestMethod,"get") == 0)
      content = cgi->cgiQueryString;
    else if(strcasecmp(cgi->cgiRequestMethod,"post") == 0 \
         && strcasecmp(cgi->cgiContentType,"application/x-www-form-urlencoded") == 0) {
       if(cgi->cgiPostContent) content = cgi->cgiPostContent;
       else content = "";
    }
    
    if(content && strlen(content) <= 1024) {
      if(act->controller)
        write_log_r(LLINFO,"[action:%s/%s] %s",act->controller,act->name,content);
      else
        write_log_r(LLINFO,"[action:%s] %s",act->name,content);

      if(act->logrt) flush_log();
    }
  }

  // parse referer
  if(act->refparse && cgi->cgiReferrer) {
    char wordbuf[1024];
    if(referer_parse(refdata,cgi->cgiReferrer,wordbuf,sizeof(wordbuf)))
      tcmapput2(sysvars,"@refword",wordbuf);
  }
 
  // parse form 
  if(act->form && form_parse(cgi,act->form,sysvars) < 0) {
    write_log_r(LLERROR,"action#%s form#%s parse error.",act->name,act->form->name);
    if(act->redirect) {
      tcmapput2(sysvars,"@redirect",act->redirect);
      return -2; // form parse error return -2
    } else
      return -1;
  }

  // push all statement id in sysmap
  dbi_stmt_pushid(&stmtset,sysvars);

  // bind clauses
  if(act->page->clausenum) {
    argvals = dbi_stmt_bind(envs,envnum,&stmtset,dals,act->page->clauses,act->page->clausenum,sysvars,&argnum);
    if(!argvals && argnum < 0) {
      write_log_r(LLERROR,"action#%s page#%s bind clauses error.",act->name,act->page->name);
      return -1;
    }
  }

  // execute page mqs
  for(i=0;i<act->page->mqsnum;i++) {    
    if(dbi_stmt_exec_multi(envs,envnum,&stmtset,dals,act->page->mqs[i].divs,act->page->mqs[i].stmts,\
                           act->page->mqs[i].stmtnum,sysvars) < 0) {
       write_log_r(LLERROR,"action#%s page#%s mqs#%d execute error.",act->name,act->page->name,i);
       goto FREE;
    }
  }

  // execute page div dbc
  for(i=0;i<act->page->divnum;i++) {
    for(j=0;j<act->page->divs[i]->dbc.stmtnum;j++) {
      if(!act->page->divs[i]->dbc.inmqs[j]) {
        if(dbi_stmt_exec(envs,envnum,&stmtset,dals,act->page->divs[i]->name,act->page->divs[i]->dbc.stmts[j],sysvars) < 0) {
          write_log_r(LLERROR,"action#%s page#%s div#%s stmt#%s execute error.",act->name,act->page->name,\
                    act->page->divs[i]->name,act->page->divs[i]->dbc.stmts[j]);
          goto FREE;
        }
      }
    }
  }

  // free bind arguments
  if(act->page->clausenum)
    dbi_stmt_freeargs(argvals,argnum);

  // make page
  if((pagesize = page_make(act->page,pagebuf,bufsize,sysvars)) < 0) {
    write_log_r(LLERROR,"action#%s page#%s make error.",act->name,act->page->name);
    return -1;
  }

  return pagesize;

FREE:
  if(act->page->clausenum)
    dbi_stmt_freeargs(argvals,argnum);
  return -1;
}

