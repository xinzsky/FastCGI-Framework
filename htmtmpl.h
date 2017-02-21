#ifndef HTMTMPL_H
#define HTMTMPL_H

#define HT_OUTPUT_NULL      0
#define HT_OUTPUT_STDOUT    1
#define HT_OUTPUT_STRING    2

#ifdef __cplusplus
extern "C" {
#endif

void *HTLoadFile(char* File,int type);
void HTSetVar(void *p,char* Name, char* Value);
void HTParse(void *p,char* Name, int ReverseFlag);
int  HTFinish(void *p,int OutputType, char* Buffer,int size);

#ifdef __cplusplus
}
#endif


#endif
