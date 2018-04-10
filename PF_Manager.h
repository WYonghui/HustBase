#ifndef PF_MANAGER_H_H
#define PF_MANAGER_H_H

#include <stdio.h>
/*#include <stdlib.h>*/
#include<io.h>
#include<fcntl.h>
#include<sys/types.h>

#include<sys/stat.h>
#include<string.h>
#include <time.h>
#include <malloc.h>
#include<windows.h>
#define PF_PAGE_SIZE ((1<<12)-4)
#define PF_FILESUBHDR_SIZE (sizeof(PF_FileSubHeader))
#define PF_BUFFER_SIZE 50//页面缓冲区的大小
#define PF_PAGESIZE (1<<12)
typedef unsigned int PageNum;

typedef struct{
	PageNum pageNum;
	char pData[PF_PAGE_SIZE];
}Page;

typedef struct{
	PageNum pageCount;
	int nAllocatedPages;
}PF_FileSubHeader;

typedef struct{
	bool bDirty;
	unsigned int pinCount;
	clock_t  accTime;
	char *fileName;
	int fileDesc;
	Page page;
}Frame;

typedef struct{
	bool bopen;
	char *fileName;
	int fileDesc;
	Frame *pHdrFrame;
	Page *pHdrPage;
	char *pBitmap;
	PF_FileSubHeader *pFileSubHeader;
}PF_FileHandle;

typedef struct{
	int nReads;
	int nWrites;
	Frame frame[PF_BUFFER_SIZE];
	bool allocated[PF_BUFFER_SIZE];
}BF_Manager;



typedef struct{
	bool bOpen;
	Frame *pFrame;
}PF_PageHandle;

#ifndef RC_HH
#define RC_HH
typedef enum{
	SUCCESS=0,
		PF_EXIST,
		PF_FILEERR,
		PF_INVALIDNAME,
		PF_WINDOWS,
		PF_FHCLOSED,
		PF_FHOPEN,
		PF_PHCLOSED,
		PF_PHOPEN,
		PF_NOBUF,
		PF_EOF,
		PF_INVALIDPAGENUM,
		PF_NOTINBUF,
		PF_PAGEPINNED,
		RM_FHCLOSED,
		RM_FHOPENNED,
		RM_INVALIDRECSIZE,
		RM_INVALIDRID,
		RM_FSCLOSED,
		RM_NOMORERECINMEM,
		RM_FSOPEN,
		RM_NOIDLERECORDSLOT,
		RM_EOF,
		IX_IHOPENNED,
		IX_IHCLOSED,
		IX_INVALIDKEY,
		IX_NOMEM,
		RM_NOMOREIDXINMEM,
		IX_EOF,
		IX_SCANCLOSED,
		IX_ISCLOSED,
		IX_NOMOREIDXINMEM,
		IX_SCANOPENNED,
		TABLE_NOTEXITST,    //查询的表不存在
		FAIL,
		SQL_SYNTAX=-10
}RC;
#endif


const RC CreateFile(const char *fileName);
const RC OpenFile(char *fileName,PF_FileHandle *fileHandle);
const RC CloseFile(PF_FileHandle *fileHandle);

const RC GetThisPage(PF_FileHandle *fileHandle,PageNum pageNum,PF_PageHandle *pageHandle);
const RC AllocatePage(PF_FileHandle *fileHandle,PF_PageHandle *pageHandle);
const RC GetPageNum(PF_PageHandle *pageHandle,PageNum *pageNum);

const RC GetData(PF_PageHandle *pageHandle,char **pData);
const RC DisposePage(PF_FileHandle *fileHandle,PageNum pageNum);

const RC MarkDirty(PF_PageHandle *pageHandle);

const RC UnpinPage(PF_PageHandle *pageHandle);

#endif