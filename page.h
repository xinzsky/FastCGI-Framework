#ifndef  PAGE_H
#define  PAGE_H

#include "tcutil.h"

enum {
  FIELD_TYPE_NORMAL,
  FIELD_TYPE_RECID,
  FIELD_TYPE_TITLE,
  FIELD_TYPE_URL,         // from url, used for snapshot
  FIELD_TYPE_RAW,         // raw page, used for snapshot
  FIELD_TYPE_MULTIFRAG    // eg. tags or category
};

enum {
  DIV_TYPE_UNKNOW,    // 未知
  DIV_TYPE_MAIN,      // 主体内容
  DIV_TYPE_RAND,      // 随机
  DIV_TYPE_HOT,       // 热门
  DIV_TYPE_LATEST,    // 最新
  DIV_TYPE_RELATIVE,  // 相关
  DIV_TYPE_HEADWORD,  // 中心词
  DIV_TYPE_TAGS,      // Tags
  DIV_TYPE_OTHER      // 其他
};

enum {
  PAGE_TYPE_UNKNOW, 
  PAGE_TYPE_INDEX,      // 网站或频道首页
  PAGE_TYPE_BROWSE,     // 浏览页面
  PAGE_TYPE_SEARCH,     // 搜索结果页面
  PAGE_TYPE_CONTENT,    // 内容页面
  PAGE_TYPE_SNAPSHOT    // 快照页面
};

enum {
  ENCODE_TYPE_NONE,
  ENCODE_TYPE_URL = 1,
  ENCODE_TYPE_HTML,
  ENCODE_TYPE_HTML2,
  ENCODE_TYPE_BASE64,
  ENCODE_TYPE_MD5
};

// static url format: xxxx/name_value-name_value/
// value: base64 encode
// dynamic url format: xxxx?name=value&name=value
// value: url encode

enum {
  URL_TYPE_STATIC,
  URL_TYPE_DYNAMIC
};

#define STATIC_URL_SEPARATOR       '-'
#define STATIC_URL_DELIMITER       '_'

#define DYNAMIC_URL_SEPARATOR      '&'
#define DYNAMIC_URL_DELIMITER      '='

// page structure define.
// tv: template variable, tb: template block

typedef struct {
  int   type;       // 字段类型
  char *name;       // 字段名 
  char *ifout;      // 字段值输出的条件，条件为空表示无条件输出
  char *title;      // 字段标题
  int   isnav;      // 字段是否加入到导航目录中
  int   isnull;     // 字段值是否允许为空
  char *defval;     // 字段默认值，当字段值为空则采用默认值
  int   maxlen;     // 字段值最大长度，超过则截取并添加"..."
  int   encode;     // 字段值/片段值需要进行的编码类型,如果是多片段字段，则指明片段的编码类型
  
  char *tv_name;    // 字段名对于模板变量
  char *tv_value;   // 字段值/片段值对应模板变量
  char *tv_title;   // 字段标题对应模板变量
  char *tv_cut;     // 截取后字段值对应的模板变量，用于标题作为anchor text时
  char *tv_encode;  // 字段值/片段值encode对应的模板变量，一般用于URL: <a href="/tv_encode" title="tv_value">tv_cut</a>
  char *tb_field;   // 字段对应模板块，比如一个记录的多个字段采用相同模板变量时就需要使用模板块

  // used for multi-fragment field
  char *delimiter;  // 字段中片段之间的分隔符
  char *separator;  // 页面展示时内容之间的分隔符
  int   fragnum;    // 总共展示片段数
  int   numinline;  // 一行展示片段数
  char *linestart;
  char *lineclose;
  char *tv_sep;  
  char *tv_linestart;
  char *tv_lineclose;
  char *tb_frag;
  char *tb_value;
}pg_field_t;

typedef struct {  
  char *title;      // 目录标题
  char *tv_title;   // 目录标题对应的模板变量
  char *tv_itemname;
  char *tv_itemvalue;
  char *tb_item;    // 目录项对应的模板块
  char *tb_dir;     // 目录对应的模板块
}pg_dir_t;

typedef struct {
  int         fieldnum;  
  pg_field_t *field;
  pg_dir_t    dir;
  char       *tb_record; // 记录对应模板块，一个记录就是一个块的实例
}pg_record_t;

typedef struct {
  int   rndef;           // 每页默认显示结果数
  char *rnvar;           // result number对应的表单变量,eg. "rn"
  char *snvar;           // 开始结果号(start number)对应的表单变量,eg. "sn"
  int  maxretnum;        // 最大返回结果数
  int   urltype;         // 0: static,1:dynamic
  char *urlprefix;       // prefix/name_value-name_value/ or prefix?name=value&name=value
  int   urlvarnum;
  char **urlvars;        // URL需要用到的变量，这些变量值都可以从系统变量中获得,不要设置sn
  int   linknum;         // 分页显示链接数
  char *prevnext[2];     // 上一页:下一页   
  char *firstlast[2];    // 首页:尾页，如果没有设置则不显示"首页""尾页"
  char *curattr;         // 当前页显示属性，eg. <a href="/a" class="on">1</a> curattr=class="on"
  char *tv_paging;       // 模板变量
  char *tb_paging;       // 模板块
}pg_paging_t;

typedef struct {
  int stmtnum;
  char **stmts;
  int *inmqs;             // 该语句是否在MQS中。
  char **tvs;             // 该语句执行结果对应的模板变量。
}dbc_t;

typedef struct {
  char *name;            // 版块名
  int   type;            // 版块类型
  char *title;           // 版块标题
  char *tv_title;        // 版块标题在模板中对应的变量
  dbc_t  dbc;            // 数据库查询语句配置,最多一个read语句，可以有多个write count语句。
  pg_record_t record;    // 记录配置信息(用于read语句)
  pg_paging_t paging;    // 版块是否需要分页(用于read语句)
  char *tv_error;        // 错误信息(版块创建时发生了错误)
  char **booltext;       // 布尔值的文字说明，比如: 成功，失败
  char **queryempty;     // 查询结果内容为空时的提示。
  char *tb_div;          // 版块对应模板块，当没有记录时整个版块都不显示，包括版块标题
  unsigned int  randmod; // 随机展示版块取模
  char *tv_hlpos;        // 快照中高亮定位模板变量
  char *hlwords;         // 快照中高亮关键词查询变量
}pg_div_t;

typedef struct {
  int divnum;
  pg_div_t *divs;
}pg_divset_t;

typedef struct {
  char *name;            // 模板文件名，在template目录下。
  char *content;
  int size;
}pg_tpt_t;

typedef struct {
  int stmtnum;
  char **stmts;
  char **divs;
}mqs_t;


typedef struct {  
  char *name;
  int type;
  
  int tvbfnum;                     
  char **tv_before;    // 生成版块之前需要设置的预定义模板变量
  
  int divnum;
  char **divnames;      
  pg_div_t **divs;
  
  int tvafnum;
  char **tv_after;     // 生成版块之后需要设置的预定义模板变量

  char *tptdir;         // 模板目录
  int tptnum;           // 模板数
  pg_tpt_t *tpts;       // 一个页面可以搭配多个模板
  char *deftpt;         // 默认使用哪个模板
  char *tptvar;         // 模板变量名,eg. "tn"

  int mqsnum;           // multi-queries statement
  mqs_t *mqs;

  int clausenum;        // 页面中子句数
  char **clauses;
}pg_page_t;

typedef struct {
  int pagenum;
  pg_page_t *pages;
}pg_pageset_t;

#ifdef __cplusplus
extern "C" {
#endif

 
  int page_init(const char *confpath,const char *sectname,pg_divset_t *divset,pg_pageset_t *pageset,const char *home); 
  void page_free(pg_divset_t *divset,pg_pageset_t *pageset);
  int page_makepaging(int restotal,int start,int resnum,
                 int linknum,char **prevnext,char **firstlast,const char *pageurl,const char *snargs,
                 const char *curattr,char *outbuf,int bufsize);
  int page_makepagingbar(void *htmtmpl,pg_div_t *div,TCMAP *sysvars,int stmtindex);  
  int page_makefragment(void *htmtmpl,pg_field_t *field,const char *fieldvalue);
  int page_makefield(void *htmtmpl,pg_field_t *field,const char *fieldvalue);
  int page_makerecord(void *htmtmpl,pg_record_t *record,char **fieldvalues);
  int page_makediv(void *htmtmpl,pg_div_t *div,TCMAP *sysvars);
  int page_make(pg_page_t *page,char *pagebuf,int bufsize,TCMAP *sysvars);

#ifdef __cplusplus
}
#endif

#endif

