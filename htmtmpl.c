/*++
Modified: liuxingzhi@2008.12.4: used for multi-thread environment.
修改:
(1)修改能够用于多线程环境。
(2)变量采用[...]格式，而不用原来的{...}。因为HTML模板中css样式会使用{...}需要修改四行。
(3)变量采用^...$格式，因为js中数组会使用[]。
(4)最大模板文件大小改为256K.
(5)最大文件名修改为128
--*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "htmtmpl.h"

/************************************************************************/
/* Linklist.                                                    */
/************************************************************************/

/*NOINC*/
#define TRUE 1
#define FALSE 0

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY* Flink;
    struct _LIST_ENTRY* Blink;
} LIST_ENTRY;
typedef LIST_ENTRY* PLIST_ENTRY;

/*
 * Linked List Manipulation Functions - from NDIS.H
 */

/*
 * Calculate the address of the base of the structure given its type, and an
 * address of a field within the structure. - from NDIS.H
 */
#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                          (unsigned char*)(address) - \
                          (unsigned char*)(&((type *)0)->field)))
#endif  /* CONTAINING_RECORD */

/*
 *  Doubly-linked list manipulation routines.  Implemented as macros
 */
#define InitializeListHead(ListHead) \
    ((ListHead)->Flink = (ListHead)->Blink = (ListHead) )

#define IsListEmpty(ListHead) \
    (( ((ListHead)->Flink == (ListHead)) ? TRUE : FALSE ) )

#define RemoveHeadList(ListHead) \
    (ListHead)->Flink;\
    {\
    PLIST_ENTRY FirstEntry;\
    FirstEntry = (ListHead)->Flink;\
    FirstEntry->Flink->Blink = (ListHead);\
    (ListHead)->Flink = FirstEntry->Flink;\
    }

#define RemoveEntryList(Entry) do {\
    PLIST_ENTRY _EX_Entry;\
    _EX_Entry = (Entry);\
    _EX_Entry->Blink->Flink = _EX_Entry->Flink;\
    _EX_Entry->Flink->Blink = _EX_Entry->Blink;\
    } while(0)

PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead);

#define InsertTailList(ListHead,Entry) do {\
    (Entry)->Flink = (ListHead);\
    (Entry)->Blink = (ListHead)->Blink;\
    (ListHead)->Blink->Flink = (Entry);\
    (ListHead)->Blink = (Entry);\
    } while(0)

#define InsertHeadList(ListHead,Entry) do {\
    (Entry)->Flink = (ListHead)->Flink;\
    (Entry)->Blink = (ListHead);\
    (ListHead)->Flink->Blink = (Entry);\
    (ListHead)->Flink = (Entry);\
    } while (0)

/*INC*/

/* the added macros to extend the linklist operation */

/* insert a new entry before a list entry */
#define InsertBeforeList(ListEntry,Entry) do {\
    (Entry)->Flink = (ListEntry);\
    (Entry)->Blink = (ListEntry)->Blink;\
    (ListEntry)->Blink->Flink = (Entry);\
    (ListEntry)->Blink = (Entry);\
    } while (0)

/* insert a new entry after a list entry */
#define InsertAfterList(ListEntry,Entry) do {\
    (Entry)->Flink = (ListEntry)->Flink;\
    (Entry)->Blink = (ListEntry);\
    (ListEntry)->Flink->Blink = (Entry);\
    (ListEntry)->Flink = (Entry);\
    } while (0)

/*
 * List access methods.
 */
#define ListFor(ListEntry,ListHead)  \
	for(ListEntry=(ListHead)->Flink;ListEntry!=ListHead;ListEntry=ListEntry->Flink)

PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY _Tail_Entry;
    _Tail_Entry = ListHead->Blink;
    RemoveEntryList(_Tail_Entry);
    return _Tail_Entry;
}

/************************************************************************/
/* Type declaration.                                                    */
/************************************************************************/

#define MAX_HT_FILE_SIZE  (256*1024)
#define MAX_HT_NAME_SIZE  128        //32

#define HT_TYPE_TEXT  0
#define HT_TYPE_VAR   1
#define HT_TYPE_BLOCK 2

typedef struct _HT_BLOCK
{
    LIST_ENTRY Node;

    int  Type;
    char Name[MAX_HT_NAME_SIZE+1];

    union
    {
        char* Text;
        char* Value;
        LIST_ENTRY SubBlockList;
    };

}HT_BLOCK, *PHT_BLOCK;


/************************************************************************/
/* Global variables.                                                    */
/************************************************************************/

//LIST_ENTRY MainList;


/************************************************************************/
/* Help functions.                                                      */
/************************************************************************/

static char* LoadFile(char* File, int* Length)
{
    FILE* HtmFile;
    char* Buffer;
    int FileSize;

    /*
     *  Read file to buffer.
     */
    HtmFile = fopen(File, "r+b");
    if(HtmFile == NULL)
        return NULL;

    fseek(HtmFile, 0, SEEK_END);
    FileSize = ftell(HtmFile);
    if(FileSize == -1 || FileSize > MAX_HT_FILE_SIZE)
    {
        fclose(HtmFile);
        return NULL;
    }

    Buffer = (char*)malloc(FileSize+1);
    if(Buffer == NULL)
    {
        fclose(HtmFile);
        return NULL;
    }

    fseek(HtmFile, 0, SEEK_SET);
    if(fread(Buffer, 1, FileSize, HtmFile) != (size_t)FileSize)
    {
        fclose(HtmFile);
        free(Buffer);
        return NULL;
    }

    fclose(HtmFile);
    Buffer[FileSize] = '\0';
    *Length = FileSize;

    return Buffer;
}

static int DeterminType(char* Buffer, int Length)
{
    if(Buffer[0] == '^')
        return HT_TYPE_VAR;

    if(Length > 9 && memcmp(Buffer, "<!--Begin", 9)==0)
        return HT_TYPE_BLOCK;

    else
        return HT_TYPE_TEXT;
}

static int GetTextSize(char* Buffer, int Length)
{
    int i;

    for(i=0; i<Length; i++)
    {
        if(Buffer[i] == '^')
            break;

        if(Buffer[i] == '<')
        {
            if(Length -i > 9 && memcmp(&Buffer[i], "<!--Begin", 9)==0)
                break;
        }
    }

    return i;
}

static int GetVarNameSize(char* Buffer, int Length)
{
    int i;

    if(Buffer[0] != '^')
        return 0;

    for(i=0; i<Length; i++)
    {
        if(Buffer[i] == '$')
            break;
    }

    return i-1;
}

static int GetBlockNameSize(char* Buffer, int Length)
{
    int i;
    
    for(i=0; i<Length; i++)
    {
        if(Buffer[i] == '-')
        {
            if(Length -i > 3 && memcmp(&Buffer[i], "-->", 3)==0)
                break;
        }
    }

    return i-9;
}

static int GetBlockSize(char* Buffer, int Length, char* BlockName, int* Begin, int* End)
{
    int i;
    int BlockSize;    
    char BeginTag[MAX_HT_NAME_SIZE+13];
    char EndTag[MAX_HT_NAME_SIZE+11];
    int BeginTagLen;
    int EndTagLen;
    
    BlockSize = 0;
    sprintf(BeginTag, "<!--Begin%s-->", BlockName);
    sprintf(EndTag, "<!--End%s-->", BlockName);
    BeginTagLen = strlen(BeginTag);
    EndTagLen = strlen(EndTag);
    
    *Begin = BeginTagLen;

    for(i=BeginTagLen; i<Length; i++)
    {
        if(Buffer[i] == '<')
        {
            if(Length -i > EndTagLen && memcmp(&Buffer[i], EndTag, EndTagLen)==0)
            {
                *End = i;
                BlockSize = *End + EndTagLen;
                break;
            }
        }
    }

    return BlockSize;
}

static int ProcessBlock(PLIST_ENTRY pList, char* Buffer, int Length)
{
    PHT_BLOCK pBlock;
    int Size, BlockSize, Begin, End;
    
    if(Buffer == NULL)
        return -1;

    if(Length == 0)
        return 0;

    /*
     *  Process current Block.
     */
    pBlock = (PHT_BLOCK)malloc(sizeof(HT_BLOCK));
    if(pBlock == NULL)
        return -1;

    memset(pBlock, 0, sizeof(HT_BLOCK));
    InsertTailList(pList, &pBlock->Node);

    pBlock->Type = DeterminType(Buffer, Length);

    switch(pBlock->Type)
    {
    case HT_TYPE_TEXT:
        {
            strcpy(pBlock->Name, "Text");
            Size = GetTextSize(Buffer, Length);
            
            pBlock->Text = (char*)malloc(Size+1);
            if(pBlock->Text == NULL)
                return -1;
            
            memcpy(pBlock->Text, Buffer, Size);
            pBlock->Text[Size] = '\0';

            BlockSize = Size;
        }
        break;

    case HT_TYPE_VAR:
        {
            Size = GetVarNameSize(Buffer, Length);
            if(Size > MAX_HT_NAME_SIZE)
                return -1;

            memcpy(pBlock->Name, Buffer+1, Size);
            pBlock->Name[Size] = '\0';

            BlockSize = Size + 2;
        }
        break;

    case HT_TYPE_BLOCK:
        {
            Size = GetBlockNameSize(Buffer, Length);
            if(Size > MAX_HT_NAME_SIZE)
                return -1;
            
            memcpy(pBlock->Name, Buffer+9, Size);
            pBlock->Name[Size] = '\0';

            BlockSize = GetBlockSize(Buffer, Length, pBlock->Name, &Begin, &End);
            if(Begin<12 || End<12 || BlockSize<22)
                return -1;

            InitializeListHead(&pBlock->SubBlockList);            
            if(ProcessBlock(&pBlock->SubBlockList, Buffer+Begin, End-Begin) != 0)
                return -1;
        }
        break;

    default:
        return -1;
    }

    /*
     *  Process next block.
     */
    return ProcessBlock(pList, Buffer+BlockSize, Length-BlockSize);
}

//返回buffer中数据实际长度。size是buffer的大小。
static int OutputBlock(PLIST_ENTRY List, int OutputType, char* Buffer,int size)
{
    PLIST_ENTRY pNode;
    PHT_BLOCK pBlock;
    int Length,pos,len;

    Length = 0;
    pos = 0;
    if(Buffer)
        Buffer[0] = '\0';

    ListFor(pNode, List)
    {
        pBlock = CONTAINING_RECORD(pNode, HT_BLOCK, Node);

        switch(pBlock->Type)
        {
        case HT_TYPE_TEXT:
            {
                len = strlen(pBlock->Text);
                Length += len;
                if(OutputType == HT_OUTPUT_STDOUT)
                {
                    printf("%s", pBlock->Text);
                }
                else if(OutputType == HT_OUTPUT_STRING)
                {
                    if(len >= size - pos)
                        return -1;
                    memcpy(Buffer+pos,pBlock->Text,len);
                     pos += len;
                     Buffer[pos] = 0;
                }
            }
            break;

        case HT_TYPE_VAR:
            {
                if(pBlock->Value)
                {
                     len = strlen(pBlock->Text);
                     Length += len;
                    if(OutputType == HT_OUTPUT_STDOUT)
                    {
                        printf("%s", pBlock->Value);
                    }
                    else if(OutputType == HT_OUTPUT_STRING)
                    {
                        if(len >= size - pos)
                          return -1;
                        memcpy(Buffer+pos,pBlock->Text,len);
                        pos += len;
                        Buffer[pos] = 0;
                    }
                }
            }
            break;

        case HT_TYPE_BLOCK:
            /*Only parsed block is output*/
            break;

        default:
            break;
        }
    }

    return Length;
}

static void SetVar(PLIST_ENTRY List, char* Name, char* Value)
{
    PLIST_ENTRY pNode;
    PHT_BLOCK pBlock;

    ListFor(pNode, List)
    {
        pBlock = CONTAINING_RECORD(pNode, HT_BLOCK, Node);
        if(pBlock->Type == HT_TYPE_VAR) {
            if(strcmp(pBlock->Name, Name)==0) {
                if(pBlock->Value != NULL)
                    free(pBlock->Value);
                
                pBlock->Value = (char*)malloc(strlen(Value)+1);
                if(pBlock->Value != NULL)
                    strcpy(pBlock->Value, Value);
            }
        } else if(pBlock->Type == HT_TYPE_BLOCK)
            SetVar(&pBlock->SubBlockList, Name, Value);
    }
}

static int ParseBlock(PLIST_ENTRY List, char* Name, int ReverseFlag)
{
    PLIST_ENTRY pNode;
    PHT_BLOCK pBlock;
    PHT_BLOCK pNewBlock;
    int ParsedLength;

    ListFor(pNode, List)
    {
        pBlock = CONTAINING_RECORD(pNode, HT_BLOCK, Node);
        
        if(pBlock->Type == HT_TYPE_BLOCK)
        {
            if(strcmp(pBlock->Name, Name)==0)
            {
                ParsedLength = OutputBlock(&pBlock->SubBlockList, HT_OUTPUT_NULL, NULL,0);
                if(ParsedLength == 0)
                    return -1;

                pNewBlock = (PHT_BLOCK)malloc(sizeof(HT_BLOCK));
                if(pNewBlock == NULL)
                    return -1;

                strcpy(pNewBlock->Name, "Text");
                pNewBlock->Type = HT_TYPE_TEXT;
                pNewBlock->Text = (char*)malloc(ParsedLength+1);
                if(pNewBlock->Text == NULL)
                {
                    free(pNewBlock);
                    return -1;
                }

                OutputBlock(&pBlock->SubBlockList, HT_OUTPUT_STRING, pNewBlock->Text,ParsedLength+1);

                if(!ReverseFlag)
                    InsertBeforeList(pNode, &pNewBlock->Node);
                else
                    InsertAfterList(pNode, &pNewBlock->Node);

                return 0;
            } else {
                if(ParseBlock(&pBlock->SubBlockList, Name, ReverseFlag) == 0)
                    return 0;
            }
        }
    }

    return -1;
}

static void FreeList(PLIST_ENTRY pList)
{
    PLIST_ENTRY pNode;
    PHT_BLOCK pBlock;

    while(!IsListEmpty(pList))
    {
        pNode = RemoveHeadList(pList);
        pBlock = CONTAINING_RECORD(pNode, HT_BLOCK, Node);

        switch(pBlock->Type)
        {
        case HT_TYPE_TEXT:
            {
                free(pBlock->Text);
                free(pBlock);
            }
            break;

        case HT_TYPE_VAR:
            {
                if(pBlock->Value)
                    free(pBlock->Value);
                free(pBlock);
            }
            break;

        case HT_TYPE_BLOCK:
            {
                FreeList(&pBlock->SubBlockList);
                free(pBlock);
            }
            break;

        default:
            break;
        }
    }
}


/************************************************************************/
/* Interface.                                                           */
/************************************************************************/
//type: file or file content buffer.
//如果type > 0,file则是指文件内容，type是指文件内容长度，已经loaded
//如果type <= 0,file则是指文件路径，需要load
void *HTLoadFile(char* File,int type)
{
    char* Buffer;
    int Length;

    PLIST_ENTRY MainList;
    MainList = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
    if(!MainList)
        return NULL;
    
    /*Initialize it to avoid user invoke other functions when this function return -1*/
    InitializeListHead(MainList);

   if(type <= 0)
   {
         /* Read file to buffer.*/
         Buffer = LoadFile(File, &Length);
         if(Buffer == NULL)
         {
             free(MainList);
            return NULL;
         }
   }
   else if(type <= MAX_HT_FILE_SIZE)
   {
        Buffer = File;
        Length = type;
   }
   else
   {
        free(MainList);
        return NULL;
   }

    /*
     *  Process main block.
     */
    if(ProcessBlock(MainList, Buffer, Length) != 0)
    {
        if(type<=0)  free(Buffer);
        FreeList(MainList);
        free(MainList);
        return NULL;
    }

    if(type<=0) free(Buffer);

    return MainList;
}

void HTSetVar(void *p,char* Name, char* Value)
{
    PLIST_ENTRY MainList = (PLIST_ENTRY)p;
    SetVar(MainList, Name, Value);
}

void HTParse(void *p,char* Name, int ReverseFlag)
{
    PLIST_ENTRY MainList = (PLIST_ENTRY)p;
    ParseBlock(MainList, Name, ReverseFlag);
}

int  HTFinish(void *p,int OutputType, char* Buffer,int size)
{
    int len;
    
    PLIST_ENTRY MainList = (PLIST_ENTRY)p;
    len = OutputBlock(MainList, OutputType,Buffer,size);
    FreeList(MainList);
    free(MainList);
    return len;
}

#ifdef TEST_HTMTMPL

int main()
{
    //测试block嵌套和多次实例化
    char *tmpl = "BeginTest:\n\
<!--BeginAll-->all records:\n\
<!--BeginRec-->\
<!--BeginField-->Field#^number$: ^name$=^value$\n<!--EndField-->\
==========================================\n\
<!--EndRec-->\n\
<!--EndAll-->:EndTest";
    void *vp;
    char resbuf[1024],buf[32];
    int i,j;

    vp = HTLoadFile(tmpl,strlen(tmpl));
    if(!vp) {
        printf("HTLoadFile error.\n");
        return -1;
    }

    for(i=0;i<2;i++) {
      for(j=0;j<2;j++) {
        sprintf(buf,"%d",j);
        HTSetVar(vp,"number",buf);
        HTSetVar(vp,"name","name");
        HTSetVar(vp,"value","value");
        HTParse(vp,"Field",0);
      }
      HTParse(vp,"Rec",0);
    }
   
    HTParse(vp,"All",0);
    
    if(HTFinish(vp,HT_OUTPUT_STRING,resbuf,1024) < 0) {
        printf("HTFinish error.\n");
        return -1;
    }

    printf("%s.\n",resbuf);
    return 0;
}

/* output:
BeginTest:
all records:
Field#0: name=value
Field#1: name=value
==========================================
Field#0: name=value
Field#1: name=value
Field#0: name=value
Field#1: name=value
==========================================
*/

#endif

