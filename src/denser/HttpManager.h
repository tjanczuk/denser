#pragma once

#include "stdafx.h"

class HttpManager
{
private:
	Denser* denser;	
	DWORD maxConcurrentRequests;
	HTTP_IO_CONTEXT* requests;	
	v8::Persistent<v8::Object> shutdown;
	unsigned int urlCount;

	HRESULT StartListen();
	HRESULT StopListen();

public:
	HttpManager(Denser* denser, DWORD maxConcurrentRequests);
	~HttpManager();

	v8::Persistent<v8::Object> callback;
	HANDLE requestHandle;

	HRESULT AddUrl(PCWSTR url);
	HRESULT RemoveUrl(PCWSTR url);	
	void SetHttpCallbacks(v8::Handle<v8::Object> callback, v8::Handle<v8::Object> shutdown);
	HRESULT SendResponseHeaders(ULONG requestSlot, int statusCode, LPCWSTR reason, v8::Handle<v8::Object> headers, BOOL closeResponse);
	HRESULT SendResponseBodyChunk(ULONG requestSlot, LPCWSTR chunk, BOOL closeResponse);
	HRESULT PumpRequest(ULONG requestSlot);
	HRESULT AbortRequest(ULONG requestSlot, DWORD hresult);
	HRESULT EnterShutdownMode();
	void ReleaseResources();
};