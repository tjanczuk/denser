#include "stdafx.h"

LogModule::LogModule(int maxEntries)
	: maxEntries(maxEntries), current(0), entries(NULL)
{
	this->entries = new	LOG_ENTRY[maxEntries];
	RtlZeroMemory(this->entries, maxEntries * sizeof(LOG_ENTRY));
	InitializeCriticalSection(&this->syncRoot);
}

LogModule::~LogModule()
{
	if (this->entries)
	{
		for (int i = 0; i < this->maxEntries; i++)
		{
			if (this->entries[i].entry)
			{
				delete [] this->entries[i].entry;
			}
		}
		delete [] this->entries;
	}
	DeleteCriticalSection(&this->syncRoot);
}

// implements circular buffer logic
HRESULT LogModule::Log(LPCWSTR entry)
{
	// TODO, tjanczuk, make console logging configurable
	_tprintf(entry);
	_tprintf(L"\n");

	ENTER_CS(this->syncRoot)

	if (this->entries[this->current].entry)
	{
		delete [] this->entries[this->current].entry;
	}
	size_t size = wcslen(entry) + 1;
	this->entries[this->current].entry = new WCHAR[size];
	memcpy(this->entries[this->current].entry, entry, size * sizeof(WCHAR));
	GetSystemTime(&this->entries[this->current].timestamp);
	this->current++;
	this->current %= this->maxEntries;

	LEAVE_CS(this->syncRoot)

	return S_OK;
}

HRESULT LogModule::GetLog(v8::Handle<v8::Object>& result)
{
	v8::HandleScope handleScope;

	v8::Handle<v8::Array> a = v8::Array::New();
	ErrorIf(a.IsEmpty());	

	ErrorIf(S_OK != this->GetLogCore(a));

	result = handleScope.Close(a);

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT LogModule::GetLogCore(v8::Handle<v8::Array> a)
{
	ENTER_CS(this->syncRoot)

	int i = (this->current + 1) % this->maxEntries;
	int index = 0;
	while (i != this->current)
	{
		if (this->entries[i].entry)
		{			
			v8::Handle<v8::Object> entry = v8::Object::New();
			ErrorIf(entry.IsEmpty());
			ErrorIf(!a->Set(index, entry));
			v8::Handle<v8::Object> timestamp = v8::Object::New();
			ErrorIf(timestamp.IsEmpty());
			ErrorIf(S_OK != ::SetVarProperty(entry, L"time", timestamp));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"y", this->entries[i].timestamp.wYear));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"m", this->entries[i].timestamp.wMonth));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"d", this->entries[i].timestamp.wDay));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"h", this->entries[i].timestamp.wHour));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"mi", this->entries[i].timestamp.wMinute));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"s", this->entries[i].timestamp.wSecond));
			ErrorIf(S_OK != ::SetIntProperty(timestamp, L"ms", this->entries[i].timestamp.wMilliseconds));
			ErrorIf(S_OK != ::SetStringProperty(entry, L"entry", this->entries[i].entry, -1));
			index++;
		}
		i++;
		i %= this->maxEntries;		
	}

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return E_FAIL;
}