#ifndef _DMPYTHON_TRC_H
#define _DMPYTHON_TRC_H

#include <time.h>
#include <stdarg.h>
#include "DPItypes.h"
#include <Python.h>

#define DMPYTHON_TRACE_OFF     0
#define DMPYTHON_TRACE_ON      1

extern udint4   dmpython_trace_mod;

#ifdef WIN32
#define DMPYTHON_TRACE_FILE    ".\\dmPython_trace.log"
#else
#define DMPYTHON_TRACE_FILE    "./dmPython_trace.log"
#endif

#ifdef DM64
#ifdef WIN32
#define slengthprefix   "%I64d"
#else
#define slengthprefix   "%lld"
#endif
#else
#define slengthprefix   "%ld"
#endif

#define DMPYTHON_TRACE_INFO(act)\
if (dmpython_trace_mod != DMPYTHON_TRACE_OFF)\
{\
    act;\
}\

void
dpy_trace(    
    PyObject*       statement,
    PyObject*       args,
    sdbyte*         info,
    ...
);

unsigned int                               /*dst的数据长度*/
aq_sprintf_inner(
    char* dst,            /*IN:目标缓冲区*/
    int                    dst_len,        /*IN:目标长度*/
    char* fmt,            /*IN:格式串*/
    ...                                    /*IN:格式串中的参数*/
);

#ifdef _DEBUG
#define aq_sprintf(dst, dst_len, fmt, ...)          aq_sprintf_inner(dst, dst_len, fmt, ##__VA_ARGS__)
#else
#define aq_sprintf(dst, dst_len, fmt, ...)          sprintf((char*)dst, (const char*)fmt, ##__VA_ARGS__)
#endif 


#endif #_DMPATHON_TRC_H