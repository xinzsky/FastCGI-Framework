/* The CGI_C library, by Thomas Boutell, version 2.01. CGI_C is intended
	to be a high-quality API to simplify CGI programming tasks. */

/* Make sure this is only included once. */

#ifndef CGI_C
#define CGI_C 1

/* Bring in standard I/O since some of the functions refer to
	types defined by it, such as FILE *. */

#include <stdio.h>
#include "fcgi_config.h"
#include "fcgiapp.h"

typedef struct cgiFormEntryStruct cgiFormEntry;

// 可以直接从CGI_T结构体中读取环境变量，不需要调用FCGX_GetParam()
typedef struct {
  char *cgiServerSoftware;
  char *cgiServerName;
  char *cgiGatewayInterface;
  char *cgiServerProtocol;
  char *cgiServerPort;
  char *cgiRequestMethod;
  char *cgiPathInfo;
  char *cgiPathTranslated;
  char *cgiScriptName;
  char *cgiQueryString;
  char *cgiRemoteHost;
  char *cgiRemoteAddr;
  char *cgiRemotePort;
  char *cgiAuthType;
  char *cgiRemoteUser;
  char *cgiRemoteIdent;
  char cgiContentTypeData[1024];
  char *cgiContentType;
  char *cgiMultipartBoundary;
  char *cgiCookie;
  int cgiContentLength;
  char *cgiAccept;
  char *cgiUserAgent;
  char *cgiReferrer;
  char *cgiPostContent;

  /* Internal used. */
  FILE *cgiIn;           /* used for cgi */
  FILE *cgiOut;          /* used for cgi */
  FCGX_Request *request; /* used for fcgi */
  
  int cgiRestored;                 /* True if CGI environment was restored from a file. */
  cgiFormEntry *cgiFormEntryFirst; /* The first form entry. */
  int cgiHexValue[256];
  char *cgiFindTarget;
  cgiFormEntry *cgiFindPos;
}CGI_T;


/* A macro providing the same incorrect spelling that is
	found in the HTTP/CGI specifications */
#define cgiReferer cgiReferrer

/* Possible return codes from the cgiForm family of functions (see below). */

typedef enum {
	cgiFormSuccess,
	cgiFormTruncated,
	cgiFormBadType,
	cgiFormEmpty,
	cgiFormNotFound,
	cgiFormConstrained,
	cgiFormNoSuchChoice,
	cgiFormMemory,
	cgiFormNoFileName,
	cgiFormNoContentType,
	cgiFormNotAFile,
	cgiFormOpenFailed,
	cgiFormIO,
	cgiFormEOF
} cgiFormResultType;


/* These functions are used to retrieve form data. See
	cgic.html for documentation. */

#ifdef __cplusplus
extern "C" {
#endif

extern CGI_T *cgiNew(void);
extern void cgiDel(CGI_T *cgi);
extern int cgiInit(CGI_T *cgi,void *request);
extern void cgiFree(CGI_T *cgi);

extern cgiFormResultType cgiFormString(CGI_T *cgi,
	char *name, char *result, int max);

extern cgiFormResultType cgiFormStringNoNewlines(CGI_T *cgi,
	char *name, char *result, int max);


extern cgiFormResultType cgiFormStringSpaceNeeded(CGI_T *cgi,
	char *name, int *length);


extern cgiFormResultType cgiFormStringMultiple(CGI_T *cgi,
	char *name, char ***ptrToStringArray);

extern void cgiStringArrayFree(char **stringArray);

extern cgiFormResultType cgiFormInteger(CGI_T *cgi,
	char *name, int *result, int defaultV);

extern cgiFormResultType cgiFormIntegerBounded(CGI_T *cgi,
	char *name, int *result, int min, int max, int defaultV);

extern cgiFormResultType cgiFormDouble(CGI_T *cgi,
	char *name, double *result, double defaultV);

extern cgiFormResultType cgiFormDoubleBounded(CGI_T *cgi,
	char *name, double *result, double min, double max, double defaultV);

extern cgiFormResultType cgiFormSelectSingle(CGI_T *cgi,
	char *name, char **choicesText, int choicesTotal, 
	int *result, int defaultV);	


extern cgiFormResultType cgiFormSelectMultiple(CGI_T *cgi,
	char *name, char **choicesText, int choicesTotal, 
	int *result, int *invalid);

/* Just an alias; users have asked for this */
#define cgiFormSubmitClicked cgiFormCheckboxSingle

extern cgiFormResultType cgiFormCheckboxSingle(CGI_T *cgi,
	char *name);

extern cgiFormResultType cgiFormCheckboxMultiple(CGI_T *cgi,
	char *name, char **valuesText, int valuesTotal, 
	int *result, int *invalid);

extern cgiFormResultType cgiFormRadio(CGI_T *cgi,
	char *name, char **valuesText, int valuesTotal, 
	int *result, int defaultV);	

/* The paths returned by this function are the original names of files
	as reported by the uploading web browser and shoult NOT be
	blindly assumed to be "safe" names for server-side use! */
extern cgiFormResultType cgiFormFileName(CGI_T *cgi,
	char *name, char *result, int max);

/* The content type of the uploaded file, as reported by the browser.
	It should NOT be assumed that browsers will never falsify
	such information. */
extern cgiFormResultType cgiFormFileContentType(CGI_T *cgi,
	char *name, char *result, int max);

extern cgiFormResultType cgiFormFileSize(CGI_T *cgi,
	char *name, int *sizeP);

typedef struct cgiFileStruct *cgiFilePtr;

extern cgiFormResultType cgiFormFileOpen(CGI_T *cgi,
	char *name, cgiFilePtr *cfpp);

extern cgiFormResultType cgiFormFileRead(
	cgiFilePtr cfp, char *buffer, int bufferSize, int *gotP);

extern cgiFormResultType cgiFormFileClose(
	cgiFilePtr cfp);

extern cgiFormResultType cgiCookieString(CGI_T *cgi,
	char *name, char *result, int max);

extern cgiFormResultType cgiCookieInteger(CGI_T *cgi,
	char *name, int *result, int defaultV);

cgiFormResultType cgiCookies(CGI_T *cgi,
	char ***ptrToStringArray);

/* path can be null or empty in which case a path of / (entire site) is set. 
	domain can be a single web site; if it is an entire domain, such as
	'boutell.com', it should begin with a dot: '.boutell.com' */
extern void cgiHeaderCookieSetString(CGI_T *cgi,char *name, char *value, 
	int secondsToLive, char *path, char *domain,int secure,int httponly);
extern void cgiHeaderCookieSetInteger(CGI_T *cgi,char *name, int value,
	int secondsToLive, char *path, char *domain,int secure,int httponly);
extern void cgiHeaderLocation(CGI_T *cgi,char *redirectUrl);
extern void cgiHeaderStatus(CGI_T *cgi,int status, char *statusMessage);
//extern void cgiHeaderContentType(CGI_T *cgi,char *mimeType);

extern void cgiOutputHeader(CGI_T *cgi,char *name,char *value);
extern void cgiHeaderP3P(CGI_T *cgi,char *policyref,char *cp);
extern void cgiHeaderLastModified(CGI_T *cgi,int lastmodified);
extern void cgiHeaderCachePragma(CGI_T *cgi);
extern void cgiHeaderCacheExpire(CGI_T *cgi,int secondsToLive);

enum {
  CACHE_TYPE_NONE = 0,
  CACHE_TYPE_PUBLIC = 1,
  CACHE_TYPE_PRIVATE,
  CACHE_TYPE_PRIVATENOEXPIRE,
  CACHE_TYPE_NOCACHE
};
extern void cgiHeaderCacheControl(CGI_T *cgi,int type,int expiretime);
extern void cgiHeaderContentLength(CGI_T *cgi,int contentlen);
extern void cgiHeaderContentType(CGI_T *cgi,char *mimeType,char *charset);
extern int cgiOutputContent(CGI_T *cgi,char *content,int contentlen);


typedef enum {
	cgiEnvironmentIO,
	cgiEnvironmentMemory,
	cgiEnvironmentSuccess,
	cgiEnvironmentWrongVersion
} cgiEnvironmentResultType;

extern cgiEnvironmentResultType cgiWriteEnvironment(CGI_T *cgi,char *filename);
extern cgiEnvironmentResultType cgiReadEnvironment(CGI_T *cgi,char *filename);

extern cgiFormResultType cgiFormEntries(CGI_T *cgi,
	char ***ptrToStringArray);

/* Output string with the <, &, and > characters HTML-escaped. 
	's' is null-terminated. Returns cgiFormIO in the event
	of error, cgiFormSuccess otherwise. */
cgiFormResultType cgiHtmlEscape(CGI_T *cgi,char *s);

/* Output data with the <, &, and > characters HTML-escaped. 
	'data' is not null-terminated; 'len' is the number of
	bytes in 'data'. Returns cgiFormIO in the event
	of error, cgiFormSuccess otherwise. */
cgiFormResultType cgiHtmlEscapeData(CGI_T *cgi,char *data, int len);

/* Output string with the " character HTML-escaped, and no
	other characters escaped. This is useful when outputting
	the contents of a tag attribute such as 'href' or 'src'.
	's' is null-terminated. Returns cgiFormIO in the event
	of error, cgiFormSuccess otherwise. */
cgiFormResultType cgiValueEscape(CGI_T *cgi,char *s);

/* Output data with the " character HTML-escaped, and no
	other characters escaped. This is useful when outputting
	the contents of a tag attribute such as 'href' or 'src'.
	'data' is not null-terminated; 'len' is the number of
	bytes in 'data'. Returns cgiFormIO in the event
	of error, cgiFormSuccess otherwise. */
cgiFormResultType cgiValueEscapeData(CGI_T *cgi,char *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* CGI_C */

