#ifndef IX_MANAGER_H_H
#define IX_MANAGER_H_H

#include "RM_Manager.h"
#include "PF_Manager.h"

typedef struct{
	int attrLength;
	int keyLength;
	AttrType attrType;
	PageNum rootPage;
	PageNum first_leaf;
	int order;
}IX_FileHeader;

typedef struct{
	bool bOpen;
	PF_FileHandle fileHandle;
	IX_FileHeader fileHeader;
}IX_IndexHandle;

typedef struct{
	int is_leaf;
	int keynum;
	PageNum parent;
	PageNum brother;
	char *keys;
	RID *rids;
}IX_Node;

typedef struct{
	bool bOpen;		/*扫描是否打开 */
	IX_IndexHandle *pIXIndexHandle;	//指向索引文件操作的指针
	CompOp compOp;  /* 用于比较的操作符*/
	char *value;		 /* 与属性行比较的值 */
    PF_PageHandle pfPageHandles[PF_BUFFER_SIZE]; // 固定在缓冲区页面所对应的页面操作列表
	PageNum pnNext; 	//下一个将要被读入的页面号
	int  ridIx;		//扫描即将处理的索引项编号
}IX_IndexScan;

RC CreateIndex(char * fileName,AttrType attrType,int attrLength);
RC OpenIndex(char *fileName,IX_IndexHandle *indexHandle);
RC CloseIndex(IX_IndexHandle *indexHandle);

RC InsertEntry(IX_IndexHandle *indexHandle, void *pData, RID * rid);
RC DeleteEntry(IX_IndexHandle *indexHandle,void *pData, RID * rid);
RC OpenIndexScan(IX_IndexScan *indexScan,IX_IndexHandle *indexHandle,CompOp compOp,char *value);
RC IX_GetNextEntry(IX_IndexScan *indexScan,RID * rid);
RC CloseIndexScan(IX_IndexScan *indexScan);

//将给定的关键字和指针写到指定的页面
void insertKeyAndRidToPage(PF_PageHandle *pageHandle, int order, AttrType attrType, int attrLength, void *pData, RID *rid, bool insertIfTrue);
//设置新页面节点控制信息
void copyNewNodeInfoToPage(PF_PageHandle *pageHandle, PageNum brother, PageNum parent, int is_leaf, int keynum);
//复制分裂后的关键字和指针到新页面
void  copyKeysAndRIDsToPage(PF_PageHandle *pageHandle, void *keySrc, int attrLength, int num, int order, void *ridSrc);
//设置新页面节点控制信息
void copyNewNodeInfoToPage(PF_PageHandle *pageHandle, PageNum brother, PageNum parent, int is_leaf, int keynum);
//删除前查找节点中的关键字
void findKeyAndRidForDelete(PF_PageHandle *pageHandle, int order, AttrType attrType, int attrLength, void *pData, RID *rid, bool *existence);
//RID的比较
bool compareRid(RID *src, RID *des);
//从兄弟节点中借节点或者合并
void getFromBrother(PF_PageHandle *pageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, int *status);
//简单地从某个父节点页面中删除某个分支或者修改关键字
void deleteChildNode(PF_PageHandle *parentPageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, PageNum nodePageNum, bool deleteIfTrue, void *pData);
void theFirstEqualScan(IX_IndexHandle *indexHandle, void *pData, PageNum *startPage, int *ridIx, bool *existence);     //找到第一个关键字相等所在的页面startPage和位置ridIx
																													   //大于扫描
void theLEqualScan(IX_IndexHandle *indexHandle, void *pData, PageNum *startPage, int *ridIx);    //找到第一个关键字大于所在的页面startPage和位置ridIx
																								 //关键字的比较
int keyCompare(void *data1, void *data2, AttrType attrType);
//找出当前节点的左兄弟节点
void findLeftBrother(PF_PageHandle *pageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, PageNum *leftBrother);
//与左兄弟节点进行处理
void getFromLeft(PF_PageHandle *pageHandle, PF_PageHandle *leftHandle, int order, AttrType attrType, int attrLength, int *status);
//与右兄弟节点进行处理
void getFromRight(PF_PageHandle *pageHandle, PF_PageHandle *rightHandle, int order, AttrType attrType, int attrLength, int *status);

#endif