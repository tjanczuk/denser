#include "stdafx.h"

HttpManager::HttpManager(Denser* denser, DWORD maxConcurrentRequests)
	: maxConcurrentRequests(maxConcurrentRequests), denser(denser), requests(NULL), urlCount(0), requestHandle(NULL)
{
}

HttpManager::~HttpManager() 
{
	this->ReleaseResources();
}

void HttpManager::ReleaseResources()
{
	this->StopListen();

	if (!this->callback.IsEmpty())
	{
		this->callback.Dispose();
		this->callback = v8::Persistent<v8::Object>();
	}

	if (!this->shutdown.IsEmpty())
	{
		this->shutdown.Dispose();
		this->shutdown = v8::Persistent<v8::Object>();
	}

	if (this->requestHandle != NULL)
	{
		CloseHandle(this->requestHandle);
		this->requestHandle = NULL;
	}
}

HRESULT HttpManager::AddUrl(PCWSTR url)
{
	HRESULT hr;

	if (this->urlCount == 0)
	{
		ErrorIf(S_OK != (hr = HttpCreateHttpHandle(&this->requestHandle, 0)));
		ErrorIf(S_OK != (hr = this->denser->http->AddRequestHandle(this->requestHandle)));
		ErrorIf(S_OK != (hr = this->StartListen()));
	}
	ErrorIf(S_OK != (hr = HttpAddUrl(this->requestHandle, url, NULL)));	
	this->urlCount++;
	this->denser->EventSourceAdded();

	return S_OK;
Error:
	return hr; 
}

HRESULT HttpManager::RemoveUrl(PCWSTR url)
{
	HRESULT hr;

	ErrorIf(S_OK != (hr = HttpRemoveUrl(this->requestHandle, url)));
	this->denser->EventSourceRemoved();
	if (--this->urlCount == 0)
	{
		HANDLE requestHandleTmp = this->requestHandle;
		this->requestHandle = NULL;
		CloseHandle(this->requestHandle);
		ErrorIf(S_OK != (hr = this->denser->http->RequestHandleRemoved()));
	}

	return S_OK;
Error:
	return hr;
}

void HttpManager::SetHttpCallbacks(v8::Handle<v8::Object> callback, v8::Handle<v8::Object> shutdown)
{
	if (!this->callback.IsEmpty())
	{
		this->callback.Dispose();
	}
	this->callback = v8::Persistent<v8::Object>::New(callback);

	if (!this->shutdown.IsEmpty())
	{
		this->shutdown.Dispose();
	}
	this->shutdown = v8::Persistent<v8::Object>::New(shutdown);
}

HRESULT HttpManager::SendResponseHeaders(ULONG requestSlot, int statusCode, LPCWSTR reason, v8::Handle<v8::Object> headers, BOOL closeResponse)
{
	return this->denser->http->SendResponseHeaders(&this->requests[requestSlot], statusCode, reason, headers, closeResponse);
}

HRESULT HttpManager::SendResponseBodyChunk(ULONG requestSlot, LPCWSTR chunk, BOOL closeResponse)
{
	return this->denser->http->SendResponseBodyChunk(&this->requests[requestSlot], chunk, closeResponse);
}

HRESULT HttpManager::PumpRequest(ULONG requestSlot)
{
	return this->denser->http->PumpRequest(&this->requests[requestSlot]);
}

HRESULT HttpManager::AbortRequest(ULONG requestSlot, DWORD hresult)
{
	return this->denser->http->AbortRequest(&this->requests[requestSlot], hresult);
}

HRESULT HttpManager::StartListen()
{
	if (this->requests == NULL)
	{
		ErrorIf(NULL == (this->requests = new HTTP_IO_CONTEXT[this->maxConcurrentRequests]));
		RtlZeroMemory(this->requests, this->maxConcurrentRequests * sizeof(HTTP_IO_CONTEXT));
	}
	
	for (DWORD i = 0; i < this->maxConcurrentRequests; i++)
	{
		this->requests[i].denser = this->denser;
		this->requests[i].requestSlot = i;
		if (this->requests[i].buffer == NULL)
		{
			this->requests[i].bufferLength = INITIAL_HTTP_HEADER_BUFFER_SIZE;
			ErrorIf(NULL == (this->requests[i].buffer = malloc(this->requests[i].bufferLength)));
		}
		this->denser->http->InitializeAsyncReceive(&this->requests[i], NULL);
		ErrorIf(this->requests[i].state == Failed);
	}

	return S_OK;
Error:
	if (this->requests != NULL)
	{
		for (DWORD i = 0; i < this->maxConcurrentRequests; i++)
		{
			if (this->requests[i].buffer != NULL) 
			{
				free(this->requests[i].buffer);
			}			
		}
		delete [] this->requests;
		this->requests = NULL;
	}
	return E_FAIL;
}

HRESULT HttpManager::StopListen()
{
	if (this->requests != NULL)
	{
		for (DWORD i = 0; i < this->maxConcurrentRequests; i++)
		{
			if (this->requests[i].buffer != NULL) 
			{
				free(this->requests[i].buffer);
			}
			this->denser->http->ReleaseHttpResponseChunk(&this->requests[i].chunk);
			this->denser->http->ReleaseHttpResponseV1(&this->requests[i].response);
		}
		delete [] this->requests;
		this->requests = NULL;
	}
	return S_OK;
}

HRESULT HttpManager::EnterShutdownMode()
{
	v8::Isolate::Scope is(this->denser->isolate);
	v8::Context::Scope cs(this->denser->context);
	v8::HandleScope hs;
	Event* ev = (Event *) new SimpleEvent(this->denser, this->shutdown, 0, NULL);
	ev->ignorable = false;
	return this->denser->loop->PostEvent(this->denser, ev);
}