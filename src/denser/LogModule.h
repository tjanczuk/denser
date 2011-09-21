#pragma once

#include "stdafx.h"

class LogModule
{
private:
	typedef struct {
		SYSTEMTIME timestamp;
		LPWSTR entry;
	} LOG_ENTRY;

	LOG_ENTRY* entries;
	int current;
	int maxEntries;
	CRITICAL_SECTION syncRoot;

	HRESULT GetLogCore(v8::Handle<v8::Array> a);

public:
	LogModule(int maxEntries);
	~LogModule();

	HRESULT Log(LPCWSTR entry);
	HRESULT GetLog(v8::Handle<v8::Object>& result);
};
