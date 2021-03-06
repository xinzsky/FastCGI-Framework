/* cgicTempDir is the only setting you are likely to need
	to change in this file. */

/* Used only in Unix environments, in conjunction with mkstemp(). 
	Elsewhere (Windows), temporary files go where the tmpnam() 
	function suggests. If this behavior does not work for you, 
	modify the getTempFileName() function to suit your needs. */

#define cgicTempDir "/tmp"

#if CGICDEBUG
#define CGICDEBUGSTART \
	{ \
		FILE *dout; \
		dout = fopen("/home/boutell/public_html/debug", "a"); \
	
#define CGICDEBUGEND \
		fclose(dout); \
	}
#else /* CGICDEBUG */
#define CGICDEBUGSTART
#define CGICDEBUGEND
#endif /* CGICDEBUG */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <io.h>

/* cgic 2.01 */
#include <fcntl.h>

#else
#include <unistd.h>
#endif /* WIN32 */
#include "cgic.h"

#define cgiStrEq(a, b) (!strcmp((a), (b)))

typedef enum {
	cgiParseSuccess,
	cgiParseMemory,
	cgiParseIO
} cgiParseResultType;

/* One form entry, consisting of an attribute-value pair,
	and an optional filename and content type. All of
	these are guaranteed to be valid null-terminated strings,
	which will be of length zero in the event that the
	field is not present, with the exception of tfileName
	which will be null when 'in' is null. DO NOT MODIFY THESE 
	VALUES. Make local copies if modifications are desired. */

struct cgiFormEntryStruct {
        char *attr;
	/* value is populated for regular form fields only.
		For file uploads, it points to an empty string, and file
		upload data should be read from the file tfileName. */ 
	char *value;
	/* When fileName is not an empty string, tfileName is not null,
		and 'value' points to an empty string. */
	/* Valid for both files and regular fields; does not include
		terminating null of regular fields. */
	int valueLength;
	char *fileName;	
	char *contentType;
	/* Temporary file name for working storage of file uploads. */
	char *tfileName;
        struct cgiFormEntryStruct *next;
} ;

static void cgiGetenv(CGI_T *cgi,char **s, char *var);
static cgiParseResultType cgiParseGetFormInput(CGI_T *cgi);
static cgiParseResultType cgiParsePostFormInput(CGI_T *cgi);
static cgiParseResultType cgiParsePostMultipartInput(CGI_T *cgi);
static cgiParseResultType cgiParseFormInput(CGI_T *cgi,char *data, int length);
static void cgiSetupConstants(CGI_T *cgi);
static void cgiFreeResources(CGI_T *cgi);
static int cgiStrEqNc(char *s1, char *s2);
static int cgiStrBeginsNc(char *s1, char *s2);
int action_check(void *checkargs,const char * cgiScriptName,const char *method,const char *enctype,int contentlen);

CGI_T *cgiNew(void)
{
  CGI_T *cgi;

  cgi = (CGI_T *)calloc(1,sizeof(CGI_T));
  if(!cgi)
    return NULL;
  
  return cgi;
}

void cgiDel(CGI_T *cgi)
{
  free(cgi);
}


int cgiInit(CGI_T *cgi,void *request)
{
	char *cgiContentLengthString;
	char *e;

  memset(cgi,0,sizeof(CGI_T));
  cgi->cgiContentType = cgi->cgiContentTypeData;
  cgi->cgiRestored = 0; 
  cgi->cgiFindTarget = NULL;
  cgi->cgiFindPos = NULL;
  cgi->request = (FCGX_Request *)request;

  cgiSetupConstants(cgi);
	cgiGetenv(cgi,&cgi->cgiServerSoftware, "SERVER_SOFTWARE");
	cgiGetenv(cgi,&cgi->cgiServerName, "SERVER_NAME");
	cgiGetenv(cgi,&cgi->cgiGatewayInterface, "GATEWAY_INTERFACE");
	cgiGetenv(cgi,&cgi->cgiServerProtocol, "SERVER_PROTOCOL");
	cgiGetenv(cgi,&cgi->cgiServerPort, "SERVER_PORT");
	cgiGetenv(cgi,&cgi->cgiRequestMethod, "REQUEST_METHOD");
	cgiGetenv(cgi,&cgi->cgiPathInfo, "PATH_INFO");
	cgiGetenv(cgi,&cgi->cgiPathTranslated, "PATH_TRANSLATED");
	cgiGetenv(cgi,&cgi->cgiScriptName, "SCRIPT_NAME");
	cgiGetenv(cgi,&cgi->cgiQueryString, "QUERY_STRING");
	cgiGetenv(cgi,&cgi->cgiRemoteHost, "REMOTE_HOST");
	cgiGetenv(cgi,&cgi->cgiRemoteAddr, "REMOTE_ADDR");
  cgiGetenv(cgi,&cgi->cgiRemotePort, "REMOTE_PORT");
	cgiGetenv(cgi,&cgi->cgiAuthType, "AUTH_TYPE");
	cgiGetenv(cgi,&cgi->cgiRemoteUser, "REMOTE_USER");
	cgiGetenv(cgi,&cgi->cgiRemoteIdent, "REMOTE_IDENT");
	/* 2.0: the content type string needs to be parsed and modified, so
		copy it to a buffer. */
	e = FCGX_GetParam("CONTENT_TYPE",cgi->request->envp);
	if (e) {
		if (strlen(e) < sizeof(cgi->cgiContentTypeData)) {
			strcpy(cgi->cgiContentType, e);
		} else {
			/* Truncate safely in the event of what is almost certainly
				a hack attempt */
			strncpy(cgi->cgiContentType, e, sizeof(cgi->cgiContentTypeData));
			cgi->cgiContentType[sizeof(cgi->cgiContentTypeData) - 1] = '\0';
		}
	} else {
		cgi->cgiContentType[0] = '\0';
	}
	/* Never null */
	cgi->cgiMultipartBoundary = "";
	/* 2.0: parse semicolon-separated additional parameters of the
		content type. The one we're interested in is 'boundary'.
		We discard the rest to make cgiContentType more useful
		to the typical programmer. */
	if (strchr(cgi->cgiContentType, ';')) {
		char *sat = strchr(cgi->cgiContentType, ';');
		while (sat) {
			*sat = '\0';
			sat++;
			while (isspace(*sat)) {
				sat++;
			}	
			if (cgiStrBeginsNc(sat, "boundary=")) {
				char *s;
				cgi->cgiMultipartBoundary = sat + strlen("boundary=");
				s = cgi->cgiMultipartBoundary;
				while ((*s) && (!isspace(*s))) {
					s++;
				}
				*s = '\0';
				break;
			} else {
				sat = strchr(sat, ';');
			} 	
		}
	}
	cgiGetenv(cgi,&cgiContentLengthString, "CONTENT_LENGTH");
	cgi->cgiContentLength = atoi(cgiContentLengthString);	
	cgiGetenv(cgi,&cgi->cgiAccept, "HTTP_ACCEPT");
	cgiGetenv(cgi,&cgi->cgiUserAgent, "HTTP_USER_AGENT");
	cgiGetenv(cgi,&cgi->cgiReferrer, "HTTP_REFERER");
	cgiGetenv(cgi,&cgi->cgiCookie, "HTTP_COOKIE");
#ifdef CGICDEBUG
	CGICDEBUGSTART
	fprintf(dout, "%d\n", cgi->cgiContentLength);
	fprintf(dout, "%s\n", cgi->cgiRequestMethod);
	fprintf(dout, "%s\n", cgi->cgiContentType);
	CGICDEBUGEND	
#endif /* CGICDEBUG */
#ifdef WIN32
	/* 1.07: Must set stdin and stdout to binary mode */
	/* 2.0: this is particularly crucial now and must not be removed */
	_setmode( _fileno( stdin ), _O_BINARY );
	_setmode( _fileno( stdout ), _O_BINARY );
#endif /* WIN32 */
	cgi->cgiFormEntryFirst = 0;
	cgi->cgiIn = stdin;
	cgi->cgiOut = stdout;
	cgi->cgiRestored = 0;

	if (cgiStrEqNc(cgi->cgiRequestMethod, "post")) {      
#ifdef CGICDEBUG
		CGICDEBUGSTART
		fprintf(dout, "POST recognized\n");
		CGICDEBUGEND
#endif /* CGICDEBUG */
		if (cgiStrEqNc(cgi->cgiContentType, "application/x-www-form-urlencoded")) {	
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "Calling PostFormInput\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
			if (cgiParsePostFormInput(cgi) != cgiParseSuccess) {
#ifdef CGICDEBUG
				CGICDEBUGSTART
				fprintf(dout, "PostFormInput failed\n");
				CGICDEBUGEND	
#endif /* CGICDEBUG */
				cgiFreeResources(cgi);
				return -1;
			}	
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "PostFormInput succeeded\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
		} else if (cgiStrEqNc(cgi->cgiContentType, "multipart/form-data")) {
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "Calling PostMultipartInput\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
			if (cgiParsePostMultipartInput(cgi) != cgiParseSuccess) {
#ifdef CGICDEBUG
				CGICDEBUGSTART
				fprintf(dout, "PostMultipartInput failed\n");
				CGICDEBUGEND	
#endif /* CGICDEBUG */
				cgiFreeResources(cgi);
				return -1;
			}	
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "PostMultipartInput succeeded\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
		}
	} else if (cgiStrEqNc(cgi->cgiRequestMethod, "get")) {	
		/* The spec says this should be taken care of by
			the server, but... it isn't */
		cgi->cgiContentLength = strlen(cgi->cgiQueryString);
		if (cgiParseGetFormInput(cgi) != cgiParseSuccess) {
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "GetFormInput failed\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
			cgiFreeResources(cgi);
			return -1;
		} else {	
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "GetFormInput succeeded\n");
			CGICDEBUGEND	
#endif /* CGICDEBUG */
		}
	}
  
	return 0;
}

void cgiFree(CGI_T *cgi)
{
  if(cgi->cgiPostContent)
    free(cgi->cgiPostContent);
  cgiFreeResources(cgi);
}

static void cgiGetenv(CGI_T *cgi,char **s, char *var) {
  *s = FCGX_GetParam(var,cgi->request->envp);
	if (!(*s)) {
		*s = "";
	}
}

static cgiParseResultType cgiParsePostFormInput(CGI_T *cgi) {
	char *input;
	cgiParseResultType result;
	if (!cgi->cgiContentLength) {
		return cgiParseSuccess;
	}
	input = (char *) malloc(cgi->cgiContentLength+1);
	if (!input) {
		return cgiParseMemory;	
	}
	if ((FCGX_GetStr(input,cgi->cgiContentLength,cgi->request->in)) != cgi->cgiContentLength)
	{
    free(input);
		return cgiParseIO;
	}	
  input[cgi->cgiContentLength] = 0; 
	result = cgiParseFormInput(cgi,input, cgi->cgiContentLength);
  if(result != cgiParseSuccess)
	  free(input);
  else
	  cgi->cgiPostContent = input;
	return result;
}

/* 2.0: A virtual datastream supporting putback of 
	enough characters to handle multipart boundaries easily.
	A simple memset(&mp, 0, sizeof(mp)) is suitable initialization. */

typedef struct {
	/* Buffer for putting characters back */
	char putback[1024];	
	/* Position in putback from which next character will be read.
		If readPos == writePos, then next character should
		come from cgiIn. */
	int readPos;
	/* Position in putback to which next character will be put back.
		If writePos catches up to readPos, as opposed to the other
		way around, the stream no longer functions properly.
		Calling code must guarantee that no more than 
		sizeof(putback) bytes are put back at any given time. */
	int writePos;
	/* Offset in the virtual datastream; can be compared
		to cgiContentLength */
	int offset;
} mpStream, *mpStreamPtr;

int mpRead(CGI_T *cgi,mpStreamPtr mpp, char *buffer, int len)
{
	int ilen = len;
	int got = 0;
	while (len) {
		if (mpp->readPos != mpp->writePos) {
			*buffer++ = mpp->putback[mpp->readPos++];
			mpp->readPos %= sizeof(mpp->putback);
			got++;
			len--;
		} else {
			break;
		}	
	}
	/* Refuse to read past the declared length in order to
		avoid deadlock */
	if (len > (cgi->cgiContentLength - mpp->offset)) {
		len = cgi->cgiContentLength - mpp->offset;
	}
	if (len) {
		int fgot = FCGX_GetStr(buffer,len,cgi->request->in);
		if (fgot >= 0) {
			mpp->offset += (got + fgot);
			return got + fgot;
		} else if (got > 0) {
			mpp->offset += got;
			return got;
		} else {
			/* EOF or error */
			return fgot;
		}
	} else if (got) {
		return got;
	} else if (ilen) {	
		return EOF;
	} else {
		/* 2.01 */
		return 0;
	}
}

void mpPutBack(mpStreamPtr mpp, char *data, int len)
{
	mpp->offset -= len;
	while (len) {
		mpp->putback[mpp->writePos++] = *data++;
		mpp->writePos %= sizeof(mpp->putback);
		len--;
	}
}

/* This function copies the body to outf if it is not null, otherwise to
	a newly allocated character buffer at *outP, which will be null
	terminated; if both outf and outP are null the body is not stored.
	If bodyLengthP is not null, the size of the body in bytes is stored
	to *bodyLengthP, not including any terminating null added to *outP. 
	If 'first' is nonzero, a preceding newline is not expected before
	the boundary. If 'first' is zero, a preceding newline is expected.
	Upon return mpp is positioned after the boundary and its trailing 
	newline, if any; if the boundary is followed by -- the next two 
	characters read after this function returns will be --. Upon error, 
	if outP is not null, *outP is a null pointer; *bodyLengthP 
	is set to zero. Returns cgiParseSuccess, cgiParseMemory 
	or cgiParseIO. */

static cgiParseResultType afterNextBoundary(CGI_T *cgi,mpStreamPtr mpp,
	FILE *outf,
	char **outP,
	int *bodyLengthP,
	int first
	);

static int readHeaderLine(CGI_T *cgi,
	mpStreamPtr mpp,	
	char *attr,
	int attrSpace,
	char *value,
	int valueSpace);

static void decomposeValue(char *value,
	char *mvalue, int mvalueSpace,
	char **argNames,
	char **argValues,
	int argValueSpace);

/* tfileName must be 1024 bytes to ensure adequacy on
	win32 (1024 exceeds the maximum path length and
	certainly exceeds observed behavior of _tmpnam).
	May as well also be 1024 bytes on Unix, although actual
	length is strlen(cgiTempDir) + a short unique pattern. */
	
static cgiParseResultType getTempFileName(char *tfileName);

static cgiParseResultType cgiParsePostMultipartInput(CGI_T *cgi) {
	cgiParseResultType result;
	cgiFormEntry *n = 0, *l = 0;
	int got;
	FILE *outf = 0;
	char *out = 0;
	char tfileName[1024];
	mpStream mp;
	mpStreamPtr mpp = &mp;
	memset(&mp, 0, sizeof(mp));
	if (!cgi->cgiContentLength) {
		return cgiParseSuccess;
	}
	/* Read first boundary, including trailing newline */
	result = afterNextBoundary(cgi,mpp, 0, 0, 0, 1);
	if (result == cgiParseIO) {	
		/* An empty submission is not necessarily an error */
		return cgiParseSuccess;
	} else if (result != cgiParseSuccess) {
		return result;
	}
	while (1) {
		char d[1024];
		char fvalue[1024];
		char fname[1024];
		int bodyLength = 0;
		char ffileName[1024];
		char fcontentType[1024];
		char attr[1024];
		char value[1024];
		fvalue[0] = 0;
		fname[0] = 0;
		ffileName[0] = 0;
		fcontentType[0] = 0;
		out = 0;
		outf = 0;
		/* Check for EOF */
		got = mpRead(cgi,mpp, d, 2);
		if (got < 2) {
			/* Crude EOF */
			break;
		}
		if ((d[0] == '-') && (d[1] == '-')) {
			/* Graceful EOF */
			break;
		}
		mpPutBack(mpp, d, 2);
		/* Read header lines until end of header */
		while (readHeaderLine(cgi,
				mpp, attr, sizeof(attr), value, sizeof(value))) 
		{
			char *argNames[3];
			char *argValues[2];
			/* Content-Disposition: form-data; 
				name="test"; filename="googley.gif" */
			if (cgiStrEqNc(attr, "Content-Disposition")) {
				argNames[0] = "name";
				argNames[1] = "filename";
				argNames[2] = 0;
				argValues[0] = fname;
				argValues[1] = ffileName;
				decomposeValue(value, 
					fvalue, sizeof(fvalue),
					argNames,
					argValues,
					1024);	
			} else if (cgiStrEqNc(attr, "Content-Type")) {
				argNames[0] = 0;
				decomposeValue(value, 
					fcontentType, sizeof(fcontentType),
					argNames,
					0,
					0);
			}
		}
		if (!cgiStrEqNc(fvalue, "form-data")) {
			/* Not form data */	
			continue;
		}
		/* Body is everything from here until the next 
			boundary. So, set it aside and move past boundary. 
			If a filename was submitted as part of the
			disposition header, store to a temporary file.
			Otherwise, store to a memory buffer (it is
			presumably a regular form field). */
		if (strlen(ffileName)) {
			if (getTempFileName(tfileName) != cgiParseSuccess) {
				return cgiParseIO;
			}	
			outf = fopen(tfileName, "w+b");
		} else {
			outf = 0;
			tfileName[0] = '\0';
		}	
		result = afterNextBoundary(cgi,mpp, outf, &out, &bodyLength, 0);
		if (result != cgiParseSuccess) {
			/* Lack of a boundary here is an error. */
			if (outf) {
				fclose(outf);
				unlink(tfileName);
			}
			if (out) {
				free(out);
			}
			return result;
		}
		/* OK, we have a new pair, add it to the list. */
		n = (cgiFormEntry *) malloc(sizeof(cgiFormEntry));	
		if (!n) {
			goto outOfMemory;
		}
		memset(n, 0, sizeof(cgiFormEntry));
		/* 2.01: one of numerous new casts required
			to please C++ compilers */
		n->attr = (char *) malloc(strlen(fname) + 1);
		if (!n->attr) {
			goto outOfMemory;
		}
		strcpy(n->attr, fname);
		if (out) {
			n->value = out;
			out = 0;
		} else if (outf) {
			n->value = (char *) malloc(1);
			if (!n->value) {
				goto outOfMemory;
			}
			n->value[0] = '\0';
			fclose(outf);
		}
		n->valueLength = bodyLength;
		n->next = 0;
		if (!l) {
			cgi->cgiFormEntryFirst = n;
		} else {
			l->next = n;
		}
		n->fileName = (char *) malloc(strlen(ffileName) + 1);
		if (!n->fileName) {
			goto outOfMemory;
		}
		strcpy(n->fileName, ffileName);
		n->contentType = (char *) malloc(strlen(fcontentType) + 1);
		if (!n->contentType) {
			goto outOfMemory;
		}
		strcpy(n->contentType, fcontentType);
		n->tfileName = (char *) malloc(strlen(tfileName) + 1);
		if (!n->tfileName) {
			goto outOfMemory;
		}
		strcpy(n->tfileName, tfileName);

		l = n;			
	}	
	return cgiParseSuccess;
outOfMemory:
	if (n) {
		if (n->attr) {
			free(n->attr);
		}
		if (n->value) {
			free(n->value);
		}
		if (n->fileName) {
			free(n->fileName);
		}
		if (n->tfileName) {
			free(n->tfileName);
		}
		if (n->contentType) {
			free(n->contentType);
		}
		free(n);
	}
	if (out) {
		free(out);
	}
	if (outf) {
		fclose(outf);
		unlink(tfileName);
	}
	return cgiParseMemory;
}

static cgiParseResultType getTempFileName(char *tfileName)
{
#ifndef WIN32
	/* Unix. Use the robust 'mkstemp' function to create
		a temporary file that is truly unique, with
		permissions that are truly safe. The 
		fopen-for-write destroys any bogus information
		written by potential hackers during the brief
		window between the file's creation and the
		chmod call (glibc 2.0.6 and lower might
		otherwise have allowed this). */
	int outfd; 
	strcpy(tfileName, cgicTempDir "/cgicXXXXXX");
	outfd = mkstemp(tfileName);
	if (outfd == -1) {
		return cgiParseIO;
	}
	close(outfd);
	/* Fix the permissions */
	if (chmod(tfileName, 0600) != 0) {
		unlink(tfileName);
		return cgiParseIO;
	}
#else
	/* Non-Unix. Do what we can. */
	if (!tmpnam(tfileName)) {
		return cgiParseIO;
	}
#endif
	return cgiParseSuccess;
}


#define APPEND(string, char) \
	{ \
		if ((string##Len + 1) < string##Space) { \
			string[string##Len++] = (char); \
		} \
	}

#define RAPPEND(string, ch) \
	{ \
		if ((string##Len + 1) == string##Space)  { \
			char *sold = string; \
			string##Space *= 2; \
			string = (char *) realloc(string, string##Space); \
			if (!string) { \
				string = sold; \
				goto outOfMemory; \
			} \
		} \
		string[string##Len++] = (ch); \
	}
		
#define BAPPEND(ch) \
	{ \
		if (outf) { \
			putc(ch, outf); \
			outLen++; \
		} else if (out) { \
			RAPPEND(out, ch); \
		} \
	}

cgiParseResultType afterNextBoundary(CGI_T *cgi,mpStreamPtr mpp, FILE *outf, char **outP,
	int *bodyLengthP, int first)
{
	int outLen = 0;
	int outSpace = 256;
	char *out = 0;
	cgiParseResultType result;
	int boffset;
	int got;
	char d[2];	
	/* This is large enough, because the buffer into which the
		original boundary string is fetched is shorter by more
		than four characters due to the space required for
		the attribute name */
	char workingBoundaryData[1024];
	char *workingBoundary = workingBoundaryData;
	int workingBoundaryLength;
	if ((!outf) && (outP)) {
		out = (char *) malloc(outSpace);
		if (!out) {
			goto outOfMemory;
		}
	}
	boffset = 0;
	sprintf(workingBoundaryData, "\r\n--%s", cgi->cgiMultipartBoundary);
	if (first) {
		workingBoundary = workingBoundaryData + 2;
	}
	workingBoundaryLength = strlen(workingBoundary);
	while (1) {
		got = mpRead(cgi,mpp, d, 1);
		if (got != 1) {
			/* 2.01: cgiParseIO, not cgiFormIO */
			result = cgiParseIO;
			goto error;
		}
		if (d[0] == workingBoundary[boffset]) {
			/* We matched the next byte of the boundary.
				Keep track of our progress into the
				boundary and don't emit anything. */
			boffset++;
			if (boffset == workingBoundaryLength) {
				break;
			} 
		} else if (boffset > 0) {
			/* We matched part, but not all, of the
				boundary. Now we have to be careful:
				put back all except the first
				character and try again. The 
				real boundary could begin in the
				middle of a false match. We can
				emit the first character only so far. */
			BAPPEND(workingBoundary[0]);
			mpPutBack(mpp, 
				workingBoundary + 1, boffset - 1);
			mpPutBack(mpp, d, 1);
			boffset = 0;
		} else {		
			/* Not presently in the middle of a boundary
				match; just emit the character. */
			BAPPEND(d[0]);
		}	
	}
	/* Read trailing newline or -- EOF marker. A literal EOF here
		would be an error in the input stream. */
	got = mpRead(cgi,mpp, d, 2);
	if (got != 2) {
		result = cgiParseIO;
		goto error;
	}	
	if ((d[0] == '\r') && (d[1] == '\n')) {
		/* OK, EOL */
	} else if (d[0] == '-') {
		/* Probably EOF, but we check for
			that later */
		mpPutBack(mpp, d, 2);
	}	
	if (out && outSpace) {
		char *oout = out;
		out[outLen] = '\0';
		out = (char *) realloc(out, outLen + 1);
		if (!out) {
			/* Surprising if it happens; and not fatal! We were
				just trying to give some space back. We can
				keep it if we have to. */
			out = oout;
		}
		*outP = out;
	}
	if (bodyLengthP) {
		*bodyLengthP = outLen;
	}
	return cgiParseSuccess;
outOfMemory:
	result = cgiParseMemory;
	if (outP) {
		if (out) {
			free(out);
		}
		*outP = '\0';	
	}
error:
	if (bodyLengthP) {
		*bodyLengthP = 0;
	}
	if (out) {
		free(out);
	}
	if (outP) {
		*outP = 0;	
	}
	return result;
}

static void decomposeValue(char *value,
	char *mvalue, int mvalueSpace,
	char **argNames,
	char **argValues,
	int argValueSpace)
{
	char argName[1024];
	int argNameSpace = sizeof(argName);
	int argNameLen = 0;
	int mvalueLen = 0;
	char *argValue;
	int argNum = 0;
	while (argNames[argNum]) {
		if (argValueSpace) {
			argValues[argNum][0] = '\0';
		}
		argNum++;
	}
	while (isspace(*value)) {
		value++;
	}
	/* Quoted mvalue */
	if (*value == '\"') {
		value++;
		while ((*value) && (*value != '\"')) {
			APPEND(mvalue, *value);
			value++;
		}
		while ((*value) && (*value != ';')) {
			value++;
		}
	} else {
		/* Unquoted mvalue */
		while ((*value) && (*value != ';')) {
			APPEND(mvalue, *value);
			value++;
		}	
	}	
	if (mvalueSpace) {
		mvalue[mvalueLen] = '\0';
	}
	while (*value == ';') {
		int argNum;
		int argValueLen = 0;
		/* Skip the ; between parameters */
		value++;
		/* Now skip leading whitespace */
		while ((*value) && (isspace(*value))) { 
			value++;
		}
		/* Now read the parameter name */
		argNameLen = 0;
		while ((*value) && (isalnum(*value))) {
			APPEND(argName, *value);
			value++;
		}
		if (argNameSpace) {
			argName[argNameLen] = '\0';
		}
		while ((*value) && isspace(*value)) {
			value++;
		}
		if (*value != '=') {
			/* Malformed line */
			return;	
		}
		value++;
		while ((*value) && isspace(*value)) {
			value++;
		}
		/* Find the parameter in the argument list, if present */
		argNum = 0;
		argValue = 0;
		while (argNames[argNum]) {
			if (cgiStrEqNc(argName, argNames[argNum])) {
				argValue = argValues[argNum];
				break;
			}
			argNum++;
		}		
		/* Finally, read the parameter value */
		if (*value == '\"') {
			value++;
			while ((*value) && (*value != '\"')) {
				if (argValue) {
					APPEND(argValue, *value);
				}
				value++;
			}
			while ((*value) && (*value != ';')) {
				value++;
			}
		} else {
			/* Unquoted value */
			while ((*value) && (*value != ';')) {
				if (argNames[argNum]) {
					APPEND(argValue, *value);
				}
				value++;
			}	
		}	
		if (argValueSpace) {
			argValue[argValueLen] = '\0';
		}
	}	 	
}

static int readHeaderLine(CGI_T *cgi,
	mpStreamPtr mpp,
	char *attr,
	int attrSpace,
	char *value,
	int valueSpace)
{	
	int attrLen = 0;
	int valueLen = 0;
	int valueFound = 0;
	while (1) {
		char d[1];
		int got = mpRead(cgi,mpp, d, 1);
		if (got != 1) {	
			return 0;
		}
		if (d[0] == '\r') {
			got = mpRead(cgi,mpp, d, 1);
			if (got == 1) {	
				if (d[0] == '\n') {
					/* OK */
				} else {
					mpPutBack(mpp, d, 1);
				}
			}
			break;
		} else if (d[0] == '\n') {
			break;
		} else if ((d[0] == ':') && attrLen) {
			valueFound = 1;
			while (mpRead(cgi,mpp, d, 1) == 1) {
				if (!isspace(d[0])) {
					mpPutBack(mpp, d, 1);
					break;
				} 
			}
		} else if (!valueFound) {
			if (!isspace(*d)) {
				if (attrLen < (attrSpace - 1)) {
					attr[attrLen++] = *d;
				}
			}		
		} else if (valueFound) {	
			if (valueLen < (valueSpace - 1)) {
				value[valueLen++] = *d;
			}
		}
	}
	if (attrSpace) {
		attr[attrLen] = '\0';
	}
	if (valueSpace) {
		value[valueLen] = '\0';
	}
	if (attrLen && valueLen) {
		return 1;
	} else {
		return 0;
	}
}

static cgiParseResultType cgiParseGetFormInput(CGI_T *cgi) {
	return cgiParseFormInput(cgi,cgi->cgiQueryString, cgi->cgiContentLength);
}

typedef enum {
	cgiEscapeRest,
	cgiEscapeFirst,
	cgiEscapeSecond
} cgiEscapeState;

typedef enum {
	cgiUnescapeSuccess,
	cgiUnescapeMemory
} cgiUnescapeResultType;

static cgiUnescapeResultType cgiUnescapeChars(CGI_T *cgi,char **sp, char *cp, int len);

static cgiParseResultType cgiParseFormInput(CGI_T *cgi,char *data, int length) {
	/* Scan for pairs, unescaping and storing them as they are found. */
	int pos = 0;
	cgiFormEntry *n;
	cgiFormEntry *l = 0;
	while (pos != length) {
		int foundEq = 0;
		int foundAmp = 0;
		int start = pos;
		int len = 0;
		char *attr;
		char *value;
		while (pos != length) {
			if (data[pos] == '=') {
				foundEq = 1;
				pos++;
				break;
			}
			pos++;
			len++;
		}
		if (!foundEq) {
			break;
		}
		if (cgiUnescapeChars(cgi,&attr, data+start, len)
			!= cgiUnescapeSuccess) {
			return cgiParseMemory;
		}	
		start = pos;
		len = 0;
		while (pos != length) {
			if (data[pos] == '&') {
				foundAmp = 1;
				pos++;
				break;
			}
			pos++;
			len++;
		}
		/* The last pair probably won't be followed by a &, but
			that's fine, so check for that after accepting it */
		if (cgiUnescapeChars(cgi,&value, data+start, len)
			!= cgiUnescapeSuccess) {
			free(attr);
			return cgiParseMemory;
		}	
		/* OK, we have a new pair, add it to the list. */
		n = (cgiFormEntry *) malloc(sizeof(cgiFormEntry));	
		if (!n) {
			free(attr);
			free(value);
			return cgiParseMemory;
		}
		n->attr = attr;
		n->value = value;
		n->valueLength = strlen(n->value);
		n->fileName = (char *) malloc(1);
		if (!n->fileName) {
			free(attr);
			free(value);
			free(n);
			return cgiParseMemory;
		}	
		n->fileName[0] = '\0';
		n->contentType = (char *) malloc(1);
		if (!n->contentType) {
			free(attr);
			free(value);
			free(n->fileName);
			free(n);
			return cgiParseMemory;
		}	
		n->contentType[0] = '\0';
		n->tfileName = (char *) malloc(1);
		if (!n->tfileName) {
			free(attr);
			free(value);
			free(n->fileName);
			free(n->contentType);
			free(n);
			return cgiParseMemory;
		}	
		n->tfileName[0] = '\0';
		n->next = 0;
		if (!l) {
			cgi->cgiFormEntryFirst = n;
		} else {
			l->next = n;
		}
		l = n;
		if (!foundAmp) {
			break;
		}			
	}
	return cgiParseSuccess;
}

cgiUnescapeResultType cgiUnescapeChars(CGI_T *cgi,char **sp, char *cp, int len) {
	char *s;
	cgiEscapeState escapeState = cgiEscapeRest;
	int escapedValue = 0;
	int srcPos = 0;
	int dstPos = 0;
	s = (char *) malloc(len + 1);
	if (!s) {
		return cgiUnescapeMemory;
	}
	while (srcPos < len) {
		int ch = cp[srcPos];
		switch (escapeState) {
			case cgiEscapeRest:
			if (ch == '%') {
				escapeState = cgiEscapeFirst;
			} else if (ch == '+') {
				s[dstPos++] = ' ';
			} else {
				s[dstPos++] = ch;	
			}
			break;
			case cgiEscapeFirst:
			escapedValue = cgi->cgiHexValue[ch] << 4;	
			escapeState = cgiEscapeSecond;
			break;
			case cgiEscapeSecond:
			escapedValue += cgi->cgiHexValue[ch];
			s[dstPos++] = escapedValue;
			escapeState = cgiEscapeRest;
			break;
		}
		srcPos++;
	}
	s[dstPos] = '\0';
	*sp = s;
	return cgiUnescapeSuccess;
}		
	
static void cgiSetupConstants(CGI_T *cgi) {
	int i;
	for (i=0; (i < 256); i++) {
		cgi->cgiHexValue[i] = 0;
	}
	cgi->cgiHexValue['0'] = 0;	
	cgi->cgiHexValue['1'] = 1;	
	cgi->cgiHexValue['2'] = 2;	
	cgi->cgiHexValue['3'] = 3;	
	cgi->cgiHexValue['4'] = 4;	
	cgi->cgiHexValue['5'] = 5;	
	cgi->cgiHexValue['6'] = 6;	
	cgi->cgiHexValue['7'] = 7;	
	cgi->cgiHexValue['8'] = 8;	
	cgi->cgiHexValue['9'] = 9;
	cgi->cgiHexValue['A'] = 10;
	cgi->cgiHexValue['B'] = 11;
	cgi->cgiHexValue['C'] = 12;
	cgi->cgiHexValue['D'] = 13;
	cgi->cgiHexValue['E'] = 14;
	cgi->cgiHexValue['F'] = 15;
	cgi->cgiHexValue['a'] = 10;
	cgi->cgiHexValue['b'] = 11;
	cgi->cgiHexValue['c'] = 12;
	cgi->cgiHexValue['d'] = 13;
	cgi->cgiHexValue['e'] = 14;
	cgi->cgiHexValue['f'] = 15;
}

static void cgiFreeResources(CGI_T *cgi) {
	cgiFormEntry *c = cgi->cgiFormEntryFirst;
	cgiFormEntry *n;
	while (c) {
		n = c->next;
		free(c->attr);
		free(c->value);
		free(c->fileName);
		free(c->contentType);
		if (strlen(c->tfileName)) {
			unlink(c->tfileName);
		}
		free(c->tfileName);
		free(c);
		c = n;
	}
	/* If the cgi environment was restored from a saved environment,
		then these are in allocated space and must also be freed */
	if (cgi->cgiRestored) {
		free(cgi->cgiServerSoftware);
		free(cgi->cgiServerName);
		free(cgi->cgiGatewayInterface);
		free(cgi->cgiServerProtocol);
		free(cgi->cgiServerPort);
		free(cgi->cgiRequestMethod);
		free(cgi->cgiPathInfo);
		free(cgi->cgiPathTranslated);
		free(cgi->cgiScriptName);
		free(cgi->cgiQueryString);
		free(cgi->cgiRemoteHost);
		free(cgi->cgiRemoteAddr);
    free(cgi->cgiRemotePort);
		free(cgi->cgiAuthType);
		free(cgi->cgiRemoteUser);
		free(cgi->cgiRemoteIdent);
		free(cgi->cgiContentType);
		free(cgi->cgiAccept);
		free(cgi->cgiUserAgent);
		free(cgi->cgiReferrer);
	}
	/* 2.0: to clean up the environment for cgiReadEnvironment,
		we must set these correctly */
	cgi->cgiFormEntryFirst = 0;
	cgi->cgiRestored = 0;
}

static cgiFormResultType cgiFormEntryString(
	cgiFormEntry *e, char *result, int max, int newlines);

static cgiFormEntry *cgiFormEntryFindFirst(CGI_T *cgi,char *name);
static cgiFormEntry *cgiFormEntryFindNext(CGI_T *cgi);

cgiFormResultType cgiFormString(CGI_T *cgi,
        char *name, char *result, int max) {
	cgiFormEntry *e;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		strcpy(result, "");
		return cgiFormNotFound;
	}
	return cgiFormEntryString(e, result, max, 1);
}

cgiFormResultType cgiFormFileName(CGI_T *cgi,
	char *name, char *result, int resultSpace)
{
	cgiFormEntry *e;
	int resultLen = 0;
	char *s;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		strcpy(result, "");
		return cgiFormNotFound;
	}
	s = e->fileName;
	while (*s) {
		APPEND(result, *s);
		s++;
	}	
	if (resultSpace) {
		result[resultLen] = '\0';
	}
	if (!strlen(e->fileName)) {
		return cgiFormNoFileName;
	} else if (((int) strlen(e->fileName)) > (resultSpace - 1)) {
		return cgiFormTruncated;
	} else {
		return cgiFormSuccess;
	}
}

cgiFormResultType cgiFormFileContentType(CGI_T *cgi,
	char *name, char *result, int resultSpace)
{
	cgiFormEntry *e;
	int resultLen = 0;
	char *s;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		if (resultSpace) {
			result[0] = '\0';
		}	
		return cgiFormNotFound;
	}
	s = e->contentType;
	while (*s) {
		APPEND(result, *s);
		s++;
	}	
	if (resultSpace) {
		result[resultLen] = '\0';
	}
	if (!strlen(e->contentType)) {
		return cgiFormNoContentType;
	} else if (((int) strlen(e->contentType)) > (resultSpace - 1)) {
		return cgiFormTruncated;
	} else {
		return cgiFormSuccess;
	}
}

cgiFormResultType cgiFormFileSize(CGI_T *cgi,
	char *name, int *sizeP)
{
	cgiFormEntry *e;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		if (sizeP) {
			*sizeP = 0;
		}
		return cgiFormNotFound;
	} else if (!strlen(e->tfileName)) {
		if (sizeP) {
			*sizeP = 0;
		}
		return cgiFormNotAFile;
	} else {
		if (sizeP) {
			*sizeP = e->valueLength;
		}
		return cgiFormSuccess;
	}
}

typedef struct cgiFileStruct {
	FILE *in;
} cgiFile;

cgiFormResultType cgiFormFileOpen(CGI_T *cgi,
	char *name, cgiFilePtr *cfpp)
{
	cgiFormEntry *e;
	cgiFilePtr cfp;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		*cfpp = 0;
		return cgiFormNotFound;
	}
	if (!strlen(e->tfileName)) {
		*cfpp = 0;
		return cgiFormNotAFile;
	}
	cfp = (cgiFilePtr) malloc(sizeof(cgiFile));
	if (!cfp) {
		*cfpp = 0;
		return cgiFormMemory;
	}
	cfp->in = fopen(e->tfileName, "rb");
	if (!cfp->in) {
		free(cfp);
		return cgiFormIO;
	}
	*cfpp = cfp;
	return cgiFormSuccess;
}

cgiFormResultType cgiFormFileRead(
	cgiFilePtr cfp, char *buffer, 
	int bufferSize, int *gotP)
{
	int got = 0;
	if (!cfp) {
		return cgiFormOpenFailed;
	}
	got = fread(buffer, 1, bufferSize, cfp->in);
	if (got <= 0) {
		return cgiFormEOF;
	}
	*gotP = got;
	return cgiFormSuccess;
}

cgiFormResultType cgiFormFileClose(cgiFilePtr cfp)
{
	if (!cfp) {
		return cgiFormOpenFailed;
	}
	fclose(cfp->in);
	free(cfp);
	return cgiFormSuccess;
}

cgiFormResultType cgiFormStringNoNewlines(CGI_T *cgi,
        char *name, char *result, int max) {
	cgiFormEntry *e;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		strcpy(result, "");
		return cgiFormNotFound;
	}
	return cgiFormEntryString(e, result, max, 0);
}

cgiFormResultType cgiFormStringMultiple(CGI_T *cgi,
        char *name, char ***result) {
	char **stringArray;
	cgiFormEntry *e;
	int i;
	int total = 0;
	/* Make two passes. One would be more efficient, but this
		function is not commonly used. The select menu and
		radio box functions are faster. */
	e = cgiFormEntryFindFirst(cgi,name);
	if (e != 0) {
		do {
			total++;
		} while ((e = cgiFormEntryFindNext(cgi)) != 0); 
	}
	stringArray = (char **) malloc(sizeof(char *) * (total + 1));
	if (!stringArray) {
		*result = 0;
		return cgiFormMemory;
	}
	/* initialize all entries to null; the last will stay that way */
	for (i=0; (i <= total); i++) {
		stringArray[i] = 0;
	}
	/* Now go get the entries */
	e = cgiFormEntryFindFirst(cgi,name);
#ifdef CGICDEBUG
	CGICDEBUGSTART
	fprintf(dout, "StringMultiple Beginning\n");
	CGICDEBUGEND
#endif /* CGICDEBUG */
	if (e) {
		i = 0;
		do {
			int max = (int) (strlen(e->value) + 1);
			stringArray[i] = (char *) malloc(max);
			if (stringArray[i] == 0) {
				/* Memory problems */
				cgiStringArrayFree(stringArray);
				*result = 0;
				return cgiFormMemory;
			}	
			strcpy(stringArray[i], e->value);
			cgiFormEntryString(e, stringArray[i], max, 1);
			i++;
		} while ((e = cgiFormEntryFindNext(cgi)) != 0); 
		*result = stringArray;
#ifdef CGICDEBUG
		CGICDEBUGSTART
		fprintf(dout, "StringMultiple Succeeding\n");
		CGICDEBUGEND
#endif /* CGICDEBUG */
		return cgiFormSuccess;
	} else {
		*result = stringArray;
#ifdef CGICDEBUG
		CGICDEBUGSTART
		fprintf(dout, "StringMultiple found nothing\n");
		CGICDEBUGEND
#endif /* CGICDEBUG */
		return cgiFormNotFound;
	}	
}

cgiFormResultType cgiFormStringSpaceNeeded(CGI_T *cgi,
        char *name, int *result) {
	cgiFormEntry *e;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		*result = 1;
		return cgiFormNotFound; 
	}
	*result = ((int) strlen(e->value)) + 1;
	return cgiFormSuccess;
}

static cgiFormResultType cgiFormEntryString(
	cgiFormEntry *e, char *result, int max, int newlines) {
	char *dp, *sp;
	int truncated = 0;
	int len = 0;
	int avail = max-1;
	int crCount = 0;
	int lfCount = 0;	
	dp = result;
	sp = e->value;	
	while (1) {
		int ch;
		/* 1.07: don't check for available space now.
			We check for it immediately before adding
			an actual character. 1.06 handled the
			trailing null of the source string improperly,
			resulting in a cgiFormTruncated error. */
		ch = *sp;
		/* Fix the CR/LF, LF, CR nightmare: watch for
			consecutive bursts of CRs and LFs in whatever
			pattern, then actually output the larger number 
			of LFs. Consistently sane, yet it still allows
			consecutive blank lines when the user
			actually intends them. */
		if ((ch == 13) || (ch == 10)) {
			if (ch == 13) {
				crCount++;
			} else {
				lfCount++;
			}	
		} else {
			if (crCount || lfCount) {
				int lfsAdd = crCount;
				if (lfCount > crCount) {
					lfsAdd = lfCount;
				}
				/* Stomp all newlines if desired */
				if (!newlines) {
					lfsAdd = 0;
				}
				while (lfsAdd) {
					if (len >= avail) {
						truncated = 1;
						break;
					}
					*dp = 10;
					dp++;
					lfsAdd--;
					len++;		
				}
				crCount = 0;
				lfCount = 0;
			}
			if (ch == '\0') {
				/* The end of the source string */
				break;				
			}	
			/* 1.06: check available space before adding
				the character, because a previously added
				LF may have brought us to the limit */
			if (len >= avail) {
				truncated = 1;
				break;
			}
			*dp = ch;
			dp++;
			len++;
		}
		sp++;	
	}	
	*dp = '\0';
	if (truncated) {
		return cgiFormTruncated;
	} else if (!len) {
		return cgiFormEmpty;
	} else {
		return cgiFormSuccess;
	}
}

static int cgiFirstNonspaceChar(char *s);

cgiFormResultType cgiFormInteger(CGI_T *cgi,
        char *name, int *result, int defaultV) {
	cgiFormEntry *e;
	int ch;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		*result = defaultV;
		return cgiFormNotFound; 
	}	
	if (!strlen(e->value)) {
		*result = defaultV;
		return cgiFormEmpty;
	}
	ch = cgiFirstNonspaceChar(e->value);
	if (!(isdigit(ch)) && (ch != '-') && (ch != '+')) {
		*result = defaultV;
		return cgiFormBadType;
	} else {
		*result = atoi(e->value);
		return cgiFormSuccess;
	}
}

cgiFormResultType cgiFormIntegerBounded(CGI_T *cgi,
        char *name, int *result, int min, int max, int defaultV) {
	cgiFormResultType error = cgiFormInteger(cgi,name, result, defaultV);
	if (error != cgiFormSuccess) {
		return error;
	}
	if (*result < min) {
		*result = min;
		return cgiFormConstrained;
	} 
	if (*result > max) {
		*result = max;
		return cgiFormConstrained;
	} 
	return cgiFormSuccess;
}

cgiFormResultType cgiFormDouble(CGI_T *cgi,
        char *name, double *result, double defaultV) {
	cgiFormEntry *e;
	int ch;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		*result = defaultV;
		return cgiFormNotFound; 
	}	
	if (!strlen(e->value)) {
		*result = defaultV;
		return cgiFormEmpty;
	} 
	ch = cgiFirstNonspaceChar(e->value);
	if (!(isdigit(ch)) && (ch != '.') && (ch != '-') && (ch != '+')) {
		*result = defaultV;
		return cgiFormBadType;
	} else {
		*result = atof(e->value);
		return cgiFormSuccess;
	}
}

cgiFormResultType cgiFormDoubleBounded(CGI_T *cgi,
        char *name, double *result, double min, double max, double defaultV) {
	cgiFormResultType error = cgiFormDouble(cgi,name, result, defaultV);
	if (error != cgiFormSuccess) {
		return error;
	}
	if (*result < min) {
		*result = min;
		return cgiFormConstrained;
	} 
	if (*result > max) {
		*result = max;
		return cgiFormConstrained;
	} 
	return cgiFormSuccess;
}

cgiFormResultType cgiFormSelectSingle(CGI_T *cgi,
	char *name, char **choicesText, int choicesTotal, 
	int *result, int defaultV) 
{
	cgiFormEntry *e;
	int i;
	e = cgiFormEntryFindFirst(cgi,name);
#ifdef CGICDEBUG
	CGICDEBUGSTART
	fprintf(dout, "%d\n", (int) e);
	CGICDEBUGEND
#endif /* CGICDEBUG */
	if (!e) {
		*result = defaultV;
		return cgiFormNotFound;
	}
	for (i=0; (i < choicesTotal); i++) {
#ifdef CGICDEBUG
		CGICDEBUGSTART
		fprintf(dout, "%s %s\n", choicesText[i], e->value);
		CGICDEBUGEND
#endif /* CGICDEBUG */
		if (cgiStrEq(choicesText[i], e->value)) {
#ifdef CGICDEBUG
			CGICDEBUGSTART
			fprintf(dout, "MATCH\n");
			CGICDEBUGEND
#endif /* CGICDEBUG */
			*result = i;
			return cgiFormSuccess;
		}
	}
	*result = defaultV;
	return cgiFormNoSuchChoice;
}

cgiFormResultType cgiFormSelectMultiple(CGI_T *cgi,
	char *name, char **choicesText, int choicesTotal, 
	int *result, int *invalid) 
{
	cgiFormEntry *e;
	int i;
	int hits = 0;
	int invalidE = 0;
	for (i=0; (i < choicesTotal); i++) {
		result[i] = 0;
	}
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		*invalid = invalidE;
		return cgiFormNotFound;
	}
	do {
		int hit = 0;
		for (i=0; (i < choicesTotal); i++) {
			if (cgiStrEq(choicesText[i], e->value)) {
				result[i] = 1;
				hits++;
				hit = 1;
				break;
			}
		}
		if (!(hit)) {
			invalidE++;
		}
	} while ((e = cgiFormEntryFindNext(cgi)) != 0);

	*invalid = invalidE;

	if (hits) {
		return cgiFormSuccess;
	} else {
		return cgiFormNotFound;
	}
}

cgiFormResultType cgiFormCheckboxSingle(CGI_T *cgi,
	char *name)
{
	cgiFormEntry *e;
	e = cgiFormEntryFindFirst(cgi,name);
	if (!e) {
		return cgiFormNotFound;
	}
	return cgiFormSuccess;
}

extern cgiFormResultType cgiFormCheckboxMultiple(CGI_T *cgi,
	char *name, char **valuesText, int valuesTotal, 
	int *result, int *invalid)
{
	/* Implementation is identical to cgiFormSelectMultiple. */
	return cgiFormSelectMultiple(cgi,name, valuesText, 
		valuesTotal, result, invalid);
}

cgiFormResultType cgiFormRadio(CGI_T *cgi,
	char *name, 
	char **valuesText, int valuesTotal, int *result, int defaultV)
{
	/* Implementation is identical to cgiFormSelectSingle. */
	return cgiFormSelectSingle(cgi,name, valuesText, valuesTotal, 
		result, defaultV);
}

cgiFormResultType cgiCookieString(CGI_T *cgi,
	char *name,
	char *value,
	int space)
{
	char *p = cgi->cgiCookie;
	while (*p) {
		char *n = name;
		/* 2.02: if cgiCookie is exactly equal to name, this
			can cause an overrun. The server probably wouldn't
			allow it, since a name without values makes no sense 
			-- but then again it might not check, so this is a
			genuine security concern. Thanks to Nicolas 
			Tomadakis. */
		while (*p == *n) {
			if ((p == '\0') && (n == '\0')) {
				/* Malformed cookie header from client */
				return cgiFormNotFound;
			}
			p++;
			n++;
		}
		if ((!*n) && (*p == '=')) {
			p++;
			while ((*p != ';') && (*p != '\0') &&
				(space > 1)) 
			{
				*value = *p;
				value++;
				p++;
				space--;
			}
			if (space > 0) {
				*value = '\0';
			}
			/* Correct parens: 2.02. Thanks to
				Mathieu Villeneuve-Belair. */
			if (!(((*p) == ';') || ((*p) == '\0')))
			{
				return cgiFormTruncated;
			} else {	
				return cgiFormSuccess;
			}
		} else {
			/* Skip to next cookie */	
			while (*p) {
				if (*p == ';') {
					break;
				}
				p++;
			}
			if (!*p) {
				/* 2.01: default to empty */
				if (space) {
					*value = '\0';
				}
				return cgiFormNotFound;
			}
			p++;	
			/* Allow whitespace after semicolon */
			while ((*p) && isspace(*p)) {
				p++;
			} 
		}
	}
	/* 2.01: actually the above loop never terminates except
		with a return, but do this to placate gcc */
	if (space) {
		*value = '\0';
	}
	return cgiFormNotFound;
}

cgiFormResultType cgiCookieInteger(CGI_T *cgi,
	char *name,
	int *result,
	int defaultV)
{
	char buffer[256];
	cgiFormResultType r = 
		cgiCookieString(cgi,name, buffer, sizeof(buffer));
	if (r != cgiFormSuccess) {
		*result = defaultV;
	} else {
		*result = atoi(buffer);
	}
	return r;
}

void cgiHeaderCookieSetInteger(CGI_T *cgi,char *name, int value, int secondsToLive,
	char *path, char *domain,int secure,int httponly)
{
	char svalue[256];
	sprintf(svalue, "%d", value);
	cgiHeaderCookieSetString(cgi,name, svalue, secondsToLive, path, domain,secure,httponly);
}

char *days[] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

char *months[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

void cgiHeaderCookieSetString(CGI_T *cgi,char *name, char *value, int secondsToLive,
	char *path, char *domain,int secure,int httponly)
{
	/* cgic 2.02: simpler and more widely compatible implementation.
		Thanks to Chunfu Lai. 
	   cgic 2.03: yes, but it didn't work. Reimplemented by
		Thomas Boutell. ; after last element was a bug. 
	   Examples of real world cookies that really work:
   	   Set-Cookie: MSNADS=UM=; domain=.slate.com; 
             expires=Tue, 26-Apr-2022 19:00:00 GMT; path=/
	   Set-Cookie: MC1=V=3&ID=b5bc08af2b8a43ff85fcb5efd8b238f0; 
             domain=.slate.com; expires=Mon, 04-Oct-2021 19:00:00 GMT; path=/
	*/
	time_t now;
	time_t then;
	struct tm *gt;
  char sec[16],ho[16];

  if(secondsToLive != 0) {
	  time(&now);
	  then = now + secondsToLive;
	  gt = gmtime(&then);
  }

  sec[0] = 0;
  ho[0] = 0;
  if(secure) strcpy(sec,"; secure");
  if(httponly) strcpy(ho,"; httponly");

  if(secondsToLive == 0)
    FCGX_FPrintF(cgi->request->out, 
    "Set-Cookie: %s=%s; domain=%s; path=%s%s%s\r\n",
    name, value, domain, 
    path, sec, ho);
  else
	  FCGX_FPrintF(cgi->request->out, 
		"Set-Cookie: %s=%s; domain=%s; expires=%s, %02d-%s-%04d %02d:%02d:%02d GMT; path=%s%s%s\r\n",
		name, value, domain, 
		days[gt->tm_wday],
		gt->tm_mday,
		months[gt->tm_mon],
		gt->tm_year + 1900, 	
		gt->tm_hour,
		gt->tm_min,
		gt->tm_sec,
		path, sec, ho);
}

void cgiHeaderLocation(CGI_T *cgi,char *redirectUrl) {
	FCGX_FPrintF(cgi->request->out, "Location: %s\r\n\r\n", redirectUrl);
}

void cgiHeaderStatus(CGI_T *cgi,int status, char *statusMessage) {
	FCGX_FPrintF(cgi->request->out, "Status: %d %s\r\n\r\n", status, statusMessage);
}

void cgiOutputHeader(CGI_T *cgi,char *name,char *value)
{
  FCGX_FPrintF(cgi->request->out, "%s: %s\r\n", name,value);
}

//P3P: policyref="http://googleads.g.doubleclick.net/pagead/gcn_p3p_.xml", CP="CURa ADMa DEVa TAIo PSAo PSDo OUR IND UNI PUR INT DEM STA PRE COM NAV OTC NOI DSP COR"
void cgiHeaderP3P(CGI_T *cgi,char *policyref,char *cp)
{
  if(policyref && cp)
    FCGX_FPrintF(cgi->request->out, "P3P: policyref=\"%s\", CP=\"%s\"\r\n", policyref,cp);
  else if(cp)
    FCGX_FPrintF(cgi->request->out, "P3P: CP=\"%s\"\r\n", cp);
  else
    return;
}

void cgiHeaderLastModified(CGI_T *cgi,int lastmodified)
{
	struct tm *gt;
  
	gt = gmtime((time_t *)&lastmodified);
  
  FCGX_FPrintF(cgi->request->out,
    "Last-Modified: %s, %02d-%s-%04d %02d:%02d:%02d GMT\r\n",
    days[gt->tm_wday],
		gt->tm_mday,
		months[gt->tm_mon],
		gt->tm_year + 1900, 	
		gt->tm_hour,
		gt->tm_min,
		gt->tm_sec);
}


// Browser Caching
void cgiHeaderCachePragma(CGI_T *cgi)
{
  FCGX_FPrintF(cgi->request->out, "Pragma: no-cache\r\n");
}

void cgiHeaderCacheExpire(CGI_T *cgi,int secondsToLive)
{
  time_t now;
	time_t then;
	struct tm *gt;
  
	time(&now);
	then = now + secondsToLive;
	gt = gmtime(&then);
  
  FCGX_FPrintF(cgi->request->out,
    "Expires: %s, %02d-%s-%04d %02d:%02d:%02d GMT\r\n",
    days[gt->tm_wday],
		gt->tm_mday,
		months[gt->tm_mon],
		gt->tm_year + 1900, 	
		gt->tm_hour,
		gt->tm_min,
		gt->tm_sec);
}

void cgiHeaderCacheControl(CGI_T *cgi,int type,int expiretime) 
{
  char buf[1024];
  
  switch (type)
  {
    case CACHE_TYPE_PUBLIC:
      cgiHeaderCacheExpire(cgi,expiretime);
      snprintf(buf, 1024 , "public, max-age=%d", expiretime); 
      cgiOutputHeader(cgi,"Cache-Control: ",buf);
      break;
    case CACHE_TYPE_PRIVATE:
      cgiOutputHeader(cgi,"Expires: ","Thu, 19 Nov 1981 08:52:00 GMT");
      snprintf(buf, 1024 , "private, max-age=%d, pre-check=%d", expiretime, expiretime); 
      cgiOutputHeader(cgi,"Cache-Control: ",buf);
      break;
    case CACHE_TYPE_PRIVATENOEXPIRE:
      snprintf(buf, 1024 , "private, max-age=%d, pre-check=%d", expiretime, expiretime); 
      cgiOutputHeader(cgi,"Cache-Control: ",buf);
      break;
    case CACHE_TYPE_NOCACHE:
      cgiOutputHeader(cgi,"Expires: ","Thu, 19 Nov 1981 08:52:00 GMT");
      /* For HTTP/1.1 conforming clients and the rest (MSIE 5) */
      cgiOutputHeader(cgi,"Cache-Control: ","no-store, no-cache, must-revalidate, post-check=0, pre-check=0");
      /* For HTTP/1.0 conforming clients */
      cgiHeaderCachePragma(cgi);
      break;
    default:
      return;
  }
}

// content
void cgiHeaderContentLength(CGI_T *cgi,int contentlen)
{
  FCGX_FPrintF(cgi->request->out, "Content-Length: %d\r\n", contentlen);
}

//charset: UTF-8,GB2312,mimeType: text/html
void cgiHeaderContentType(CGI_T *cgi,char *mimeType,char *charset) {
  if(charset)
    FCGX_FPrintF(cgi->request->out, "Content-Type: %s; charset=%s\r\n\r\n", mimeType,charset);
  else
	  FCGX_FPrintF(cgi->request->out, "Content-Type: %s\r\n\r\n", mimeType);
}

int cgiOutputContent(CGI_T *cgi,char *content,int contentlen)
{
  return FCGX_PutStr(content,contentlen,cgi->request->out);
}


static int cgiWriteString(FILE *out, char *s);

static int cgiWriteInt(FILE *out, int i);

#define CGIC_VERSION "2.0"

cgiEnvironmentResultType cgiWriteEnvironment(CGI_T *cgi,char *filename) {
	FILE *out;
	cgiFormEntry *e;
	/* Be sure to open in binary mode */
	out = fopen(filename, "wb");
	if (!out) {
		/* Can't create file */
		return cgiEnvironmentIO;
	}
	if (!cgiWriteString(out, "CGIC2.0")) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiServerSoftware)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiServerName)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiGatewayInterface)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiServerProtocol)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiServerPort)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiRequestMethod)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiPathInfo)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiPathTranslated)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiScriptName)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiQueryString)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiRemoteHost)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiRemoteAddr)) {
		goto error;
	}
  if (!cgiWriteString(out, cgi->cgiRemotePort)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiAuthType)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiRemoteUser)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiRemoteIdent)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiContentType)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiAccept)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiUserAgent)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiReferrer)) {
		goto error;
	}
	if (!cgiWriteString(out, cgi->cgiCookie)) {
		goto error;
	}
	if (!cgiWriteInt(out, cgi->cgiContentLength)) {
		goto error;
	}
	e = cgi->cgiFormEntryFirst;
	while (e) {
		cgiFilePtr fp;
		if (!cgiWriteString(out, e->attr)) {
			goto error;
		}
		if (!cgiWriteString(out, e->value)) {
			goto error;
		}
		/* New 2.0 fields and file uploads */
		if (!cgiWriteString(out, e->fileName)) {
			goto error;
		}
		if (!cgiWriteString(out, e->contentType)) {
			goto error;
		}
		if (!cgiWriteInt(out, e->valueLength)) {
			goto error;
		}
		if (cgiFormFileOpen(cgi,e->attr, &fp) == cgiFormSuccess) {
			char buffer[1024];
			int got;
			if (!cgiWriteInt(out, 1)) {
				cgiFormFileClose(fp);
				goto error;
			}
			while (cgiFormFileRead(fp, buffer, 
				sizeof(buffer), &got) == cgiFormSuccess)
			{
				if (((int) fwrite(buffer, 1, got, out)) != got) {
					cgiFormFileClose(fp);
					goto error;
				}
			}
			if (cgiFormFileClose(fp) != cgiFormSuccess) {
				goto error;
			}
		} else {
			if (!cgiWriteInt(out, 0)) {
				goto error;
			}
		}
		e = e->next;
	}
	fclose(out);
	return cgiEnvironmentSuccess;
error:
	fclose(out);
	/* If this function is not defined in your system,
		you must substitute the appropriate 
		file-deletion function. */
	unlink(filename);
	return cgiEnvironmentIO;
}

static int cgiWriteString(FILE *out, char *s) {
	int len = (int) strlen(s);
	cgiWriteInt(out, len);
	if (((int) fwrite(s, 1, len, out)) != len) {
		return 0;
	}
	return 1;
}

static int cgiWriteInt(FILE *out, int i) {
	if (!fwrite(&i, sizeof(int), 1, out)) {
		return 0;
	}
	return 1;
}

static int cgiReadString(FILE *out, char **s);

static int cgiReadInt(FILE *out, int *i);

cgiEnvironmentResultType cgiReadEnvironment(CGI_T *cgi,char *filename) {
	FILE *in;
	cgiFormEntry *e = 0, *p;
	char *version;
	/* Prevent compiler warnings */
	cgiEnvironmentResultType result = cgiEnvironmentIO;
	/* Free any existing data first */
	cgiFreeResources(cgi);
	/* Be sure to open in binary mode */
	in = fopen(filename, "rb");
	if (!in) {
		/* Can't access file */
		return cgiEnvironmentIO;
	}
	if (!cgiReadString(in, &version)) {
		goto error;
	}
	if (strcmp(version, "CGIC" CGIC_VERSION)) {
		/* 2.02: Merezko Oleg */
		free(version);
		return cgiEnvironmentWrongVersion;
	}	
	/* 2.02: Merezko Oleg */
	free(version);
	if (!cgiReadString(in, &cgi->cgiServerSoftware)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiServerName)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiGatewayInterface)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiServerProtocol)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiServerPort)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiRequestMethod)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiPathInfo)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiPathTranslated)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiScriptName)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiQueryString)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiRemoteHost)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiRemoteAddr)) {
		goto error;
	}
  if (!cgiReadString(in, &cgi->cgiRemotePort)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiAuthType)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiRemoteUser)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiRemoteIdent)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiContentType)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiAccept)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiUserAgent)) {
		goto error;
	}
	if (!cgiReadString(in, &cgi->cgiReferrer)) {
		goto error;
	}
	/* 2.0 */
	if (!cgiReadString(in, &cgi->cgiCookie)) {
		goto error;
	}
	if (!cgiReadInt(in, &cgi->cgiContentLength)) {
		goto error;
	}
	p = 0;
	while (1) {
		int fileFlag;
		e = (cgiFormEntry *) calloc(1, sizeof(cgiFormEntry));
		if (!e) {
			cgiFreeResources(cgi);
			fclose(in);
			return cgiEnvironmentMemory;
		}
		memset(e, 0, sizeof(cgiFormEntry));
		if (!cgiReadString(in, &e->attr)) {
			/* This means we've reached the end of the list. */
			/* 2.02: thanks to Merezko Oleg */
			free(e);
			break;
		}
		if (!cgiReadString(in, &e->value)) {
			goto outOfMemory;
		}
		if (!cgiReadString(in, &e->fileName)) {
			goto outOfMemory;
		}
		if (!cgiReadString(in, &e->contentType)) {
			goto outOfMemory;
		}
		if (!cgiReadInt(in, &e->valueLength)) {
			goto outOfMemory;
		}
		if (!cgiReadInt(in, &fileFlag)) {
			goto outOfMemory;
		}
		if (fileFlag) {
			char buffer[1024];
			FILE *out;
			char tfileName[1024];
			int got;
			int len = e->valueLength;
			if (getTempFileName(tfileName)
				!= cgiParseSuccess)
			{
				result = cgiEnvironmentIO;
				goto error;
			}
			out = fopen(tfileName, "w+b");
			if (!out) {
				result = cgiEnvironmentIO;
				goto error;
			}
			while (len > 0) {		
				/* 2.01: try is a bad variable name in
					C++, and it wasn't being used
					properly either */
				int tryr = len;
				if (tryr > ((int) sizeof(buffer))) {
					tryr = sizeof(buffer);
				}
				got = fread(buffer, 1, tryr, in);
				if (got <= 0) {
					result = cgiEnvironmentIO;
					fclose(out);
					unlink(tfileName);
					goto error;
				}
				if (((int) fwrite(buffer, 1, got, out)) != got) {
					result = cgiEnvironmentIO;
					fclose(out);
					unlink(tfileName);
					goto error;
				}
				len -= got;
			}
			/* cgic 2.05: should be fclose not rewind */
			fclose(out);
			e->tfileName = (char *) malloc((int) strlen(tfileName) + 1);
			if (!e->tfileName) {
				result = cgiEnvironmentMemory;
				unlink(tfileName);
				goto error;
			}
			strcpy(e->tfileName, tfileName);
		} else {
			e->tfileName = (char *) malloc(1);
			if (!e->tfileName) {
				result = cgiEnvironmentMemory;
				goto error;
			}
		}	
		e->next = 0;
		if (p) {
			p->next = e;
		} else {
			cgi->cgiFormEntryFirst = e;
		}	
		p = e;
	}
	fclose(in);
	cgi->cgiRestored = 1;
	return cgiEnvironmentSuccess;
outOfMemory:
	result = cgiEnvironmentMemory;
error:
	cgiFreeResources(cgi);
	fclose(in);
	if (e) {
		if (e->attr) {
			free(e->attr);
		}
		if (e->value) {
			free(e->value);
		}
		if (e->fileName) {
			free(e->fileName);
		}
		if (e->contentType) {
			free(e->contentType);
		}
		if (e->tfileName) {
			free(e->tfileName);
		}
		free(e);
	}
	return result;
}

static int cgiReadString(FILE *in, char **s) {
	int len;
	/* 2.0 fix: test cgiReadInt for failure! */ 
	if (!cgiReadInt(in, &len)) {
		return 0;
	}
	*s = (char *) malloc(len + 1);
	if (!(*s)) {
		return 0;
	}	
	if (((int) fread(*s, 1, len, in)) != len) {
		return 0;
	}
	(*s)[len] = '\0';
	return 1;
}

static int cgiReadInt(FILE *out, int *i) {
	if (!fread(i, sizeof(int), 1, out)) {
		return 0;
	}
	return 1;
}

static int cgiStrEqNc(char *s1, char *s2) {
	while(1) {
		if (!(*s1)) {
			if (!(*s2)) {
				return 1;
			} else {
				return 0;
			}
		} else if (!(*s2)) {
			return 0;
		}
		if (isalpha(*s1)) {
			if (tolower(*s1) != tolower(*s2)) {
				return 0;
			}
		} else if ((*s1) != (*s2)) {
			return 0;
		}
		s1++;
		s2++;
	}
}

static int cgiStrBeginsNc(char *s1, char *s2) {
	while(1) {
		if (!(*s2)) {
			return 1;
		} else if (!(*s1)) {
			return 0;
		}
		if (isalpha(*s1)) {
			if (tolower(*s1) != tolower(*s2)) {
				return 0;
			}
		} else if ((*s1) != (*s2)) {
			return 0;
		}
		s1++;
		s2++;
	}
}

static cgiFormEntry *cgiFormEntryFindFirst(CGI_T *cgi,char *name) {
	cgi->cgiFindTarget = name;
	cgi->cgiFindPos = cgi->cgiFormEntryFirst;
	return cgiFormEntryFindNext(cgi);
}

static cgiFormEntry *cgiFormEntryFindNext(CGI_T *cgi) {
	while (cgi->cgiFindPos) {
		cgiFormEntry *c = cgi->cgiFindPos;
		cgi->cgiFindPos = c->next;
		if (!strcmp(c ->attr, cgi->cgiFindTarget)) {
			return c;
		}
	}
	return 0;
}

static int cgiFirstNonspaceChar(char *s) {
	int len = strspn(s, " \n\r\t");
	return s[len];
}

void cgiStringArrayFree(char **stringArray) {
	char *p;
	char **arrayItself = stringArray;
	p = *stringArray;
	while (p) {
		free(p);
		stringArray++;
		p = *stringArray;
	}
	/* 2.0: free the array itself! */
	free(arrayItself);
}	

cgiFormResultType cgiCookies(CGI_T *cgi,char ***result) {
	char **stringArray;
	int i;
	int total = 0;
	char *p;
	char *n;
	p = cgi->cgiCookie;
	while (*p) {
		if (*p == '=') {
			total++;
		}
		p++;
	}
	stringArray = (char **) malloc(sizeof(char *) * (total + 1));
	if (!stringArray) {
		*result = 0;
		return cgiFormMemory;
	}
	/* initialize all entries to null; the last will stay that way */
	for (i=0; (i <= total); i++) {
		stringArray[i] = 0;
	}
	i = 0;
	p = cgi->cgiCookie;
	while (*p) {
		while (*p && isspace(*p)) {
			p++;
		}
		n = p;
		while (*p && (*p != '=')) {
			p++;
		}
		if (p != n) {
			stringArray[i] = (char *) malloc((p - n) + 1);
			if (!stringArray[i]) {
				cgiStringArrayFree(stringArray);
				*result = 0;
				return cgiFormMemory;
			}	
			memcpy(stringArray[i], n, p - n);
			stringArray[i][p - n] = '\0';
			i++;
		}
		while (*p && (*p != ';')) {
			p++;	
		}
		if (!*p) {
			break;
		}
		if (*p == ';') {
			p++;
		}
	}
	*result = stringArray;
	return cgiFormSuccess;
}

cgiFormResultType cgiFormEntries(CGI_T *cgi,char ***result) {
	char **stringArray;
	cgiFormEntry *e, *pe;
	int i;
	int total = 0;
	e = cgi->cgiFormEntryFirst;
	while (e) {
		/* Don't count a field name more than once if
			multiple values happen to be present for it */
		pe = cgi->cgiFormEntryFirst;
		while (pe != e) {
			if (!strcmp(e->attr, pe->attr)) {
				goto skipSecondValue;
			}
			pe = pe->next;					
		}
		total++;
skipSecondValue:
		e = e->next;
	}
	stringArray = (char **) malloc(sizeof(char *) * (total + 1));
	if (!stringArray) {
		*result = 0;
		return cgiFormMemory;
	}
	/* initialize all entries to null; the last will stay that way */
	for (i=0; (i <= total); i++) {
		stringArray[i] = 0;
	}
	/* Now go get the entries */
	e = cgi->cgiFormEntryFirst;
	i = 0;
	while (e) {
		int space;
		/* Don't return a field name more than once if
			multiple values happen to be present for it */
		pe = cgi->cgiFormEntryFirst;
		while (pe != e) {
			if (!strcmp(e->attr, pe->attr)) {
				goto skipSecondValue2;
			}
			pe = pe->next;					
		}		
		space = (int) strlen(e->attr) + 1;
		stringArray[i] = (char *) malloc(space);
		if (stringArray[i] == 0) {
			/* Memory problems */
			cgiStringArrayFree(stringArray);
			*result = 0;
			return cgiFormMemory;
		}	
		strcpy(stringArray[i], e->attr);
		i++;
skipSecondValue2:
		e = e->next;
	}
	*result = stringArray;
	return cgiFormSuccess;
}

#define TRYPUTC(ch) \
	{ \
		if (FCGX_PutChar((ch), cgi->request->out) == EOF) { \
			return cgiFormIO; \
		} \
	} 

cgiFormResultType cgiHtmlEscapeData(CGI_T *cgi,char *data, int len)
{
	while (len--) {
		if (*data == '<') {
			TRYPUTC('&');
			TRYPUTC('l');
			TRYPUTC('t');
			TRYPUTC(';');
		} else if (*data == '&') {
			TRYPUTC('&');
			TRYPUTC('a');
			TRYPUTC('m');
			TRYPUTC('p');
			TRYPUTC(';');
		} else if (*data == '>') {
			TRYPUTC('&');
			TRYPUTC('g');
			TRYPUTC('t');
			TRYPUTC(';');
		} else {
			TRYPUTC(*data);
		}
		data++;
	}
	return cgiFormSuccess;
}

cgiFormResultType cgiHtmlEscape(CGI_T *cgi,char *s)
{
	return cgiHtmlEscapeData(cgi,s, (int) strlen(s));
}

/* Output data with the " character HTML-escaped, and no
	other characters escaped. This is useful when outputting
	the contents of a tag attribute such as 'href' or 'src'.
	'data' is not null-terminated; 'len' is the number of
	bytes in 'data'. Returns cgiFormIO in the event
	of error, cgiFormSuccess otherwise. */
cgiFormResultType cgiValueEscapeData(CGI_T *cgi,char *data, int len)
{
	while (len--) {
		if (*data == '\"') {
			TRYPUTC('&');
			TRYPUTC('#');
			TRYPUTC('3');
			TRYPUTC('4');
			TRYPUTC(';');
		} else {
			TRYPUTC(*data);
		}
		data++;
	}
	return cgiFormSuccess;
}

cgiFormResultType cgiValueEscape(CGI_T *cgi,char *s)
{
	return cgiValueEscapeData(cgi,s, (int) strlen(s));
}


