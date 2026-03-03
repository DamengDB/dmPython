//-----------------------------------------------------------------------------
// Cursor.c
//   Definition of the Python type Cursor.
//-----------------------------------------------------------------------------
#include "py_Dameng.h"
#include "row.h"
#include "Error.h"
#include "Buffer.h"
#include "var_pub.h"
#include "stdio.h"
#include "trc.h"

#include <datetime.h>

sdint4 Cursor_escape_quotes(char* dst, int dst_len, char* src, int src_len);

static
PyObject*
Cursor_GetDescription(
    dm_Cursor   *self,
    void*       args
);

PyObject*
Cursor_Execute_inner(
    dm_Cursor*      self, 
    PyObject*       statement,
    PyObject*       executeArgs,
    int             is_many,
    int             exec_direct,
    int             from_call
);

static
sdint2
Cursor_GetParamDescFromDm(
    dm_Cursor*     self
);

PyObject*
Cursor_MakeupProcParams(
	dm_Cursor*     self
);

static
void
Cursor_ExecRs_Clear(
    dm_Cursor*     self    // cursor to set the rowcount on
);

static 
sdint2
Cursor_SetRowCount(
    dm_Cursor*     self    // cursor to set the rowcount on
);

/************************************************************************
purpose:
    �������ִ��id
************************************************************************/
static 
sdint2
Cursor_SetExecId(
    dm_Cursor*     self    /*IN: cursor to set the rowcount on*/
);

void
Cursor_Data_init()
{
	PyDateTime_IMPORT;
}

static 
void
Cursor_init_inner(
    dm_Cursor*     self
)
{
    Py_INCREF(Py_None);
    self->statement     = Py_None;

    Py_INCREF(Py_None);
    self->environment   = (dm_Environment*)Py_None;

    Py_INCREF(Py_None);
    self->connection    = (dm_Connection*)Py_None;

    Py_INCREF(Py_None);
    self->rowFactory    = Py_None;

    Py_INCREF(Py_None);
    self->inputTypeHandler  = Py_None;

    Py_INCREF(Py_None);
    self->outputTypeHandler = Py_None;

    Py_INCREF(Py_None);
    self->description       = Py_None;

    Py_INCREF(Py_None);
    self->map_name_to_index = Py_None;

    Py_INCREF(Py_None);
    self->column_names      = Py_None;

    Py_INCREF(Py_None);
    self->lastrowid_obj     = Py_None;

    Py_INCREF(Py_None);
    self->execid_obj        = Py_None;

    self->rowNum            = 0;
    self->with_rows         = 0;
    self->rowCount          = -1;

    self->col_variables     = NULL;
    self->param_variables   = NULL;
    self->execute_num       = 0;
}

static 
sdint2
Cursor_IsOpen_without_err(
    dm_Cursor*     self
)
{
    if (self->isOpen <= 0)
    {
        return -1;
    }

    return 0;
}

static 
sdint2
Cursor_IsOpen(
    dm_Cursor*     self
)
{
	if (Cursor_IsOpen_without_err(self) < 0){
		PyErr_SetString(g_InternalErrorException, "Not Open");
		return -1;
	}

	return 0;
}

sdint2
Cursor_AllocHandle(
    dm_Cursor*     self
)
{
    DPIRETURN		rt = DSQL_SUCCESS;
    dhstmt			hstmt;	

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_alloc_stmt(self->connection->hcon, &hstmt);	
        rt = dpi_set_stmt_attr(hstmt, DSQL_ATTR_CURSOR_TYPE, (dpointer)DSQL_CURSOR_STATIC, 0);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->connection->hcon, DSQL_HANDLE_DBC, rt, "Cursor_Init():dpi_alloc_stmt") < 0)
            return -1;	

    self->handle    = hstmt;
    return 0;
}

/************************************************************************/
/* purpose:
    set default schema
/************************************************************************/
sdint2  /* ���ش����� */
Cursor_SetSchema_And_Parsetype(
    dm_Cursor*     self        /* IN:cursor���� */
)
{
    DPIRETURN		rt = DSQL_SUCCESS;
    dhstmt			hstmt = self->handle;
    dm_Buffer       sch_buf, parse_buf;
    sdbyte          sql[300];
    sdbyte          schema_name[NAMELEN * 2 + 1];

    //if schema does not set, then return
    if (self->connection->schema != Py_None)
    {
        //get schema from connection obj
        if (dmBuffer_FromObject(&sch_buf, self->connection->schema, self->environment->encoding) < 0)
        {
            PyErr_SetString(PyExc_TypeError, "expecting a None or string schema arguement");
            return -1;
        }

        Cursor_escape_quotes(schema_name, NAMELEN * 2 + 1, sch_buf.ptr, sch_buf.size);

        //set schema
        aq_sprintf(sql, 300, "set schema \"%s\";", (sdbyte*)schema_name);

        Py_BEGIN_ALLOW_THREADS
            rt = dpi_exec_direct(self->handle, sql);
        Py_END_ALLOW_THREADS

        dmBuffer_Clear(&sch_buf);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_InternalPrepare(): prepare") < 0)
        {
            return -1;
        }
    }

    //if parse_type does not set, then return
    if (self->connection->parse_type != Py_None)
    {
        //get parse_type from connection obj
        if (dmBuffer_FromObject(&parse_buf, self->connection->parse_type, self->environment->encoding) < 0)
        {
            PyErr_SetString(PyExc_TypeError, "expecting a None or string parse_type arguement");
            return -1;
        }

        //set parse_type
        aq_sprintf(sql, 300, "SP_SET_SESSION_PARSE_TYPE('%s');", (sdbyte*)parse_buf.ptr);

        Py_BEGIN_ALLOW_THREADS
            rt = dpi_exec_direct(self->handle, sql);
        Py_END_ALLOW_THREADS

        dmBuffer_Clear(&parse_buf);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_InternalPrepare(): prepare") < 0)
        {
            return -1;
        }
    }

    return 0;
}

PyObject*
Cursor_New(    
    dm_Connection*     connection
)
{
    dm_Cursor*         self;
    
    self                = (dm_Cursor*)g_CursorType.tp_alloc(&g_CursorType, 0);
    if (self == NULL)
        return NULL;    

    Cursor_init_inner(self);

    Py_INCREF(connection);
    self->connection    = connection;

    Py_INCREF(connection->environment);
    self->environment   = connection->environment;

    // ���������	
    if (Cursor_AllocHandle(self) < 0)
    {
        Cursor_free_inner(self);
        Py_TYPE(self)->tp_free((PyObject*) self);

        return NULL;
    }

    //����ģʽ
    if (Cursor_SetSchema_And_Parsetype(self))
    {
        Cursor_free_inner(self);
        Py_TYPE(self)->tp_free((PyObject*) self);

        return NULL;
    }
    
    self->execute_num   = 0;
    self->arraySize     = 50;
    self->org_arraySize = self->arraySize;
    self->bindArraySize = 1;    
    self->org_bindArraySize = self->bindArraySize;
    self->statementType = -1;
    self->outputSize    = -1;
    self->outputSizeColumn = -1;
    self->isOpen        = 1;
    self->isClosed      = 0;

    self->bindColDesc   = NULL;
    self->bindParamDesc = NULL;
    self->paramCount    = 0;
    self->colCount      = 0;
    self->rowNum        = 0;

    //��Cursor_New�����ã�����close��ȡ����rowcountֵ
    self->totalRows     = -1;

    self->is_iter       = 0;
    self->output_stream = 0;
    self->outparam_num  = 0;
    self->param_value   = NULL;
    return (PyObject*)self;
}

static 
PyObject*
Cursor_Repr(
    dm_Cursor*     cursor
)
{
	PyObject *connectionRepr, *module, *name, *result, *format, *formatArgs;

    format = dmString_FromAscii("<%s.%s on %s>");
    if (!format)
        return NULL;

    connectionRepr = PyObject_Repr((PyObject*) cursor->connection);
    if (!connectionRepr) {
        Py_DECREF(format);
        return NULL;
    }

    if (GetModuleAndName(Py_TYPE(cursor), &module, &name) < 0) {
        Py_DECREF(format);
        Py_DECREF(connectionRepr);
        return NULL;
    }

    formatArgs = PyTuple_Pack(3, module, name, connectionRepr);
    Py_DECREF(module);
    Py_DECREF(name);
    Py_DECREF(connectionRepr);
    if (!formatArgs) {
        Py_DECREF(format);
        return NULL;
    }

    result = PyUnicode_Format(format, formatArgs);
    Py_DECREF(format);
    Py_DECREF(formatArgs);
    return result;
}


//-----------------------------------------------------------------------------
// Cursor_FreeHandle()
//   Free the handle 
//-----------------------------------------------------------------------------
sdint2 
Cursor_FreeHandle(
    dm_Cursor*      self,       // cursor object
	int             raiseException      // raise an exception, if necesary?
)
{
    DPIRETURN   rt = DSQL_SUCCESS;

	if (self->handle) 
    {
		Py_BEGIN_ALLOW_THREADS
			rt = dpi_free_handle(DSQL_HANDLE_STMT, self->handle);
		Py_END_ALLOW_THREADS
		if (raiseException && 
			Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, "Cursor_FreeHandle():cursor free") < 0)
			return -1;
	}

    self->handle = NULL;
	return  0;
}

void
Cursor_free_paramdesc(
	dm_Cursor*     self
)
{	    
    self->hdesc_param   = NULL;

	if (self->bindParamDesc != NULL)
    {        
		PyMem_Free(self->bindParamDesc);
	}
    self->bindParamDesc = NULL;  

    self->paramCount    = 0;

    self->outparam_num  = 0;
}

void
Cursor_free_coldesc(
	dm_Cursor*     self
)
{	
    self->hdesc_col = NULL;

	if (self->bindColDesc != NULL)
	{		
		PyMem_Free(self->bindColDesc);
	}
	self->bindColDesc = NULL;    
}

void
Cursor_free_inner(
    dm_Cursor*     self
)
{
    Cursor_free_paramdesc(self);
    Cursor_free_coldesc(self);

    Py_CLEAR(self->statement);
    Py_DECREF(self->environment);
    Py_DECREF(self->connection);      
    Py_CLEAR(self->rowFactory);    
    Py_CLEAR(self->inputTypeHandler);    
    Py_CLEAR(self->outputTypeHandler);    
    Py_CLEAR(self->description);    
    Py_CLEAR(self->map_name_to_index);
    Py_CLEAR(self->column_names);
    Py_CLEAR(self->param_variables);
    Py_CLEAR(self->col_variables);
    Py_CLEAR(self->lastrowid_obj);
    Py_CLEAR(self->execid_obj);
}

sdint2
Cursor_InternalClose(
	dm_Cursor*     self
)
{
	Py_BEGIN_ALLOW_THREADS	
	dpi_close_cursor(self->handle);
	Py_END_ALLOW_THREADS

	return 0;
}

static 
PyObject*
Cursor_Close_inner(
    dm_Cursor*     self
)
{
    /** ����ʾ���ù�Cursor_Close���򷵻� **/
	if (Cursor_IsOpen(self) < 0)
    {
		PyErr_Clear();

        Py_RETURN_NONE;
    }

    /** ������δ�Ͽ�����ִ�о����Դ�ͷ� **/
    if (self->connection->isConnected == 1)
    {
        Cursor_InternalClose(self);

        Cursor_FreeHandle(self, 0);
    }	

    /** �ͷ�Cursor�ڲ�������Դ **/
    Cursor_free_inner(self);

    Cursor_init_inner(self);

	self->isOpen = 0;
    self->isClosed = 1;

	Py_INCREF(Py_None);
	return Py_None;
}

static 
PyObject*
Cursor_Close(
    dm_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_Close\n"));

    retObj      = Cursor_Close_inner(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_Close, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
void
Cursor_Free(
    dm_Cursor*     self
)
{	
    if (Cursor_IsOpen_without_err(self) >= 0)    
        Cursor_Close(self);    

    Cursor_free_inner(self);

	Py_TYPE(self)->tp_free((PyObject*) self);
}

static 
sdint2
Cursor_IsDDL(
    sdint2      stmtType
)
{
	switch(stmtType){
		case DSQL_DIAG_FUNC_CODE_CREATE_TAB:
		case DSQL_DIAG_FUNC_CODE_DROP_TAB:
		case DSQL_DIAG_FUNC_CODE_CREATE_VIEW:
		case DSQL_DIAG_FUNC_CODE_DROP_VIEW:
		case DSQL_DIAG_FUNC_CODE_CREATE_INDEX:
		case DSQL_DIAG_FUNC_CODE_DROP_INDEX:
		case DSQL_DIAG_FUNC_CODE_CREATE_USER:
		case DSQL_DIAG_FUNC_CODE_DROP_USER:
		case DSQL_DIAG_FUNC_CODE_CREATE_ROLE:
		case DSQL_DIAG_FUNC_CODE_DROP_ROLE:
		case DSQL_DIAG_FUNC_CODE_DROP:
		case DSQL_DIAG_FUNC_CODE_CREATE_SCHEMA:
		case DSQL_DIAG_FUNC_CODE_CREATE_CONTEXT_INDEX:
		case DSQL_DIAG_FUNC_CODE_DROP_CONTEXT_INDEX:
		case DSQL_DIAG_FUNC_CODE_CREATE_LINK:
			return 0;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Cursor_GetStatementType()
//   Determine if the cursor is executing a select statement.
//-----------------------------------------------------------------------------
static 
sdint2 
Cursor_GetStatementType(
    dm_Cursor *self        // cursor to perform binds on
)
{
	sdint4          statementType;
	slength         len;
	DPIRETURN       status = DSQL_SUCCESS;
    Py_ssize_t      size, cols;
    dm_Var*         dm_var;

	Py_BEGIN_ALLOW_THREADS
	status = dpi_get_diag_field(DSQL_HANDLE_STMT, self->handle, 0, 
		DSQL_DIAG_DYNAMIC_FUNCTION_CODE, (dpointer) &statementType, 0, &len);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_GetStatementType()") < 0)
	{
		return -1;
	}

	self->statementType = statementType;
    //bug633895 ��Ϊ�������տ��������ӹرպ�ִ�У������ѱ����ٵ�obj���ظ��ͷţ�������ǰ�ͷ����õ�vobject��
    if (self->col_variables == NULL)
        cols = 0;
    else
        cols = PyList_GET_SIZE(self->col_variables);
    for (size = 0; size < cols; size++)
    {
        dm_var = (dm_Var*)PyList_GET_ITEM(self->col_variables, size);
        if (dm_var->type == &vt_Object)
        {
            (*dm_var->type->finalizeProc)(dm_var);
        }
    }
    Py_CLEAR(self->col_variables);

	return 0;
}

/* �ж��Ƿ�ִ�й�prepare������ȡִ����� */
static
sdint2
Cursor_hasPrepared(
    dm_Cursor*      self,               // cursor to perform prepare on
    PyObject**      statement,
    dm_Buffer*      buffer,
    int             direct_flag
)
{
    /* û��ִ����䣬Ҳû��ִ�й�prepare */
    if ((*statement == Py_None) && 
        (self->statement == NULL || self->statement == Py_None)) 
    {
        PyErr_SetString(g_ProgrammingErrorException,
            "no statement specified and no prior statement prepared");

        return -1;
    }    

    /* ���ڷ�DDL��䣬����Ҫ�ٴ�prepare, executedirect����ִ��prepare����Ҫ������ȡ�ϴ�ִ����� */
    if (*statement == Py_None || *statement == self->statement)        
    {
        if(!direct_flag && Cursor_IsDDL (self->statementType) < 0)
            return 1;

        //Py_INCREF(self->statement);
        *statement = self->statement;
    }

    if (dmBuffer_FromObject(buffer, *statement, self->environment->encoding) < 0)
    {
        //Py_XDECREF(*statement);		
        return -1;
    }

    /* ��䳤��Ϊ0 */    
    if (strlen((char*)buffer->ptr) == 0)
    {
        PyErr_SetString(g_ProgrammingErrorException,
            "no statement specified and no prior statement prepared");

        dmBuffer_Clear(buffer); 
        return -1;
    }

    Py_CLEAR(self->statement);        
    return 0;
}

static
void
Cursor_clearDescExecInfo(
    dm_Cursor*      self,
    int             clear_param
)
{
    /* �ر��α� */
    Cursor_InternalClose(self);

    /* ��������������Ϣ */
    if (clear_param)
    {
        Cursor_free_paramdesc(self);
    }

    /* ������������Ϣ */
    Cursor_free_coldesc(self);

    /** ����ϴ�ִ�н�� **/
    Cursor_ExecRs_Clear(self);
}

//-----------------------------------------------------------------------------
// Cursor_InternalPrepare()
//   Internal method for preparing a statement for execution.
//-----------------------------------------------------------------------------
static 
sdint2 
Cursor_InternalPrepare(
    dm_Cursor*      self,               // cursor to perform prepare on
    PyObject*       statement           // statement to prepare    
)
{
    dm_Buffer       statementBuffer;  
    DPIRETURN       status = DSQL_SUCCESS;
    sdint2          ret;

    /* �ж�����Ѿ�ִ��prepare����ֱ�ӷ��� */
    ret = Cursor_hasPrepared(self, &statement, &statementBuffer, 0);
    if (ret != 0)
        return ret;

    /* �����ϴε�������ִ����Ϣ */
    Cursor_clearDescExecInfo(self, 1);

	// prepare statement
    Py_BEGIN_ALLOW_THREADS
        //prepare֮ǰ�����һ�ΰ󶨲�����Ϣ
        status = dpi_unbind_params(self->handle);
        status = dpi_prepare(self->handle, (sdbyte*)statementBuffer.ptr);
    Py_END_ALLOW_THREADS

    dmBuffer_Clear(&statementBuffer);    
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
            "Cursor_InternalPrepare(): prepare") < 0) 
	{
        return -1;
	}

    // clear bind variables, if applicable
    if (!self->setInputSizes) 
    {
        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    // clear row factory, if spplicable
    Py_XDECREF(self->rowFactory);
    self->rowFactory = NULL;


    /* ��ȡ��������������Ϣ��cursor.prepare��execute�е�prepare����Ҫ��ȡ������Ϣ */
    if (Cursor_GetParamDescFromDm(self) < 0)
        return -1;

    Py_INCREF(statement);
    self->statement     = statement;

    return 0;
}

static 
sdint2 
Cursor_InternalExecDirect(
    dm_Cursor*      self,               // cursor to perform prepare on
    PyObject*       statement           // statement to prepare      
)
{
    dm_Buffer       statementBuffer;
    DPIRETURN       status = DSQL_SUCCESS;

    /* dpi_exec_direct����Ҫִ��prepare�����ô˽ӿ����ڻ�ȡִ����� */
    if (Cursor_hasPrepared(self, &statement, &statementBuffer, 1) < 0)
        return -1;

    /* �����ϴε�������ִ����Ϣ */
    Cursor_clearDescExecInfo(self, 1);

    // prepare statement
    Py_BEGIN_ALLOW_THREADS
        status = dpi_exec_direct(self->handle, (sdbyte*)statementBuffer.ptr);
    Py_END_ALLOW_THREADS

    dmBuffer_Clear(&statementBuffer);
    
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
            "Cursor_InternalPrepare(): prepare") < 0) 
    {
        return -1;
    }

    // clear bind variables, if applicable
    if (!self->setInputSizes) 
    {
        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    // clear row factory, if spplicable
    Py_XDECREF(self->rowFactory);
    self->rowFactory = NULL;

    // determine if statement is a query
    if (Cursor_GetStatementType(self) < 0)
        return -1;	

    /* ��ȡִ�н����Ϣ */
    if (Cursor_SetRowCount(self) < 0)
        return -1;

    /* ����execid */
    if (Cursor_SetExecId(self))
    {
        return -1;
    }

    Py_INCREF(statement);
    self->statement     = statement;

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_ExecRs_Clear()
//   ����ϴ�ִ�н��Ӱ��
//-----------------------------------------------------------------------------
static
void
Cursor_ExecRs_Clear(
    dm_Cursor*     self    // cursor to set the rowcount on
)
{
    // ����ϴ�ִ�еĽ����������¼
    if (self->description != Py_None)
    {
        Py_CLEAR(self->description);

        Py_INCREF(Py_None);
        self->description = Py_None;		
    }

    if (self->map_name_to_index != Py_None)
    {
        Py_CLEAR(self->map_name_to_index);

        Py_INCREF(Py_None);
        self->map_name_to_index = Py_None;
    }

    if (self->column_names != Py_None)
    {
        Py_CLEAR(self->column_names);

        Py_INCREF(Py_None);
        self->column_names  = Py_None;
    }

    self->colCount  = 0;
    self->rowNum    = 0;
    self->rowCount  = -1;
    self->with_rows = 0;
}

//-----------------------------------------------------------------------------
// Cursor_SetRowCount()
//   Set the rowcount variable.
//-----------------------------------------------------------------------------
static 
sdint2
Cursor_SetRowCount(
    dm_Cursor*     self    // cursor to set the rowcount on
)
{
	sdint8      rowCount;
	DPIRETURN   status = DSQL_SUCCESS; 
#ifdef DSQL_ROWID
    sdbyte      lastrowid[12];
#else
    sdint8      lastrowid;
#endif
    sdbyte      lastrowid_str[20];
    udint4      len;

    if (self->statementType == DSQL_DIAG_FUNC_CODE_SELECT||
        self->statementType == DSQL_DIAG_FUNC_CODE_CALL) {		
		self->rowCount = 0;				
		// ��¼һ��fetch������ȡ�������ʣ������
		self->actualRows    = -1;

		Py_BEGIN_ALLOW_THREADS
			status = dpi_row_count(self->handle, &rowCount);
		Py_END_ALLOW_THREADS

		if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
			"Cursor_SetRowCount()") < 0)
        {
			return -1;
        }

		self->totalRows = (slength)rowCount;	

        /** ���Ƿ���ڽ������ʶ **/
        if (self->totalRows > 0)
        {
            self->with_rows = 1;
        }

	} else if (self->statementType == DSQL_DIAG_FUNC_CODE_INSERT ||
		self->statementType == DSQL_DIAG_FUNC_CODE_UPDATE ||
		self->statementType == DSQL_DIAG_FUNC_CODE_DELETE ||
        self->statementType == DSQL_DIAG_FUNC_CODE_MERGE) {
			Py_BEGIN_ALLOW_THREADS
				status = dpi_row_count(self->handle, &rowCount);
			Py_END_ALLOW_THREADS
			if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
				"Cursor_SetRowCount()") < 0)
			{
				return -1;
			}

			self->totalRows = (slength)rowCount;
	} else {
		self->totalRows     = -1;
	}

    /** ׷�ӻ�ȡlastrowid **/
    Py_DECREF(self->lastrowid_obj);
    if (self->statementType == DSQL_DIAG_FUNC_CODE_INSERT ||
        self->statementType == DSQL_DIAG_FUNC_CODE_UPDATE ||
        self->statementType == DSQL_DIAG_FUNC_CODE_DELETE )
    {
        Py_BEGIN_ALLOW_THREADS
            status = dpi_get_diag_field(DSQL_HANDLE_STMT, self->handle, 0, DSQL_DIAG_ROWID, (dpointer)&lastrowid, sizeof(lastrowid), NULL);
        Py_END_ALLOW_THREADS
            if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                "Cursor_SetRowCount()") < 0)
            {
                return -1;
            }
#ifdef DSQL_ROWID
            status  = dpi_rowid_to_char(self->connection->hcon, lastrowid, sizeof(lastrowid), lastrowid_str, sizeof(lastrowid_str), &len);
            if (status == 0 && len > 0)
            {
                self->lastrowid_obj = Py_BuildValue("s#", lastrowid_str, len);
            }
            else
            {
                Py_INCREF(Py_None);
                self->lastrowid_obj     = Py_None;
            }
#else
            self->lastrowid_obj = Py_BuildValue("l", lastrowid);
#endif
    }
    else
    {
        Py_INCREF(Py_None);
        self->lastrowid_obj     = Py_None;
    }

	return 0;
}

/************************************************************************
purpose:
    �������ִ��id
************************************************************************/
static 
sdint2
Cursor_SetExecId(
    dm_Cursor*     self    /*IN: cursor to set the rowcount on*/
)
{
    DPIRETURN   status = DSQL_SUCCESS; 
    udint4      execid;

    /** ��ȡexecid **/
    Py_DECREF(self->execid_obj);

    Py_BEGIN_ALLOW_THREADS
    status = dpi_get_diag_field(DSQL_HANDLE_STMT, self->handle, 0, DSQL_DIAG_EXECID, (dpointer)&execid, 0, NULL);
    Py_END_ALLOW_THREADS

    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
        "Cursor_SetRowCount()") < 0)
    {
        return -1;
    }

    self->execid_obj = Py_BuildValue("l", execid);
   
    return 0;
}

static
sdint2
Cursor_PutDatadmVar_onerow(
    dm_Cursor*      self,
    Py_ssize_t      irow
)
{
    udint4          i;
    dm_Var*         var;

    for (i = 0; i < self->paramCount; i ++)
    {
        var     = (dm_Var*)PyList_GET_ITEM(self->param_variables, i);

        if (dmVar_PutDataAftExec(var, (udint4)irow) < 0)
        {
            return -1;
        }
    }

    return 0;
}

static
sdint2
Cursor_PutDataVariable(
    dm_Cursor*      self,
    Py_ssize_t      rowsize
)
{    
    int             rt = 0;
    Py_ssize_t      i;

    /* dpi_param_data�������ã�ָ��ĳ��Ҫput_data���У����в���put�꣬�ٵ���һ�Σ�֪ͨ������� */
    for (i = 0; i < rowsize; i ++)
    {
        rt      = Cursor_PutDatadmVar_onerow(self, i);
        if (rt < 0)
        {
            return rt;
        }
    }
 
    Py_BEGIN_ALLOW_THREADS
        rt = dpi_param_data(self->handle, NULL);
    Py_END_ALLOW_THREADS
    /* ��Ϊ0����ֱ�ӷ��أ�˵������Ҫ��ֵ */
    if (rt == DSQL_SUCCESS || rt == DSQL_PARAM_DATA_AVAILABLE)
        return rt;

    /* ����ʧ�ܣ�������DSQL_NEED_DATA���򱨴� */
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
        "vLong_PutData():dpi_param_data") < 0)
    {
        return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_InternalExecute()
//   Perform the work of executing a cursor and set the rowcount appropriately
// regardless of whether an error takes place.
//-----------------------------------------------------------------------------
static 
sdint2
Cursor_InternalExecute(
	dm_Cursor*      self,
    Py_ssize_t      rowsize
)
{
    DPIRETURN       status      = DSQL_SUCCESS;
    DPIRETURN       rt          = DSQL_SUCCESS;
    sdint2          ret;
    dpointer        ptr         = NULL;
    dpointer        data_ptr    = NULL;
    dm_Var*         dm_var;
    udint4          cols        = 0;
    PyObject*       newParamVal;
    sdint4*         length      = PyMem_Malloc(sizeof(sdint4));
    udint4          i           = 0;

    Cursor_clearDescExecInfo(self, 0);

	Py_BEGIN_ALLOW_THREADS
		status = dpi_exec(self->handle);
	Py_END_ALLOW_THREADS

    /* ��NEED_DATA����long string/binary�����򲹳����� */
    if (status == DSQL_NEED_DATA)
    {
        status = Cursor_PutDataVariable(self, rowsize);
        if (status < 0)
        {
            PyMem_Free(length);
            return -1;        
        }
    }

    if(self->output_stream == 1 && self->outparam_num > 0)
    {
        while (status == DSQL_SUCCESS)
        {
            Py_BEGIN_ALLOW_THREADS
                status = dpi_more_results(self->handle);
            Py_END_ALLOW_THREADS
        }
    }

    //�������ֵΪDSQL_PARAM_DATA_AVAILABLE������ʽ��ȡ�������
    if (self->output_stream == 1 && status == DSQL_PARAM_DATA_AVAILABLE)
    {
        self->param_value =(PyObject**) PyMem_Malloc((self->outparam_num)*sizeof(PyObject*));

        for(i = 0; i < self->outparam_num; i++)
        {
            self->param_value[i]= PyList_New(0);
        }

        while(1)
        {
            Py_BEGIN_ALLOW_THREADS
            rt = dpi_param_data(self->handle, &ptr);
            Py_END_ALLOW_THREADS

            if (rt == DSQL_PARAM_DATA_AVAILABLE)
            {
                dm_var = (dm_Var*)PyList_GET_ITEM(self->param_variables, ((udbyte)ptr)-1);
                data_ptr = (dpointer)dm_var->data;

                if (Py_TYPE(dm_var) == &g_LongBinaryVarType ||
                    Py_TYPE(dm_var) == &g_LongStringVarType)
                {
                    Py_BEGIN_ALLOW_THREADS
                        rt = dpi_get_data(self->handle, (udbyte)ptr, dm_var->type->cType, NULL, 0, length);
                    Py_END_ALLOW_THREADS
                    if (!DSQL_SUCCEEDED(rt))
                        *length = 0;
                    if(((sdint8*)dm_var->data)[0] != 0)
                        PyMem_FREE((char*)(int3264)((sdint8*)dm_var->data)[0]);
                    ((sdint8*)dm_var->data)[0] = (sdint8)(int3264)PyMem_Malloc(*length + 1);
                    data_ptr = (dpointer)(int3264)((sdint8*)dm_var->data)[0];
                    dm_var->bufferSize = *length + 1;
                }

                Py_BEGIN_ALLOW_THREADS
                    rt = dpi_get_data(self->handle, (udbyte)ptr, dm_var->type->cType, data_ptr, dm_var->bufferSize, length);
                Py_END_ALLOW_THREADS

                if (DSQL_SUCCEEDED(rt))
                {
                    dm_var->indicator[0] = *length;
                    dm_var->actualLength[0] = *length;
                    newParamVal = dmVar_GetValue((dm_Var*)dm_var, 0);
                    PyList_Append(self->param_value[cols++], newParamVal);
                }
                else if (rt == DSQL_NO_DATA)
                {
                    PyList_Append(self->param_value[cols++], Py_None);
                }
                else if (rt == DSQL_ERROR)
                {
                    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                        "Cursor_InternalExecute()") < 0)
                    {
                        for (i = 0; i < self->outparam_num; i++)
                        {
                            Py_DECREF(self->param_value[i]);
                        }
                        PyMem_Free(self->param_value);
                        PyMem_Free(length);
                        return -1;
                    }
                }
            }
            else if (rt == DSQL_SUCCESS)
            {
                Py_BEGIN_ALLOW_THREADS
                rt = dpi_more_results(self->handle);
                Py_END_ALLOW_THREADS

                if (rt == DSQL_NO_DATA)
                    break;

                cols = 0;
            }
            else 
            {
                if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                    "Cursor_InternalExecute()") < 0)
                {
                    for (i = 0; i < self->outparam_num; i++)
                    {
                        Py_DECREF(self->param_value[i]);
                    }
                    PyMem_Free(self->param_value);
                    PyMem_Free(length);
                    return -1;
                }
            }
        }
    }
    else
    {
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status, 
            "Cursor_InternalExecute()") < 0)
        {
            PyMem_Free(length);
            return -1;
        }
    }

    if (Cursor_SetExecId(self) < 0)
    {
        PyMem_Free(length);
        return -1;
    }

    // determine if statement is a query
    if (Cursor_GetStatementType(self) < 0)
    {
        PyMem_Free(length);
        return -1;
    }

    ret     = Cursor_SetRowCount(self);

    //���ڰ󶨲�������unbind param
    if (self->paramCount > 0)
    {
        Py_BEGIN_ALLOW_THREADS
            status = dpi_unbind_params(self->handle);
        Py_END_ALLOW_THREADS
    }
    PyMem_Free(length);
    return ret;
}

static
sdint2
Cursor_GetColDescFromDm_low(
    dm_Cursor*      self,
    dhdesc          hdesc_col
)
{    
    DPIRETURN   rt = DSQL_SUCCESS;
    udint2      icol;
    sdint4      val_len;

    self->bindColDesc = PyMem_Malloc(self->colCount * sizeof(DmColDesc));
    if (self->bindColDesc == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }
    memset(self->bindColDesc, 0, self->colCount * sizeof(DmColDesc));    

    for (icol = 0; icol < self->colCount; icol ++)
    {
        rt  = dpi_desc_column(self->handle, icol + 1, 
                              self->bindColDesc[icol].name, sizeof(self->bindColDesc[icol].name), &self->bindColDesc[icol].nameLen,
                              &self->bindColDesc[icol].sql_type, &self->bindColDesc[icol].prec, 
                              &self->bindColDesc[icol].scale, &self->bindColDesc[icol].nullable);
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_desc_column") < 0)
        {
            return -1;		
        }

        rt  = dpi_get_desc_field(hdesc_col, icol + 1, DSQL_DESC_DISPLAY_SIZE, (dpointer)&self->bindColDesc[icol].display_size,
            0, &val_len);
        if (Environment_CheckForError(self->environment, hdesc_col, DSQL_HANDLE_DESC, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field[DSQL_DESC_DISPLAY_SIZE]") < 0)
        {
            return -1;		
        }        
    }

    return 0;
}

static
sdint2
Cursor_GetColDescFromDm(
    dm_Cursor*     self
)
{
    DPIRETURN   rt = DSQL_SUCCESS;
    sdint4      val_len;

    /* ������������� */    
    Py_BEGIN_ALLOW_THREADS
        rt  = dpi_get_stmt_attr(self->handle, DSQL_ATTR_IMP_ROW_DESC, (dpointer)&self->hdesc_col, 0, &val_len);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
            "Cursor_GetColDescFromDm():dpi_get_stmt_attr") < 0)
            return -1;	    

    return Cursor_GetColDescFromDm_low(self, self->hdesc_col);        
}

static 
sdint2
Cursor_SetColVariables(
    dm_Cursor*     self
)
{
    udint2          icol;
    dm_Var*         dm_var;
    udint2          varchar_flag = 0;
    sdbyte          attr[10] = {0};
    DPIRETURN       rt = DSQL_SUCCESS;

    if ((int)self->arraySize < 0 || self->arraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ErrorException, "Invalid cursor arraysize\n");
        return -1;
    }

    Py_CLEAR(self->col_variables);

    self->col_variables = PyList_New(self->colCount);
    if (self->col_variables == NULL)
    {
        if (!PyErr_Occurred())
            PyErr_NoMemory();

        return -1;
    }
//bug653857 ����nls_numeric_characters��dmPython�ӿ���Ҫ����֧��
#ifdef DSQL_ATTR_NLS_NUMERIC_CHARACTERS
    rt = dpi_get_con_attr(self->connection->hcon, DSQL_ATTR_NLS_NUMERIC_CHARACTERS, attr, 10, NULL);
    if (!DSQL_SUCCEEDED(rt) || strcmp(attr, ".,") == 0)
    {
        varchar_flag = 0;
    }
    else
    {
        varchar_flag = 1;
    }
#endif

    for (icol = 0; icol < self->colCount; icol ++)
    {
        dm_var = dmVar_Define(self, self->hdesc_col, icol + 1, (udint4)self->arraySize, varchar_flag);
        if (dm_var == NULL)
            return -1;

        PyList_SET_ITEM(self->col_variables, icol, (PyObject*)dm_var);
    }

    self->org_bindArraySize = self->bindArraySize;

    return 0;
}

static 
sdint2
Cursor_PerformDefine(
    dm_Cursor*      self,
    sdint2*         isQuery
)
{
	DPIRETURN status = DSQL_SUCCESS;	
	sdint2	i = 0;
    PyObject*       desc;

    if (isQuery)
    {
        *isQuery = 0;
    }

	// determine number of items in select-list
	Py_BEGIN_ALLOW_THREADS
	status = dpi_number_columns(self->handle, &self->colCount);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_PerformDefine()") < 0)
	{
		return -1;		
	}

    /* ������Ϊ0�� ��ֱ�ӷ��� */
    if (self->colCount == 0)
    {        
        return 0;
    }

    if (isQuery)
    {
        *isQuery = 1;
    }

    /** ��ȡ����������Ϣ **/
    if (Cursor_GetColDescFromDm(self) < 0)
        return -1;    

    /** �����а� **/
    if (Cursor_SetColVariables(self) < 0)
        return -1;

    /** ��ȡ������Ϣ **/
    desc    = Cursor_GetDescription(self, NULL);
    if (desc == NULL)
        return -1;
    //Py_CLEAR(desc);
    Py_DECREF(desc);

	return 0;
}

static
sdint2
Cursor_GetParamDescFromDm_low(
    dm_Cursor*     self
)
{    
    DPIRETURN   rt = DSQL_SUCCESS;
    udint2      iparam;    

    self->bindParamDesc = PyMem_Malloc(self->paramCount * sizeof(DmParamDesc));
    if (self->bindParamDesc == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }
    memset(self->bindParamDesc, 0, self->paramCount * sizeof(DmParamDesc));

    self->outparam_num =0;
    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        rt  = dpi_desc_param(self->handle, iparam + 1, 
                             &self->bindParamDesc[iparam].sql_type, &self->bindParamDesc[iparam].prec, 
                             &self->bindParamDesc[iparam].scale, &self->bindParamDesc[iparam].nullable);
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_desc_param") < 0)
        {
            return -1;		
        }     

        rt  = dpi_get_desc_field(self->hdesc_param, iparam + 1, DSQL_DESC_PARAMETER_TYPE, 
                                 (dpointer)&self->bindParamDesc[iparam].param_type, 0, NULL);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field") < 0)
        {
            return -1;		
        }

        if(self->bindParamDesc[iparam].param_type == DSQL_PARAM_OUTPUT && self->bindParamDesc[iparam].sql_type != DSQL_RSET)
            self->outparam_num += 1;

        /* ��ȡ��������DSQL_DESC_NAME */
        rt  = dpi_get_desc_field(self->hdesc_param, iparam + 1, DSQL_DESC_NAME, 
            (dpointer)self->bindParamDesc[iparam].name, 128, &self->bindParamDesc[iparam].namelen);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field") < 0)
        {
            return -1;		
        }
    }

    return 0;
}

static
sdint2
Cursor_GetParamDescFromDm(
    dm_Cursor*     self
)
{
    DPIRETURN   rt = DSQL_SUCCESS;
    sdint4      val_len;

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_number_params(self->handle, &self->paramCount);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_InternalPrepare(): dpi_number_params") < 0) 
            return -1;  

    if (self->paramCount <= 0)
    {
        return 0;
    }

    /** �������������� **/    
    Py_BEGIN_ALLOW_THREADS
        rt = dpi_get_stmt_attr(self->handle, DSQL_ATTR_IMP_PARAM_DESC, &self->hdesc_param, 0, &val_len);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
            "Cursor_GetParamDescFromDm():dpi_get_stmt_attr") < 0)
            return -1;

    return Cursor_GetParamDescFromDm_low(self);
}

static
sdint2
Cursor_SetParamRowSize_Oper(
	dm_Cursor*      cursor,
	udint4			paramrowSize
)
{
	DPIRETURN		rt = DSQL_SUCCESS;

	Py_BEGIN_ALLOW_THREADS
		rt = dpi_set_stmt_attr(cursor->handle, DSQL_ATTR_PARAMSET_SIZE, (dpointer)paramrowSize, 0);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(cursor->environment, cursor->handle, DSQL_HANDLE_STMT, rt,
		"Cursor_SetParamRowSize_Oper():dpi_set_stmt_attr") < 0)
		return -1;

	return 0;
}

static
sdint2
Cursor_setParamVariablesHelper(
    dm_Cursor*      self,
    PyObject*       iValue,
    unsigned        numElements, 
    unsigned        irow,   /* �󶨲������кţ�0-based */
    unsigned        ipos,   /* �󶨲����ı�ţ�1-based */
    dm_Var*         org_var,
    dm_Var**        new_var
)
{
    dm_Var*         dm_var = NULL;    
    int             is_udt = 0;    

    *new_var    = NULL;
    is_udt      = dmVar_Check(iValue); 

    /** �Ѿ����ڣ���ΪNone�����滻Ϊ�µı������ͣ������������Ͳ�һ�����򱨴� **/
    if (org_var != NULL)
    {
        /** �Զ������ͣ��滻;���򣬽���ֵ���뵽org_var�� **/
        if (is_udt == 1)
        {
            /** ����ͬһ�����������滻������ **/
            if ((PyObject*)org_var != iValue)
            {
                Py_INCREF(iValue);
                *new_var    = (dm_Var*)iValue;                
            }            
        }
        else if (numElements > ((dm_Var*)org_var)->allocatedElements)
        {
            *new_var    = dmVar_NewByVarType(self, org_var->type, numElements, org_var->size);
            if (!*new_var)
                return -1;

            if (dmVar_SetValue(*new_var, irow, iValue) < 0)
                return -1;
        }
        else if (dmVar_SetValue(org_var, irow, iValue) < 0)
        {
            // executemany() should simply fail after the first element
            if (irow > 0)
                return -1;

            // anything other than index error or type error should fail
            if (!PyErr_ExceptionMatches(PyExc_IndexError) &&
                !PyErr_ExceptionMatches(PyExc_TypeError))
                return -1;

            // clear the exception and try to create a new variable
            PyErr_Clear();
            org_var = NULL;
        }        
    }    

    /** ��ԭ���ޱ����������¶��� **/
    if (org_var == NULL)
    {
        /** ��Ϊ�Զ������ͣ���ֱ��ת��;���򣬸���Python���ݴ���udt����**/
        if (is_udt)
        {
            Py_INCREF(iValue);

            dm_var             = (dm_Var*)iValue;
            dm_var->boundPos   = 0;
        }
        else
        {
            dm_var             = dmVar_NewByValue(self, iValue, numElements, ipos);
            if (dm_var == NULL)
                return -1;

            if (dmVar_SetValue(dm_var, irow, iValue) < 0)
                return -1;
        }

        (*new_var)  = dm_var;
    }

    return 0;
}

/** ���ݰ󶨲������ƴ�dict���ҵ�Ŀ�����ֵ **/
static
PyObject*
Cursor_getParamValue_FromDict(
    dm_Cursor*      self,
    PyObject*       dict,
    PyObject*       dickKeys,
    int             iparam
)
{
    PyObject*       iValue = Py_None;
    PyObject*       keyObj;
    Py_ssize_t      key_num;
    Py_ssize_t      key_i;
    char*           strvalue;

    iValue      = PyDict_GetItemString(dict, self->bindParamDesc[iparam].name);
    if (iValue != NULL)
        return iValue;

    iValue      = Py_None;
    key_num     = PyList_GET_SIZE(dickKeys);
    for (key_i = 0; key_i < key_num; key_i ++)
    {
        keyObj      = PyList_GetItem(dickKeys, key_i); 
        if (keyObj == NULL)
            return NULL;

        strvalue    = py_String_asString(keyObj);
#ifdef WIN32
        if (stricmp(strvalue, self->bindParamDesc[iparam].name) == 0)
#else        
        if (strcasecmp(strvalue, self->bindParamDesc[iparam].name) == 0)
#endif        
        {
            iValue  = PyDict_GetItemString(dict, strvalue);
            if (iValue == NULL)
            {
                PyErr_SetString(PyExc_ValueError, 
                    "Error occurs in dict to be bound");
                return NULL;
            }

            break;
        }
    }

    return iValue;
}   

/** ���ò�����������֧�ְ�λ�ð� **/
static
sdint2
Cursor_setParamVariables_oneRow(
    dm_Cursor*      self,
    PyObject*       parameters,
    Py_ssize_t      irow,
    Py_ssize_t      n_row
)
{
    sdint2          ret = -1;
    int             boundByPos = 0;
    int             iparam;    
    Py_ssize_t      param_num = 0;
    PyObject*       iValue;    
    PyObject*       dictKeys = NULL;
    dm_Var*         new_var = NULL;
    dm_VarType*     new_varType = NULL;     //��������
    dm_VarType*     tmp_varType = NULL;     //��ʱ����
    udint4          size;
    int             is_udt;
    sdint2          param_type;
    DmParamDesc*    bindParamDesc;          // �󶨲�����Ϣ
    int             dec_flag = 0;           // �Ƿ���Ҫ����iValue���ü����ı�־
    
    /** �������������У��򱨴� **/
    if (parameters != NULL && parameters != Py_None)
    {
        if (PySequence_Check(parameters))
        {
            param_num   = PySequence_Size(parameters);        
            boundByPos  = 1;
        }
        else if (PyDict_Check(parameters))
        {
            param_num   = PyDict_Size(parameters);
            boundByPos  = 0;
            dictKeys    = PyDict_Keys(parameters);
        }
        else
        {
            PyErr_SetString(g_ProgrammingErrorException, 
                "only bound by Position or Name supported.");

            return -1;
        }            
    }    
    
    /* �˴�param_variables����������в�������һ����ȣ����б��м���ֵ */    
    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {       
        if(dec_flag)
        {
            Py_DECREF(iValue);
            dec_flag = 0;
        }
        iValue          = Py_None;

        bindParamDesc   = &self->bindParamDesc[iparam];
        param_type      = bindParamDesc->param_type;

        if (param_num > 0)
        {
            if (!boundByPos)
            {           
                iValue  = Cursor_getParamValue_FromDict(self, parameters, dictKeys, iparam);
                if (iValue == NULL)
                    goto fun_end;
            }
            else if (iparam < param_num)
            {
                iValue  = PySequence_GetItem(parameters, iparam);
                if (!iValue)
                    goto fun_end;
                dec_flag = 1;
                //Py_DECREF(iValue);
            }
        }

        /* ��ΪPy_None������һ�� */
        if (iValue == Py_None)
        {
            continue;
        }

        /* �����Ƿ�Ϊ�û��Զ��� */
        is_udt  = dmVar_Check(iValue); 

        /* prepareʱ�Ѿ�׼���ÿյ�param_variables�������ŵ���δ��ֵ�Ķ��� */
        new_var = (dm_Var*)PyList_GET_ITEM(self->param_variables, iparam);

        /* ��ǰ�����е�һ�γ��ַ�None�İ󶨲���ֵ�������±���������뵽List�� */
        if (new_var == NULL)
        {
            new_varType = dmVar_TypeByValue(iValue, &size);
            if (new_varType == NULL)
                goto fun_end;

            /* ��Ϊ�������������������������ͣ��Ҳ�����������ΪLongString����LongBinary�������ת�� */
            if (param_type == DSQL_PARAM_INPUT_OUTPUT ||
                param_type == DSQL_PARAM_OUTPUT ||
                param_type == DSQL_PARAM_INPUT)
            {
                tmp_varType = dmVar_TypeBySQLType(bindParamDesc->sql_type, 1);
                if (tmp_varType == NULL)
                {
                    goto fun_end;
                }

                //sql_type�Ƿ������Ƽ����ͣ������varchar������������bytes�����������binary
                if (bindParamDesc->sql_type == DSQL_VARCHAR && new_varType == &vt_Binary)
                {
                    bindParamDesc->sql_type = DSQL_BINARY;

                    if (bindParamDesc->prec > size)
                    {
                        bindParamDesc->prec = size;
                    }
                }

                // bug631212 ������ݿ��ڲ�ʹ��numeric����ʹ��dmPython����int��������ʱ��������Ȼʹ��float���ͷ����һ��ʹ��int��֮��󶨲������ᶪʧС������
                if (bindParamDesc->sql_type == DSQL_DEC && new_varType != &vt_Boolean && param_type == DSQL_PARAM_INPUT && (n_row > 1))
                {
                    new_varType = &vt_Float;
                }

                //sql_type�Ƿ������Ƽ����ͣ������varchar������������datetime�����������timestamp
                if (bindParamDesc->sql_type == DSQL_VARCHAR && new_varType == &vt_Timestamp)
                {
                    bindParamDesc->sql_type = DSQL_TIMESTAMP;

                    bindParamDesc->prec     = 26;
                    bindParamDesc->scale    = 6;
                }

                // bug627535 ���������ΪCLOB����ʱ����ת���ᵼ���������������Ϊreturnging�������ʱ����text���Ͳ���������ڴ˴��������������SQL����ΪCLOB����ʱ���ٽ���ת��
                if(!((param_type == DSQL_PARAM_OUTPUT)&&(bindParamDesc->sql_type == DSQL_CLOB)))
                {
                    //python2.7�г���������������ȡ������long str���ͣ���ʱ�� ��������sql���� ӳ��� �������� ȥ��
                    if (new_varType == &vt_String || new_varType == &vt_Binary || new_varType == &vt_LongString)
                    {
                        if (new_varType == tmp_varType ||
                            tmp_varType == &vt_LongString || tmp_varType == &vt_LongBinary)
                        {
                            new_varType = tmp_varType;
                            size = -1;
                        }
                    }
                }          
            }

            //����󶨲������û��������ͣ�������Ϊ�����������ֱ�ӽ��ò����󶨣�����Ҫ���·���һ��new_var
            if ((param_type == DSQL_PARAM_INPUT_OUTPUT || param_type == DSQL_PARAM_OUTPUT) &&
                is_udt == 1)
            {
                new_var = iValue;
                Py_INCREF(iValue);
            }
            else
            {
                new_var     = dmVar_NewByVarType(self, new_varType, n_row, size);
                if (new_var == NULL)
                {
                    goto fun_end;
                }

                /** ��Ϊ�������ͣ����������ɾ������ **/
                if (new_var->type->pythonType == &g_ObjectVarType &&
                    ObjectVar_GetParamDescAndObjHandles((dm_ObjectVar*)new_var, self->hdesc_param, iparam + 1) < 0)
                {
                    goto fun_end;
                }

                /* �������ɵı��������и�ֵ */
                if (dmVar_SetValue(new_var, irow, iValue) < 0)
                {
                    goto fun_end;
                }
            }

            /* ���Ѿ���ֵ�ı�������Ĳ����б��У�����ʹ�� */
            PyList_SetItem(self->param_variables, iparam, new_var);

            continue;
        }
        
        /* һ���������󶨣�ǰ������Ѿ����ڣ��������У�ֱ�����ñ���ֵ */
        if (dmVar_SetValue(new_var, irow, iValue) < 0)
        {
            goto fun_end;
        }                                   
    }    
    
    ret     = 0;

fun_end:
    Py_XDECREF(dictKeys);

    if(dec_flag)
    {
        Py_DECREF(iValue);
        dec_flag = 0;
    }

    return ret;
}

static
sdint2
Cursor_setParamVariables(
    dm_Cursor*      self,
    PyObject*       parameters,
    int             is_many,
    Py_ssize_t*     prow_size
)
{
    int                 boundByPos;
    PyObject*           tmp_param = NULL;    
    Py_ssize_t          irow;
    int                 iparam;    
    dm_Var*             new_var = NULL;
    dm_VarType*         new_varType;    

    if (is_many)
    {
        if (parameters == NULL || parameters == Py_None || PyDict_Check(parameters))
        {
            PyErr_SetString(g_InterfaceErrorException, "expecting a sequence of parameters");

            return -1;
        }

        /** �������������У��򱨴� **/
        boundByPos      = PySequence_Check(parameters);
        if (boundByPos == 0)
        {
            PyErr_SetString(g_ProgrammingErrorException, 
                "only bound by Position supported.");
            
            return -1;
        }

        // ensure that input sizes are reset
        // this is done before binding is attempted so that if binding fails and
        // a new statement is prepared, the bind variables will be reset and
        // spurious errors will not occur
        self->setInputSizes         = 0;
    }

    /** ����󶨲������������������㹻�İ󶨲����ռ� **/
    if (is_many == 0)
        *prow_size          = 1;
    else
        *prow_size          = PySequence_Size(parameters);

    Py_CLEAR(self->param_variables);

    /* ��������������list�Ĵ�С��list�е�ÿ��item��ʾÿ���������󶨵�ֵ�����а�ʱÿ��item�ж��ֵ */
    self->param_variables   = PyList_New(self->paramCount);
    if (self->param_variables == NULL)
    {
        return -1; 
    }

    /* ����������ֵ����δָ������ΪNULL */
    for (irow = 0; irow < *prow_size; irow ++)
    {
        /* �ǲ���ֱ��ȡ */
        if (irow == 0 && is_many == 0)
        {
            tmp_param   = parameters;
        }
        else /* ����ȡ���е�һ�� */
        {
            tmp_param   = PySequence_GetItem(parameters, irow);
            Py_DECREF(tmp_param);
        }        

        if (Cursor_setParamVariables_oneRow(self, tmp_param, irow, *prow_size) < 0)
        {
            return -1;
        }
    }

    /* ���Ͻ��������ڰ󶨲���ֵ���������ĳһ��δ�󶨹����߾�ΪNone����˴�����SQL_TYPE����󶨶��� */
    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        new_var         = PyList_GET_ITEM(self->param_variables, iparam);
        if (new_var != NULL)
            continue;

        new_varType     = dmVar_TypeBySQLType(self->bindParamDesc[iparam].sql_type, 1);
        if (new_varType == NULL)
        {
            return -1;
        }

        new_var         = dmVar_NewByVarType(self, new_varType, *prow_size, new_varType->size);
        if (new_var == NULL)
        {
            return -1;
        }
        
        /* ������������ͣ���Ϊ�������ͣ����������ɾ������ **/
        if ((self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT_OUTPUT ||
            self->bindParamDesc[iparam].param_type == DSQL_PARAM_OUTPUT) &&
            new_var->type->pythonType == &g_ObjectVarType &&
            ObjectVar_GetParamDescAndObjHandles((dm_ObjectVar*)new_var, self->hdesc_param, iparam + 1) < 0)
        {
            return -1;        
        }

        PyList_SetItem(self->param_variables, iparam, new_var);
    }

    return 0;
}

static
sdint2
Cursor_BindParamVariable(
   dm_Cursor*       self,
   Py_ssize_t       rowsize
)
{
    DPIRETURN		rt = DSQL_SUCCESS;
    udint2          iparam;
    dm_Var*         dm_var;
    ulength         rsize;

    rsize           = (ulength)rowsize;

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_set_stmt_attr(self->handle, DSQL_ATTR_PARAMSET_SIZE, (dpointer)rsize, 0);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Desc_SetParamRowSize_Oper():dpi_set_stmt_attr") < 0)
            return -1;

    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        dm_var = (dm_Var*)PyList_GET_ITEM(self->param_variables, iparam);
        if (dm_var == NULL)
        {
            PyErr_SetString(g_ProgrammingErrorException,
                            "Not all parameters bound.");
            return -1;
        }

        if (dmVar_Bind(dm_var, self, iparam + 1) < 0)
            return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_PerformBind()
//   Perform the binds on the cursor.
//-----------------------------------------------------------------------------
static
sdint2
Cursor_PerformBind(
   dm_Cursor*       self,                   // cursor to perform binds on
   PyObject*        parameters,	               // parameters to bind
   sdint2           isMany,						// �Ƿ�ִ�ж��в���
   Py_ssize_t*      rowsize
)
{
    *rowsize        = 0;

    /** ��������setinputsize���󶨲������������setinputsize�е��и���һ�� **/
    if (self->setInputSizes)
    {
        if (PyList_Check(self->param_variables))
        {
            if (PyList_GET_SIZE(self->param_variables) != self->paramCount)
            {
                self->setInputSizes = 0;

                Py_XDECREF(self->param_variables);
                self->param_variables = NULL;

                return 0;
            }
        }
    }

    /** ����������Ϊ0����ֱ�ӷ��� **/
    if (self->paramCount == 0)
        return 0;        

    /** ���ݸ�����������Ϣ������������� **/
    if (Cursor_setParamVariables(self, parameters, isMany, rowsize) < 0)
        return -1;

    /** �󶨲��� **/
    return Cursor_BindParamVariable(self, *rowsize);
}

// �������Ϊ��̬��̬���ܵ���ϵͳ����ͳһ����������
sdint4
Cursor_ParseArgs(
	PyObject		*args,
	PyObject		**specArg,		// SQL���ȵ�һ���������
	PyObject		**seqArg		// �������в���
)
{
	Py_ssize_t  argCount = PyTuple_GET_SIZE(args);  
	sdint4      iparam;
	PyObject*   itemParam = NULL;
    PyObject*   itemParam_fst = NULL;

    if (specArg != NULL)
        *specArg = NULL;

    if (seqArg != NULL)
        *seqArg = NULL;

    if (argCount == 0)
        return 0;
	
	*specArg = PyTuple_GetItem(args, 0);
	if (!*specArg)
		return -1;	

    if (argCount == 1)
        return 0;

	// ����һ��������tuple�ҷ�list�ҷ�dict������Ϊ�Ƕ�̬����
	itemParam_fst   = PyTuple_GetItem(args, 1);
	if (itemParam_fst == NULL)
		return -1;
    
    itemParam   = itemParam_fst;

	// ��̬����
	if (!PyTuple_Check(itemParam) && !PyList_Check(itemParam) && !PyDict_Check(itemParam))
	{
		*seqArg = PyList_New(argCount - 1);
		if (!*seqArg)
			return -1;
		        
        Py_INCREF(itemParam);
		PyList_SetItem(*seqArg, 0, itemParam);		

		for (iparam = 2; iparam < argCount; iparam ++)
		{
			itemParam = PyTuple_GetItem(args, iparam);
			if (itemParam == NULL)
				return -1;

            Py_INCREF(itemParam);
			PyList_SetItem(*seqArg, iparam - 1, itemParam);			
		}
	}	
	else if(argCount == 2)
	{
		Py_INCREF(itemParam);
		*seqArg = itemParam;
	}
    else
    {
        PyErr_SetString(PyExc_ValueError, 
            "expecting a sequence or dict parameters");
        return -1;
    }
	
	return 0;
}

/* ����executedirect��������Ӧdpi_exec_direct */
static 
PyObject*
Cursor_ExecuteDirect(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    PyObject*       statement = NULL;
    PyObject*       ret_obj = NULL;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ExecuteDirect\n"));
    
    if (!PyArg_ParseTuple(args, "O", &statement))
        goto fun_end;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "ENTER Cursor_ExecuteDirect,before Cursor_Execute_inner\n"));

    ret_obj         = Cursor_Execute_inner(self, statement, NULL, 0, 1, 0);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "EXIT Cursor_ExecuteDirect, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

int
Cursor_outparam_exist(
    dm_Cursor*     self
)
{
    udint2      i;

    if (self->paramCount == 0 ||
        self->bindParamDesc == NULL)
        return 0;

    for (i = 0; i < self->paramCount; i ++)
    {
        if (self->bindParamDesc[i].param_type == DSQL_PARAM_INPUT_OUTPUT ||
            self->bindParamDesc[i].param_type == DSQL_PARAM_OUTPUT)
            return 1;
    }

    return 0;
}

void
Cursor_BoundParamAndCols_Clear(
    dm_Cursor*     self
)
{
    Py_ssize_t      size;
    Py_ssize_t      i;
    PyObject*       item;

    if (self->param_variables != NULL)
    {
        size    = PyList_GET_SIZE(self->param_variables);
        
        for (i = 0; i < size; i ++)
        {
            item    = PyList_GET_ITEM(self->param_variables, i);
            if (item != NULL)
            {
                dmVar_Finalize((dm_Var*)item);
            }
        }
    }

    if (self->col_variables != NULL)
    {
        size    = PyList_GET_SIZE(self->col_variables);

        for (i = 0; i < size; i ++)
        {
            item    = PyList_GET_ITEM(self->col_variables, i);
            if (item != NULL)
            {
                dmVar_Finalize((dm_Var*)item);
            }
        }
    }
}

PyObject*
Cursor_Execute_inner(
    dm_Cursor*      self, 
    PyObject*       statement,
    PyObject*       executeArgs,
    int             is_many,
    int             exec_direct,
    int             from_call
)
{
    sdint2          isQuery = 0;
    PyObject*       paramsRet = NULL;
    Py_ssize_t      rowsize;

    /** statementΪNULL������ **/
    if (statement == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "expecting a None or string statement arguement");
        
        return NULL;
    }

    if (executeArgs && 
        !PySequence_Check(executeArgs) && !PyDict_Check(executeArgs))
    {
        PyErr_SetString(PyExc_TypeError, "expecting a sequence or dict args");
        
        return NULL;
    }

    // make sure the cursor is open
    if (Cursor_IsOpen(self) < 0)
        return NULL;

    self->execute_num   += 1;

    // prepare the statement, if applicable
    if (exec_direct == 1)
    {
        if (Cursor_InternalExecDirect(self, statement) < 0)
            return NULL;
    }
    else
    {
        if (Cursor_InternalPrepare(self, statement) < 0)
        {
            goto fun_end;
        }

        // perform binds
        if (Cursor_PerformBind(self, executeArgs, is_many, &rowsize) < 0)
        {
            goto fun_end;
        }

        // execute the statement
        if (Cursor_InternalExecute(self, rowsize) < 0)
        {
            goto fun_end;
        }
    }

    // perform defines, if necessary    
    if ((self->statementType == DSQL_DIAG_FUNC_CODE_SELECT ||
        self->statementType == DSQL_DIAG_FUNC_CODE_CALL) && 
        Cursor_PerformDefine(self, &isQuery) < 0)
    {
        goto fun_end;
    }

    // reset the values of setoutputsize()
    self->outputSize = -1;
    self->outputSizeColumn = -1; 

    //reset the values of setinputsize()
    if (self->setInputSizes)
    {
        self->setInputSizes = 0;

        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    /** ����CALL�������ߴ��������������ֱ�ӷ��ز����б� **/
    if (from_call == 1 || Cursor_outparam_exist(self))
    {
        paramsRet = Cursor_MakeupProcParams(self);
        if (paramsRet == NULL)
        {
            goto fun_end;
        }

        /* paramsRet����PyList_NEW���������ü���Ĭ��Ϊ1�����ﲻ��Ҫ�ټ�1��ֱ�ӷ��� */
        //Py_INCREF(paramsRet);
        return paramsRet;        
    }        

    // for queries, return the cursor for convenience
    if (isQuery) 
    {
        Py_INCREF(self);
        return (PyObject*) self;        
    }

    // for all other statements, simply return None
    Py_INCREF(Py_None);
    return Py_None;    

fun_end:
    /** ִ��ʧ�ܣ��ͷű��� **/
    Cursor_BoundParamAndCols_Clear(self);

    return NULL;
}

static 
PyObject*
Cursor_Execute(
    dm_Cursor*      self, 
    PyObject*       args, 
    PyObject*       keywordArgs
)
{
	PyObject*       statement = NULL;
    PyObject*       executeArgs = NULL; /** Ϊ�ڲ����룬�������ͷ� **/
    PyObject*       retObject = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Execute\n"));

	if (Cursor_ParseArgs(args, &statement, &executeArgs) < 0)
		goto fun_end;

    if (executeArgs == NULL && keywordArgs != NULL)
    {       
        executeArgs = keywordArgs;
        Py_INCREF(executeArgs);
    }
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, executeArgs, "ENTER Cursor_Execute,before Cursor_Execute_inner\n"));

    retObject       = Cursor_Execute_inner(self, statement, executeArgs, 0, 0, 0);
    Py_CLEAR(executeArgs);

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_Execute, %s\n", retObject == NULL ? "FAILED" : "SUCCESS"));

    return retObject;
}

static
PyObject*
Cursor_nextset_inner(
    dm_Cursor*     self
)
{
    DPIRETURN       rt = DSQL_SUCCESS;

    rt      = dpi_more_results(self->handle);
    if (!DSQL_SUCCEEDED(rt) && rt != DSQL_NO_DATA)
    {
        Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,"Cursor_nextset_inner()");        

        return NULL;
    }

    if (rt == DSQL_NO_DATA)
    {
        Py_RETURN_NONE;
    }

    Py_RETURN_TRUE;
}

static
PyObject*
Cursor_nextset_Inner_ex(
    dm_Cursor*     self
)
{    
    PyObject*       ret;    

    /** ����ϴ�ִ�н�� **/
    Cursor_ExecRs_Clear(self);

    /** ��������� **/
    Cursor_free_coldesc(self);

    /** �ж��Ƿ񻹴��ڽ���������ޣ�����ʧ�ܣ���ֱ�ӷ��� **/
    ret     = Cursor_nextset_inner(self);
    if (!ret || ret == Py_None)
        return ret;
    
    /** ���ڽ���� **/
    if (Cursor_PerformDefine(self, NULL) < 0)
        return NULL;

    if (Cursor_SetRowCount(self) < 0)
        return NULL;

    Py_RETURN_TRUE;
}

static
PyObject*
Cursor_nextset(
    dm_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_nextset\n"));

    retObj          = Cursor_nextset_Inner_ex(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_nextset, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

/************************************************************************
purpose:
    Cursor_ContextManagerEnter()
    Called when the cursor is used as a context manager and simply returns it
    to the caller.
************************************************************************/
static
PyObject*       /*����py����*/
Cursor_ContextManagerEnter(
    dm_Cursor*  cursor,     /*IN:cursor*/
    PyObject*   args        /*IN:args*/
)
{
    Py_INCREF(cursor);
    return (PyObject*) cursor;
}

/************************************************************************
purpose:
    Cursor_ContextManagerExit()
    Called when the cursor is used as a context manager and simply closes the
    cursor.
************************************************************************/
static
PyObject*       /*����py����*/
Cursor_ContextManagerExit(
    dm_Cursor*  cursor,     /*IN:cursor*/
    PyObject*   args        /*IN:args*/
)
{
    PyObject *excType, *excValue, *excTraceback, *result;

    if (!PyArg_ParseTuple(args, "OOO", &excType, &excValue, &excTraceback))
        return NULL;
    result = Cursor_Close(cursor);
    if (!result)
        return NULL;
    Py_DECREF(result);
    Py_INCREF(Py_False);
    return Py_False;
}

static 
PyObject*
Cursor_ExecuteMany(
    dm_Cursor*      self, 
    PyObject*       args
)
{
	PyObject*       statement;
    PyObject*       argsList;
    PyObject*       rowParams;
    PyObject*       retObj = NULL;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ExecuteMany\n"));

	if (!PyArg_ParseTuple(args, "OO", &statement, &argsList))
		return NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, argsList, "ENTER Cursor_ExecuteMany, after parse args\n"));

	if (PyIter_Check(argsList))
	{	
        Py_INCREF(Py_None);
        retObj      = Py_None;

		while(1)
		{
			rowParams = PyIter_Next(argsList);
			if (rowParams == NULL)
				break;

            Py_XDECREF(retObj);
            retObj  = Cursor_Execute_inner(self, statement, rowParams, 0, 0, 0);

            DMPYTHON_TRACE_INFO(dpy_trace(statement, rowParams, "ENTER Cursor_ExecuteMany, Cursor_Execute_inner Per Row, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

            if (retObj == NULL)
            {
                Py_DECREF(rowParams);
                return NULL;
            }

			Py_DECREF(rowParams);
		}
        
        return retObj;
	}
	
    retObj  = Cursor_Execute_inner(self, statement, argsList, 1, 0, 0);

    DMPYTHON_TRACE_INFO(dpy_trace(statement, argsList, "ENTER Cursor_ExecuteMany, Cursor_Execute_inner Per Row, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_Prepare(
    dm_Cursor*      self, 
    PyObject*       args
)
{
	PyObject*   	statement = NULL;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Prepare\n"));

	// statement text and optional tag is expected	
	if (!PyArg_ParseTuple(args, "O", &statement))
		goto fun_end;

	// make sure the cursor is open
	if (Cursor_IsOpen(self) < 0)
		goto fun_end;

    self->execute_num   += 1;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "ENTER Cursor_Prepare,before Cursor_InternalPrepare\n"));

	// prepare the statement
	if (Cursor_InternalPrepare(self, statement) < 0)
		goto fun_end;

	Py_INCREF(Py_None);	
    ret_obj     = Py_None;

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "EXIT Cursor_Prepare, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
sdint2
Cursor_FixupBoundCursor(
    dm_Cursor*         self
)
{
	if (self->handle && self->statementType < 0)
	{
        Cursor_ExecRs_Clear(self);

		if (Cursor_GetStatementType(self) < 0)
			return -1;

		if (Cursor_PerformDefine(self, NULL) < 0)
			return -1;

		if (Cursor_SetRowCount(self) < 0)
			return -1;
	}

	return 0;
}

static 
sdint2
Cursor_VerifyFetch(
    dm_Cursor*     self
)
{
	if (Cursor_IsOpen(self) < 0)
		return -1;

	if (Cursor_FixupBoundCursor(self) < 0)
		return -1;

	if (self->colCount <= 0)
	{
		PyErr_SetString(g_InterfaceErrorException, "not a query");
		return -1;
	}
	
	return 0;
}

static 
sdint2
Cursor_InternalFetch(
    dm_Cursor*     self
)  
{
    DPIRETURN       status = DSQL_SUCCESS;
    ulength         rowCount;
    ulength         realToGet;
    ulength         rowleft;
    int             i;
    dm_Var*         var;
    ulength         array_size;

    if (!self->colCount || self->col_variables == NULL) 
    {
        PyErr_SetString(g_InterfaceErrorException, "query not executed");
        return -1;
    }

    /** fetch֮ǰ����������������arraysize **/
    if ((int)self->arraySize < 0 || self->arraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ErrorException, "Invalid cursor arraysize\n");
        return -1;
    }

    /** ����fetch֮ǰ������������arraysize **/
    array_size      = self->arraySize;
    if (self->arraySize > self->org_arraySize)
    {
        array_size  = self->org_arraySize;
    }

    rowleft         = (ulength)(self->totalRows - self->rowCount);   
    /** ȡ����֮���С�� **/
	realToGet       = array_size < rowleft ? array_size : rowleft;    

    for (i = 0; i < PyList_GET_SIZE(self->col_variables); i ++)
    {
        var = (dm_Var*) PyList_GET_ITEM(self->col_variables, i);

        var->internalFetchNum++;
        if (var->type->preFetchProc) 
        {
            if ((*var->type->preFetchProc)(var, self->hdesc_col, i + 1) < 0)
                return -1;
        }
    }

    Py_BEGIN_ALLOW_THREADS	
		status = dpi_set_stmt_attr(self->handle, DSQL_ATTR_ROW_ARRAY_SIZE, (dpointer)realToGet, sizeof(realToGet));
	Py_END_ALLOW_THREADS

	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_InternalFetch(): dpi_set_stmt_attr") < 0)
		return -1;

	Py_BEGIN_ALLOW_THREADS
		status = dpi_fetch(self->handle, &rowCount);
    Py_END_ALLOW_THREADS
    if (status != DSQL_NO_DATA) {
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                "Cursor_InternalFetch(): fetch") < 0)
            return -1;
    }

	self->rowNum = 0;
	self->actualRows = rowCount - self->rowNum;

    return 0;
}

static 
sdint2
Cursor_MoreRows(
    dm_Cursor*     self
)
{
    /*��ʼ��Ϊ-1*/
	if (self->actualRows == (ulength)(-1) ||  
        self->rowNum >= self->actualRows)
	{
		if (self->rowCount >= self->totalRows)
			return 0;

		if (self->actualRows == (ulength)(-1) || self->rowNum == self->actualRows)
			if (Cursor_InternalFetch(self) < 0)
				return -1;		
	}
	
	return 1;
}

//-----------------------------------------------------------------------------
// Cursor_CreateRow()
//   Create an object for the row. The object created is a tuple unless a row
// factory function has been defined in which case it is the result of the
// row factory function called with the argument tuple that would otherwise be
// returned.
//-----------------------------------------------------------------------------
/*static 
PyObject*
Cursor_CreateRow(
	dm_Cursor*      self                   // cursor object
)
{
	PyObject*       item;
	int             numItems, pos;
	PyObject**      apValues;
    dm_Var*         dm_var;

	// create a new tuple
	numItems = self->colCount;
	apValues = PyMem_Malloc(sizeof(PyObject*) * numItems);
	if (!apValues)
		return PyErr_NoMemory();

	// acquire the value for each item
	for (pos = 0; pos < numItems; pos++) 
    {
        dm_var     = (dm_Var*)PyList_GET_ITEM(self->col_variables, pos);
        if (dm_var != NULL)
        {
            item    = dmVar_GetValue(dm_var, self->rowNum);		
        }
        
		if (dm_var == NULL || item == NULL)
		{
			FreeRowValues(pos, apValues);
			return NULL;
		}

		apValues[pos] = item;
	}

	// increment row counters
	self->rowCount++;
	self->rowNum ++;	

	return (PyObject*)Row_New(self->description, self->map_name_to_index, numItems, apValues);
}*/

static 
PyObject*
Cursor_CreateRow_AsTuple(
	dm_Cursor*      self                   // cursor object
)
{
    PyObject*       item;
    PyObject*       tuple;
    int             numItems, pos;
    dm_Var*         dm_var;

    // create a new tuple
    numItems    = self->colCount;
    tuple       = PyTuple_New(numItems);
    if (tuple == NULL)
        return NULL;

    // acquire the value for each item
    for (pos = 0; pos < numItems; pos++) 
    {
        dm_var     = (dm_Var*)PyList_GET_ITEM(self->col_variables, pos);
        if (dm_var != NULL)
        {
            item    = dmVar_GetValue(dm_var, self->rowNum);		
        }

        if (dm_var == NULL || item == NULL)
        {
            Py_XDECREF(tuple);
            return NULL;
        }

        PyTuple_SetItem(tuple, pos, item);
    }

    // increment row counters
    self->rowCount++;
    self->rowNum ++;	

    return tuple;
}

static 
PyObject*
Cursor_CreateRow_AsDict(
	dm_Cursor*      self                   // cursor object
)
{
    PyObject*       item = NULL;
    PyObject*       dict = NULL;
    PyObject*       key = NULL;
    int             numItems, pos;
    DmColDesc       *colinfo;
    dm_Var*         dm_var;

    // create a new tuple
    numItems    = self->colCount;

    dict        = PyDict_New();
    if (dict == NULL)
        return NULL;

    // acquire the value for each item
    for (pos = 0; pos < numItems; pos++) 
    {
        dm_var     = (dm_Var*)PyList_GET_ITEM(self->col_variables, pos);
        if (dm_var != NULL)
        {
            item    = dmVar_GetValue(dm_var, self->rowNum);		
        }

        if (dm_var == NULL || item == NULL)
        {
            Py_XDECREF(dict);
            return NULL;
        }

        colinfo     = &self->bindColDesc[pos];

        key         = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);

        PyDict_SetItem(dict, key, item);

        /* PyDict_SetItem��ʹindex,key���ڴ������1��ѭ����index,keyֻ��һ�Σ������������ڴ������1 */
        Py_DECREF(item);
        Py_XDECREF(key);
    }

    // increment row counters
    self->rowCount++;
    self->rowNum ++;	

    return dict;
}

PyObject *
Cursor_One_Fetch(
	dm_Cursor*     self
)
{
	sdint4	rt;
	
	rt = Cursor_MoreRows(self);
	if (rt < 0)
		return NULL;
	else if (rt > 0)
    {
        if (self->connection->cursor_class == DICT_CURSOR)
        {
            return Cursor_CreateRow_AsDict(self);
        }
        else
        {
            return Cursor_CreateRow_AsTuple(self); /*BUG553553����Ϊ����tuple*/
        }
    }

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject*
Cursor_Many_Fetch(
	dm_Cursor*      self,
	ulength			rowSize
)
{
	ulength		index;
	PyObject	*list, *tuple;

	list = PyList_New(rowSize);
	for (index = 0; index < rowSize; index ++){
		tuple = Cursor_One_Fetch(self);
		if (tuple == NULL){
			Py_DECREF(list);
			return NULL;
		}

		PyList_SET_ITEM(list, index, tuple);
	}

	//Py_INCREF(list);
	return list;
}


static 
PyObject*
Cursor_FetchOne(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchOne\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchOne,before Cursor_One_Fetch\n"));

	ret_obj         = Cursor_One_Fetch(self);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchOne, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
PyObject*
Cursor_FetchMany(
    dm_Cursor*      self, 
    PyObject*       args, 
    PyObject*       keywords
)
{
    static char*    keywordList[] = { "rows", NULL };
    ulength		    rowToGet;
    ulength         rowleft;
    Py_ssize_t      inputRow = self->arraySize;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchMany\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", keywordList, &inputRow))
        goto fun_end;

	if (inputRow < 0 || inputRow >= INT_MAX)
    {
		PyErr_SetString(g_InterfaceErrorException, "Invalid rows value");
		goto fun_end;
	}	

    /* ����rowsС��δ��ȡ����rowleft���򷵻�rows�����ݣ����򷵻�ʣ�������� */
    rowleft     = (ulength)(self->totalRows - self->rowCount);
	rowToGet    = (ulength)inputRow < rowleft ? (ulength)inputRow : rowleft;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchMany,before Cursor_Many_Fetch rowleft ["slengthprefix"], rowToGet ["slengthprefix"]\n", rowleft, rowToGet));
	
    ret_obj     = Cursor_Many_Fetch(self, rowToGet);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchMany, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static
PyObject*
Cursor_FetchAll(
    dm_Cursor*      self, 
    PyObject*       args
)
{
	ulength		    rowToGet;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchAll\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;

	rowToGet    = (ulength)(self->totalRows - self->rowCount);
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchAll,before Cursor_Many_Fetch rowToGet ["slengthprefix"]\n", rowToGet));

	ret_obj     = Cursor_Many_Fetch(self, rowToGet);

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchAll, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static
PyObject*
Cursor_GetIter(
    dm_Cursor*     self
)
{
	if (Cursor_VerifyFetch(self) < 0)
		return NULL;

    self->is_iter   = 1;

	Py_INCREF(self);
	return (PyObject*)self;
}

static 
PyObject*
Cursor_GetNext_Inner(
    dm_Cursor*     self
)
{
	PyObject		*retObj;

	if (Cursor_VerifyFetch(self) < 0)
		return NULL;

	retObj = Cursor_One_Fetch(self);

    if (retObj != Py_None)
    {
        return retObj;
    }

    //PyErr_SetString(PyExc_StopIteration, "No data");

    if (self->is_iter == 1)
    {
        self->is_iter   = 0;

        return NULL;
    }
    else
    {
        Py_RETURN_NONE;
    }
}

static 
PyObject*
Cursor_GetNext(
    dm_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_GetNext\n"));

    retObj      = Cursor_GetNext_Inner(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_GetNext\n"));

    return retObj;
}

static
udint4
Cursor_CalcStmtSize(
    dm_Cursor*      self,
    char*           procName,
    udint4          paramCount,
    udbyte          ret_value   /** 0���޷���ֵ��1���з���ֵ **/
)
{
    /************************************************************************/
    /* �����ʽ��
    /* begin
    /* ? = func(); ==>�洢����
    /*  proc();     ==>�洢����
    /* end;
    /************************************************************************/
    udint4          size = 20; /** = 5(begin)+2('"''"') + 1(' ') + 3('('')'';') + 1(' ') + 4(end;) **/
    
    if (ret_value != 0)
    {
        size    += 4; /** '?'' ''='' '**/
    }

    size        += (udint4)strlen(procName);

    if (paramCount > 0)
    {
        size    += paramCount;      /*?*/
        size    += (paramCount - 1);/*,*/ 
        size    += (paramCount - 1);/*' '*/
    }

    return size;
}

sdint4 Cursor_escape_quotes(char* dst, int dst_len, char* src, int src_len)
{
    char* to_start = dst;
    char* end = NULL;
    char* to_end = to_start + (dst_len ? dst_len - 1 : 2 * src_len);
    int		overflow = 0;

    for (end = src + src_len; src < end; src++)
    {
        if (*src == '\"')
        {
            if (dst + 2 > to_end)
            {
                overflow = 1;
                break;
            }
            *dst++ = '\"';
            *dst++ = '\"';
        }
        else
        {
            if (dst + 1 > to_end)
            {
                overflow = 1;
                break;
            }
            *dst++ = *src;
        }
    }
    *dst = 0;

    return overflow ? -1 : (int)(dst - to_start);
}

static
PyObject*
Cursor_MakeStmtSQL(
    dm_Cursor*      self,
	char*           procName,
	udint4          paramCount,
    udbyte          ret_value   /** 0���޷���ֵ��1���з���ֵ **/
)
{
    udint4          sql_len;
    sdbyte*         sql = NULL;	
	udint4	        iparam;
    PyObject*       sql_obj;
    char*           pos = NULL;

    sql_len = Cursor_CalcStmtSize(self, procName, paramCount, ret_value);
    sql     = PyMem_Malloc(sql_len + 1); /* Ԥ����β�� */
    if (sql == NULL)
    {
        return PyErr_NoMemory();
    }

    aq_sprintf(sql, sql_len + 1, "begin ");
    
    if (ret_value != 0)
    {
        strcat(sql, "? = ");
    }

    strcat(sql, "\"");
    pos = strstr(procName, ".");
    if(pos == NULL)
    {
        strcat(sql, procName);
        strcat(sql, "\"");
    }
    else
    {
        *pos = 0;
        strcat(sql, procName);
        strcat(sql, "\".\"");
        strcat(sql, pos + 1);
        strcat(sql, "\"");
        *pos = '.';
    }

    strcat(sql, "(");	
	for (iparam = 0; iparam < paramCount; iparam ++)
	{
		if (iparam != paramCount -1)
			strcat(sql, "?, ");
		else
			strcat(sql, "?");
	}
	strcat(sql, "); end;");

	sql_obj = dmString_FromAscii(sql);

    PyMem_Free(sql);

    return sql_obj;
}


PyObject*
Cursor_MakeupProcParams(
	dm_Cursor*     self
)
{
	sdint4		iparam;
    sdint4      paramCount = self->paramCount;	
	PyObject*   paramVal;
    PyObject*   newParamVal;
    PyObject*   paramsRet;
    sdint4      ioutparam  = 0;

	paramsRet = PyList_New(paramCount);
    if(self->output_stream != 1)
    { 
	    for (iparam = 0; iparam < paramCount; iparam ++)
	    {
            paramVal    = PyList_GET_ITEM(self->param_variables, iparam);
            if (paramVal == NULL)
            {
                Py_DECREF(paramsRet);
                return NULL;
            }       

            /** ����OBJECT���͵�����������������������ֱ�ӷ��ذ�ʱ�Ķ������� **/
            if (((dm_Var*)paramVal)->type->pythonType == &g_ObjectVarType &&
                self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT)
            {
                newParamVal = ObjectVar_GetBoundExObj((dm_ObjectVar*)paramVal, 0);
            }
            else
            {
                newParamVal = dmVar_GetValue((dm_Var*)paramVal, 0);
            }
            if (newParamVal == NULL)
            {
                Py_DECREF(paramsRet);
                return NULL;
            }

		    PyList_SetItem(paramsRet, iparam, newParamVal);
	    }
    }
    else
    {
        for (iparam = 0; iparam < paramCount; iparam++)
        {
            paramVal = PyList_GET_ITEM(self->param_variables, iparam);
            if (paramVal == NULL)
            {
                Py_DECREF(paramsRet);
                return NULL;
            }

            /** ����OBJECT���͵�����������������������ֱ�ӷ��ذ�ʱ�Ķ������� **/
            if (((dm_Var*)paramVal)->type->pythonType == &g_ObjectVarType &&
                self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT)
            {
                newParamVal = ObjectVar_GetBoundExObj((dm_ObjectVar*)paramVal, 0);
            }
            else if (self->bindParamDesc[iparam].param_type == DSQL_PARAM_OUTPUT)
            {
                if(self->param_value == NULL || self->param_value[ioutparam] == NULL)
                    newParamVal = Py_None;
                else
                    newParamVal = self->param_value[ioutparam];
                ioutparam++;
            }
            else
            {
                newParamVal = dmVar_GetValue((dm_Var*)paramVal, 0);
            }
            if (newParamVal == NULL)
            {
                Py_DECREF(paramsRet);
                return NULL;
            }

            PyList_SetItem(paramsRet, iparam, newParamVal);
        }
        if (self->param_value)
        {
            PyMem_Free(self->param_value);
            self->param_value = NULL;
        }
            
    }
		
	return paramsRet;
}

static
PyObject*
Cursor_CallExec_inner(
    dm_Cursor*      self, 
    PyObject*       args, 
    udint4          ret_value   /* �Ƿ���Ҫ����ֵ */
)
{
    PyObject*       nameObj = NULL;
    PyObject*       parameters = NULL;
    PyObject*       sql = NULL;    
    char*           procName = NULL;
    Py_ssize_t      paramCount = 0;
    dm_Buffer       buffer;  
    PyObject*       retObj = NULL;

    if (Cursor_ParseArgs(args, &nameObj, &parameters) < 0)
        return NULL;

    if (nameObj == NULL || nameObj == Py_None)
    {
        PyErr_SetString(g_InterfaceErrorException, "procedure name is illegal");
        return NULL;
    }

    // ��������	
    if (dmBuffer_FromObject(&buffer, nameObj, self->environment->encoding) < 0)
        return NULL;

    procName = PyMem_Malloc(buffer.size * 2 + 1);
    if (procName == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    Cursor_escape_quotes(procName, buffer.size * 2 + 1, buffer.ptr, buffer.size);
    dmBuffer_Clear(&buffer);    

    // ����󶨲�������
    if (parameters != NULL)
        paramCount = PySequence_Size(parameters);
    else
        paramCount = 0;	

    // ����SQL���
    sql = Cursor_MakeStmtSQL(self, procName, (udint4)paramCount, ret_value);
    PyMem_Free(procName);

    if (ret_value != 0)
    {
        /** ����Ҫ����ֵ������None��parameters�ĵ�һ��λ�� **/
        //Py_XINCREF(parameters);

        if (parameters == NULL || parameters == Py_None)
        {            
            parameters  = PyList_New(1);

            /* PyList_SetItem:��steals�� a reference to item��
            �˴�Py_None�ڴ�����������ط����ü���û���ۼӹ���
            �����Ҫ������1������parameters������ʱ���ϵͳ����PyNone������1��  */
            Py_INCREF(Py_None);
            PyList_SetItem(parameters, 0, Py_None);
        }
        else
        {
            PyList_Insert(parameters, 0, Py_None);            
        }        
    }

    /** ִ�� **/
    retObj  = Cursor_Execute_inner(self, sql, parameters, 0, 0, 1);
    Py_CLEAR(sql);
    Py_CLEAR(parameters);
    
    return retObj;
}

static 
PyObject*
Cursor_CallProc(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    PyObject*       retObj;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_CallProc\n"));

	retObj  = Cursor_CallExec_inner(self, args, 0);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_CallProc, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_CallFunc(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_CallFunc\n"));

	retObj      = Cursor_CallExec_inner(self, args, 1);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_CallFunc, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static
PyObject*
Cursor_GetDescription(
	dm_Cursor   *self,
    void*       args
)
{
	PyObject*           desc = NULL;
    PyObject*           coldesc = NULL;
    dm_VarType*         varType = NULL;
    PyObject*           typecode = NULL;
    PyObject*           colmap = NULL;
    PyObject*           index = NULL;
    PyObject*           colname_arr = NULL;
    PyObject*           colname = NULL;
	DmColDesc           *colinfo;
	sdint2		        icol;
    PyObject*           retObj = NULL;
    PyObject*           key = NULL;

    if (Cursor_IsOpen(self) < 0)
    {
        return NULL;
    }

    if (Cursor_FixupBoundCursor(self) < 0)
    {
        return NULL;
    }

    if (self->colCount <= 0)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (self->description != Py_None)
    {
        Py_INCREF(self->description);
        return self->description;
    }

    colname_arr = PyList_New(self->colCount);
	desc        = PyList_New(self->colCount);
	colmap      = PyDict_New();
	for (icol = 0; icol < self->colCount; icol ++)
	{
		colinfo = &self->bindColDesc[icol];

		// ��׼��Ҫ��7��������Ϣ��name,type_code,display_size ,internal_size, precision, scale, null_ok
        varType = dmVar_TypeBySQLType(colinfo->sql_type, 0);
        if (varType == NULL)
        {
            goto done;
        }
        typecode    = (PyObject*)varType->pythonType;

        colname     = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);
        if (colname == NULL)
        {
            PyErr_SetString(g_OperationalErrorException, "NULL String");
            goto done;
        }          
        
        coldesc     = Py_BuildValue("(OOiiiii)",
                                colname,            
                                typecode,
                                colinfo->display_size,
                                colinfo->prec,
                                colinfo->prec,            
                                colinfo->scale,            
                                colinfo->nullable);

        /* Py_BuildValue��ʹcolname�ڴ�������1�Σ�colnameֻ��һ�Σ�������������1 */
        Py_XDECREF(colname);

        if (colinfo == NULL)
        {
            goto done;
        }

		// map_name_to_index
#if PY_MAJOR_VERSION < 3
		index = PyInt_FromLong(icol);
#else
        index = PyLong_FromLong(icol);
#endif
		if (!index)
        {
			goto done;
        }

        key             = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);

		PyDict_SetItem(colmap, 
                       key,
                       index);
        /* PyDict_SetItem��ʹindex,key���ڴ������1��ѭ����index,keyֻ��һ�Σ������������ڴ������1 */
		Py_DECREF(index);       // SetItemString increments
        Py_XDECREF(key);
        index           = NULL;

		PyList_SetItem(desc, icol, coldesc);
		coldesc         = NULL;            // reference stolen by SET_ITEM
        //Py_XDECREF(coldesc);

        PyList_SetItem(colname_arr, 
                       icol, 
                       dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding));
	}

	Py_XDECREF(self->description);
	self->description = desc;
	desc    = NULL;

    Py_XDECREF(self->map_name_to_index);
	self->map_name_to_index = colmap;
	colmap  = NULL;

    Py_XDECREF(self->column_names);
    self->column_names  = colname_arr;
    colname_arr = NULL;

done:

    Py_INCREF(self->description);
	retObj  = self->description;

	return retObj;
}

static 
PyObject*
Cursor_Parse(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Parse, NOT support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

static 
PyObject*
Cursor_SetInputSizes_inner(
    dm_Cursor*      self,
    PyObject*       args,
    PyObject*       keywords
)
{
    int             numArgs;
    int             numkeywordArgs=0;
    PyObject*       value;
    dm_Var*         var;
    Py_ssize_t      i;
    PyObject*       key;

    // make sure the cursor is open
    if (Cursor_IsOpen(self) < 0)
        return NULL;

    // eliminate existing bind variables
    Py_CLEAR(self->param_variables);

    // if number of argument is 0, then return None;else create a new one
    numArgs = PyTuple_Size(args);
    // �������keywords������keywords������
    if (keywords)
        numkeywordArgs = PyDict_Size(keywords);
    // args��keywords����ͬʱ����
    if (numArgs > 0 && numkeywordArgs>0)
        Py_RETURN_NONE;
    // ����������ڷ��ؿ�
    if (numArgs == 0 && numkeywordArgs == 0)
    {
        return NULL;
    }
    // ���keywords���ڴ����ֵ䣬���򴴽�����
    if (numkeywordArgs > 0)
        self->param_variables = PyDict_New();
    else
        self->param_variables   = PyList_New(numArgs);
    if (self->param_variables == NULL)
        return NULL;

    if ((sdint4)self->bindArraySize < 0 ||
        self->bindArraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ProgrammingErrorException, 
            "invalid value of bindarraysize");

        return NULL;
    }
    
    // set the flag of inputSize 1
    self->setInputSizes     = 1;

    // process each input
    if (numkeywordArgs > 0)
    {
        i = 0;
        // ���δ���keywords
        while (PyDict_Next(keywords, &i, &key, &value))
        {
            var = dmVar_NewByType(self, value, self->bindArraySize);
            if (!var)
                return NULL;
            //�����µ��ֵ��ֵ�Լ����ֵ�
            if (PyDict_SetItem(self->param_variables, key, (PyObject*)var) < 0)
            {
                Py_DECREF(var);
                return NULL;
            }
            Py_DECREF(var);
        }
    }
    else
    {
        for (i = 0; i < numArgs; i++)
        {
            value = PyTuple_GET_ITEM(args, i);
            if (value == Py_None)
            {
                Py_INCREF(Py_None);
                PyList_SET_ITEM(self->param_variables, i, Py_None);
            }
            else
            {
                var = dmVar_NewByType(self, value, self->bindArraySize);
                if (!var)
                    return NULL;
                PyList_SET_ITEM(self->param_variables, i, (PyObject*)var);
            }
        }
    }
    self->org_bindArraySize = self->bindArraySize;

    Py_INCREF(self->param_variables);
    return self->param_variables;
}

static 
PyObject*
Cursor_SetInputSizes(
    dm_Cursor*      self,
    PyObject*       args,
    PyObject*       keywords
)
{
    PyObject*       ret_obj;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_SetInputSizes\n"));

    ret_obj         = Cursor_SetInputSizes_inner(self, args, keywords);
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_SetInputSizes, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
PyObject*
Cursor_SetOutputSize_inner(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    self->outputSizeColumn = -1;
    if (!PyArg_ParseTuple(args, "i|i", &self->outputSize,
        &self->outputSizeColumn))
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static 
PyObject*
Cursor_SetOutputSize(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_SetOutputSize\n"));

    retObj      = Cursor_SetOutputSize_inner(self, args);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_SetOutputSize, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_Var(
    dm_Cursor*      self, 
    PyObject*       args, 
    PyObject*       keywords
)
{
    PyObject*           retObj = NULL;
    dm_VarType*         varType;

    static char *keywordList[] = { "typ", "size", "arraysize", "inconverter",
        "outconverter", "typename", "encoding_errors", "bypass_decode",
        "encodingErrors", NULL };

    Py_ssize_t encodingErrorsLength, encodingErrorsDeprecatedLength;
    const char *encodingErrors, *encodingErrorsDeprecated;
    PyObject *inConverter, *outConverter, *typeNameObj;
    int size, arraySize, bypassDecode;
    PyObject *type;
    dm_Var *var = NULL;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Var\n"));

    //��������
    size                = 0;
    bypassDecode        = 0;
    arraySize           = self->arraySize;
    encodingErrors      = NULL;
    encodingErrorsDeprecated = NULL;
    inConverter         = NULL;
    outConverter        = NULL;
    typeNameObj         = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "O|iiOOOz#pz#",
        keywordList, &type, &size, &arraySize, &inConverter, &outConverter,
        &typeNameObj, &encodingErrors, &encodingErrorsLength,
        &bypassDecode, &encodingErrorsDeprecated,
        &encodingErrorsDeprecatedLength))
        return NULL;

    varType = dmVar_TypeByPythonType(self, type);
    
    if (varType != NULL)
    {
        var = dmVar_NewByVarType(self, varType, 1, varType->size);
    }

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_Var, %s\n", var == NULL ? "FAILED" : "SUCCESS"));

    retObj = (PyObject*)(var);

    return retObj;
}

static 
PyObject*
Cursor_ArrayVar(
    dm_Cursor*      self, 
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ArrayVar Not Support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

static 
PyObject*
Cursor_BindNames(
    dm_Cursor*      self,
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_BindNames Not Support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

//-----------------------------------------------------------------------------
// declaration of methods for Python type "Cursor"
//-----------------------------------------------------------------------------
static PyMethodDef g_CursorMethods[] = {
    { "execute",        (PyCFunction) Cursor_Execute,           METH_VARARGS | METH_KEYWORDS},
    { "executedirect",  (PyCFunction) Cursor_ExecuteDirect,     METH_VARARGS},
    { "fetchall",       (PyCFunction) Cursor_FetchAll,          METH_NOARGS },
    { "fetchone",       (PyCFunction) Cursor_FetchOne,          METH_NOARGS },
    { "fetchmany",      (PyCFunction) Cursor_FetchMany,         METH_VARARGS | METH_KEYWORDS },
    { "prepare",        (PyCFunction) Cursor_Prepare,           METH_VARARGS },
    { "parse",          (PyCFunction) Cursor_Parse,             METH_VARARGS },
    { "setinputsizes",  (PyCFunction) Cursor_SetInputSizes,     METH_VARARGS | METH_KEYWORDS },
    { "executemany",    (PyCFunction) Cursor_ExecuteMany,       METH_VARARGS },
    { "callproc",       (PyCFunction) Cursor_CallProc,          METH_VARARGS},
    { "callfunc",       (PyCFunction) Cursor_CallFunc,          METH_VARARGS},
    { "setoutputsize",  (PyCFunction) Cursor_SetOutputSize,     METH_VARARGS },
    { "var",            (PyCFunction) Cursor_Var,               METH_VARARGS | METH_KEYWORDS },
    { "arrayvar",       (PyCFunction) Cursor_ArrayVar,          METH_VARARGS },
    { "bindnames",      (PyCFunction) Cursor_BindNames,         METH_NOARGS },
    { "close",          (PyCFunction) Cursor_Close,             METH_NOARGS },
    { "next",           (PyCFunction) Cursor_GetNext,           METH_NOARGS },
    { "nextset",        (PyCFunction) Cursor_nextset,           METH_NOARGS },
    { "__enter__",      (PyCFunction) Cursor_ContextManagerEnter, METH_NOARGS },
    { "__exit__",       (PyCFunction) Cursor_ContextManagerExit,METH_VARARGS },
    { NULL,             NULL }
};


//-----------------------------------------------------------------------------
// declaration of members for Python type "Cursor"
//-----------------------------------------------------------------------------
static PyMemberDef g_CursorMembers[] = {
    { "arraysize",      T_INT,          offsetof(dm_Cursor, arraySize),        0 },
    { "bindarraysize",  T_INT,          offsetof(dm_Cursor, bindArraySize),    0 },
    { "rowcount",       T_INT,          offsetof(dm_Cursor, totalRows),        READONLY },  /** ����������� **/
    { "rownumber",      T_INT,          offsetof(dm_Cursor, rowCount),         READONLY },   /** �α����ڵ�ǰλ��0-based **/
    { "with_rows",      T_BOOL,         offsetof(dm_Cursor, with_rows),        READONLY },   /** �α����ڵ�ǰλ��0-based **/
    { "statement",      T_OBJECT,       offsetof(dm_Cursor, statement),        READONLY },
    { "connection",     T_OBJECT_EX,    offsetof(dm_Cursor, connection),       READONLY },       
    { "column_names",   T_OBJECT_EX,    offsetof(dm_Cursor, column_names),     READONLY },
    { "lastrowid",      T_OBJECT,       offsetof(dm_Cursor, lastrowid_obj),    READONLY },
    { "execid",         T_OBJECT,       offsetof(dm_Cursor, execid_obj),       READONLY },
    { "_isClosed",      T_INT,          offsetof(dm_Cursor, isClosed),         READ_RESTRICTED },
    { "_statement",     T_OBJECT,       offsetof(dm_Cursor, statement),        READ_RESTRICTED },
    { "output_stream",  T_INT,          offsetof(dm_Cursor, output_stream),    0 },
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of calculated members for Python type "Connection"
//-----------------------------------------------------------------------------
static PyGetSetDef g_CursorCalcMembers[] = {
    { "description",            (getter) Cursor_GetDescription, 0,  0,  0},
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of Python type "Cursor"
//-----------------------------------------------------------------------------
PyTypeObject g_CursorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "DmdbCursor",                     // tp_name
    sizeof(dm_Cursor),                  // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) Cursor_Free,           // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    (reprfunc) Cursor_Repr,             // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    (getiterfunc) Cursor_GetIter,       // tp_iter
    (iternextfunc) Cursor_GetNext,      // tp_iternext
    g_CursorMethods,                    // tp_methods
    g_CursorMembers,                    // tp_members
    g_CursorCalcMembers,                // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    0,                                  // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};