#include "stdafx.h"
#include "IX_Manager.h"


int threshold;

//索引文件的创建
/*
* fileName: 索引文件名
* attrType: 被索引属性的类型
* attrLength: 被索引属性的长度
*/
//RC CreateIndex(const char * fileName, AttrType attrType, int attrLength)
RC CreateIndex(char * fileName, AttrType attrType, int attrLength)
{
	CreateFile(fileName);  //创建索引文件

	PF_FileHandle *fileHandle = NULL;
	fileHandle = (PF_FileHandle *)malloc(sizeof(PF_FileHandle));
	OpenFile(fileName, fileHandle);	//打开索引文件

	PF_PageHandle *firstPageHandle = NULL;
	firstPageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	AllocatePage(fileHandle, firstPageHandle);		//分配索引文件的第一个页面
	PageNum pageNum;
	GetPageNum(firstPageHandle, &pageNum);

	//生成索引控制信息
	IX_FileHeader index_FileHeader;
	index_FileHeader.attrLength = attrLength;
	index_FileHeader.keyLength = attrLength + sizeof(RID);
	index_FileHeader.attrType = attrType;
	index_FileHeader.rootPage = pageNum;
	index_FileHeader.first_leaf = pageNum;
	int order = (PF_PAGE_SIZE - (sizeof(IX_FileHeader) + sizeof(IX_Node))) / (2 * sizeof(RID) + attrLength);
	index_FileHeader.order = order;
	threshold = order >> 1;

	//获取第一页的数据区
	char *pData;
	GetData(firstPageHandle, &pData);
	memcpy(pData, &index_FileHeader, sizeof(IX_FileHeader));	//将索引控制信息复制到第一页

																//初始化节点控制信息，将根节点的置为叶子节点，关键字数为0，
	IX_Node index_NodeControl;
	index_NodeControl.brother = 0;
	index_NodeControl.is_leaf = 1;
	index_NodeControl.keynum = 0;
	index_NodeControl.parent = 0;
	memcpy(pData + sizeof(IX_FileHeader), &index_NodeControl, sizeof(IX_Node));

	MarkDirty(firstPageHandle);

	UnpinPage(firstPageHandle);

	//关闭索引文件
	CloseFile(fileHandle);
	free(firstPageHandle);
	free(fileHandle);
	return SUCCESS;
}

//索引文件的打开
RC OpenIndex(char *fileName, IX_IndexHandle *indexHandle)
{
	//打开索引文件
	PF_FileHandle fileHandle;
	RC rc;
	if ((rc = OpenFile(fileName, &fileHandle)) != SUCCESS) {
		return rc;
	}

	PF_PageHandle *pageHandle = NULL;
	pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	GetThisPage(&fileHandle, 1, pageHandle);   //获取第一页

	char *pData;
	GetData(pageHandle, &pData);    //获取第一页的数据区

	IX_FileHeader fileHeader;
	memcpy(&fileHeader, pData, sizeof(IX_FileHeader));  //复制第一页索引控制信息

	indexHandle->bOpen = true;
	indexHandle->fileHandle = fileHandle;
	indexHandle->fileHeader = fileHeader;
	free(pageHandle);
	return SUCCESS;
}

//关闭索引文件
RC CloseIndex(IX_IndexHandle *indexHandle)
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	CloseFile(&fileHandle);
	return SUCCESS;
}

//索引的插入
RC InsertEntry(IX_IndexHandle *indexHandle, void *pData, RID * rid)
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	IX_FileHeader fileHeader = indexHandle->fileHeader;
	PF_PageHandle *pageHandle = NULL;

	//索引文件页面的序数
	int order = fileHeader.order;

	//索引关键字的长度
	int attrLength = fileHeader.attrLength;

	//获取根节点页面
	pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	pageHandle->bOpen = false;
	GetThisPage(&fileHandle, fileHeader.rootPage, pageHandle);

	//获取根节点页面的数据区
	char *pageData;
	GetData(pageHandle, &pageData);

	//获取根节点页面得节点控制信息
	IX_Node* index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));

	//判断节点如果是叶子节点
	while (index_NodeControlInfo->is_leaf != 1)
	{
		RID tempRid; 
		insertKeyAndRidToPage(pageHandle, order, fileHeader.attrType, fileHeader.attrLength, pData, &tempRid, false);    //查找将要插入关键字的页面
		GetThisPage(&fileHandle, tempRid.pageNum, pageHandle);
		GetData(pageHandle, &pageData);
		index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
	}

	insertKeyAndRidToPage(pageHandle, order, fileHeader.attrType, fileHeader.attrLength, pData, rid, true);    //向页面插入关键字	

	while (index_NodeControlInfo->keynum == order)
	{   //进行分裂

		int keynum = index_NodeControlInfo->keynum;
		//获取关键字区
		char *keys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		//获取指针区
		char *rids = keys + order*attrLength;

		PageNum nodePage;
		GetPageNum(pageHandle, &nodePage);

		//新叶子节点页面
		PF_PageHandle *newLeafPageHandle = NULL;
		newLeafPageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
		newLeafPageHandle->bOpen = false;
		AllocatePage(&fileHandle, newLeafPageHandle);
		PageNum newLeafPage;
		GetPageNum(newLeafPageHandle, &newLeafPage);

		int divide1 = keynum >> 1;
		int divide2 = keynum - divide1;

		if (index_NodeControlInfo->parent == 0)   //说明当前分裂的节点为根节点
		{
			//生成新的根页面
			PF_PageHandle *newRootPageHandle = NULL;
			newRootPageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
			newRootPageHandle->bOpen = false;
			AllocatePage(&fileHandle, newRootPageHandle);
			PageNum newRootPage;
			GetPageNum(newRootPageHandle, &newRootPage);
			copyNewNodeInfoToPage(newRootPageHandle, 0, 0, 0, 0);	//设置新根节点的节点控制信息

			copyNewNodeInfoToPage(newLeafPageHandle, index_NodeControlInfo->brother, newRootPage, index_NodeControlInfo->is_leaf, divide2);  //设置新分裂节点的节点控制信息			
			copyNewNodeInfoToPage(pageHandle, newLeafPage, newRootPage, index_NodeControlInfo->is_leaf, divide1);	//设置原节点控制信息

			copyKeysAndRIDsToPage(newLeafPageHandle, keys + divide1*attrLength, attrLength, divide2, order, rids + divide1 * sizeof(RID));   //复制关键字和指针到分裂后新的页面中

			char *tempData;
			GetData(pageHandle, &tempData);
			RID tempRid;
			tempRid.bValid = false;
			tempRid.pageNum = nodePage;
			insertKeyAndRidToPage(newRootPageHandle, order, fileHeader.attrType, fileHeader.attrLength, tempData, &tempRid, true);   //向新的根节点插入子节点的关键字和指针

			GetData(newLeafPageHandle, &tempData);
			tempRid.pageNum = newLeafPage;
			insertKeyAndRidToPage(newRootPageHandle, order, fileHeader.attrType, fileHeader.attrLength, tempData, &tempRid, true);  //向新的根节点插入子节点的关键字和指针

			indexHandle->fileHeader.rootPage = newRootPage;		//修改索引控制信息中的根节点页面
			free(newRootPageHandle);
		}
		else		//说明当前分裂的节点不是根节点
		{
			PageNum parentPage = index_NodeControlInfo->parent;
			copyNewNodeInfoToPage(newLeafPageHandle, nodePage, parentPage, index_NodeControlInfo->is_leaf, divide2);  //设置新分裂节点的节点控制信息
			copyNewNodeInfoToPage(pageHandle, newLeafPage, parentPage, index_NodeControlInfo->is_leaf, divide1);	//设置原节点控制信息
			copyKeysAndRIDsToPage(newLeafPageHandle, keys + divide1*attrLength, attrLength, divide2, order, rids + divide1 * sizeof(RID));   //复制关键字和指针到分裂后新的页面中

			char *tempData;
			GetData(newLeafPageHandle, &tempData);

			RID tempRid;
			tempRid.bValid = false;
			tempRid.pageNum = newLeafPage;

			GetThisPage(&fileHandle, parentPage, pageHandle);   //令pageHandle指向其父节点页面
			insertKeyAndRidToPage(pageHandle, order, fileHeader.attrType, fileHeader.attrLength, tempData, &tempRid, true);  //向父节点插入新子节点的关键字和指针

			GetData(pageHandle, &pageData);   //令pageData指向父节点的数据区
			index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));   //父节点的节点控制信息
		}
		free(newLeafPageHandle);
	}

	free(pageHandle);

	return SUCCESS;
}

//索引的删除
RC DeleteEntry(IX_IndexHandle *indexHandle, void *pData, RID * rid)
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	IX_FileHeader fileHeader = indexHandle->fileHeader;
	PF_PageHandle *pageHandle = NULL;

	//索引文件页面的序数
	int order = fileHeader.order;

	//索引关键字的长度
	int attrLength = fileHeader.attrLength;

	//获取根节点页面
	GetThisPage(&fileHandle, fileHeader.rootPage, pageHandle);

	//获取根节点页面的数据区
	char *pageData;
	GetData(pageHandle, &pageData);

	//获取根节点页面得节点控制信息
	IX_Node* index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));

	//判断节点如果不是叶子节点
	while (index_NodeControlInfo->is_leaf != 1)
	{
		RID tempRid;
		bool existence = false;
		findKeyAndRidForDelete(pageHandle, order, fileHeader.attrType, fileHeader.attrLength, pData, &tempRid, &existence);   //查找关键字对应的页面
		if (existence)
		{
			GetThisPage(&fileHandle, tempRid.pageNum, pageHandle);
			GetData(pageHandle, &pageData);
			index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
		}
		else
			return IX_EOF;
	}

	int keynum = index_NodeControlInfo->keynum;

	char *parentKeys;
	char *parentRids;
	int flag = 0;

	//获取关键字区
	parentKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	parentRids = parentKeys + order*attrLength;

	int position = 0;
	switch (fileHeader.attrType)
	{
	case chars:
		for (; position < keynum; position++) {
			if (strcmp(parentKeys + position*attrLength, (char*)pData) > 0)
				break;
			else if (strcmp((char*)pData, parentKeys + position*attrLength) == 0 && compareRid(rid, (RID*)(parentRids + position * sizeof(RID))))
			{
				flag = 1;
				break;
			}
		}
		break;
	case ints:
		int data1;
		data1 = *((int *)pData);
		for (; position < keynum; position++) {
			int data2 = *((int *)parentKeys + position*attrLength);
			if (data2 > data1)
				break;
			if (data1 == data2 && compareRid(rid, (RID*)(parentRids + position * sizeof(RID))))
			{
				flag = 1;
				break;
			}
		}
		break;
	case floats:
		float data_floats;
		data_floats = *((float *)pData);
		for (; position < keynum; position++) {
			float data2 = *((float *)parentKeys + position*attrLength);
			if (data2 > data_floats)
				break;
			if (data_floats == data2 && compareRid(rid, (RID*)(parentRids + position * sizeof(RID))))
			{
				flag = 1;
				break;
			}
		}
		break;
	}

	if (flag == 1)   //说明找到相应的关键字和记录
	{

		memcpy(parentKeys + position*attrLength, parentKeys + (position + 1)*attrLength, (keynum - position - 1)*attrLength);   //将关键字往前移动一个位置
		memcpy(parentRids + position * sizeof(RID), parentRids + (position + 1) * sizeof(RID), (keynum - position - 1) * sizeof(RID));   //将记录指针往前移动一个单位
		keynum--;   //关键字个数减1
		index_NodeControlInfo->keynum = keynum;

		while (index_NodeControlInfo->parent != 0)   //说明为非根节点
		{
			int status = 0;
			if (keynum < threshold)   //B+树的非根节点的分支树必须大于threshold，需要向兄弟节点借一个节点，或者与兄弟节点进行合并
			{
				getFromBrother(pageHandle, &fileHandle, order, fileHeader.attrType, attrLength, &status);   //对兄弟节点进行处理
			}

			PF_PageHandle *parentPageHandle = NULL;
			PageNum nodePageNum;

			GetThisPage(&fileHandle, index_NodeControlInfo->parent, parentPageHandle);
			GetPageNum(pageHandle, &nodePageNum);


			if (status == 2)   //说明与左节点进行了合并
			{
				deleteChildNode(parentPageHandle, &fileHandle, order, fileHeader.attrType, attrLength, nodePageNum, true, NULL);   //进行删除
				pageHandle = parentPageHandle;    //指向父节点
				GetData(pageHandle, &pageData);
				index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
				//获取关键字区
				parentKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);   //父节点的关键字区
				keynum = index_NodeControlInfo->keynum;
			}
			else if (status == 4)   //说明右节点被进行合并了
			{
				if (position == 0)   //同时说明该节点的第一个元素被删除掉了，需要修改
				{
					deleteChildNode(parentPageHandle, &fileHandle, order, fileHeader.attrType, attrLength, nodePageNum, false, parentKeys);   //递归进行修改
					position = (position == 0 ? 1 : position);   //防止再次重复该动作
				}

				GetThisPage(&fileHandle, index_NodeControlInfo->brother, pageHandle);   //令pageHandle指向右节点
				GetData(pageHandle, &pageData);    //获取右节点的数据区
				GetPageNum(pageHandle, &nodePageNum);   //获取右节点的页面号

				index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
				GetThisPage(&fileHandle, index_NodeControlInfo->parent, parentPageHandle);    //获取右节点的父节点

				deleteChildNode(parentPageHandle, &fileHandle, order, fileHeader.attrType, attrLength, nodePageNum, true, NULL);    //从父节点中删除右节点对应的关键字

				pageHandle = parentPageHandle;    //指向父节点
				GetData(pageHandle, &pageData);
				index_NodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
				//获取关键字区
				parentKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);   //父节点的关键字区
				keynum = index_NodeControlInfo->keynum;
			}
			else if (status == 1 || position == 0)   //向左节点借，或者position=0，
			{
				deleteChildNode(parentPageHandle, &fileHandle, order, fileHeader.attrType, attrLength, nodePageNum, false, parentKeys);   //递归进行修改
				break;   //终止循环
			}

		}
		return FAIL;
	}
	else
		return FAIL;

}


//打开基于条件的扫描
RC OpenIndexScan(IX_IndexScan *indexScan, IX_IndexHandle *indexHandle, CompOp compOp, char *value)
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	IX_FileHeader fileHeader = indexHandle->fileHeader;


	indexScan->compOp = compOp;
	indexScan->value = value;
	indexScan->bOpen = true;
	indexScan->pIXIndexHandle = indexHandle;


	int ridIx = 0;
	PageNum startPage = 0;
	bool existence;

	switch (compOp)
	{
	case EQual:
		theFirstEqualScan(indexHandle, value, &startPage, &ridIx, &existence);
		if (existence)
			indexScan->pnNext = startPage;
		else
			indexScan->pnNext = 0;
		indexScan->ridIx = ridIx;
		break;
	case LEqual:
		indexScan->pnNext = fileHeader.first_leaf;
		indexScan->ridIx = ridIx;
		break;
	case NEqual:
		indexScan->pnNext = fileHeader.first_leaf;
		indexScan->ridIx = ridIx;
		break;
	case LessT:
		indexScan->pnNext = fileHeader.first_leaf;
		indexScan->ridIx = ridIx;
		break;
	case GEqual:
		theFirstEqualScan(indexHandle, value, &startPage, &ridIx, &existence);
		indexScan->pnNext = startPage;
		indexScan->ridIx = ridIx;
		break;
	case GreatT:
		theLEqualScan(indexHandle, value, &startPage, &ridIx);
		indexScan->pnNext = startPage;
		indexScan->ridIx = ridIx;
		break;
	case NO_OP:
		indexScan->pnNext = fileHeader.first_leaf;
		indexScan->ridIx = ridIx;
		break;
	default:
		break;
	}

	return SUCCESS;
}

//获取记录
RC IX_GetNextEntry(IX_IndexScan *indexScan, RID * rid)
{
	if (indexScan->bOpen)   //判断索引扫描是否已经打开
	{
		PageNum pageNum = indexScan->pnNext;    //获取即将处理的页面
		CompOp compOp = indexScan->compOp;   //比较符
		IX_IndexHandle *indexHandle = indexScan->pIXIndexHandle;    //索引文件操作符
		char *value = indexScan->value;   //属性值
		int  ridIx = indexScan->ridIx;    //即将处理的索引项编号

		if (pageNum == 0)   //说明不存在或者已经扫描完毕
			rid = NULL;
		else
		{
			PF_FileHandle fileHandle = indexHandle->fileHandle;
			IX_FileHeader fileHeader = indexHandle->fileHeader;

			//索引文件页面的序数
			int order = fileHeader.order;

			//索引关键字的长度
			int attrLength = fileHeader.attrLength;

			PF_PageHandle *pageHandle = NULL;
			GetThisPage(&fileHandle, pageNum, pageHandle);   //获取页面

			char *pageData;
			char *pageKeys;
			char *pageRids;
			GetData(pageHandle, &pageData);
			IX_Node* pageNodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
			int pageKeynum = pageNodeControlInfo->keynum;
			//获取关键字区
			pageKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
			//获取指针区
			pageRids = pageKeys + order*attrLength;

			if (ridIx == pageKeynum)   //说明已经扫描完当前页了
			{
				if (pageNodeControlInfo->brother == 0)   //说明已经扫描到文件的末尾
				{
					rid = NULL;
					return IX_EOF;
				}
				else
				{
					pageNum = pageNodeControlInfo->brother;
					indexScan->pnNext = pageNum;   //修改扫描结构体中相对应的页面

					GetThisPage(&fileHandle, pageNum, pageHandle);   //获取下一个叶子页面
					GetData(pageHandle, &pageData);
					pageNodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
					pageKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
					pageRids = pageKeys + order*attrLength;
					ridIx = 0;
				}
			}

			int temp = keyCompare(pageKeys + ridIx*attrLength, value, fileHeader.attrType);   //关键字比较
			switch (compOp)
			{
			case EQual:
				if (temp == 0)   //说明仍然符合条件
				{
					memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
					ridIx++;
					indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
					return SUCCESS;
				}
				else
				{
					rid = NULL;
					return SUCCESS;
				}

				break;
			case LEqual:
				if (temp <= 0)   //说明仍然符合条件
				{
					memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
					ridIx++;
					indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
					return SUCCESS;
				}
				else
				{
					rid = NULL;
					return SUCCESS;
				}
				break;
			case NEqual:
				if (temp != 0)   //说明仍然符合条件
				{
					memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
					ridIx++;
					indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
					return SUCCESS;
				}
				else
				{
					rid = NULL;
					return SUCCESS;
				}
				break;
			case LessT:
				if (temp < 0)   //说明仍然符合条件
				{
					memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
					ridIx++;
					indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
					return SUCCESS;
				}
				else
				{
					rid = NULL;
					return SUCCESS;
				}
				break;
			case GEqual:    //直接扫描到最后即可
				memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
				ridIx++;
				indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
				break;
			case GreatT:   //直接扫描直到最后
				memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
				ridIx++;
				indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
				break;
			case NO_OP:    //直接扫描到最后即可
				memcpy(rid, pageRids + ridIx * sizeof(RID), sizeof(RID));
				ridIx++;
				indexScan->ridIx = ridIx;   //修改扫描结构体中索引项编号
				break;
			}
		}
		return SUCCESS;
	}
	else {
		rid = NULL;
		return SUCCESS;
	}
}

//关闭索引扫描
RC CloseIndexScan(IX_IndexScan *indexScan)
{
	free(indexScan);
	indexScan = NULL;
	return SUCCESS;
}


void theFirstEqualScan(IX_IndexHandle *indexHandle, void *pData, PageNum *startPage, int *ridIx, bool *existence)     //找到第一个关键字相等所在的页面startPage和位置ridIx
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	IX_FileHeader fileHeader = indexHandle->fileHeader;

	PF_PageHandle *pageHandle = NULL;

	//索引文件页面的序数
	int order = fileHeader.order;
	//索引关键字的长度
	int attrLength = fileHeader.attrLength;
	//关键字类型
	AttrType attrType = fileHeader.attrType;

	char *pageData;
	char *pKeys;
	char *pRids;
	//获取根节点页面
	GetThisPage(&fileHandle, fileHeader.rootPage, pageHandle);

	//获取根节点页面的数据区
	GetData(pageHandle, &pageData);

	//获取根节点页面得节点控制信息
	IX_Node* nodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));

	int keynum = nodeControlInfo->keynum;

	int position;
	int flag;
	while (true)   //往下遍历直到叶子节点
	{
		//获取关键字区
		pKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		//获取指针区
		pRids = pKeys + order*attrLength;

		for (position = 0, flag = 0; position < keynum; position++)
		{
			int temp = keyCompare(pKeys + position*attrLength, pData, attrType);
			if (temp == 0)
			{
				flag = 1;
				break;
			}
			else if (temp > 0)
				break;
		}
		if (flag == 0 && nodeControlInfo->is_leaf == 0)     //说明还不是叶子节点
		{
			if (position > 0)
				position--;
		}
		else if (nodeControlInfo->is_leaf == 1)   //说明当前节点已经是叶子节点
		{
			if (flag == 1)
				*existence = true;
			else
				*existence = false;

			GetPageNum(pageHandle, startPage);  //说明找到第一个关键字相等或者第一个大于的关键字
			*ridIx = position;
			return;
		}
		RID *tempRid = (RID*)(pRids + position * sizeof(RID));
		GetThisPage(&fileHandle, tempRid->pageNum, pageHandle);   //获取其子节点页面
		GetData(pageHandle, &pageData);   //获取子节点数据区
		nodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
		keynum = nodeControlInfo->keynum;
	}

}

//大于扫描
void theLEqualScan(IX_IndexHandle *indexHandle, void *pData, PageNum *startPage, int *ridIx)    //找到第一个关键字大于所在的页面startPage和位置ridIx
{
	PF_FileHandle fileHandle = indexHandle->fileHandle;
	IX_FileHeader fileHeader = indexHandle->fileHeader;

	PF_PageHandle *pageHandle = NULL;

	//索引文件页面的序数
	int order = fileHeader.order;
	//索引关键字的长度
	int attrLength = fileHeader.attrLength;
	//关键字类型
	AttrType attrType = fileHeader.attrType;

	char *pageData;
	char *pKeys;
	char *pRids;
	//获取根节点页面
	GetThisPage(&fileHandle, fileHeader.rootPage, pageHandle);

	//获取根节点页面的数据区
	GetData(pageHandle, &pageData);

	//获取根节点页面得节点控制信息
	IX_Node* nodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));

	int keynum = nodeControlInfo->keynum;

	int position;
	int flag;
	while (true)   //往下遍历直到叶子节点
	{
		//获取关键字区
		pKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		//获取指针区
		pRids = pKeys + order*attrLength;

		for (position = keynum - 1, flag = 0; position >= 0; position--)   //从后往前遍历
		{
			int temp = keyCompare(pKeys + position*attrLength, pData, attrType);
			if (temp <= 0)
				break;
		}

		if (nodeControlInfo->is_leaf == 1)   //说明当前节点已经是叶子节点
		{
			position++;
			*ridIx = position;
			GetPageNum(pageHandle, startPage);
			return;
		}
		RID *tempRid = (RID*)(pRids + position * sizeof(RID));
		GetThisPage(&fileHandle, tempRid->pageNum, pageHandle);   //获取其子节点页面
		GetData(pageHandle, &pageData);   //获取子节点数据区
		nodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
		keynum = nodeControlInfo->keynum;
	}

}


//关键字的比较
int keyCompare(void *data1, void *data2, AttrType attrType)
{
	int temp;
	switch (attrType)
	{
	case chars:
		temp = strcmp((char*)data1, (char*)data2);
		break;
	case ints:
		int tempData1;
		tempData1 = *((int *)data1);
		int tempData2;
		tempData2 = *((int *)data2);
		if (tempData1 > tempData2)
			temp = 1;
		else if (tempData1 < tempData2)
			temp = -1;
		else
			temp = 0;
		break;
	case floats:
		float tempData3;
		tempData3 = *((float *)data1);
		float tempData4;
		tempData4 = *((float *)data2);
		if (tempData3 > tempData4)
			temp = 1;
		else if (tempData3 < tempData4)
			temp = -1;
		else
			temp = 0;
		break;
	}
	return temp;
}


//从兄弟节点中借节点或者合并
void getFromBrother(PF_PageHandle *pageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, int *status)
{
	PageNum leftPageNum;
	//int status;
	findLeftBrother(pageHandle, fileHandle, order, attrType, attrLength, &leftPageNum);    //首先从左兄弟节点处理
	if (leftPageNum != 0)   //如果左兄弟节点存在，对左兄弟进行处理
	{
		PF_PageHandle *leftHandle = NULL;
		GetThisPage(fileHandle, leftPageNum, leftHandle);
		getFromLeft(pageHandle, leftHandle, order, attrType, attrLength, status);   //对左兄弟进行处理
	}
	else   //说明节点的左兄弟节点不存在，对右兄弟进行处理
	{
		PF_PageHandle *rightHandle = NULL;
		char *tempData;
		GetData(pageHandle, &tempData);
		IX_Node* tempNodeControlInfo = (IX_Node*)(tempData + sizeof(IX_FileHeader));
		GetThisPage(fileHandle, tempNodeControlInfo->brother, rightHandle);
		getFromRight(pageHandle, rightHandle, order, attrType, attrLength, status);  //对右兄弟进行处理


		PF_PageHandle *parentPageHandle = NULL;
		GetData(rightHandle, &tempData);
		tempNodeControlInfo = (IX_Node*)(tempData + sizeof(IX_FileHeader));
		GetThisPage(fileHandle, tempNodeControlInfo->parent, parentPageHandle);
		PageNum nodePageNum;
		GetPageNum(rightHandle, &nodePageNum);
		char *tempKeys = tempData + sizeof(IX_FileHeader) + sizeof(IX_Node);

		if (*status == 3)   //说明向右兄弟借一个关键字
		{
			deleteChildNode(parentPageHandle, fileHandle, order, attrType, attrLength, nodePageNum, false, tempKeys);  //递归修改右兄弟节点
		}
	}
}


//与左兄弟节点进行处理
void getFromLeft(PF_PageHandle *pageHandle, PF_PageHandle *leftHandle, int order, AttrType attrType, int attrLength, int *status)
{

	char *pageData;
	char *pageKeys;
	char *pageRids;

	char *leftData;
	char *leftKeys;
	char *leftRids;

	GetData(leftHandle, &leftData);
	//获取左节点页面得节点控制信息
	IX_Node* leftNodeControlInfo = (IX_Node*)(leftData + sizeof(IX_FileHeader));
	//获取关键字区
	leftKeys = leftData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	leftRids = leftKeys + order*attrLength;

	GetData(pageHandle, &pageData);
	IX_Node* pageNodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
	int pageKeynum = pageNodeControlInfo->keynum;
	//获取关键字区
	pageKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	pageRids = pageKeys + order*attrLength;

	int leftKeynum = leftNodeControlInfo->keynum;
	if (leftKeynum > threshold)   //说明可以借出去
	{

		memcpy(pageKeys + attrLength, pageKeys, pageKeynum*attrLength);   //关键字整体后移
		memcpy(pageRids + sizeof(RID), pageRids, pageKeynum * sizeof(RID));   //关键字指针整体后移

		memcpy(pageKeys, leftKeys + (leftKeynum - 1)*attrLength, attrLength);  //复制左节点的最后一个关键字
		memcpy(pageRids, leftRids + (leftKeynum - 1) * sizeof(RID), sizeof(RID));  //复制左节点最后一个关键字指针

		leftKeynum--;
		pageKeynum++;
		leftNodeControlInfo->keynum = leftKeynum;    //修改关键字个数
		pageNodeControlInfo->keynum = pageKeynum;   //修改关键字个数
		*status = 1;

	}
	else   //说明不能借，只能进行合并
	{
		memcpy(leftKeys + leftKeynum*attrLength, pageKeys, pageKeynum*attrLength);   //关键字整体复制到左节点中
		memcpy(leftRids + leftKeynum * sizeof(RID), pageRids, pageKeynum * sizeof(RID));   //关键字指针整体复制到左节点中
		leftKeynum = leftKeynum + pageKeynum;
		pageKeynum = 0;
		leftNodeControlInfo->keynum = leftKeynum;    //修改关键字个数
		pageNodeControlInfo->keynum = pageKeynum;   //修改关键字个数
		leftNodeControlInfo->brother = pageNodeControlInfo->brother;    //修改叶子页面链表指针
		*status = 2;
	}

}


//与右兄弟节点进行处理
void getFromRight(PF_PageHandle *pageHandle, PF_PageHandle *rightHandle, int order, AttrType attrType, int attrLength, int *status)
{
	char *pageData;
	char *pageKeys;
	char *pageRids;

	char *rightData;
	char *rightKeys;
	char *rightRids;

	GetData(pageHandle, &pageData);
	IX_Node* pageNodeControlInfo = (IX_Node*)(pageData + sizeof(IX_FileHeader));
	int pageKeynum = pageNodeControlInfo->keynum;
	//获取关键字区
	pageKeys = pageData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	pageRids = pageKeys + order*attrLength;



	GetData(rightHandle, &rightData);
	//获取y节点页面得节点控制信息
	IX_Node* rightNodeControlInfo = (IX_Node*)(rightData + sizeof(IX_FileHeader));
	//获取关键字区
	rightKeys = rightData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	rightRids = rightKeys + order*attrLength;
	PageNum rightPageNum;
	GetPageNum(rightHandle, &rightPageNum);



	int rightKeynum = rightNodeControlInfo->keynum;
	if (rightKeynum > threshold)   //说明可以借出去
	{

		memcpy(pageKeys + pageKeynum*attrLength, rightKeys, attrLength);  //复制右节点的第一个关键字
		memcpy(pageRids + pageKeynum * sizeof(RID), rightRids, sizeof(RID));  //复制右节点的第一个关键字指针


		memcpy(rightKeys, rightKeys + attrLength, (rightKeynum - 1) *attrLength);   //关键字整体前移一个位置
		memcpy(rightRids, rightRids + sizeof(RID), (rightKeynum - 1) * sizeof(RID));   //关键字指针整体前移一个位置

		rightKeynum--;
		pageKeynum++;
		rightNodeControlInfo->keynum = rightKeynum;    //修改关键字个数
		pageNodeControlInfo->keynum = pageKeynum;   //修改关键字个数
		*status = 3;
	}
	else   //说明不能借，只能进行合并
	{
		memcpy(pageKeys + pageKeynum*attrLength, rightKeys, rightKeynum*attrLength);  //复制右节点的所有关键字
		memcpy(pageRids + pageKeynum * sizeof(RID), rightRids, rightKeynum * sizeof(RID));  //复制右节点的所有关键字指针

		pageKeynum = rightKeynum + pageKeynum;
		rightKeynum = 0;
		rightNodeControlInfo->keynum = rightKeynum;    //修改关键字个数
		pageNodeControlInfo->keynum = pageKeynum;   //修改关键字个数
		*status = 4;

		pageNodeControlInfo->brother = rightNodeControlInfo->brother;   //修改页面链表指针
	}
}


//简单地从某个父节点页面中删除某个分支或者修改关键字
void deleteChildNode(PF_PageHandle *parentPageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, PageNum nodePageNum, bool deleteIfTrue, void *pData)
{
	char *parentData;
	char *parentKeys;
	char *parentRids;
	while (true)
	{
		GetData(parentPageHandle, &parentData);
		IX_Node* nodeControlInfo = (IX_Node*)(parentData + sizeof(IX_FileHeader));
		int keynum = nodeControlInfo->keynum;
		//获取关键字区
		parentKeys = parentData + sizeof(IX_FileHeader) + sizeof(IX_Node);
		//获取指针区
		parentRids = parentKeys + order*attrLength;
		for (int i = 0;; i++)
		{
			RID *tempRid = (RID*)parentRids + i * sizeof(RID);
			if (tempRid->pageNum == nodePageNum)
			{
				if (deleteIfTrue)
				{
					//对关键字和指针进行覆盖删除
					memcpy(parentKeys + i*attrLength, parentKeys + (i + 1)*attrLength, (keynum - i - 1)*attrLength);
					memcpy(parentRids + i * sizeof(RID), parentRids + (i + 1) * sizeof(RID), (keynum - i - 1) * sizeof(RID));
					keynum--;
					nodeControlInfo->keynum = keynum;
					return;
				}
				else
				{
					//修改关键字
					memcpy(parentKeys + i*attrLength, pData, attrLength);
					if (i == 0 && nodeControlInfo->parent != 0)   //说明修改的关键字为第一个，需要递归地进行修改
					{
						GetPageNum(parentPageHandle, &nodePageNum);
						GetThisPage(fileHandle, nodeControlInfo->parent, parentPageHandle);   //递归地进行修改
					}
					else
						return;
				}
			}
		}
	}
}


//找出当前节点的左兄弟节点
void findLeftBrother(PF_PageHandle *pageHandle, PF_FileHandle *fileHandle, int order, AttrType attrType, int attrLength, PageNum *leftBrother)
{

	char *data;
	PageNum nowPage;
	GetPageNum(pageHandle, &nowPage);   //获取当前页面号
	GetData(pageHandle, &data);
	IX_Node* nodeControlInfo = (IX_Node*)(data + sizeof(IX_FileHeader));

	PF_PageHandle *parentPageHandle = NULL;
	GetThisPage(fileHandle, nodeControlInfo->parent, parentPageHandle);
	char *parentData;
	char *parentKeys;
	char *parentRids;

	GetData(parentPageHandle, &parentData);
	//获取关键字区
	parentKeys = parentData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	parentRids = parentKeys + order*attrLength;
	for (int i = 0;; i++)
	{
		RID *tempRid = (RID*)parentRids + i * sizeof(RID);
		if (tempRid->pageNum == nowPage)
		{
			if (i != 0)
			{
				i--;
				tempRid = (RID*)parentRids + i * sizeof(RID);
				*leftBrother = tempRid->pageNum;
			}
			else
				*leftBrother = 0;
			return;
		}
	}
}


//RID的比较
bool compareRid(RID *src, RID *des)
{
	if ((src->bValid == des->bValid) && (src->pageNum == des->pageNum) && (src->slotNum == des->slotNum))
		return true;
	else
		return false;
}


//删除前查找节点中的关键字
void findKeyAndRidForDelete(PF_PageHandle *pageHandle, int order, AttrType attrType, int attrLength, void *pData, RID *rid, bool *existence)
{
	char *parentData;
	char *parentKeys;
	char *parentRids;
	int flag = 0;

	GetData(pageHandle, &parentData);

	//获取根节点页面得节点控制信息
	IX_Node* nodeControlInfo = (IX_Node*)(parentData + sizeof(IX_FileHeader));
	int keynum = nodeControlInfo->keynum;

	//获取关键字区
	parentKeys = parentData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	parentRids = parentKeys + order*attrLength;

	int position = 0;
	switch (attrType)
	{
	case chars:
		for (; position < keynum; position++) {
			if (strcmp(parentKeys + position*attrLength, (char*)pData) >= 0)
			{
				if (strcmp((char*)pData, parentKeys + position*attrLength) == 0)
					flag = 1;
				break;
			}
		}
		break;
	case ints:
		int data1;
		data1 = *((int *)pData);
		for (; position < keynum; position++) {
			int data2 = *((int *)parentKeys + position*attrLength);
			if (data2 >= data1)
			{
				if (data1 == data2)
					flag = 1;
				break;
			}
		}
		break;
	case floats:
		float data_floats = *((float *)pData);
		for (; position < keynum; position++) {
			float data2 = *((float *)parentKeys + position*attrLength);
			if (data2 >= data_floats)
			{
				if (data_floats == data2)
					flag = 1;
				break;
			}
		}
		break;
	}
	if (flag == 1)
	{
		*existence = true;
		memcpy(rid, parentRids + position * sizeof(RID), sizeof(RID));
	}
	else
	{
		position--;
		if (position < 0)
			*existence = false;
		else
		{
			*existence = true;
			memcpy(rid, parentRids + position * sizeof(RID), sizeof(RID));
		}
	}
}


//设置新页面节点控制信息
void copyNewNodeInfoToPage(PF_PageHandle *pageHandle, PageNum brother, PageNum parent, int is_leaf, int keynum)
{
	IX_Node newNodeInfo;
	newNodeInfo.brother = brother;
	newNodeInfo.parent = parent;
	newNodeInfo.is_leaf = is_leaf;
	newNodeInfo.keynum = keynum;
	char *pData;
	GetData(pageHandle, &pData);
	memcpy(pData + sizeof(IX_FileHeader), &newNodeInfo, sizeof(IX_Node));
}


//复制分裂后的关键字和指针到新页面
void  copyKeysAndRIDsToPage(PF_PageHandle *pageHandle, void *keySrc, int attrLength, int num, int order, void *ridSrc)
{
	char *pData;
	GetData(pageHandle, &pData);
	pData = pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	memcpy(pData, keySrc, num*attrLength);
	pData = pData + order*attrLength;
	memcpy(pData, ridSrc, num * sizeof(RID));
}


//将给定的关键字和指针写到指定的页面
void insertKeyAndRidToPage(PF_PageHandle *pageHandle, int order, AttrType attrType, int attrLength, void *pData, RID *rid, bool insertIfTrue)
{

	char *parentData;
	char *parentKeys;
	char *parentRids;

	GetData(pageHandle, &parentData);

	//获取根节点页面得节点控制信息
	IX_Node* nodeControlInfo = (IX_Node*)(parentData + sizeof(IX_FileHeader));
	int keynum = nodeControlInfo->keynum;

	//获取关键字区
	parentKeys = parentData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	//获取指针区
	parentRids = parentKeys + order*attrLength;

	int position = 0;
	switch (attrType)
	{
	case chars:
		for (; position < keynum; position++) {
			if (strcmp(parentKeys + position*attrLength, (char*)pData) > 0)
				break;
		}
		break;
	case ints:
		int data1;
		data1 = *((int *)pData);
		for (; position < keynum; position++) {
			int data2 = *((int *)parentKeys + position*attrLength);
			if (data2 > data1)
				break;
		}
		break;
	case floats:
		float data_floats = *((float *)pData);
		for (; position < keynum; position++) {
			float data2 = *((float *)parentKeys + position*attrLength);
			if (data2 > data_floats)
				break;
		}
		break;
	}
	if (insertIfTrue)
	{
		memcpy(parentKeys + (position + 1)*attrLength, parentKeys + position*attrLength, (keynum - position)*attrLength);
		memcpy(parentKeys + position*attrLength, pData, attrLength);
		//插入关键字的指针
		memcpy(parentRids + (position + 1) * sizeof(RID), parentRids + position * sizeof(RID), (keynum - position) * sizeof(RID));
		memcpy(parentRids + position * sizeof(RID), rid, sizeof(RID));
		keynum++;
		nodeControlInfo->keynum = keynum;
	}
	else
	{
		position--;
		if (position < 0)    //关键字将会插入到第一个关键字处
		{
			position = 0;   //插入到最前面的页面
			memcpy(parentKeys, pData, attrLength);   //修改所指向页面的最小关键字
		}
		memcpy(rid, parentRids + position * sizeof(RID), sizeof(RID));
	}
}