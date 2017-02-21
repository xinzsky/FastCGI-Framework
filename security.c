/*
 +----------------------------------------------------------------------+
 | Author: Xingzhi Liu  <dudubird2006@163.com>                          |
 +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmalloc.h"
#include "xstring.h"
#include "charset.h"
#include "html.h"

// 保留的标签以及相应属性tags格式为: reservetags#reserveattrs
// tag,tag,...#attr:tag,... 
// 参数tags可以为空，这样就过滤掉所有标签。
// maxtextsize: 允许的最大文本长度
void *xss_init(const char *tags,int maxtextsize)
{
  char *p,*rattrs,*rtags;
  void *xssdata;

  if(maxtextsize <= 0)
    return NULL;

  rtags = NULL;
  rattrs = NULL;
  if(tags) {
    p = strchr(tags,'#');
    if(p) {
      rtags = xstrdupdelim(tags,p);
      rattrs = p + 1;
    } else {
      rtags = (char *)tags;
      rattrs = NULL;
    }
  }

  xssdata = html_filtertag_init(rtags,NULL,rattrs,maxtextsize,1);

  if(rtags && rtags != tags)
    xfree(rtags);

  return xssdata;
}

const char *xss_clean(void *xssdata,const char *html,int htmllen,int *sp)
{
  return html_filtertags(xssdata,html,htmllen,CODETYPE_UTF8,sp);
}

void xss_free(void *xssdata)
{
  html_filtertag_free(xssdata);
}

// 注意返回结果需要free。
char *xss_filter(const char *tags, const char * html, int htmllen, int maxtextsize)
{
  void *xssdata;
  char *r;
  const char *p;
  int sp;

  xssdata = xss_init(tags, maxtextsize);
  if(!xssdata)
    return NULL;

  r = NULL;
  p = xss_clean(xssdata,html,htmllen,&sp);
  if(p)
    r = xstrdup(p);

  xss_free(xssdata);
  return r;
}

#ifdef TEST_SECURE

  int main()
  {
    char buf[1024],*r;
    const char *tags="img,a,br,p#src:img,alt:img,href:a";

    while(fgets(buf,sizeof(buf),stdin)) {
      trim(buf);
      if(!buf[0]) continue;
      if(strcasecmp(buf,"exit") == 0) break;
      r = xss_filter(tags,buf,strlen(buf),sizeof(buf));
      if(r)
        printf("XSSFilter: %s\n",r);
      xfree(r);
    }
    
    return 0;
  }

#endif

