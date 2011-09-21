#pragma once

#include "stdafx.h"

class Denser;
class HttpListener;

typedef enum {
	Idle = 0,
	ReadingRequestHeaders = 1,
	ProcessingRequestHeaders = 2,
	ReadingRequestBody = 3,
	ProcessingRequestBody = 4,
	WaitingForResponseHeaders = 5,
	SendingResponseHeaders = 6,
	WaitingForResponseBody = 7,
	SendingResponseBody = 8,
	Failed = 9,
	Completed = 10,
	PendingAbort = 11,
	Aborted = 12
} HTTP_IO_STATE;

typedef struct {
	OVERLAPPED overlapped; // this member must be first in the struct
	void* buffer;
	ULONG bufferLength;
	HTTP_REQUEST_ID requestId;
	HTTP_IO_STATE state;
	ULONG requestSlot; // this is an index into HttpManager::requests array holding this instance and is used to correlate calls between C and JS
	HTTP_RESPONSE response;
	HTTP_DATA_CHUNK chunk;
	BOOL closeResponse;
	BOOL keepConnectionIntactAfterBodySend;
	DWORD error;
	Denser* denser;
} HTTP_IO_CONTEXT;

class HttpRequestHeadersEvent : Event
{
private:
	static PCWSTR httpVerbs[];
	HTTP_IO_CONTEXT* ctx;
	HTTP_REQUEST_V1* request;

public:
	HttpRequestHeadersEvent(HTTP_IO_CONTEXT* ctx);
	virtual ~HttpRequestHeadersEvent();

	virtual HRESULT Execute(v8::Handle<v8::Context> context);
};

class HttpRequestBodyChunkEvent : Event
{
private:
	DWORD size;
	void* chunk;
	HTTP_IO_CONTEXT* ctx;

public:
	HttpRequestBodyChunkEvent(HTTP_IO_CONTEXT* ctx, DWORD size);
	virtual ~HttpRequestBodyChunkEvent();

	virtual HRESULT Execute(v8::Handle<v8::Context> context);
};

class HttpStateTransitionEvent : Event
{
private:
	DWORD error;
	HTTP_IO_STATE state;
	HTTP_REQUEST_ID requestId;
	ULONG requestSlot;
	Denser* denser;

public:
	HttpStateTransitionEvent(HTTP_IO_CONTEXT* ctx, DWORD error);
	virtual ~HttpStateTransitionEvent();

	virtual HRESULT Execute(v8::Handle<v8::Context> context);
};

// TODO, tjanczuk, consider making INITIAL_HTTP_HEADER_BUFFER_SIZE and HTTP_BODY_BUFFER_SIZE configurable
#define INITIAL_HTTP_HEADER_BUFFER_SIZE sizeof(HTTP_REQUEST)
#define HTTP_BODY_BUFFER_SIZE 4 * 1024

class HttpListener
{
private:	
	HANDLE completionPort;
	HANDLE* serverThreads;
	DWORD serverThreadCount;
	unsigned int urlCount;	

	HRESULT StartListen();
	HRESULT StopListen();

	static unsigned int WINAPI Server(void* arg);	
	void InitializeAsyncRequestBodyRead(HTTP_IO_CONTEXT* ctx);	
	void InitializeAsyncSendResponseHeaders(HTTP_IO_CONTEXT* ctx);
	void InitializeAsyncSendResponseBody(HTTP_IO_CONTEXT* ctx);

public:
	static int httpRequestHeaderNumber;
	static PCWSTR httpRequestHeaders[];
	static int httpResponseHeaderNumber;
	static PCWSTR httpResponseHeaders[];

	HttpListener(DWORD serverThreadCount);
	virtual ~HttpListener();

	HRESULT Initialize();
	HRESULT AddRequestHandle(HANDLE requestHandle);
	HRESULT RequestHandleRemoved();	
	HRESULT SendResponseHeaders(HTTP_IO_CONTEXT* ctx, int statusCode, LPCWSTR reason, v8::Handle<v8::Object> headers, BOOL closeResponse);
	HRESULT SendResponseBodyChunk(HTTP_IO_CONTEXT* ctx, LPCWSTR chunk, BOOL closeResponse);
	HRESULT PumpRequest(HTTP_IO_CONTEXT* ctx);
	HRESULT AbortRequest(HTTP_IO_CONTEXT* ctx, DWORD hresult);
	HRESULT EnterShutdownMode();
	void InitializeAsyncReceive(HTTP_IO_CONTEXT* ctx, HTTP_REQUEST_ID requestId);	
	static void ReleaseHttpResponseV1(HTTP_RESPONSE* response);
	static void ReleaseHttpResponseChunk(HTTP_DATA_CHUNK* chunk);

	unsigned int WINAPI ServerImpl();

	static HRESULT AddRequestIdToVar(HTTP_REQUEST_ID requestId, ULONG requestSlot, v8::Handle<v8::Object> var);
};