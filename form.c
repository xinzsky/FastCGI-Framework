/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcutil.h>
#include "conf.h"
#include "xmalloc.h"
#include "comn.h"
#include "cgic.h"
#include "utils.h"
#include "log.h"
#include "form.h"
#include "page.h"
#include "xstring.h"
#include "md5.h"
#include "fieldproc.h"
#include "security.h"

int form_init(const char *confpath,const char *sectname,formvarset_t *formvarset,formset_t *formset)
{
  formvar_t formvar;
  form_t f;
  char section[128];
  char *p,*min,*max,*choice,**choices,*handler,**handlers;
  char *form,**forms,*var,**vars;
  int i,j,k,choicenum,handlernum,formnum,varnum;
  void *cfgdata;

  const char *type[] = {
  "int","float",
  "string","string_nonl","string_multi",
  "select_single","select_multi",
  "submit",
  "checkbox_single","checkbox_multi",
  "radio",
  "upload"
  };

  const int itype[] = {
    FORM_VAR_INT, FORM_VAR_FLOAT,
    FORM_VAR_STRING, FORM_VAR_STRING_NONL, FORM_VAR_STRING_MULTI,
    FORM_VAR_SELECT_SINGLE, FORM_VAR_SELECT_MULTIPLE,
    FORM_VAR_SUBMIT,
    FORM_VAR_CHECKBOX_SINGLE, FORM_VAR_CHECKBOX_MULTIPLE,
    FORM_VAR_RADIO,
    FORM_VAR_UPLOAD
  };

  const char *shandler[] = {
    "htmlencode", "urlencode", "base64encode", "base64decode","md5","xssfilter"
  };

  const int ihandler[] = {
    FORM_VAR_HTMLENCODE,FORM_VAR_URLENCODE,FORM_VAR_BASE64ENCODE,FORM_VAR_BASE64DECODE,FORM_VAR_MD5,FORM_VAR_XSS
  };

  const char *encodetypes[] = {
      "url","html","base64","md5"
    };
  
  int encodetype[] = {
    ENCODE_TYPE_URL,ENCODE_TYPE_HTML,ENCODE_TYPE_BASE64,ENCODE_TYPE_MD5
  };

  const char *method[] = {"get","post","both"};
  const char *enctype[] = {"application/x-www-form-urlencoded","multipart/form-data","both"};
  
  conf_t fvc[] = {
    {section,  "Type",    CONF_STR,    0,  NULL,   (void *)type, (void *)COUNTOF(type), (void *)itype, &formvar.type},
    {section,  "Min",     CONF_STR,    1,  NULL,   NULL,  NULL, NULL, &min},
    {section,  "Max",     CONF_STR,    1,  NULL,   NULL,  NULL, NULL, &max},
    {section,  "Choices", CONF_MULTI,  1,  NULL,   (void *)&choices, (void *)" ", (void *)&choicenum, &choice},
    {section,  "IsNull",  CONF_INT,    1,  "1",    (void *)0, (void *)1, NULL, &formvar.isnull},
    {section,  "DefVal",  CONF_STR,    1,  NULL,   NULL,  NULL, NULL, &formvar.defval},
    {section,  "MaxLen",  CONF_INT,    1,  NULL,   NULL,  NULL, NULL, &formvar.maxlen},
    {section,  "Delimiter",CONF_STR,   1,  NULL,   NULL,  NULL, NULL, &formvar.delimiter},
    {section,  "RelVar",  CONF_STR,    1,  NULL,   NULL,  NULL, NULL, &formvar.relvar},
    {section,  "Encode",  CONF_STR,    1,  NULL,   (void *)encodetypes,(void *)COUNTOF(encodetypes),(void *)encodetype, &formvar.encode},
    {section,  "Handlers",CONF_MULTI,  1,  NULL,   (void *)&handlers,(void *)" ",(void *)&handlernum,&handler}
  };

  conf_t fc[] = {
    {section,  "Vars",    CONF_MULTI,   0, NULL,   (void *)&vars,(void *)" ", (void *)&varnum,&var},
    {section,  "Method",  CONF_STR,     1, "get",  (void *)method,(void *)COUNTOF(method),NULL,&f.method},
    {section,  "EncType", CONF_STR,     1, "application/x-www-form-urlencoded",   (void *)enctype,(void *)COUNTOF(enctype),NULL,&f.enctype},
    {section,  "PostLimit",CONF_INT,    1, "0",    NULL,  NULL, NULL, &f.postlimit},
    {section,  "Process", CONF_STR,     1, NULL,   NULL,  NULL, NULL, &f.proccfg}
  };

  cfgdata = init_conf();
  if(load_conf(cfgdata,confpath) < 0) {
    write_log(LLERROR,"Can't open configure file: %s.",confpath);
    return -1;
  }

  form = getconfmulti(cfgdata,sectname,"Forms",0,NULL," ",&forms,&formnum);
  formset->formnum = formnum;
  formset->forms = (form_t *)xcalloc(formnum,sizeof(form_t));
  for(i=0;i<formnum;i++)
  {
    formset->forms[i].name = xstrdup(forms[i]);
    snprintf(section,sizeof(section),"form:%s",forms[i]);
    memset(&f,0,sizeof(form_t));
    get_conf(cfgdata,fc,COUNTOF(fc));
    formset->forms[i].varnum = varnum;
    formset->forms[i].varname = (char **)xcalloc(varnum,sizeof(char *));
    for(j=0;j<varnum;j++)
      formset->forms[i].varname[j] = xstrdup(vars[j]);
    xfree(var);
    xfree(vars);
    formset->forms[i].method = f.method;
    formset->forms[i].enctype = f.enctype;
    formset->forms[i].postlimit = f.postlimit;
    formset->forms[i].proccfg = f.proccfg;
    if(formset->forms[i].proccfg) {
      p = strchr(formset->forms[i].proccfg,':');
      if(p) {
        *p++ = 0;
        trim(formset->forms[i].proccfg);
        trim(p);
        formset->forms[i].procsect = xstrdup(p);
      } else {
        write_log(LLERROR,"Form:%s process configuration error.",formset->forms[i].name);
        return -1;
      }
    }
  }
  xfree(form);
  xfree(forms);

  var = getconfmulti(cfgdata,sectname,"Vars",0,NULL," ",&vars,&varnum);
  formvarset->varnum = varnum;
  formvarset->vars = (formvar_t *)xcalloc(varnum,sizeof(formvar_t));
  for(i=0;i<varnum;i++)
  {
    formvarset->vars[i].name = xstrdup(vars[i]);
    snprintf(section,sizeof(section),"var:%s",vars[i]);
    memset(&formvar,0,sizeof(formvar_t));
    get_conf(cfgdata,fvc,COUNTOF(fvc));
    formvarset->vars[i].type = formvar.type;
    
    if(min && formvarset->vars[i].type == FORM_VAR_INT)
      formvarset->vars[i].min.imin = atoi(min);
    else if(min && formvarset->vars[i].type == FORM_VAR_FLOAT)
      formvarset->vars[i].min.fmin = atof(min);
    
    if(max && formvarset->vars[i].type == FORM_VAR_INT)
      formvarset->vars[i].max.imax = atoi(max);
    else if(max && formvarset->vars[i].type == FORM_VAR_FLOAT)
      formvarset->vars[i].max.fmax = atof(max);
      
    xfree(min);
    xfree(max);

    if(choice) {
      formvarset->vars[i].choicenum = choicenum;
      formvarset->vars[i].choices = (char **)xcalloc(choicenum,sizeof(char *));
      for(j=0;j<choicenum;j++)
        formvarset->vars[i].choices[j] = xstrdup(choices[j]);
      xfree(choice);
      xfree(choices);
    }

    formvarset->vars[i].isnull = formvar.isnull;
    formvarset->vars[i].defval = formvar.defval;
    formvarset->vars[i].maxlen = formvar.maxlen;
    formvarset->vars[i].delimiter = formvar.delimiter;
    formvarset->vars[i].relvar = formvar.relvar;
    formvarset->vars[i].encode = formvar.encode;
    if(handler) {
      formvarset->vars[i].handnum = handlernum;
      formvarset->vars[i].handler = (int *)xcalloc(handlernum,sizeof(int));
      formvarset->vars[i].args = (char **)xcalloc(handlernum,sizeof(char *));
      for(j=0;j<handlernum;j++) {
        char *p;
        p = strchr(handlers[j],',');
        if(p)  { 
          *p++ = 0;
          trim(handlers[j]);
          trim(p);
          formvarset->vars[i].args[j] = xstrdup(p);
        }
        
        for(k=0;k<COUNTOF(shandler);k++) {
          if(strcasecmp(handlers[j],shandler[k]) == 0) {
            formvarset->vars[i].handler[j] = ihandler[k];
            break;
          }
        }

        if(k == COUNTOF(shandler)) {
          write_log(LLERROR,"[%s:%s:%s] is unsupport.",section,"Handlers",handlers[j]);
          return -1;
        }
      }
      xfree(handler);
      xfree(handlers);
    }
  }
  xfree(var);
  xfree(vars);
  
  free_conf(cfgdata);

  for(i=0;i<formvarset->varnum;i++) {
    if(!formvarset->vars[i].relvar) 
      continue;
    
    for(j=0;j<formvarset->varnum;j++) {
      if(i != j && strcasecmp(formvarset->vars[i].relvar,formvarset->vars[j].name) == 0)  {
        formvarset->vars[i].rvar = formvarset->vars + j;
        break;
      }
    }

    if(j == formvarset->varnum) {
      write_log(LLERROR,"[%s:%s:%s] set error.",formvarset->vars[i].name,"RelVar",formvarset->vars[i].relvar);
      return -1;
    }
  }

  for(i=0;i<formset->formnum;i++) {
    formset->forms[i].vars = (formvar_t **)xcalloc(formset->forms[i].varnum,sizeof(formvar_t *));
    for(j=0;j<formset->forms[i].varnum;j++) {
      for(k=0;k<formvarset->varnum;k++) {
        if(strcasecmp(formset->forms[i].varname[j],formvarset->vars[k].name) == 0) {
          formset->forms[i].vars[j] = formvarset->vars + k;
          break;
        }
      }

      if(k == formvarset->varnum) {
        write_log(LLERROR,"[%s:%s:%s] form variable inexist.",sectname,formset->forms[i].name,formset->forms[i].varname[j]);
        return -1;
      }
    }
  }

  for(i=0;i<formset->formnum;i++) {
    if(formset->forms[i].proccfg) {
      formset->forms[i].fpargs = fp_new(formset->forms[i].proccfg,formset->forms[i].procsect);
      if(!formset->forms[i].fpargs) {
        write_log(LLERROR,"Form:%s process init error.",formset->forms[i].name);
        return -1;
      }
    }
  }
  
  return 0;
}

void form_free(formvarset_t *formvarset,formset_t *formset)
{
  int i,j;

  for(i=0;i<formset->formnum;i++) {
    if(formset->forms[i].proccfg)
      fp_del(formset->forms[i].fpargs);
  }

  for(i=0;i<formvarset->varnum;i++)
  {
    xfree(formvarset->vars[i].name);
    for(j=0;j<formvarset->vars[i].choicenum;j++)
      xfree(formvarset->vars[i].choices[j]);
    xfree(formvarset->vars[i].choices);
    xfree(formvarset->vars[i].defval);
    xfree(formvarset->vars[i].delimiter);
    xfree(formvarset->vars[i].relvar);
    xfree(formvarset->vars[i].handler);
    for(j=0;j<formvarset->vars[i].handnum;j++)
      xfree(formvarset->vars[i].args[j]);
    xfree(formvarset->vars[i].args);
  }
  xfree(formvarset->vars);

  for(i=0;i<formset->formnum;i++)
  {
    xfree(formset->forms[i].name);
    for(j=0;j<formset->forms[i].varnum;j++)
      xfree(formset->forms[i].varname[j]);
    xfree(formset->forms[i].varname);
    xfree(formset->forms[i].vars);
    xfree(formset->forms[i].method);
    xfree(formset->forms[i].enctype);
    xfree(formset->forms[i].proccfg);
    xfree(formset->forms[i].procsect);
  }
  xfree(formset->forms);
}

// @return: need free it.
static char *form_handler(formvar_t *var,const char *varval)
{
  int i,len;
  char *oldval,*newval;

  if(!var->handnum || !varval || !varval[0])
    return NULL;

  oldval = xstrdup(varval);
  len = strlen(oldval);
  for(i=0;i<var->handnum;i++)
  {
    switch (var->handler[i])
    {
      case FORM_VAR_HTMLENCODE:
        newval = (char *)xmalloc(len*5+1);
        htmlencode(oldval,newval);
        break;
      case FORM_VAR_URLENCODE:
        newval = (char *)xmalloc(len*3+1);
        urlencode(oldval,newval);
        break;
      case FORM_VAR_BASE64ENCODE:
        newval = (char *)xmalloc(len*2+1);
        base64encode4url(oldval,len,newval);
        break;
      case FORM_VAR_BASE64DECODE:
        newval = (char *)xmalloc(len+1);
        base64decode4url(oldval,newval);
        break;
      case FORM_VAR_MD5:
        newval = (char *)xmalloc(33);
        md5s((unsigned char *)oldval,len,newval);
        break;
      case FORM_VAR_XSS:
        newval = xss_filter(var->args[i],oldval,len,var->maxlen);
    }

    xfree(oldval);
    oldval = newval;
    len = strlen(oldval);
  }

  return oldval;
}

static void form_put_names(formvar_t *formvar,TCMAP *vars)
{
  char buf[1024];

  // name_
  snprintf(buf,sizeof(buf),"%s%c",formvar->name,STATIC_URL_DELIMITER);
  tcmapput2(vars,buf,buf);
  
  // name=
  snprintf(buf,sizeof(buf),"%s%c",formvar->name,DYNAMIC_URL_DELIMITER);
  tcmapput2(vars,buf,buf);
  
  // -name_
  snprintf(buf,sizeof(buf),"%c%s%c",STATIC_URL_SEPARATOR,formvar->name,STATIC_URL_DELIMITER);
  tcmapput2(vars,buf,buf);
  
  // &name=
  snprintf(buf,sizeof(buf),"%c%s%c",DYNAMIC_URL_SEPARATOR,formvar->name,DYNAMIC_URL_DELIMITER);
  tcmapput2(vars,buf,buf);
}

static int form_put_values(formvar_t *formvar,const char *value,TCMAP *vars)
{
  char name[1024];
  char *p,*encode;
  int len;

  if(!value || !value[0])
    return -1;

  if(formvar->handnum) {
    snprintf(name,sizeof(name),"%s#orig",formvar->name);
    tcmapput2(vars,name,value);
    p = form_handler(formvar,value);
    if(p) {
      tcmapput2(vars,formvar->name,p);
      xfree(p);
    }
  } else {
    tcmapput2(vars,formvar->name,value);
  }

  len = strlen(value);
  switch (formvar->encode)
  {
    case ENCODE_TYPE_URL:
      encode = (char *)xcalloc(len*3+1,sizeof(char));
      urlencode((char *)value,encode);
      break;
    case ENCODE_TYPE_HTML:
      encode = (char *)xcalloc(len*5+1,sizeof(char));
      htmlencode(value,encode);
      break;
    case ENCODE_TYPE_BASE64:
      encode = (char *)xcalloc(len*2,sizeof(char));
      base64encode4url(value,len,encode);
      break;
    case ENCODE_TYPE_MD5:
      encode = (char *)xcalloc(33,sizeof(char));
      md5s((unsigned char *)value,len,encode);
      break;
    default:
      break;
  }

  if(formvar->encode != ENCODE_TYPE_NONE) {
    snprintf(name,sizeof(name),"%s#encode",formvar->name);
    tcmapput2(vars,name,encode);
    xfree(encode);
  }

  return 0;
}

static int form_parse_int(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
  int result,ival,defval;
  char buf[1024];

  if(formvar->defval) 
    defval = atoi(formvar->defval);
  else 
    defval = 0;
  
  if(!formvar->min.imin && !formvar->max.imax)
    result = cgiFormInteger(cgi,formvar->name,&ival,defval);
  else
    result = cgiFormIntegerBounded(cgi,formvar->name,&ival,\
                  formvar->min.imin,formvar->max.imax,defval);

  if(result != cgiFormSuccess) {
    if(!formvar->isnull) return -1;
    if(!formvar->defval) return 0;
  }
  
  snprintf(buf,sizeof(buf),"%d",ival);
  tcmapput2(vars,formvar->name,buf);

  form_put_names(formvar,vars);
  
  return 0;
}

static int form_parse_float(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
   double fval,fdefval;
   char buf[1024];
   int result;
 
   if(formvar->defval) 
    fdefval = atof(formvar->defval);
   else 
    fdefval = 0.0;
 
   if(!formvar->min.fmin && !formvar->max.fmax)
     result = cgiFormDouble(cgi,formvar->name,&fval,fdefval);
   else
     result = cgiFormDoubleBounded(cgi,formvar->name,&fval,\
                  formvar->min.fmin,formvar->max.fmax,fdefval);
 
   if(result != cgiFormSuccess) 
   {
     if(!formvar->isnull) return -1;
     if(!formvar->defval) return 0;
   }
 
   snprintf(buf,sizeof(buf),"%f",fval);
   tcmapput2(vars,formvar->name,buf);

   form_put_names(formvar,vars);
   return 0;
}

static int form_parse_string(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
  int result,needspace;
  char *dbuf,buf[1024];
  
  result = cgiFormStringSpaceNeeded(cgi,formvar->name,&needspace);
  if(result != cgiFormSuccess) 
  {
    if(!formvar->isnull) return -1;
    if(formvar->defval) {
      form_put_values(formvar,formvar->defval,vars);
      form_put_names(formvar,vars);
    }
    return 0;
  }

  if(formvar->maxlen && (needspace-1) > formvar->maxlen)
    return -1;
  
  if(needspace <= sizeof(buf))
    dbuf = buf;
  else 
    dbuf = (char *)xmalloc(needspace);
  
  result = cgiFormString(cgi,formvar->name,dbuf,needspace);
  if(result != cgiFormSuccess) {
    if(!formvar->isnull) return -1;
    if(formvar->defval) {
      form_put_values(formvar,formvar->defval,vars);
      form_put_names(formvar,vars);
    }
  } else {
    form_put_values(formvar,dbuf,vars);
    form_put_names(formvar,vars);
    if(needspace > sizeof(buf))
      xfree(dbuf);
  }

  return 0;
}

static int form_parse_string_nonl(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
  int result,needspace;
  char *dbuf,buf[1024];

   result = cgiFormStringSpaceNeeded(cgi,formvar->name,&needspace);
   if(result != cgiFormSuccess) 
   {
     if(!formvar->isnull) return -1;
     if(formvar->defval) {
       form_put_values(formvar,formvar->defval,vars);
       form_put_names(formvar,vars);
     }
     return 0;
   }
 
   if(formvar->maxlen && (needspace-1) > formvar->maxlen)
     return -1;
   
   if(needspace <= sizeof(buf))
     dbuf = buf;
   else 
     dbuf = (char *)xmalloc(needspace);
   
   result = cgiFormStringNoNewlines(cgi,formvar->name,dbuf,needspace);
   if(result != cgiFormSuccess) {
     if(!formvar->isnull) return -1;
     if(formvar->defval) {
       form_put_values(formvar,formvar->defval,vars);
       form_put_names(formvar,vars);
     }
   } else {
     form_put_values(formvar,dbuf,vars);
     form_put_names(formvar,vars);
     if(needspace > sizeof(buf))
       xfree(dbuf);
   }

   return 0;
}

// 多个字符串连接起来。
static int form_parse_string_multi(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
  char **vpp;
  char *dbuf,buf[1024];
  int j,result;
  int msnum,mslen,vlen,delen,isf,bufpos;

  result = cgiFormStringMultiple(cgi,formvar->name,&vpp);
  if(result == cgiFormMemory)
    return -1;
  else if(result != cgiFormSuccess) 
  {
    if(!formvar->isnull) {cgiStringArrayFree(vpp);return -1;}
    if(formvar->defval) {
      form_put_values(formvar,formvar->defval,vars);
      form_put_names(formvar,vars);
    }
  } 
  else 
  {
    msnum = 0;
    mslen = 0;
    while(vpp[msnum]) {
      mslen += strlen(vpp[msnum]);
      msnum++;
    }
    
    if(!mslen) {
      if(!formvar->isnull) {cgiStringArrayFree(vpp);return -1;}
      if(formvar->defval) {
        form_put_values(formvar,formvar->defval,vars);
        form_put_names(formvar,vars);
      }
    }
    else 
    {
      delen = 0;
      if(formvar->delimiter) {
        delen = strlen(formvar->delimiter);
        mslen += (delen * msnum);
      }
      
      if(formvar->maxlen && mslen > formvar->maxlen) {
        cgiStringArrayFree(vpp);
        return -1;
      }
      
      if(mslen >= sizeof(buf))
        dbuf = (char *)xmalloc(mslen+1);
      else
        dbuf = buf;
      
      j = 0;
      bufpos = 0;
      isf = 1;
      while(vpp[j]) {
        vlen = strlen(vpp[j]);
        if(!vlen) {j++;continue;}
        if(isf) {
          memcpy(dbuf+bufpos,vpp[j],vlen);
          bufpos += vlen;
          isf = 0;
        } else {
          if(delen) {
            memcpy(dbuf+bufpos,formvar->delimiter,delen);
            bufpos += delen;
          }
          memcpy(dbuf+bufpos,vpp[j],vlen);
          bufpos += vlen;
        }
        j++;
      }
      dbuf[bufpos] = 0;
      form_put_values(formvar,formvar->defval,vars);
      form_put_names(formvar,vars);
      if(mslen >= sizeof(buf))
        xfree(dbuf);
    }
  }
  
  cgiStringArrayFree(vpp);
  return 0;
}

static int form_parse_select_single(CGI_T *cgi,formvar_t *formvar,const char *selected,TCMAP *vars)
{
  int i,result,choice,defaultv;
  char name[1024];

  defaultv = 0;
  if(formvar->defval) {
    for(i=0;i<formvar->choicenum;i++) {
      if(strcasecmp(formvar->defval,formvar->choices[i]) == 0) {
        defaultv = i;
        break;
      }
    }

    if(i == formvar->choicenum)
      return -1;
  }
  
  result = cgiFormSelectSingle(cgi,formvar->name,formvar->choices,  \
                               formvar->choicenum,&choice,defaultv);
  if(result != cgiFormSuccess) 
  {
    if(!formvar->isnull) 
      return -1;
    
    if(formvar->defval) {
      tcmapput2(vars,formvar->name,formvar->defval);
      snprintf(name,sizeof(name),"%s#%s",formvar->name,formvar->defval);
      tcmapput2(vars,name,selected);
    }
  } else {
    tcmapput2(vars,formvar->name,formvar->choices[choice]);
    snprintf(name,sizeof(name),"%s#%s",formvar->name,formvar->choices[choice]);
    tcmapput2(vars,name,selected);
  }

  return 0;
}

// varname#selectnum,varname#select0,1,2,3...,varname#value->checked="checked" or selected="selected"
static int form_parse_select_multi(CGI_T *cgi,formvar_t *formvar,const char *selected,TCMAP *vars)
{
  char name[1024],buf[1024];
  int i,j,result,*choices,invalid,choicenum;

  invalid = 0;
  choices = (int *)xcalloc(formvar->choicenum,sizeof(int));
  result = cgiFormSelectMultiple(cgi,formvar->name,formvar->choices, \
                                 formvar->choicenum,choices,&invalid);
  if(result != cgiFormSuccess) 
  {
    if(!formvar->isnull) {
      xfree(choices); 
      return -1;
    }
    
    if(formvar->defval) {
      char **frags;
      int fragnum;
      
      fragnum = strgetfrags(formvar->defval,",",&frags);
      snprintf(name,sizeof(name),"%s#selectnum",formvar->name);
      snprintf(buf,sizeof(buf),"%d",fragnum);
      tcmapput2(vars,name,buf);
      for(i=0;i<fragnum;i++) {
        snprintf(name,sizeof(name),"%s#select%d",formvar->name,i);
        tcmapput2(vars,name,frags[i]);
        snprintf(name,sizeof(name),"%s#%s",formvar->name,frags[i]);
        tcmapput2(vars,name,selected);
        xfree(frags[i]);
      }
      xfree(frags);
    }
  } 
  else 
  {
    choicenum = 0;
    for(i=0;i<formvar->choicenum;i++) {
      if(choices[i]) 
        choicenum++;
    }

    snprintf(name,sizeof(name),"%s#selectnum",formvar->name);
    snprintf(buf,sizeof(buf),"%d",choicenum);
    tcmapput2(vars,name,buf);

    j = 0;
    for(i=0;i<formvar->choicenum;i++) {
      if(!choices[i])
        continue;
      
      snprintf(name,sizeof(name),"%s#select%d",formvar->name,j);
      tcmapput2(vars,name,formvar->choices[i]);
      snprintf(name,sizeof(name),"%s#%s",formvar->name,formvar->choices[i]);
      tcmapput2(vars,name,selected);
      j++;
    }
  }
  
  xfree(choices);
  return 0;
}

static int form_parse_checkbox_single(CGI_T *cgi,formvar_t *formvar,const char *checked,TCMAP *vars)
{
  int result;

  result = cgiFormCheckboxSingle(cgi,formvar->name);
  if(result != cgiFormSuccess) {
   if(!formvar->isnull) 
    return -1;
   if(formvar->defval)
     tcmapput2(vars,formvar->name,formvar->defval);
  }
  else
   tcmapput2(vars,formvar->name,checked);

  return 0;
}

int form_upload(CGI_T *cgi,upload_file_t *file)
{
  cgiFilePtr fp;
  char filename[1024],contentype[1024];
  int result,filesize,got,readed;
  
  if(cgiFormFileName(cgi,file->varname,filename,sizeof(filename)) != cgiFormSuccess) 
    return -1;

  if(cgiFormFileSize(cgi,file->varname,&filesize) != cgiFormSuccess)
    return -1;

  if(filesize == 0)
    return -1;

  contentype[0] = 0;
  result = cgiFormFileContentType(cgi,file->varname,contentype,sizeof(contentype));
  if(result != cgiFormSuccess && result != cgiFormNoContentType) // content-type可能不存在
    return -1;
  
  if(cgiFormFileOpen(cgi,file->varname,&fp) != cgiFormSuccess)
    return -1;

  file->filebuf = (char *)xmalloc(filesize);
  readed = 0;
  while(readed != filesize)
  {
    if(cgiFormFileRead(fp,file->filebuf+readed, filesize-readed, &got) == cgiFormSuccess)
      readed += got;
    else
      break;
  }

  if(readed != filesize)
  {
    xfree(file->filebuf);
    return -1;
  }
  
  cgiFormFileClose(fp);

  file->filename = xstrdup(filename);
  if(contentype[0])
    file->contentype = xstrdup(contentype);
  else
    file->contentype = NULL;
  file->filesize = filesize;
  return 0;
}


static int form_parse_upload(CGI_T *cgi,formvar_t *formvar,TCMAP *vars)
{
  int result;
  upload_file_t file;
  char sizebuf[256],buf[1024];
  
  memset(&file,0,sizeof(upload_file_t));
  file.varname = formvar->name;
  result = form_upload(cgi,&file);
  if(result < 0) 
  {
    if(!formvar->isnull) 
      return -1;
  }
  else 
  {
    if(formvar->maxlen && file.filesize > formvar->maxlen) {
      xfree(file.filename);
      xfree(file.contentype);
      xfree(file.filebuf);
      return -1;
    }
    
    snprintf(buf,sizeof(buf),"%s#filename",formvar->name);
    tcmapput2(vars,buf,file.filename);
    if(file.contentype) {
      snprintf(buf,sizeof(buf),"%s#filetype",formvar->name);
      tcmapput2(vars,buf,file.contentype);
    }
    snprintf(buf,sizeof(buf),"%s#filesize",formvar->name);
    snprintf(sizebuf,sizeof(sizebuf),"%d",file.filesize);
    tcmapput2(vars,buf,sizebuf);
    tcmapput(vars,formvar->name,strlen(formvar->name),file.filebuf,file.filesize);
    xfree(file.filename);
    xfree(file.contentype);
    xfree(file.filebuf);
  }

  return 0;
}

static int form_parse_relvar(form_t *form,TCMAP *vars)
{
  int i,lo,hi;
  formvar_t *formvar;
  const char *cp;
  double flo,fhi;
  
  for(i=0;i<form->varnum;i++) 
  {
    formvar = form->vars[i];
    if(formvar->relvar) 
    {
      if(formvar->type != formvar->rvar->type)
        return -1;
      
      if(formvar->type != FORM_VAR_INT && formvar->type != FORM_VAR_FLOAT)
        return -1;
      
      cp = tcmapget2(vars,formvar->name);
      if(!cp) continue;
      else if(formvar->type == FORM_VAR_INT)
        lo = atoi(cp);
      else if(formvar->type == FORM_VAR_FLOAT)
        flo = atof(cp); 

      cp = tcmapget2(vars,formvar->rvar->name);
      if(!cp) 
        continue;
      else if(formvar->rvar->type == FORM_VAR_INT) {
        hi = atoi(cp);
        if(lo > hi) return -1;
      }
      else if(formvar->rvar->type == FORM_VAR_FLOAT) {
        fhi = atof(cp);
        if(flo > fhi) return -1;
      }
    }
  }

  return 0;
}

int form_parse(CGI_T *cgi,form_t *form,TCMAP *vars)
{
  formvar_t *formvar;
  int i,r;

  for(i=0;i<form->varnum;i++)
  {
    formvar = form->vars[i];
    switch(formvar->type)
    {
      case FORM_VAR_INT:
        r = form_parse_int(cgi,formvar,vars);
        break;
      case FORM_VAR_FLOAT:
        r = form_parse_float(cgi,formvar,vars);
        break;
      case FORM_VAR_STRING:
        r = form_parse_string(cgi,formvar,vars);
        break;
      case FORM_VAR_STRING_NONL:
        r = form_parse_string_nonl(cgi,formvar,vars);
        break;
      case FORM_VAR_STRING_MULTI:
        r = form_parse_string_multi(cgi,formvar,vars);
        break;
      case FORM_VAR_SELECT_SINGLE:
        r = form_parse_select_single(cgi,formvar,"selected=\"selected\"",vars);
        break;
      case FORM_VAR_RADIO:
        r = form_parse_select_single(cgi,formvar,"checked=\"checked\"",vars);
        break;
      case FORM_VAR_SELECT_MULTIPLE:
        r = form_parse_select_multi(cgi,formvar,"selected=\"selected\"",vars);
        break;
      case FORM_VAR_CHECKBOX_MULTIPLE:
        r = form_parse_select_multi(cgi,formvar,"checked=\"checked\"",vars);
        break;
      case FORM_VAR_SUBMIT:
        r = form_parse_checkbox_single(cgi,formvar,"1",vars);
        break;
      case FORM_VAR_CHECKBOX_SINGLE:
        r = form_parse_checkbox_single(cgi,formvar,"checked=\"chekced\"",vars);
        break;
      case FORM_VAR_UPLOAD:
        r = form_parse_upload(cgi,formvar,vars);
        break;
      default:
        return -1;
    }

    if(r < 0)
      return -1;
  }

  if(form_parse_relvar(form,vars) < 0)
    return -1;

  if(form->proccfg && fp_exec2(form->fpargs,vars) != 0) {
    const char *errmsg;
    errmsg = tcmapget2(vars,FUNC_RESULT);
    write_log_r(LLERROR,"form process error: %s", errmsg?errmsg:"\0");
    return -1;
  }
  
  return 0;
}

// @return: error: -1, ok: 0, too large: 1
int form_check(form_t *form,const char *method,const char *enctype,int contentlen)
{
  if(!method || !method[0])
    return -1;

  if(strcasecmp(method,"post") == 0 && (!enctype || !enctype[0]))
    return -1;

  if(strcasecmp(form->method,"both") != 0 && strcasecmp(form->method,method) != 0) {
    return -1;
  }

  if(strcasecmp(method,"post") == 0) {
    if(strcasecmp(form->enctype,"both") != 0 && strcasecmp(form->enctype,enctype) != 0)
      return -1;

    if(form->postlimit && contentlen > form->postlimit)
      return 1; // post数据过大，返回1
  }

  return 0;
}

#ifdef TEST_FORM

int main()
{
  formvarset_t formvarset;
  formset_t formset;

  if(form_init("../etc/wbe.conf","web",&formvarset,&formset) < 0) {
    printf("form_init() error.\n");
    return -1;
  }

  form_free(&formvarset,&formset);
  return 0;
}

#endif

