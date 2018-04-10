#include "StdAfx.h"
#include "QU_Manager.h"

void Init_Result(SelResult * res){
	res->next_res = NULL;
}

void Destory_Result(SelResult * res){
	for(int i = 0;i<res->row_num;i++){
		
		/*for(int j = 0;j<res->col_num;j++){
			delete[] res->res[i][j];
		}*/
		delete[] res->res[i];
	}
	if(res->next_res != NULL){
		Destory_Result(res->next_res);
	}
}

RC Query(char * sql, SelResult * res) {

	RC tmp;
	sqlstr * sqlType = NULL;
	sqlType = get_sqlstr();
	tmp = parse(sql, sqlType);

	if (tmp != SUCCESS)
	{
		return tmp;
	}

	if (sqlType->flag == 1) //查询语句   这个判断可以不要，因为只有在是查询语句时才会调用该函数
	{
		selects sel = sqlType->sstr.sel;
		/*SelResult * */
		//res = (SelResult*)malloc(sizeof(SelResult));
		tmp = Select(sel.nSelAttrs, sel.selAttrs, sel.nRelations, sel.relations, sel.nConditions, sel.conditions, res);
		if (tmp != SUCCESS)
		{
			return tmp;
		}
	}

	return SUCCESS;
}

/*
* 第一组参数表示查询涉及的属性
* 第二组表示查询涉及的表
* 第三组表示查询条件
* 最后一个参数res用于返回查询结果集
* 查询优化：优化查询过程，当查询涉及多个表时，设计高效的查询过程
*/

RC Select(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{

	RC rc;
	SelResult *resHead = res;

	rc = checkTable(nRelations, relations);
	if (rc == RM_EOF)
	{
		AfxMessageBox("查询的表不存在!");
		return rc;
	}

	//分单表查询和多表查询
	if (nRelations == 1)
	{ //单表查询
		if (nConditions == 0)
		{  //无条件查询

		   //如果某个属性上有索引，则索引查询；否则，全文件扫描
			if (false)
			{ //此处判断索引情况，暂未实现

			}
			else { //无索引，全表扫描

				//首先扫描系统表获得表的属性个数和属性参数
				RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
				rc = RM_OpenFile("SYSTABLES", rm_fileHandle);
				if (rc != SUCCESS) return rc;

				RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
				RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
				Con con;

				con.attrType = chars;
				con.bLhsIsAttr = 1;
				con.LattrOffset = 0;
				con.LattrLength = strlen(*relations) + 1;
				con.compOp = EQual;
				con.bRhsIsAttr = 0;
				con.Rvalue = *relations;

				OpenScan(rm_fileScan, rm_fileHandle, 1, &con);
				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				SysTable *table = (SysTable *)record->pData;
				memcpy(&(resHead->col_num), record->pData + 21, sizeof(int));

				//resHead->col_num = table->attrcount;  //设置查询结果列数

				CloseScan(rm_fileScan);
				RM_CloseFile(rm_fileHandle);

				//获得属性的偏移量和类型
				rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
				if (rc != SUCCESS) return rc;

				OpenScan(rm_fileScan, rm_fileHandle, 1, &con);

				for (int i = 0; i < resHead->col_num; i++)
				{
					rc = GetNextRec(rm_fileScan, record);
					if (rc != SUCCESS) return rc;

					//SysColumns *column = (SysColumns *)record->pData;
					char * column = record->pData;
					//属性类型
					//resHead->attrType[i] = column->attrtype;
					memcpy(&resHead->attrType[i], column + 42, sizeof(int));
					//属性名
					memcpy(&resHead->fields[i], column + 21, 21);
					//strncpy(resHead->fields[i], column->attrname, column->attrlength);
					//属性偏移量
					memcpy(&resHead->offset[i], column + 50, sizeof(int));
					//属性长度
					memcpy(&resHead->length[i], column + 46, sizeof(int));

				}
				CloseScan(rm_fileScan);
				RM_CloseFile(rm_fileHandle);
				free(rm_fileHandle);

				//扫描记录表，找出所有记录
				rc = RM_OpenFile(*relations, rm_fileHandle);
				if (rc != SUCCESS) return rc;

				OpenScan(rm_fileScan, rm_fileHandle, 0, NULL);

				int i = 0;
				resHead->row_num = 0;
				SelResult *curRes = resHead;  //尾插法向链表中插入新结点
				while (GetNextRec(rm_fileScan, record) == SUCCESS)
				{
					if (curRes->row_num >= 100) //每个节点最多记录100条记录
					{ //当前结点已经保存100条记录时，新建结点
						curRes->next_res = (SelResult *)malloc(sizeof(SelResult));
						curRes->next_res->col_num = curRes->col_num;
						for (int j = 0; j < curRes->col_num; j++)
						{
							strncpy(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
							curRes->next_res->attrType[i] = curRes->attrType[i];
							curRes->next_res->offset[i] = curRes->offset[i];
						}

						curRes = curRes->next_res;
						curRes->next_res = NULL;
						curRes->row_num = 0;
					}

					curRes->res[curRes->row_num] = (char **)malloc(sizeof(char *));
					*(curRes->res[curRes->row_num++]) = record->pData;

					//int x;
					//memcpy(&x, record->pData + 18, sizeof(int));
					//memcpy(&x, *(curRes->res[curRes->row_num-1]) + 22, sizeof(int));
				}

				CloseScan(rm_fileScan);
				free(rm_fileScan);
				RM_CloseFile(rm_fileHandle);
				//free(rm_fileHandle);
				//free(record);
			}

		}
		else
		{ //条件查询
			singleConditionSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
		}

	}
	else if (nRelations > 1)
	{ //多表条件查询
		multiSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res);
	}

	res = resHead;

	return SUCCESS;
}

//单表条件查询
RC singleConditionSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res)
{
	RC rc;
	SelResult *resHead = res;

	//如果某个属性上有索引，则索引查询；否则，全文件扫描
	if (false)
	{ //此处判断索引情况，暂未实现

	}
	else {

		RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

		RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
		Con cons[2];

		cons[0].attrType = chars;
		cons[0].bLhsIsAttr = 1;
		cons[0].LattrOffset = 0;
		cons[0].LattrLength = strlen(*relations) + 1;
		cons[0].compOp = EQual;
		cons[0].bRhsIsAttr = 0;
		cons[0].Rvalue = *relations;

		if (nSelAttrs==1 && !strcmp((*selAttrs)->attrName, "*"))
		{  //查询结果为所有属性
			rc = RM_OpenFile("SYSTABLES", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			OpenScan(rm_fileScan, rm_fileHandle, 1, &cons[0]);
			rc = GetNextRec(rm_fileScan, record);
			if (rc != SUCCESS) return rc;

			SysTable *table = (SysTable *)record->pData;
			memcpy(&(resHead->col_num), record->pData + 21, sizeof(int));  //设置查询结果列数

			CloseScan(rm_fileScan);
			RM_CloseFile(rm_fileHandle);

			//获得属性的偏移量和类型
			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			OpenScan(rm_fileScan, rm_fileHandle, 1, &cons[0]);

			for (int i = 0; i < resHead->col_num; i++)
			{
				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//属性类型
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->attrType[i], column + 42, sizeof(int));
				//属性名
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//属性偏移量
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				//属性长度
				memcpy(&resHead->length[i], column + 46, sizeof(int));

			}
			CloseScan(rm_fileScan);
			RM_CloseFile(rm_fileHandle);
		}
		else {  //查询结果为指定属性
			resHead->col_num = nSelAttrs; //设置查询结果列数

			//获得属性的偏移量和类型
			cons[1].attrType = chars;
			cons[1].bLhsIsAttr = 1;
			cons[1].LattrOffset = 21;
			cons[1].LattrLength = 21;
			cons[1].compOp = EQual;
			cons[1].bRhsIsAttr = 0;

			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			for (int i = 0; i < resHead->col_num; i++)
			{
				cons[1].Rvalue = (selAttrs[resHead->col_num - i - 1])->attrName;
				OpenScan(rm_fileScan, rm_fileHandle, 2, cons);

				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//属性类型
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->attrType[i], column + 42, sizeof(int));
				//属性名
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//属性偏移量
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				//属性长度
				memcpy(&resHead->length[i], column + 46, sizeof(int));

				CloseScan(rm_fileScan);
			}

			RM_CloseFile(rm_fileHandle);
		}

		rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
		if (rc != SUCCESS) return rc;

		cons[1].attrType = chars;
		cons[1].bLhsIsAttr = 1;
		cons[1].LattrOffset = 21;
		cons[1].LattrLength = 21;
		cons[1].compOp = EQual;
		cons[1].bRhsIsAttr = 0;

		//以条件查询的条件作为扫描条件
		Con *selectCons = (Con *)malloc(sizeof(Con) * nConditions);
		for (int i = 0; i < nConditions; i++) {
			//只需设置con[1]->rValue,把条件里的属性设置成查询时的值
			if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
			{  //左边是值，右边是属性
				(cons + 1)->Rvalue = conditions[i].rhsAttr.attrName;
			}
			else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
			{   //左边是属性，右边是值
				(cons + 1)->Rvalue = conditions[i].lhsAttr.attrName;
			}
			else
			{  //两边都是属性或两边都是值，暂不考虑

			}

			OpenScan(rm_fileScan, rm_fileHandle, 2, cons);
			rc = GetNextRec(rm_fileScan, record);
			if (rc != SUCCESS) return rc;

			//cons[i].attrType = conditions[i].attrType;
			selectCons[i].bLhsIsAttr = conditions[i].bLhsIsAttr;
			selectCons[i].bRhsIsAttr = conditions[i].bRhsIsAttr;
			selectCons[i].compOp = conditions[i].op;
			if (conditions[i].bLhsIsAttr == 1) //左边属性
			{ //设置属性长度和偏移量
				memcpy(&selectCons[i].LattrLength, record->pData + 46, 4);
				memcpy(&selectCons[i].LattrOffset, record->pData + 50, 4);
			}
			else {
				selectCons[i].attrType = conditions[i].lhsValue.type;
				selectCons[i].Lvalue = conditions[i].lhsValue.data;
			}

			if (conditions[i].bRhsIsAttr == 1) {
				memcpy(&selectCons[i].RattrLength, record->pData + 46, 4);
				memcpy(&selectCons[i].RattrOffset, record->pData + 50, 4);
			}
			else {
				selectCons[i].attrType = conditions[i].rhsValue.type;
				selectCons[i].Rvalue = conditions[i].rhsValue.data;
			}

			CloseScan(rm_fileScan);

		}

		RM_CloseFile(rm_fileHandle);

		//扫描记录表，找出所有记录
		rc = RM_OpenFile(*relations, rm_fileHandle);
		if (rc != SUCCESS) return rc;

		OpenScan(rm_fileScan, rm_fileHandle, nConditions, selectCons);

		int i = 0;
		resHead->row_num = 0;
		SelResult *curRes = resHead;  //尾插法向链表中插入新结点
		while (GetNextRec(rm_fileScan, record) == SUCCESS)
		{
			if (curRes->row_num >= 100) //每个节点最多记录100条记录
			{ //当前结点已经保存100条记录时，新建结点
				curRes->next_res = (SelResult *)malloc(sizeof(SelResult));
				curRes->next_res->col_num = curRes->col_num;
				for (int j = 0; j < curRes->col_num; j++)
				{
					strncpy(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
					curRes->next_res->attrType[i] = curRes->attrType[i];
					curRes->next_res->offset[i] = curRes->offset[i];
				}

				curRes = curRes->next_res;
				curRes->next_res = NULL;
				curRes->row_num = 0;
			}

			curRes->res[curRes->row_num] = (char **)malloc(sizeof(char *));
			*(curRes->res[curRes->row_num++]) = record->pData;

			//int x;
			//memcpy(&x, record->pData + 18, sizeof(int));
			//memcpy(&x, *(curRes->res[curRes->row_num-1]) + 22, sizeof(int));
		}

		CloseScan(rm_fileScan);
		RM_CloseFile(rm_fileHandle);

		free(rm_fileHandle);
		free(rm_fileScan);
		free(record);
	}

	res = resHead;

	return SUCCESS;
}

/*
* 第一组参数表示查询涉及的属性
* 第二组表示查询涉及的表
* 第三组表示查询条件
* 最后一个参数res用于返回查询结果集
*/
//多表查询
RC multiSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res) 
{
	RC rc;
	SelResult *resHead = res;

	//如果某个属性上有索引，则索引查询；否则，全文件扫描
	if (false)
	{ //此处判断索引情况，暂未实现

	}
	else {
		RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
		RM_FileHandle **fileHandles = (RM_FileHandle **)malloc(sizeof(RM_FileHandle *) * nRelations);//查询涉及的每个表的文件句柄

		RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		//RM_FileScan **fileScans = (RM_FileScan **)malloc(sizeof(RM_FileScan *) * nRelations); //查询涉及的每个表的文件扫描指针

		for (int i = 0; i < nRelations; i++)
		{
			fileHandles[i] = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			//fileScans[i] = (RM_FileScan *)malloc(sizeof(RM_FileScan));

			rc = RM_OpenFile(relations[nRelations - i - 1], fileHandles[i]);  //打开所有涉及到的表文件
			if (rc != SUCCESS) return rc;
		}

		int *offsets = (int *)malloc(sizeof(int)*(nRelations + 1));  //每张表的查询结果在总查询结果中的起始偏移
																	//最后一个值存储着总查询结果的长度
																	//此查询的查询结果存储涉及到的所有表的属性信息
		offsets[0] = 0;
		for (int i = 1; i < nRelations+1; i++)
		{
			offsets[i] = offsets[i - 1] + fileHandles[i - 1]->rm_fileSubHeader->recordSize;
		}

		RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
		Con cons[2];

		cons[0].attrType = chars;
		cons[0].bLhsIsAttr = 1;
		cons[0].LattrOffset = 0;
		cons[0].LattrLength = 21;
		cons[0].compOp = EQual;
		cons[0].bRhsIsAttr = 0;
		//cons[0].Rvalue = *relations;

		if (nSelAttrs == 1 && !strcmp((*selAttrs)->attrName, "*"))
		{  //查询结果为所有属性
			
		}
		else {  //查询结果为指定属性
			resHead->col_num = nSelAttrs; //设置查询结果列数
			resHead->row_num = 0;

			//获得属性的偏移量和类型
			cons[1].attrType = chars;
			cons[1].bLhsIsAttr = 1;
			cons[1].LattrOffset = 21;
			cons[1].LattrLength = 21;
			cons[1].compOp = EQual;
			cons[1].bRhsIsAttr = 0;

			rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
			if (rc != SUCCESS) return rc;

			for (int i = 0; i < resHead->col_num; i++)
			{
				cons[0].Rvalue = (selAttrs[resHead->col_num - i - 1])->relName;  //表名
				cons[1].Rvalue = (selAttrs[resHead->col_num - i - 1])->attrName; //属性名
				OpenScan(rm_fileScan, rm_fileHandle, 2, cons);

				rc = GetNextRec(rm_fileScan, record);
				if (rc != SUCCESS) return rc;
				
				int j = 0;   //当前表在结果中的位置
				for (; j < nRelations; j++)
				{
					if (!strcmp(relations[nRelations-j-1], (char *)cons[0].Rvalue))
					{
						break;
					}
				}

				//SysColumns *column = (SysColumns *)record->pData;
				char * column = record->pData;
				//属性类型
				//resHead->attrType[i] = column->attrtype;
				memcpy(&resHead->attrType[i], column + 42, sizeof(int));
				//属性名
				memcpy(&resHead->fields[i], column + 21, 21);
				//strncpy(resHead->fields[i], column->attrname, column->attrlength);
				//属性偏移量
				memcpy(&resHead->offset[i], column + 50, sizeof(int));
				resHead->offset[i] += offsets[j];
				//属性长度
				memcpy(&resHead->length[i], column + 46, sizeof(int));

				CloseScan(rm_fileScan);
			}

			RM_CloseFile(rm_fileHandle);
		}

		for (int i = 0; i < nRelations; i++)
		{
			rc = RM_CloseFile(fileHandles[i]);  //关闭所有涉及到的表文件
			if (rc != SUCCESS) return rc;
		}

		//释放申请的内存空间
		free(rm_fileHandle);
		free(rm_fileScan);
		for (int i = 0; i < nRelations; i++)
		{
			free(fileHandles[i]);
			//free(fileScans[i]);
		}
		free(fileHandles);
		//free(fileScans);
		free(record);
		

		//递归的获取多表查询的查询结果
		recurSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res, nRelations-1, offsets, NULL);

		free(offsets);
	}

	res = resHead;

	return SUCCESS;
}

/*
* 第一组参数表示查询涉及的属性
* 第二组表示查询涉及的表
* 第三组表示查询条件
* 最后一个参数res用于返回查询结果集
*curTable: 当前要处理的表的位置
*offsets: 表查询结果在总的查询结果中的偏移
*/
//递归的获取多表查询的查询结果
RC recurSelect(int nSelAttrs, RelAttr **selAttrs, int nRelations, char **relations, int nConditions, Condition *conditions, SelResult * res, int curTable, int *offsets, char *curResult) {
	if (curTable < 0)  //递归出口
	{

		SelResult *curRes = res;  //尾插法向链表中插入新结点
		while (curRes->next_res != NULL) {
			curRes = curRes->next_res;
		}

		if (curRes->row_num >= 100) //每个节点最多记录100条记录
		{ //当前结点已经保存100条记录时，新建结点
			curRes->next_res = (SelResult *)malloc(sizeof(SelResult));
			//curRes->next_res->col_num = curRes->col_num;
			//for (int j = 0; j < curRes->col_num; j++)
			//{
				//strncpy(curRes->next_res->fields[i], curRes->fields[i], strlen(curRes->fields[i]));
				//curRes->next_res->attrType[i] = curRes->attrType[i];
				//curRes->next_res->offset[i] = curRes->offset[i];
			//}

			curRes = curRes->next_res;
			curRes->next_res = NULL;
			curRes->row_num = 0;
		}

		curRes->res[curRes->row_num] = (char **)malloc(sizeof(char *));
		*(curRes->res[curRes->row_num]) = (char *)malloc(sizeof(char)*offsets[nRelations]);
		memcpy(*(curRes->res[curRes->row_num]), curResult, offsets[nRelations]);
		curRes->row_num++;
		return SUCCESS;
	}

	RC rc;
	RM_FileHandle *rm_fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

	RM_FileScan *rm_fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	RM_Record *record = (RM_Record *)malloc(sizeof(RM_Record));
	int nSelectCons = 0;  //条件个数
	Con **selectCons = (Con **)malloc(sizeof(Con *) * nConditions);  //条件
	Con cons[2];     //用于查询系统列表，获取属性信息

	RelAttr another;
	int nAnother;

	/*selectCons[0] = (Con *)malloc(sizeof(Con));
	selectCons[0]->attrType = chars;
	selectCons[0]->bLhsIsAttr = 1;
	selectCons[0]->LattrOffset = 0;
	selectCons[0]->LattrLength = 21;
	selectCons[0]->compOp = EQual;
	selectCons[0]->bRhsIsAttr = 0;
	selectCons[0]->Rvalue = relations[curTable];*/


	rc = RM_OpenFile("SYSCOLUMNS", rm_fileHandle);
	if (rc != SUCCESS) return rc;

	//用于查询系统列表的条件
	cons[0].attrType = chars;
	cons[0].bLhsIsAttr = 1;
	cons[0].LattrOffset = 0;
	cons[0].LattrLength = 21;
	cons[0].compOp = EQual;
	cons[0].bRhsIsAttr = 0;
	cons[0].Rvalue = relations[curTable];

	cons[1].attrType = chars;
	cons[1].bLhsIsAttr = 1;
	cons[1].LattrOffset = 21;
	cons[1].LattrLength = 21;
	cons[1].compOp = EQual;
	cons[1].bRhsIsAttr = 0;

	for (int i = 0; i < nConditions; i++)
	{
		if (!((conditions[i].bLhsIsAttr == 1 && !strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			|| (conditions[i].bRhsIsAttr == 1 && !strcmp(conditions[i].rhsAttr.relName, relations[curTable]))))
			continue;   //跳过与当前表无关的条件

		selectCons[nSelectCons] = (Con *)malloc(sizeof(Con));

		//只需设置con[1]->Rvalue,把条件里的属性设置成查询时的值
		if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
		{  //左边是值，右边是属性
			cons[1].Rvalue = conditions[i].rhsAttr.attrName;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
		{   //左边是属性，右边是值
			cons[1].Rvalue = conditions[i].lhsAttr.attrName;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1)
		{  //两边都是属性
			if (!strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			{ //左边是要查询的表
				cons[1].Rvalue = conditions[i].lhsAttr.attrName;
				another.relName = conditions[i].rhsAttr.relName;
				another.attrName = conditions[i].rhsAttr.attrName;
			}
			else
			{  //右边是要查询的表
				cons[1].Rvalue = conditions[i].rhsAttr.attrName;
				another.relName = conditions[i].lhsAttr.relName;
				another.attrName = conditions[i].lhsAttr.attrName;
			}

			int j = 0;
			for (; j < nRelations; j++) {
				if (!strcmp(relations[j], another.relName))
				{
					break;
				}
			}

			if (j >= nRelations)  //语法错误
			{
				return RM_EOF;
			}
			
			if (j < curTable)  //不考虑同一表内属性比较的情况
			{
				continue;
			}

			nAnother = j;
		}
		else
		{  //两边都是值，暂不考虑

		}

		OpenScan(rm_fileScan, rm_fileHandle, 2, cons);
		rc = GetNextRec(rm_fileScan, record);
		if (rc != SUCCESS) return rc;

		//设置属性长度和偏移量
		if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
		{  //左边是值，右边是属性
			selectCons[nSelectCons]->bLhsIsAttr = conditions[i].bLhsIsAttr;
		    selectCons[nSelectCons]->bRhsIsAttr = conditions[i].bRhsIsAttr;
		    selectCons[nSelectCons]->compOp = conditions[i].op;

		   	memcpy(&selectCons[nSelectCons]->RattrLength, record->pData + 46, 4);
		   	memcpy(&selectCons[nSelectCons]->RattrOffset, record->pData + 50, 4);
		   	selectCons[nSelectCons]->attrType = conditions[i].lhsValue.type;
		   	selectCons[nSelectCons]->Lvalue = conditions[i].lhsValue.data;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
		{   //左边是属性，右边是值
			selectCons[nSelectCons]->bLhsIsAttr = conditions[i].bLhsIsAttr;
			selectCons[nSelectCons]->bRhsIsAttr = conditions[i].bRhsIsAttr;
			selectCons[nSelectCons]->compOp = conditions[i].op;

			memcpy(&selectCons[nSelectCons]->LattrLength, record->pData + 46, 4);
			memcpy(&selectCons[nSelectCons]->LattrOffset, record->pData + 50, 4);
			selectCons[nSelectCons]->attrType = conditions[i].rhsValue.type;
			selectCons[nSelectCons]->Rvalue = conditions[i].rhsValue.data;
		}
		else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 1)
		{  //两边都是属性
			selectCons[nSelectCons]->bLhsIsAttr = 1;
			selectCons[nSelectCons]->bRhsIsAttr = 0;
			selectCons[nSelectCons]->compOp = conditions[i].op;

			memcpy(&selectCons[nSelectCons]->LattrLength, record->pData + 46, 4);
			memcpy(&selectCons[nSelectCons]->LattrOffset, record->pData + 50, 4);
			memcpy(&selectCons[nSelectCons]->attrType, record->pData + 42, 4);
			//if (!strcmp(conditions[i].lhsAttr.relName, relations[curTable]))
			//{ //左边是要查询的表, 右边是做笛卡儿积的表
			//	another.relName = conditions[i].rhsAttr.relName;
			//	another.attrName = conditions[i].rhsAttr.attrName;
			//}
			//else
			//{  //右边是要查询的表, 右边是做笛卡尔积的表
			//	another.relName = conditions[i].lhsAttr.relName;
			//	another.attrName = conditions[i].lhsAttr.attrName;
			//}

			//打开系统列表，获取用于做笛卡尔积的属性的信息

			RM_FileHandle *fileHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			rc = RM_OpenFile("SYSCOLUMNS", fileHandle);
			if (rc != SUCCESS) return rc;

			RM_FileScan *fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));

			cons[0].Rvalue = another.relName;
			cons[1].Rvalue = another.attrName;
			OpenScan(fileScan, fileHandle, 2, cons);

			rc = GetNextRec(fileScan, record);
			if (rc != SUCCESS) return rc;

			//memcpy(&selectCons[nSelectCons]->attrType, record->pData + 42, 4);
			//从上层查询结果中提取出用于笛卡尔积的属性值
			int offset;
			memcpy(&offset, record->pData + 50, 4);
			selectCons[nSelectCons]->Rvalue = curResult + offsets[nRelations - nAnother - 1] + offset;

			CloseScan(fileScan);
			RM_CloseFile(fileHandle);
			free(fileScan);
			free(fileHandle);
		}

		nSelectCons++;
		CloseScan(rm_fileScan);

	}
	RM_CloseFile(rm_fileHandle);

	//扫描记录表，找出所有记录
	rc = RM_OpenFile(relations[curTable], rm_fileHandle);
	if (rc != SUCCESS) return rc;
	
	OpenScan(rm_fileScan, rm_fileHandle, nSelectCons, *selectCons);

	rc = GetNextRec(rm_fileScan, record);

	while (rc == SUCCESS)
	{
		char * result = (char *)malloc(sizeof(char)*(offsets[nRelations - curTable - 1 + 1]));
		memcpy(result, curResult, offsets[nRelations - curTable - 1]);
		memcpy(result + offsets[nRelations - curTable - 1], record->pData, rm_fileHandle->rm_fileSubHeader->recordSize);

		recurSelect(nSelAttrs, selAttrs, nRelations, relations, nConditions, conditions, res, curTable-1, offsets, result);
		free(result);

		rc = GetNextRec(rm_fileScan, record);
		//int x;
		//memcpy(&x, record->pData + 18, sizeof(int));
		//memcpy(&x, *(curRes->res[curRes->row_num-1]) + 22, sizeof(int));
	}

	CloseScan(rm_fileScan);
	RM_CloseFile(rm_fileHandle);

	free(rm_fileHandle);
	free(rm_fileScan);
	free(record);
	for (int i = 0; i < nSelectCons; i++)
	{
		free(selectCons[i]);
	}
	selectCons = NULL;
	//free(selectCons);
	return SUCCESS;
}


//判断查询涉及的表是否都存在
//输入参数
//    nRelations: 表的个数
//    relations: 指向表名数组的指针
//当任意一个表不存在时，返回RM_EOF
//都存在，则返回SUCCESS
RC checkTable(int nRelations, char **relations)
{
	RC rc;

	RM_FileHandle *sysTablesHandle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rc = RM_OpenFile("SYSTABLES", sysTablesHandle);  //首先判断表是否存在
	if (rc != SUCCESS) return rc;

	Con condition;
	condition.bLhsIsAttr = 1;
	condition.attrType = chars;
	condition.LattrOffset = 0;

	condition.bRhsIsAttr = 0;
	condition.compOp = EQual;

	RM_FileScan *sysTablesScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	RM_Record rec;
	for (int i = 0; i < nRelations; i++)
	{
		condition.Rvalue = *(relations + i);
		condition.LattrLength = strlen(*(relations + i)) + 1;
		rc = OpenScan(sysTablesScan, sysTablesHandle, 1, &condition);
		if (rc != SUCCESS) break;

		rc = GetNextRec(sysTablesScan, &rec);   //如果表不存在，则返回RM_EOF
		if (rc != SUCCESS) {
			rc = TABLE_NOTEXITST;
			break;
		}
		CloseScan(sysTablesScan);
	}
	if (sysTablesScan->bOpen)
	{
		CloseScan(sysTablesScan);
	}
	free(sysTablesScan);
	RM_CloseFile(sysTablesHandle);
	free(sysTablesHandle);
	return rc;
}