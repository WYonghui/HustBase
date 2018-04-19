#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBaseDoc.h"
#include <iostream>

/*执行sql语句并对应给出提示信息，未改*/
void ExecuteAndMessage(char *sql, CEditArea* editArea, CHustBaseDoc* pDoc)
{//根据执行的语句类型在界面上显示执行结果。此函数需修改
	std::string s_sql = sql;
	if (s_sql.find("select") == 0) {
		SelResult res;
		SelResult *temp = &res;//用于累加查询结果的行数的遍历指针

		Init_Result(&res);
		Query(sql, &res);//执行Query查询

		pDoc->selColNum = res.col_num;//这里是查询结果的列数
		pDoc->selRowNum = 0;//这里是查询结果的行数，注意表头同样需要一行;
							//在显示最终结果时单独对查询结果做了处理，这里是0还是1？执行时具体更改
		temp = &res;
		while (temp)
		{
			pDoc->selRowNum += res.row_num;
			temp = temp->next_res;
		}
		//然后按照顺序，将查询结果以字符串的形式拷贝给pDoc->selResult[i][j]，表头信息同样需要拷贝过来。
		pDoc->isEdit = 1;//构造好查询结果后将该标志位置为1，用于拖动外框时对查询结果进行重绘。

						 //首行表头
		char **fields = new char*[20];
		for (int i = 0; i < pDoc->selColNum; i++)
		{
			fields[i] = new char[20];
			memset(fields[i], '\0', 20);//置空
			memcpy(fields[i], res.fields[i], 20);
		}

		//查询获得的结果
		temp = &res;
		char ***Result = new char**[pDoc->selRowNum];
		for (int i = 0; i < pDoc->selRowNum; i++)
		{
			Result[i] = new char*[pDoc->selColNum];//存放一条记录

			int x;
			memcpy(&x, &(**temp->res[i]) + 18, sizeof(int));
			memcpy(&x, &(**temp->res[i]) + 22, sizeof(int));

			for (int j = 0; j < pDoc->selColNum; j++)
			{
				Result[i][j] = new char[20];
				memset(Result[i][j], '\0', 20);

				memcpy(Result[i][j], (**(temp->res + i)) + temp->offset[j], temp->length[j]);
				if (temp->attrType[j] == ints) {
					int x;
					memcpy(&x, Result[i][j], 4);
					sprintf(Result[i][j], "%d", x);
				}
				else if (temp->attrType[j] == floats) {
					float x;
					memcpy(&x, Result[i][j], 4);
					sprintf(Result[i][j], "%.3f", x);
				}
				//memcpy(Result[i][j], temp->res + temp->offset[j], sizeof(temp->attrType[j]));
			}//要根据结构中具体字段的类型和偏移量，将具体字段从一条记录中拆分出来
			if (i == 99)temp = temp->next_res;//一个结构体中最多100条记录
		}

		//显示
		editArea->ShowSelResult(pDoc->selColNum, pDoc->selRowNum, fields, Result);

		//释放内存空间
		for (int i = 0; i<pDoc->selColNum; i++) {
			delete[] fields[i];
		}
		delete[] fields;

		for (int i = 0; i<pDoc->selRowNum; i++) {
			for (int j = 0; j < pDoc->selColNum; j++)
				delete[] Result[i][j];
		}
		delete[] Result;
		Destory_Result(&res);
		return;
	}

	RC rc = execute(sql,pDoc);
	int row_num = 0;
	char **messages;
	switch (rc) {
	case SUCCESS:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "Successful Execution!";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	case SQL_SYNTAX:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "Syntax Error!";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	default:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "Function Not Implemented!";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	}

}

/**/
bool CanButtonClick()
{//需要重新实现
 //如果当前有数据库已经打开
	return true;
	//如果当前没有数据库打开
	//return false;

}

/*
在路径dbPath下创建一个名为dbName的空库，生成相应的系统文件
感觉这个函数中的dbname函数没有用
*/
RC CreateDB(char *dbpath, char *dbname)
{
	RC rc;
	//创建系统表文件和系统列文件
	SetCurrentDirectory(dbpath);//将生成文件的路径定位到数据库的路径
	//rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	rc = RM_CreateFile("SYSTABLES", 25);
	if (rc != SUCCESS)
	{
		return rc;
	}
	//rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumns));//使用记录文件
	rc = RM_CreateFile("SYSCOLUMNS", 76);//使用记录文件
	if (rc != SUCCESS)
	{
		return rc;
	}
	return SUCCESS;
}

//删除一个非空文件夹下所有内容
void myDeleteDirectory(CString dir_path)
{
	CFileFind tmpFind;
	CString path;

	path.Format("%s/*.*", dir_path);
	BOOL bWorking = tmpFind.FindFile(path);
	while (bWorking)
	{
		bWorking = tmpFind.FindNextFile();
		if (tmpFind.IsDirectory() && !tmpFind.IsDots()) {
			myDeleteDirectory(tmpFind.GetFilePath());//递归删除
			RemoveDirectory(tmpFind.GetFilePath());
		}//处理子文件夹
		else {
			DeleteFile(tmpFind.GetFilePath());
		}//处理文件
	}

}

/*删除一个数据库，dbName为要删除的数据库对应文件夹的路径名*/
RC DropDB(char *dbname)
{
	myDeleteDirectory(dbname);
	if (RemoveDirectory(dbname))
		AfxMessageBox("Delete Database Successfully！");//删除该数据库文件夹,但该函数只能删除空文件夹
	return SUCCESS;
}

/*改变系统的当前数据库为dbName对应的文件夹中的数据库*/
RC OpenDB(char *dbname)
{
	return SUCCESS;
}

/*关闭当前数据库。该操作将关闭当前数据库中打开的所有文件，关闭文件操作将自动使所有相关的缓冲区页面更新到磁盘
未实现
*/
RC CloseDB()
{
	return SUCCESS;
}

/*执行一条除SELECT之外的SQL语句，如果执行成功，返回SUCCESS，否则返回错误码*/
RC execute(char * sql, CHustBaseDoc* pDoc) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//只有两种返回结果SUCCESS和SQL_SYNTAX

	if (rc == SUCCESS)
	{
		//int i = 0;
		switch (sql_str->flag)
		{
			//case 1:
			////判断SQL语句为select语句
			//break;
		case 2:
			//判断SQL语句为insert语句
			Insert(sql_str->sstr.ins.relName, sql_str->sstr.ins.nValues, sql_str->sstr.ins.values);
			pDoc->m_pTreeView->PopulateTree();
			break;
		case 3:
			//判断SQL语句为update语句
			updates up = sql_str->sstr.upd;
			Update(up.relName, up.attrName, &up.value, up.nConditions, up.conditions);
			break;

		case 4:
			//判断SQL语句为delete语句
			Delete(sql_str->sstr.del.relName, sql_str->sstr.del.nConditions, sql_str->sstr.del.conditions);
			break;
			
		case 5:
			//判断SQL语句为createTable语句
			CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
			pDoc->m_pTreeView->PopulateTree(); //更新视图
			//pDoc->m_pListView->displayTabInfo(sql_str->sstr.cret.relName);//右侧刷新表名

			break;

		case 6:
			//判断SQL语句为dropTable语句
			DropTable(sql_str->sstr.cret.relName);
			pDoc->m_pTreeView->PopulateTree(); //更新视图
			//pDoc->m_pListView->displayTabInfo(sql_str->sstr.cret.relName);//右侧刷新表名

			break;

		case 7:
			//判断SQL语句为createIndex语句
			//CreateIndex(sql_str->sstr.crei.indexName, AttrType attrType, int attrLength);
			break;

		case 8:
			//判断SQL语句为dropIndex语句
			break;

		case 9:
			//判断为help语句，可以给出帮助提示
			AfxMessageBox("请自行摸索！");
			break;

		case 10:
			//判断为exit语句，可以由此进行退出操作
			AfxMessageBox("欢迎下次使用！");
			AfxGetMainWnd()->SendMessage(WM_CLOSE);//关闭窗口
			//DestroyWindow();
			break;
		}

		return SUCCESS;
	}
	else {
		AfxMessageBox(sql_str->sstr.errors);//弹出警告框，sql语句词法解析错误信息
		return rc;
	}
}

/*创建一个名为relName的表。参数attrCount表示关系中属性的数量（取值为1到MAXATTRS之间）。参数attributes是一个长度为attrCount的数组。
对于新关系中第i个属性，attributes数组中的第i个元素包含名称、类型和属性的长度（见AttrInfo结构定义）*/
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes)
{
	/*
	typedef struct _ AttrInfo AttrInfo;
	struct _AttrInfo {
	char			*attrName;			// 属性的名字
	AttrType	attrType;			// 属性的类型
	int			attrLength;			// 属性的长度
	};

	*/

	RC rc;
	//char *pData;
	RM_FileHandle *tab_Handle,*col_Handle;
	RID *rid;
	int recordsize=0;//数据表的每条记录的大小

	//打开系统表文件和系统列文件
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_Handle->bOpen = false;
	col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	col_Handle->bOpen = false;
	rid = (RID *)malloc(sizeof(RID));
	rid->bValid = false;

	rc = RM_OpenFile("SYSTABLES", tab_Handle);
	if (rc != SUCCESS)
		return rc;
	rc = RM_OpenFile("SYSCOLUMNS", col_Handle);
	if (rc != SUCCESS)
		return rc;

	//向系统表中插入记录
	char *pData = (char *)malloc(sizeof(SysTable));
	//SysTable sysTable;
	//strncpy(sysTable.tablename, relName, 21);
	//sysTable.attrcount = attrCount;
	
	//memcpy(pData, &sysTable, sizeof(SysTable));
	memcpy(pData, relName, 21);//填充表名
	memcpy(pData + 21, &attrCount, sizeof(int));//填充属性列数
	rc = InsertRec(tab_Handle, pData, rid);
	if (rc != SUCCESS)return rc;
	rc = RM_CloseFile(tab_Handle);
	if (rc != SUCCESS)return rc;
	free(tab_Handle);
	//free(pData);
	free(rid);

	//向系统列中循环填充信息 一个表中包含多个属性列，就需要循环
	for (int i = 0, offset = 0; i < attrCount; i++){
		pData = (char *)malloc(sizeof(SysColumns));
		memcpy(pData, relName, 21);
		memcpy(pData + 21, (attributes+i)->attrName, 21);
		memcpy(pData + 42, &((attributes + i)->attrType), sizeof(int));
		memcpy(pData + 42 + sizeof(int), &((attributes + i)->attrLength), sizeof(int));
		memcpy(pData + 42 + 2*sizeof(int), &offset, sizeof(int));
		memcpy(pData + 42 + 3 * sizeof(int), "0", sizeof(char));
		rid = (RID *)malloc(sizeof(RID));
		rid->bValid = false;
		rc = InsertRec(col_Handle, pData, rid);
		if (rc != SUCCESS){
			return rc;
		}
		free(pData);
		free(rid);
		offset += (attributes + i)->attrLength;
	}
	rc = RM_CloseFile(col_Handle);
	if (rc != SUCCESS)return rc;
	free(col_Handle);

	//创建数据表
	for (int i = 0; i < attrCount; i++){
		recordsize += (attributes + i)->attrLength;
	}
	rc = RM_CreateFile(relName, recordsize);
	if (rc != SUCCESS)return rc;
	return SUCCESS;
}

/*销毁名为relName的表以及在该表上建立的所有索引*/
RC DropTable(char *relName)
{
	CFile tmp;
	RM_FileHandle *tab_Handle, *col_Handle;
	RC rc;
	RM_FileScan *FileScan;
	RM_Record *tab_rec, *col_rec;

	//将系统表和系统列中对应表的相关记录删除
	//打开系统表，系统列文件
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_Handle->bOpen = false;
	col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	col_Handle->bOpen = false;
	tab_rec = (RM_Record*)malloc(sizeof(RM_Record));
	tab_rec->bValid = false;
	col_rec = (RM_Record*)malloc(sizeof(RM_Record));
	col_rec->bValid = false;

	rc = RM_OpenFile("SYSTABLES", tab_Handle);
	if (rc != SUCCESS) return rc;
	rc = RM_OpenFile("SYSCOLUMNS", col_Handle);
	if (rc != SUCCESS) return rc;

	FileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	FileScan->bOpen = false;
	rc = OpenScan(FileScan, tab_Handle, 0, NULL);
	if (rc != SUCCESS) return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于tab_rec中
	while(GetNextRec(FileScan, tab_rec) == SUCCESS){
		if (strcmp(relName, tab_rec->pData) == 0){
			DeleteRec(tab_Handle, &(tab_rec->rid));
			break;
		}
	}
	CloseScan(FileScan);

	FileScan->bOpen = false;
	rc = OpenScan(FileScan, col_Handle, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,删除
	while(GetNextRec(FileScan, col_rec) == SUCCESS){
		if (strcmp(relName, col_rec->pData) == 0){
			DeleteRec(col_Handle, &(col_rec->rid));
			//break;  
		}
	}
	CloseScan(FileScan);
	free(FileScan);


	//关闭文件句柄
	rc = RM_CloseFile(tab_Handle);if (rc != SUCCESS)return rc;
	free(tab_Handle);
	rc = RM_CloseFile(col_Handle);if (rc != SUCCESS)return rc;
	free(col_Handle);

	free(tab_rec);
	free(col_rec);
	
	DeleteFile((LPCTSTR)relName);//删除数据表文件
	return SUCCESS;
}

/*该函数在关系relName的属性attrName上创建名为indexName的索引。函数首先检查在标记属性上是否已经存在一个索引，如果存在，
则返回一个非零的错误码。否则，创建该索引。
创建索引的工作包括：①创建并打开索引文件；②逐个扫描被索引的记录，并向索引文件中插入索引项；③关闭索引*/
RC CreateIndex(char *indexName, char *relName, char *attrName)
{
	return SUCCESS;
}

/*该函数用来删除名为indexName的索引。函数首先检查索引是否存在，如果不存在，则返回一个非零的错误码。否则，销毁该索引*/
RC DropIndex(char *indexName)
{
	return SUCCESS;
}

/*该函数用来在relName表中插入具有指定属性值的新元组，nValues为属性值个数，values为对应的属性值数组。
函数根据给定的属性值构建元组，调用记录管理模块的函数插入该元组，然后在该表的每个索引中为该元组创建合适的索引项*/
RC Insert(char *relName, int nValues, Value * values)
{
	RC rc;
	RID *rid;
	RM_FileScan *FileScan;
	RM_Record *tab_rec, *col_rec;
	RM_FileHandle *tab_Handle, *col_Handle, *data_Handle;


	//扫描系统表文件的条件，查找是否存在该表，不知道对不对
	char *tableName = relName;
	Con *conditions = (Con*)malloc(sizeof(Con));
	conditions->bLhsIsAttr = 1;//条件左是表示是否有索引属性（1）
	conditions->bRhsIsAttr = 0;//条件右是表示值0有索引
	conditions->attrType = chars;//AttrType枚举类型中的
	conditions->LattrLength = 21;//tableName长度为21个字节
	conditions->RattrLength = 0;
	conditions->LattrOffset = 0;//
	conditions->RattrOffset = 0;//疑问：值的偏移是不是0？
	conditions->compOp = EQual;
	conditions->Rvalue = (void*)tableName;//若是值的话，指向对应的值

										  //打开系统表文件
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_Handle->bOpen = false;
	tab_rec = (RM_Record*)malloc(sizeof(RM_Record));
	tab_rec->bValid = false;

	rc = RM_OpenFile("SYSTABLES", tab_Handle);
	if (rc != SUCCESS)
	{
		AfxMessageBox("系统表文件打开失败！");
		return rc;
	}

	FileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	FileScan->bOpen = false;
	rc = OpenScan(FileScan, tab_Handle, 1, conditions);
	if (rc != SUCCESS) {
		AfxMessageBox("系统表文件扫描失败！");
		return rc;
	}
	rc = GetNextRec(FileScan, tab_rec);
	if (rc == SUCCESS)
	{
		int attrCount;
		//memset(&attrCount, '\0', 4);
		memcpy(&attrCount, tab_rec->pData + 21, sizeof(int));
		CloseScan(FileScan);
		free(FileScan);
		if (attrCount == nValues)
		{
			int recordSize = 0;
			SysColumns *tmp, *column;
			//打开系统列文件
			col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			col_Handle->bOpen = false;

			rc = RM_OpenFile("SYSCOLUMNS", col_Handle);
			if (rc != SUCCESS)
			{
				AfxMessageBox("系统列文件打开失败！");
				return rc;
			}
			FileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
			FileScan->bOpen = false;
			col_rec = (RM_Record*)malloc(sizeof(RM_Record));
			col_rec->bValid = false;
			rc = OpenScan(FileScan, col_Handle, 0, NULL);
			if (rc != SUCCESS) {
				AfxMessageBox("系统列文件扫描失败！");
				return rc;
			}
			int i = 0;//记录循环次数
			column = (SysColumns*)malloc(sizeof(SysColumns)*nValues);
			tmp = column;
			while (GetNextRec(FileScan, col_rec) == SUCCESS)
			{
				//char *attrLength = (char*)malloc(4 * sizeof(char));
				int attrLength;
				if (strcmp(relName, col_rec->pData) == 0)
				{
					++i;
					memcpy(&attrLength, col_rec->pData + 46, sizeof(int));
					recordSize += attrLength;//计算数据表记录大小
					memcpy(tmp->tablename, col_rec->pData, 21);
					memcpy(tmp->attrname, col_rec->pData + 21, 21);
					memcpy(&(tmp->attrtype), col_rec->pData + 42, sizeof(int));
					memcpy(&(tmp->attrlength), col_rec->pData + 42 + sizeof(int), sizeof(int));
					memcpy(&(tmp->attroffeset), col_rec->pData + 42 + 2 * sizeof(int), sizeof(int));
					++tmp;//将该表的所有属性信息复制出来
				}
				if (i == nValues)
					break;//减少循环次数
				//free(attrLength);
			}
			tmp = column;
			CloseScan(FileScan);
			free(FileScan);


			//向数据表文件中插入记录
			data_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			data_Handle->bOpen = false;

			rc = RM_OpenFile(relName, data_Handle);
			if (rc != SUCCESS)
			{
				AfxMessageBox("数据表文件打开失败！");
				return rc;
			}
			char *pData = (char*)malloc(recordSize);
			for (int i = 0; i < nValues; i++)
			{
				Value *localValue = values + nValues - i - 1;
				AttrType atype = localValue->type;
				if (atype != (tmp+i)->attrtype)
				{
					AfxMessageBox("插入语句格式错误!");
					return SQL_SYNTAX;//返回错误码
				}
				memcpy(pData + (tmp + i)->attroffeset, localValue->data, (tmp + i)->attrlength);//将插入的记录数据补为符合要求的记录
				//memcpy(pData + (tmp + i)->attroffeset, (values + i)->data, (tmp + i)->attrlength);//将插入的记录数据补为符合要求的记录
			}
			rid = (RID*)malloc(sizeof(RID));
			rid->bValid = false;
			InsertRec(data_Handle, pData, rid);//插入记录

			free(rid); free(tab_rec); free(col_rec);
			//free(pData); 
			//free(column);

			//关闭文件句柄
			rc = RM_CloseFile(tab_Handle); if (rc != SUCCESS)return rc;
			free(tab_Handle);
			rc = RM_CloseFile(data_Handle); if (rc != SUCCESS)return rc;
			free(data_Handle);
		}
		else {
			AfxMessageBox("插入语句格式错误!");
			return SQL_SYNTAX;//返回错误码
		}
	}//表存在
	else
	{
		AfxMessageBox("插入的表不存在!");
		return rc;
	}

	return SUCCESS;
}

/*该函数用来删除relName表中所有满足指定条件的元组以及该元组对应的索引项。
如果没有指定条件，则此方法删除relName关系中所有元组。如果包含多个条件，则这些条件之间为与关系*/
RC Delete(char *relName, int nConditions, Condition *conditions)
{
	RC rc;
	//RID *rid = NULL;
	RM_FileHandle *tab_Handle, *col_Handle, *data_Handle;
	RM_FileScan *fileScan;
	RM_Record *tab_rec, *col_rec, *data_rec;

	//扫描系统表文件的条件
	Con *con = (Con*)malloc(sizeof(Con));
	con->bLhsIsAttr = 1;
	con->bRhsIsAttr = 0;
	con->attrType = chars;
	con->LattrLength = 21;
	con->RattrLength = 0;
	con->LattrOffset = 0;
	con->RattrOffset = 0;
	con->compOp = EQual;
	con->Rvalue = (void *)relName;


	//首先判断是否存在更新元组所在的表
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_rec = (RM_Record*)malloc(sizeof(RM_Record));

	rc = RM_OpenFile("SYSTABLES", tab_Handle);
	if (rc != SUCCESS)
	{
		AfxMessageBox("系统表文件打开失败！");
		return rc;
	}

	fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	rc = OpenScan(fileScan, tab_Handle, 1, con);
	if (rc != SUCCESS) {
		AfxMessageBox("系统表文件扫描失败！");
		return rc;
	}

	rc = GetNextRec(fileScan, tab_rec);
	int nAttrCount;               //表的属性个数
	if (rc == SUCCESS)
	{
		memcpy(&nAttrCount, tab_rec->pData + 21, 4);//将该表的属性个数复制出来

		CloseScan(fileScan);
		free(fileScan);

		//打开系统列文件，得到对应属性的长度和偏移
		//SysColumns *tmp, *column;
		col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

		rc = RM_OpenFile("SYSCOLUMNS", col_Handle);
		if (rc != SUCCESS)
		{
			AfxMessageBox("系统列文件打开失败！");
			return rc;
		}
		fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		col_rec = (RM_Record*)malloc(sizeof(RM_Record));

		//扫描系统列文件的条件
		con = (Con*)realloc(con, 2 * sizeof(Con));
		(con + 1)->bLhsIsAttr = 1;
		(con + 1)->bRhsIsAttr = 0;
		(con + 1)->attrType = chars;
		(con + 1)->LattrLength = 21;
		(con + 1)->RattrLength = 0;
		(con + 1)->LattrOffset = 21;
		(con + 1)->RattrOffset = 0;
		(con + 1)->compOp = EQual;
		//(con + 1)->Rvalue = (void*)attrName;

		/*rc = OpenScan(fileScan, col_Handle, 1, con);
		if (rc != SUCCESS) {
			AfxMessageBox("系统列文件扫描失败！");
			return rc;
		}
		rc = GetNextRec(fileScan, col_rec);
		if (rc == SUCCESS)
		{*/

			////属性类型
			//AttrType attrType;
			//memcpy(&attrType, col_rec->pData + 42, sizeof(AttrType));
			////属性长度和偏移量
			//int attrlength, attrOffset;
			//memcpy(&attrlength, col_rec->pData + 46, sizeof(int));
			//memcpy(&attrOffset, col_rec->pData + 50, sizeof(int));

			///*tmp = column;*/
			//CloseScan(fileScan);

			Con *cons = (Con *)malloc(sizeof(Con) * nConditions);
			for (int i = 0; i < nConditions; i++) {
				//只需设置con[1]->rValue,把条件里的属性设置成查询时的值
				if (conditions[i].bLhsIsAttr == 0 && conditions[i].bRhsIsAttr == 1)
				{  //左边是值，右边是属性
					(con + 1)->Rvalue = conditions[i].rhsAttr.attrName;
				}
				else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
				{   //左边是属性，右边是值
					(con + 1)->Rvalue = conditions[i].lhsAttr.attrName;
				}
				else
				{  //两边都是属性或两边都是值，暂不考虑

				}

				OpenScan(fileScan, col_Handle, 2, con);
				rc = GetNextRec(fileScan, col_rec);
				if (rc != SUCCESS) return rc;

				//cons[i].attrType = conditions[i].attrType;
				cons[i].bLhsIsAttr = conditions[i].bLhsIsAttr;
				cons[i].bRhsIsAttr = conditions[i].bRhsIsAttr;
				cons[i].compOp = conditions[i].op;
				if (conditions[i].bLhsIsAttr == 1) //左边属性
				{ //设置属性长度和偏移量
					memcpy(&cons[i].LattrLength, col_rec->pData + 46, 4);
					memcpy(&cons[i].LattrOffset, col_rec->pData + 50, 4);
				}
				else {
					cons[i].attrType = conditions[i].lhsValue.type;
					cons[i].Lvalue = conditions[i].lhsValue.data;
				}

				if (conditions[i].bRhsIsAttr == 1) {
					memcpy(&cons[i].RattrLength, col_rec->pData + 46, 4);
					memcpy(&cons[i].RattrOffset, col_rec->pData + 50, 4);
				}
				else {
					cons[i].attrType = conditions[i].rhsValue.type;
					cons[i].Rvalue = conditions[i].rhsValue.data;
				}

				CloseScan(fileScan);

			}
			free(fileScan);


			//打开对应的数据表文件
			data_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			data_rec = (RM_Record*)malloc(sizeof(RM_Record));

			rc = RM_OpenFile(relName, data_Handle);
			if (rc != SUCCESS)
			{
				AfxMessageBox("数据表文件打开失败！");
				return rc;
			}
			fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
			rc = OpenScan(fileScan, data_Handle, nConditions, cons);
			if (rc != SUCCESS) {
				AfxMessageBox("数据表文件扫描失败！");
				return rc;
			}

			while (GetNextRec(fileScan, data_rec) == SUCCESS)
			{
				/*memcpy(data_rec->pData + attrOffset, value->data, attrlength);*/

				DeleteRec(data_Handle, &data_rec->rid);   //如果条件都满足，就删除
				//UpdateRec(fileScan->pRMFileHandle, data_rec);
			}


			CloseScan(fileScan);
			free(fileScan);
			free(cons);
		

		rc = RM_CloseFile(tab_Handle);
		rc = RM_CloseFile(data_Handle);
		//释放过程中申请的所有内存空间
		free(con);
		free(tab_Handle);
		free(data_Handle);
		free(tab_rec);
		free(data_rec);
		return SUCCESS;
	}//删除的表存在
	else
	{
		AfxMessageBox("不存在这样的表！");

		RM_CloseFile(tab_Handle);
		free(tab_Handle);
		free(tab_rec);
		CloseScan(fileScan);
		free(fileScan);
		return rc;
	}
	return SUCCESS;
}

/*该函数用于更新relName表中所有满足指定条件的元组，在每一个更新的元组中将属性attrName的值设置为一个新的值。
如果没有指定条件，则此方法更新relName中所有元组。如果要更新一个被索引的属性，应当先删除每个被更新元组对应的索引条目，
然后插入一个新的索引条目*/
RC Update(char *relName, char *attrName, Value *value, int nConditions, Condition *conditions)
{
	RC rc;
	//RID *rid = NULL;
	RM_FileHandle *tab_Handle, *col_Handle, *data_Handle;
	RM_FileScan *fileScan;
	RM_Record *tab_rec, *col_rec, *data_rec;

	//扫描系统表文件的条件
	Con *con = (Con*)malloc(sizeof(Con));
	con->bLhsIsAttr = 1;
	con->bRhsIsAttr = 0;
	con->attrType = chars;
	con->LattrLength = 21;
	con->RattrLength = 0;
	con->LattrOffset = 0;
	con->RattrOffset = 0;
	con->compOp = EQual;
	con->Rvalue = (void *)relName;


	//首先判断是否存在更新元组所在的表
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_rec = (RM_Record*)malloc(sizeof(RM_Record));

	rc = RM_OpenFile("SYSTABLES", tab_Handle);
	if (rc != SUCCESS)
	{
		AfxMessageBox("系统表文件打开失败！");
		return rc;
	}

	fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
	rc = OpenScan(fileScan, tab_Handle, 1, con);
	if (rc != SUCCESS) {
		AfxMessageBox("系统表文件扫描失败！");
		return rc;
	}

	rc = GetNextRec(fileScan, tab_rec);
	int nAttrCount;               //表的属性个数
	if (rc == SUCCESS)
	{
		memcpy(&nAttrCount, tab_rec->pData + 21, 4);//将该表的属性个数复制出来

		CloseScan(fileScan);
		free(fileScan);

		//打开系统列文件，得到对应属性的长度和偏移
//		SysColumns *tmp, *column;
		col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));

		rc = RM_OpenFile("SYSCOLUMNS", col_Handle);
		if (rc != SUCCESS)
		{
			AfxMessageBox("系统列文件打开失败！");
			return rc;
		}
		fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
		col_rec = (RM_Record*)malloc(sizeof(RM_Record));

		//扫描系统列文件的条件
		con = (Con*)realloc(con, 2 * sizeof(Con));
		(con + 1)->bLhsIsAttr = 1;
		(con + 1)->bRhsIsAttr = 0;
		(con + 1)->attrType = chars;
		(con + 1)->LattrLength = 21;
		(con + 1)->RattrLength = 0;
		(con + 1)->LattrOffset = 21;
		(con + 1)->RattrOffset = 0;
		(con + 1)->compOp = EQual;
		(con + 1)->Rvalue = (void*)attrName;

		rc = OpenScan(fileScan, col_Handle, 2, con);
		if (rc != SUCCESS) {
			AfxMessageBox("系统列文件扫描失败！");
			return rc;
		}
		rc = GetNextRec(fileScan, col_rec);
		if (rc == SUCCESS)
		{

			//属性类型
			AttrType attrType;
			memcpy(&attrType, col_rec->pData + 42, sizeof(AttrType));
			//属性长度和偏移量
			int attrlength, attrOffset;
			memcpy(&attrlength, col_rec->pData + 46, sizeof(int));
			memcpy(&attrOffset, col_rec->pData + 50, sizeof(int));

			/*tmp = column;*/
			CloseScan(fileScan);

			Con *cons = (Con *)malloc(sizeof(Con) * nConditions);
			for (int i = 0; i < nConditions; i++) {
				//只需设置con[1]->rValue,把条件里的属性设置成查询时的值
				if (conditions[i].bLhsIsAttr==0 && conditions[i].bRhsIsAttr==1)
				{  //左边是值，右边是属性
					(con + 1)->Rvalue = conditions[i].rhsAttr.attrName;
				}
				else if (conditions[i].bLhsIsAttr == 1 && conditions[i].bRhsIsAttr == 0)
				{   //左边是属性，右边是值
					(con + 1)->Rvalue = conditions[i].lhsAttr.attrName;
				}
				else
				{  //两边都是属性或两边都是值，暂不考虑

				}

				OpenScan(fileScan, col_Handle, 2, con);
				rc = GetNextRec(fileScan, col_rec);
				if (rc != SUCCESS) return rc;

				//cons[i].attrType = conditions[i].attrType;
				cons[i].bLhsIsAttr = conditions[i].bLhsIsAttr;
				cons[i].bRhsIsAttr = conditions[i].bRhsIsAttr;
				cons[i].compOp = conditions[i].op;
				if (conditions[i].bLhsIsAttr == 1) //左边属性
				{ //设置属性长度和偏移量
					memcpy(&cons[i].LattrLength, col_rec->pData + 46, 4);
					memcpy(&cons[i].LattrOffset, col_rec->pData + 50, 4);
				}
				else {
					cons[i].attrType = conditions[i].lhsValue.type;
					cons[i].Lvalue = conditions[i].lhsValue.data;
				}
				
				if (conditions[i].bRhsIsAttr == 1) {
					memcpy(&cons[i].RattrLength, col_rec->pData + 46, 4);
					memcpy(&cons[i].RattrOffset, col_rec->pData + 50, 4);
				}
				else {
					cons[i].attrType = conditions[i].rhsValue.type;
					cons[i].Rvalue = conditions[i].rhsValue.data;
				}

				CloseScan(fileScan);

			}
			free(fileScan);


			//打开对应的数据表文件
			data_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
			data_rec = (RM_Record*)malloc(sizeof(RM_Record));

			rc = RM_OpenFile(relName, data_Handle);
			if (rc != SUCCESS)
			{
				AfxMessageBox("数据表文件打开失败！");
				return rc;
			}
			fileScan = (RM_FileScan *)malloc(sizeof(RM_FileScan));
			rc = OpenScan(fileScan, data_Handle, nConditions, cons);
			if (rc != SUCCESS) {
				AfxMessageBox("数据表文件扫描失败！");
				return rc;
			}

			while (GetNextRec(fileScan, data_rec) == SUCCESS)
			{
				memcpy(data_rec->pData+attrOffset, value->data, attrlength);

				UpdateRec(fileScan->pRMFileHandle, data_rec);
			}
			

			CloseScan(fileScan); 
			free(fileScan);
			free(cons);
		}
		else
		{
			AfxMessageBox("该表不存在这种属性！");
			return rc;
		}

		rc = RM_CloseFile(tab_Handle);
		rc = RM_CloseFile(data_Handle);
		//释放过程中申请的所有内存空间
		free(con); 
		free(tab_Handle); 
		free(data_Handle);
		free(tab_rec); 
		free(data_rec);
		return SUCCESS;
	}//删除的表存在
	else
	{
		AfxMessageBox("不存在这样的表！");
		
		RM_CloseFile(tab_Handle);
		free(tab_Handle);
		free(tab_rec);
		CloseScan(fileScan);
		free(fileScan);
		return rc;
	}
return SUCCESS;
}

/*判断当前数据库中是否存在指定表名的表*/
bool hasTable(char * tableName)
{
	CFile tmp;
	RM_FileHandle *tab_Handle;
	RC rc;
	RM_FileScan *FileScan = NULL;
	RM_Record *tab_rec = NULL;

	//将系统表和系统列中对应表的相关记录删除
	tab_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	tab_Handle->bOpen = false;

	rc = RM_OpenFile("SYSTABLES", tab_Handle);

	FileScan->bOpen = false;
	rc = OpenScan(FileScan, tab_Handle, 0, NULL);
	while (GetNextRec(FileScan, tab_rec) == SUCCESS){
		if (strcmp(tableName, tab_rec->pData) == 0){
			return TRUE;//存在该表的记录
		}
	}
	CloseScan(FileScan);

	//关闭文件句柄
	rc = RM_CloseFile(tab_Handle);
	free(tab_Handle);

	return FALSE;
}

/*判断指定表中是否存在指定列*/
bool hasColumn(char * tableName, char * columnName)
{
	CFile tmp;
	RM_FileHandle *col_Handle;
	RC rc;
	RM_FileScan *FileScan = NULL;
	RM_Record *col_rec = NULL;

	//打开系统表，系统列文件
	col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	col_Handle->bOpen = false;

	rc = RM_OpenFile("SYSCOLUMNS", col_Handle);

	FileScan->bOpen = false;
	rc = OpenScan(FileScan, col_Handle, 0, NULL);
	while (GetNextRec(FileScan, col_rec) == SUCCESS){
		char *data1 = (char *)malloc(25 * sizeof(char));
		char *data2 = (char *)malloc(25 * sizeof(char));
		memset(data1, '\0', 25); memset(data2, '\0', 25);
		memcpy(data1, col_rec->pData, 21); memcpy(data2, col_rec->pData + 21, 21);
		if (!strcmp(tableName, data1) && !strcmp(columnName, data2))
		{
			return TRUE;
		}
		free(data1); free(data2);
	}
	CloseScan(FileScan);

	//关闭文件句柄
	rc = RM_CloseFile(col_Handle); 
	free(col_Handle);

	return FALSE;
}

/*判断指定表的指定字段上是否存在索引*/
bool hasIndex(char * tableName, char * columnName)
{
	CFile tmp;
	RM_FileHandle  *col_Handle;
	RC rc;
	RM_FileScan *FileScan = NULL;
	RM_Record *col_rec = NULL;

	col_Handle = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	col_Handle->bOpen = false;

	rc = RM_OpenFile("SYSCOLUMNS", col_Handle);

	FileScan->bOpen = false;
	rc = OpenScan(FileScan, col_Handle, 0, NULL);
	//循环查找表名为relName对应的系统表中的记录,删除
	while (GetNextRec(FileScan, col_rec) == SUCCESS){
		char *data1 = (char *)malloc(25 * sizeof(char));
		char *data2 = (char *)malloc(25 * sizeof(char));
		memset(data1, '\0', 25); memset(data2, '\0', 25);
		memcpy(data1, col_rec->pData, 21); memcpy(data2, col_rec->pData + 21, 21);
		if (!strcmp(tableName, data1) && !strcmp(columnName, data2))
		{
			char *data = (char *)malloc(sizeof(char));
			memset(data, '\0', 1); memcpy(data, col_rec->pData + 54,1);//将证明索引是否存在的字节粘贴出来
			if (data == "1")return TRUE;
			free(data);
		}
		free(data1); free(data2);
	}
	CloseScan(FileScan);

	//关闭文件句柄
	rc = RM_CloseFile(col_Handle);
	free(col_Handle);

	return FALSE;
}

