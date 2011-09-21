// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// from http://msdn.microsoft.com/en-us/library/e5ewb1h3(v=vs.80).aspx
// WARINING - this does not catch SysAllocStringLen allocation leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#pragma comment(lib, "httpapi.lib")


#include <vector>

#include "targetver.h"
#include <intsafe.h>
#include <http.h>
#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <ActivScp.h>
#include <atlbase.h>
#include <atlcoll.h>
#include <atlstr.h>
#include <atlsimpcoll.h>
#include <assert.h>
#include "v8.h"
#include "Utils.h"
#include "resource.h"
#include "EventLoop.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "MeterModule.h"
#include "LogModule.h"
#include "Denser.h"
#include "DenserManager.h"

#define ErrorIf(expr) if (expr) goto Error;

#define ENTER_CS(cs) EnterCriticalSection(&cs); __try {

#define LEAVE_CS(cs) } __finally { LeaveCriticalSection(&cs); }