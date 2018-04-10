#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"

RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions)//初始化扫描
{
	//初始化文件扫描结构
	rmFileScan->bOpen = true;
	rmFileScan->pRMFileHandle = fileHandle;
	rmFileScan->conNum = conNum;
	rmFileScan->conditions = conditions;
	rmFileScan->pageHandle = NULL; //处理中的页面句柄

	//扫面页面管理位图，顺序寻找第一个被分配数据页
	char *pBitMap = fileHandle->pf_fileHandle->pBitmap;
	
	if (fileHandle->pf_fileHandle->pFileSubHeader->nAllocatedPages <= 2)
	{ //无数据页，即文件中没有存入任何记录
		rmFileScan->pn = 0;
		rmFileScan->sn = 0; //pn、sn等于0，表示文件中没有记录
	}
	else { //有数据页，寻找第一个被分配的数据页页号
		rmFileScan->pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
		int i = 2;
		while (true)
		{
			char x = 1 << (i % 8);
			if ((*(pBitMap+i/8) & x) != 0)
			{ //找到一个已分配页
				rmFileScan->pn = i;
				GetThisPage(fileHandle->pf_fileHandle, rmFileScan->pn, rmFileScan->pageHandle);
				break;
			}
			else
			{
				i++;
			}
		}
	
		//扫描数据页位图，寻找第一条有效记录位置
		char *rBitMap;
		GetData(rmFileScan->pageHandle, &rBitMap);
		i = 0;
		while (true)
		{
			char x = 1 << (i % 8);
			if ((*(rBitMap+i/8) & x) != 0)
			{
				rmFileScan->sn = i;
				break;
			}
			else
			{
				i++;
			}

		}
	}

	return SUCCESS;
}

RC GetNextRec(RM_FileScan *rmFileScan,RM_Record *rec)
{
	//一个页面扫面完毕，要根据页面句柄解除缓冲区驻留
	if (rmFileScan->pn == 0)
	{ //扫描的页面号为零，表示文件中无记录
		return RM_EOF;
	}

	RM_Record *localRec = rec;

	while (true)
	{

		//获得记录
		localRec->bValid = true;
		localRec->rid.bValid = true;
		localRec->rid.pageNum = rmFileScan->pn;
		localRec->rid.slotNum = rmFileScan->sn;

		char *pData;
		GetData(rmFileScan->pageHandle, &pData);

		localRec->pData = pData + rmFileScan->pRMFileHandle->rm_fileSubHeader->firstRecordOffset
			+ localRec->rid.slotNum*rmFileScan->pRMFileHandle->rm_fileSubHeader->recordSize;

		int i;
		//进行比较
		if (rmFileScan->conNum == 0)
		{ //扫描涉及的条件个数为0，直接返回扫描到的记录
			i = rmFileScan->conNum;
		}
		else {
			//扫描到的条件个数大于0，逐个条件进行对比
			Con *con;
			//如果有任意一个条件不满足，则提前退出for循环
			i = 0;
			for (; i < rmFileScan->conNum; i++)
			{
				con = rmFileScan->conditions + i;
				bool flag = true; //当前条件的比较结果不满足要求时，flag等于false
				//判断用于比较的数据类型
				switch (con->attrType)
				{
				case chars:
					char *Lattr, *Rattr;
					if (con->bLhsIsAttr == 1)
					{ //条件的左边是属性
						Lattr = (char *)malloc(sizeof(char)*con->LattrLength);
						strncpy(Lattr, localRec->pData + con->LattrOffset, con->LattrLength);
						//strncpy_s(Lattr, localRec->pData + con->LattrOffset, con->LattrLength);
					}
					else { //条件的左边是值
						Lattr = (char *)con->Lvalue;
					}

					if (con->bRhsIsAttr == 1)
					{ //条件的右边是属性
						Rattr = (char *)malloc(sizeof(char)*con->LattrLength);
						strncpy(Lattr, localRec->pData + con->RattrOffset, con->RattrLength);
					}
					else { //条件的右边是值
						Rattr = (char *)con->Rvalue;
					}

					flag = Compare(con->compOp, con->attrType, Lattr, Rattr);
					break;
				case ints:
					int *IntLattr, *IntRattr;
					if (con->bLhsIsAttr == 1)
					{ //条件的左边是属性
						//IntLattr = (int *)malloc(sizeof(int));
						IntLattr = (int *)(localRec->pData + con->LattrOffset);
					}
					else { //条件的左边是值
						IntLattr = (int *)con->Lvalue;
					}

					if (con->bRhsIsAttr == 1)
					{ //条件的右边是属性
						//Rattr = (char *)malloc(sizeof(char)*con->LattrLength);
						IntRattr = (int *)(localRec->pData + con->RattrOffset);
					}
					else { //条件的右边是值
						IntRattr = (int *)con->Rvalue;
					}

					flag = Compare(con->compOp, con->attrType, IntLattr, IntRattr);
					break;
				case floats:
					float *FloatLattr, *FloatRattr;
					if (con->bLhsIsAttr == 1)
					{ //条件的左边是属性
					  //IntLattr = (int *)malloc(sizeof(int));
						FloatLattr = (float *)(localRec->pData + con->LattrOffset);
					}
					else { //条件的左边是值
						FloatLattr = (float *)con->Lvalue;
					}

					if (con->bRhsIsAttr == 1)
					{ //条件的右边是属性
					  //Rattr = (char *)malloc(sizeof(char)*con->LattrLength);
						FloatRattr = (float *)(localRec->pData + con->RattrOffset);
					}
					else { //条件的右边是值
						FloatRattr = (float *)con->Rvalue;
					}

					flag = Compare(con->compOp, con->attrType, FloatLattr, FloatRattr);

					break;
				default:
					break;
				}

				if (!flag)
				{
					break;
				}
			}
		
		}
		//设置记录扫描的下一个记录的页面号和槽位号
		while (true)
		{
			//首先顺序扫描当前页面的下一个记录槽
			int j = rmFileScan->sn + 1;
			char *rBitMap, x;
			GetData(rmFileScan->pageHandle, &rBitMap);
			for (; j < rmFileScan->pRMFileHandle->rm_fileSubHeader->nRecords; j++)
			{
				x = 1 << (j % 8);
				if ((*(rBitMap + j / 8) & x) != 0)
				{
					rmFileScan->sn = j;
					break;
				}

			}

			if (j < rmFileScan->pRMFileHandle->rm_fileSubHeader->nRecords)
			{ //在当前页面找到下一个有效记录
				break;
			}
			else { //在当前页面未找到下一个有效记录，查找下一个记录页面
				unsigned int k = rmFileScan->pn+1;
				char y;
				char *pBitMap = rmFileScan->pRMFileHandle->pf_fileHandle->pBitmap;
				while (k <= rmFileScan->pRMFileHandle->pf_fileHandle->pFileSubHeader->pageCount)
				{ //PageNum
					y = 1 << (k % 8);
					if ((*(pBitMap + k / 8) & y) != 0)
					{ //找到一个已分配页
						rmFileScan->pn = k;
						UnpinPage(rmFileScan->pageHandle);  //解除旧页面驻留缓冲区
						//新页面驻留缓存
						GetThisPage(rmFileScan->pRMFileHandle->pf_fileHandle, rmFileScan->pn, rmFileScan->pageHandle);
						break;
					}
					else
					{
						k++;
					}
				}

				if (k > rmFileScan->pRMFileHandle->pf_fileHandle->pFileSubHeader->pageCount)
				{
					rmFileScan->pn = 0;
					rmFileScan->sn = 0;
					break;
				}

				//扫描数据页位图，寻找第一条有效记录位置
				GetData(rmFileScan->pageHandle, &rBitMap);
				k = 0;
				while (true)
				{
					y = 1 << (k % 8);
					if ((*(rBitMap + k / 8) & y) != 0)
					{
						rmFileScan->sn = k;
						break;
					}
					else
					{
						k++;
					}
				}

			}
		}

		if (i < rmFileScan->conNum)
		{ //当前记录不满足，搜索下一个记录
		 	RC re = GetNextRec(rmFileScan, localRec);
			if (re == SUCCESS)
			{
				break;
			}
			else {
				return re;
			}
		} //当前记录满足所有条件，结束搜索
		else
		{
			break;
		}

	}

	rec = localRec;

	return SUCCESS;
}

//根据比较操作符，比较传入的两个参数
//若比较结果满足比较操作符的要求，则返回true；否则，返回false
bool Compare(CompOp compOp, AttrType attrType, void *Lvalue, void *Rvalue) {

	bool result = false;

	char *CharLv;
	char *CharRv;
	int IntLv;
	int IntRv;
	float FloatLv;
	float FloatRv;

	switch (attrType)
	{
	case chars:
		CharLv = (char *)Lvalue;
		CharRv = (char *)Rvalue;
		int re;
		re = strcmp(CharLv, CharRv);
		switch (compOp)
		{
		case EQual:		        //"="			0
			if (re == 0)
				result = true;
			break;
		case LEqual:			//"<="          1 
			if (re <= 0)
				result = true;
			break;
		case NEqual:			//"<>"			2 
			if (re != 0)
				result = true;
			break;
		case LessT:			    //"<"			3
			if (re < 0)
				result = true;
			break;
		case GEqual:			//">="			4 
			if (re >= 0)
				result = true;
			break;
		case GreatT:			//">"           5 
			if (re > 0)
				result = true;
			break;
		case NO_OP:
			result = true;
			break;
		default:
			break;
		}

		break;
	case ints:
		IntLv = *(int *)Lvalue;
		IntRv = *(int *)Rvalue;
		switch (compOp)
		{
		case EQual:		        //"="			0
			if (IntLv == IntRv)
				result = true;
			break;
		case LEqual:			//"<="          1
			if (IntLv <= IntRv)
				result = true;
			break;
		case NEqual:			//"<>"			2 
			if (IntLv != IntRv)
				result = true;
			break;
		case LessT:			    //"<"			3
			if (IntLv < IntRv)
				result = true;
			break;
		case GEqual:			//">="			4 
			if (IntLv >= IntRv)
				result = true;
			break;
		case GreatT:			//">"           5 
			if (IntLv > IntRv)
				result = true;
			break;
		case NO_OP:
			result = true;
			break;
		default:
			break;
		}

		break;
	case floats:
		FloatLv = *(float *)Lvalue;
		FloatRv = *(float *)Rvalue;
		switch (compOp)
		{
		case EQual:		        //"="			0
			if (FloatLv == FloatRv)
				result = true;
			break;
		case LEqual:			//"<="          1
			if (FloatLv <= FloatRv)
				result = true;
			break;
		case NEqual:			//"<>"			2 
			if (FloatLv != FloatRv)
				result = true;
			break;
		case LessT:			    //"<"			3
			if (FloatLv < FloatRv)
				result = true;
			break;
		case GEqual:			//">="			4 
			if (FloatLv >= FloatRv)
				result = true;
			break;
		case GreatT:			//">"           5 
			if (FloatLv > FloatRv)
				result = true;
			break;
		case NO_OP:
			result = true;
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}

	return result;

}

RC CloseScan(RM_FileScan *rmFileScan) 
{
	rmFileScan->bOpen = false;
	rmFileScan->pRMFileHandle = NULL;
	rmFileScan->conNum = 0;
	rmFileScan->conditions = NULL;
	if (rmFileScan->pageHandle != NULL)
	{
		UnpinPage(rmFileScan->pageHandle);
		free(rmFileScan->pageHandle);
		rmFileScan->pageHandle = NULL;
	}

	return SUCCESS;
}

RC GetRec (RM_FileHandle *fileHandle,RID *rid, RM_Record *rec) 
{
	PF_PageHandle* pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	RM_Record *localRec = rec;
	char *pData;
	char *pRec;

	GetThisPage(fileHandle->pf_fileHandle, rid->pageNum, pageHandle);
	GetData(pageHandle, &pData);

	pRec = pData + fileHandle->rm_fileSubHeader->firstRecordOffset + rid->slotNum*fileHandle->rm_fileSubHeader->recordSize;
	memcpy(localRec, pRec, fileHandle->rm_fileSubHeader->recordSize);
	/*if (!localRec->bValid)      //判断记录是否有效
	{
		return 
	}*/

	rec = localRec;
	UnpinPage(pageHandle);
	free(pageHandle);

	return SUCCESS;
}

RC InsertRec (RM_FileHandle *fileHandle,char *pData, RID *rid)
{
	PF_PageHandle* pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	RID *localRid = rid;
	int pageNum;      //已分配页的数目
	char *pf_bitMap;
	char *rm_bitMap;
	char *pageData;

	pageNum = fileHandle->pf_fileHandle->pFileSubHeader->nAllocatedPages;
	
	pf_bitMap = fileHandle->pf_fileHandle->pBitmap;
	rm_bitMap = fileHandle->rBitMap;
	localRid->bValid = false;
	for (unsigned int i = 2; i <= fileHandle->pf_fileHandle->pFileSubHeader->pageCount; i++)
	{

		if ((*(pf_bitMap+i/8) & (1<<(i%8))) != 0)
		{ //找到一个已分配页
			if ((*(rm_bitMap+i/8) & (1<<(i%8))) != 0) //该页面已满
			{
				continue;
			}

			GetThisPage(fileHandle->pf_fileHandle, i, pageHandle);
			GetData(pageHandle, &pageData);

			//该页面未满，在该页面中找到一个空闲记录槽
			for (int j = 0; j < fileHandle->rm_fileSubHeader->recordsPerPage; j++)
			{
				if ((*(pageData+j/8) & (1<<j%8)) == 0) //找到一个空闲记录槽
				{
					localRid->bValid = true;
					localRid->pageNum = i;
					localRid->slotNum = j;
					break;
				}
			}

			if (localRid->bValid)
			{  //找到一个空闲记录槽
				break;
			}
		}
	}

	
	if (!localRid->bValid)
	{ //如果没找到空闲记录槽，则新分配一个数据页
		RC tmp;
		if ((tmp = AllocatePage(fileHandle->pf_fileHandle, pageHandle)) != SUCCESS) {
			return tmp;
		}

		GetData(pageHandle, &pageData);
		GetPageNum(pageHandle, &localRid->pageNum);
		localRid->bValid = true;
		localRid->slotNum = 0;

		char x = ~(1 << (localRid->pageNum % 8));
		*(rm_bitMap + localRid->pageNum / 8) = *(rm_bitMap + localRid->pageNum / 8) & x;

	}
	
	//找到一个空闲记录槽,将记录插入,修改控制页记录数量,修改数据页位图并判断是否修改控制页位图
	
	memcpy(pageData + fileHandle->rm_fileSubHeader->firstRecordOffset + localRid->slotNum*fileHandle->rm_fileSubHeader->recordSize,
		pData, fileHandle->rm_fileSubHeader->recordSize);

	//修改位图，将该记录槽标记为有效
	*(pageData + localRid->slotNum / 8) = *(pageData + localRid->slotNum / 8) | (1 << localRid->slotNum % 8);
	//判断该数据页是否已满，来决定是否修改控制页位图
	int i = localRid->slotNum + 1;
	for ( ; i < fileHandle->rm_fileSubHeader->recordsPerPage; i++)
	{
		if ((*(pageData + i / 8) & (1 << i % 8)) == 0)
		{ //从余下的记录槽中找到一个空闲槽
			break;
		}
	}
	if (i >= fileHandle->rm_fileSubHeader->recordsPerPage)
	{
		*(rm_bitMap + localRid->pageNum / 8) = *(rm_bitMap + localRid->pageNum / 8) | (1 << (localRid->pageNum % 8));
	}

	fileHandle->rm_fileSubHeader->nRecords++;

	MarkDirty(pageHandle);
	if (!fileHandle->pageHandle->pFrame->bDirty)
	{
		MarkDirty(fileHandle->pageHandle);
	}

	
	UnpinPage(pageHandle);
	free(pageHandle);

	return SUCCESS;
}

RC DeleteRec (RM_FileHandle *fileHandle,const RID *rid)
{
	PF_PageHandle* pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	char *pageData;

	GetThisPage(fileHandle->pf_fileHandle, rid->pageNum, pageHandle);
	GetData(pageHandle, &pageData);
	//修改所在数据页的位图，对应位置置为无效
	char x = ~(1 << (rid->slotNum % 8));
	*(pageData + rid->slotNum / 8) = *(pageData + rid->slotNum / 8) & x;
	
	//记录有效位置无效
	//char * record = pageData+fileHandle->rm_fileSubHeader->firstRecordOffset+rid->slotNum*fileHandle->rm_fileSubHeader->recordSize;
	//bool f = false;
	//memcmp(record, &f, sizeof(bool));
	//fileHandle->rm_fileSubHeader->nRecords--;

	//修改记录控制页的位图信息
	char * rm_bitMap = fileHandle->rBitMap;
	x = ~(1 << (rid->pageNum % 8));
	*(rm_bitMap + rid->pageNum / 8) = *(rm_bitMap + rid->pageNum / 8) & x;

	//char * pf_bitMap = fileHandle->pf_fileHandle->pBitmap;
	int i = 0;
	for (; i < fileHandle->rm_fileSubHeader->recordsPerPage; i++)
	{
		if ((*(pageData + i / 8) & (1 << i % 8)) != 0)
		{ //从余下的记录槽中找到一个非空闲槽就退出
			break;
		}
	}

	if (i >= fileHandle->rm_fileSubHeader->recordsPerPage)
	{ //没有非空闲槽，删除该页面
		DisposePage(fileHandle->pf_fileHandle, rid->pageNum);
		//*(rm_bitMap + localRid->pageNum / 8) = *(rm_bitMap + localRid->pageNum / 8) | (1 << (localRid->pageNum % 8));
	}
	//else
	//{
		MarkDirty(pageHandle);
	//}

	if (!fileHandle->pageHandle->pFrame->bDirty)
	{
		MarkDirty(fileHandle->pageHandle);
	}

	UnpinPage(pageHandle);
	free(pageHandle);

	return SUCCESS;
}

RC UpdateRec (RM_FileHandle *fileHandle,const RM_Record *rec)
{
	PF_PageHandle* pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	char *pageData;

	GetThisPage(fileHandle->pf_fileHandle, rec->rid.pageNum, pageHandle);
	GetData(pageHandle, &pageData);

	//更新记录内容
	memcpy(pageData + fileHandle->rm_fileSubHeader->firstRecordOffset + rec->rid.slotNum*fileHandle->rm_fileSubHeader->recordSize,
		rec->pData, fileHandle->rm_fileSubHeader->recordSize);

	MarkDirty(pageHandle);

	UnpinPage(pageHandle);
	free(pageHandle);
	return SUCCESS;
}

RC RM_CreateFile (char *fileName, int recordSize)
{
	PF_FileHandle *fileHandle = (PF_FileHandle *)malloc(sizeof(PF_FileHandle));
	//PF_FileHandle fileHandle;
	PF_PageHandle *pageHandle = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	PF_PageHandle *pageHandle1 = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	char *subHeaderChars = (char *)malloc(sizeof(PF_FileSubHeader));
	//PF_FileSubHeader *pf_fileSubHeader;
	RM_FileSubHeader *rm_fileSubHeader = (RM_FileSubHeader *)malloc(sizeof(RM_FileSubHeader));
	int recordsPerPage, bitMapLength;

	char *pData1;

	//生成文件
	CreateFile(fileName);
	//打开文件
	OpenFile(fileName, fileHandle);

	//初始化记录文件

	//获得页面信息控制页句柄
	GetThisPage(fileHandle, 0, pageHandle);

	//获得数据区指针
//	GetData(pageHandle, &pData);
//	fileSubHeader = (PF_FileSubHeader *)strncpy(subHeaderChars, pData, sizeof(PF_FileSubHeader));
	
	//分配新页,分配到的新页页号应为1
	AllocatePage(fileHandle, pageHandle1);
	MarkDirty(pageHandle);  //将页面信息控制页标记为dirty

	rm_fileSubHeader->nRecords = 0;
	rm_fileSubHeader->recordSize = recordSize;
	
	//确定记录起始位置firstRecordOffset和每个页面可以装载的记录数recordsPerPage
	//数据区是4092字节
	recordsPerPage = 4092 / recordSize;
	bitMapLength = (recordsPerPage + 7) / 8;

	while (recordsPerPage*recordSize+bitMapLength > 4092) {
		recordsPerPage--;
		bitMapLength = (recordsPerPage + 7) / 8;
	}

	rm_fileSubHeader->firstRecordOffset = bitMapLength;
	rm_fileSubHeader->recordsPerPage = recordsPerPage;

	GetData(pageHandle1, &pData1);
	memcpy(pData1, (char *)rm_fileSubHeader, sizeof(RM_FileSubHeader));
	MarkDirty(pageHandle1);

	//初始时记录数为0，用于记录的页面数量也为0，无需设置记录页面位图

	//解除驻留缓冲区限制
	UnpinPage(pageHandle);
	UnpinPage(pageHandle1);

	//关闭文件
	CloseFile(fileHandle);
	free(subHeaderChars);
	free(fileHandle);
	free(pageHandle);

	return SUCCESS;
}

RC RM_OpenFile(char *fileName, RM_FileHandle *fileHandle)
{
	RM_FileHandle *pfileHandle = fileHandle;
	PF_FileHandle *pf_fileHandle = (PF_FileHandle *)malloc(sizeof(PF_FileHandle));
	PF_PageHandle *pageHandle1 = (PF_PageHandle *)malloc(sizeof(PF_PageHandle));
	char *pData;

	//打开文件，获得页面文件句柄
	RC tmp;
	tmp =  OpenFile(fileName, pf_fileHandle);
	if (tmp == PF_FILEERR)
	{
		printf("PF_FILEERR\n");
		return PF_FILEERR;
	}

	pfileHandle->bOpen = true;
	pfileHandle->pf_fileHandle = pf_fileHandle;

	//获得记录管理页面句柄
	GetThisPage(pf_fileHandle, 1, pageHandle1);  //记录管理页面在记录文件关闭时由文件关闭函数解除驻留缓存限制
	GetData(pageHandle1, &pData);

	pfileHandle->rBitMap = pData + sizeof(RM_FileSubHeader);
	pfileHandle->rm_fileSubHeader = (RM_FileSubHeader *)pData;
	pfileHandle->pageHandle = pageHandle1;

	fileHandle = pfileHandle;

	return SUCCESS;
}

RC RM_CloseFile(RM_FileHandle *fileHandle)
{
	RC tmp;

	//解除页面管理页缓冲驻留
	UnpinPage(fileHandle->pageHandle);

	if ((tmp = CloseFile(fileHandle->pf_fileHandle)) != SUCCESS)
	{ //关闭对应的页面文件
		return tmp;
	}

	return SUCCESS;
}
