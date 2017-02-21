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
#include "html.h"

#define MAX_WORDS_LEN       1024
#define MAX_HLIGHT_LEN      256
#define MAX_HLIGHT_BUFSIZE  (MAX_HLIGHT_LEN * MAX_HLIGHT_KW)
#define MAX_POS_LEN         64
#define MAX_POS_BUFSIZE     (MAX_POS_LEN * MAX_HLIGHT_KW)

static const char *color_table[MAX_HLIGHT_KW] =
{
  "#ffff00",          // Yellow
  "#00ff00",          // Lime
  "#00ffff",          // Aqua
  "#ff00ff" ,         // Fuchsia
  "#ff0000" ,         // red
  "#0000ff",          // Blue
  "#008000",          // Green
  "#800080"           // Purple
};

// 功能: 给页面关键词增加高亮和定义信息，并且返回关键词高亮和定位条
// @newpagesize,@barsize是输入输出参数。
int snap_makehighlightpos(const char *page,int pagsize,
                                       const char *keywords,const char *url,
                                       char *newpage,int *newpagesize,
                                       char *keywordsbar,int *barsize)
{
  int i,len,wordnum,barpos;
  char *words[MAX_HLIGHT_KW];
  char wordbuf[MAX_WORDS_LEN];
  char *hlight[MAX_HLIGHT_KW];
  char hlightbuf[MAX_HLIGHT_BUFSIZE];
  char *pos[MAX_HLIGHT_KW];
  char posbuf[MAX_POS_BUFSIZE];

  for (i = 0;i < MAX_HLIGHT_KW;i++)
    hlight[i] = hlightbuf + MAX_HLIGHT_LEN * i;

  for (i = 0;i < MAX_HLIGHT_KW;i++)
    pos[i] = posbuf + MAX_POS_LEN * i;

  if(strlen(keywords) >= MAX_WORDS_LEN)
    return -1;
  strcpy(wordbuf,keywords);
  wordnum = split(wordbuf, ",",words, MAX_HLIGHT_KW);
  if(wordnum <= 0)
    return -1;

  barpos = 0;
  for (i = 0;i < wordnum;i++) {
   len = snprintf(hlight[i], MAX_HLIGHT_LEN, "<span style=\"background:%s;color:black;font-weight:bold\">%s</span>", color_table[i], words[i]);
   if (len <= 0 || len >= MAX_HLIGHT_LEN) 
    return -1;
   len = snprintf(pos[i], MAX_POS_LEN, "<a name=\"gksnapshot%d\"></a>", i);
   len = snprintf(keywordsbar+ barpos, *barsize-barpos, " <a href=\"%s#gksnapshot%d\">%s  </a>", url, i, hlight[i]);
   if (len <= 0 || len >= *barsize-barpos) 
    return -1;
   barpos += len;
  }
  *barsize = barpos;

  len = html_highlight_position((char *)page, pagsize, words , hlight, pos, wordnum, newpage, *newpagesize, NULL, 0);
  if (len <= 0)
    return -1;
  else
    *newpagesize = len;

  return 0;
}


