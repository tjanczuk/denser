#include "stdafx.h"

#define ALLOC_BUFFER_OR_DIE(size) \
		if (ctx->buffer == NULL)\
		{\
			ctx->bufferLength = size;\
			if (NULL == (ctx->buffer = malloc(ctx->bufferLength)))\
			{\
				result = ERROR_NOT_ENOUGH_MEMORY;\
				shutdown = true;\
				continue;\
			}\
		} \


HttpRequestBodyChunkEvent::HttpRequestBodyChunkEvent(HTTP_IO_CONTEXT* ctx, DWORD size)
	: chunk(ctx->buffer), ctx(ctx), size(size)
{
	if (this->size == 0 && ctx->buffer != NULL)
	{
		free(ctx->buffer);		
		this->chunk = NULL;
	}
	ctx->buffer = NULL; // ownership of the buffer is taken
}

HttpRequestBodyChunkEvent::~HttpRequestBodyChunkEvent()
{
	if (this->chunk != NULL)
	{
		free(this->chunk);
	}
}

HRESULT HttpRequestBodyChunkEvent::Execute(v8::Handle<v8::Context> context)
{
	HRESULT hr;
	v8::TryCatch tryCatch;
	v8::HandleScope handleScope;
	v8::Context::Scope scope(context);

	// create JS representation of the HTTP request

	v8::Handle<v8::Object> bodyChunk = v8::Object::New();
	ErrorIf(bodyChunk.IsEmpty());

	// set requestId and body data
		
	ErrorIf(S_OK != HttpListener::AddRequestIdToVar(this->ctx->requestId, this->ctx->requestSlot, bodyChunk));
	ErrorIf(S_OK != ::SetIntProperty(bodyChunk, L"state", ProcessingRequestBody));
	if (this->size > 0)
	{
		v8::Handle<v8::String> value = v8::String::New((PCSTR)this->chunk, this->size);
		ErrorIf(value.IsEmpty());
		ErrorIf(!bodyChunk->Set(v8::String::New("data"), value));
	}

	// call into JS dispatcher function from http.js
	
	v8::Handle<v8::Value> argv[] = { bodyChunk };
	v8::Handle<v8::Value> v = this->ctx->denser->httpManager->callback->CallAsFunction(context->Global(), 1, argv);
	ErrorIf(S_OK != this->ctx->denser->log->Log(tryCatch));
	if (v.IsEmpty())
	{
		// Processing of HTTP body chunk failed. The error is non-fatal, issue a new HTTP request instead.
		hr = E_FAIL;
		ErrorIf(S_OK != this->ctx->denser->httpManager->AbortRequest(this->ctx->requestSlot, hr)); // if aborting of a request fails, consider this fatal
	}

	return S_OK;
Error:
	return E_FAIL;
}

HttpStateTransitionEvent::HttpStateTransitionEvent(HTTP_IO_CONTEXT* ctx, DWORD error)
	: requestId(ctx->requestId), state(ctx->state), requestSlot(ctx->requestSlot), error(error), denser(ctx->denser)
{
}

HttpStateTransitionEvent::~HttpStateTransitionEvent()
{
}

HRESULT HttpStateTransitionEvent::Execute(v8::Handle<v8::Context> context)
{
	HRESULT hr;
	v8::TryCatch tryCatch;
	v8::HandleScope handleScope;
	v8::Context::Scope scope(context);

	// create JS representation of an error

	v8::Handle<v8::Object> stateData = v8::Object::New();
	ErrorIf(stateData.IsEmpty());

	// set requestId and body data

	ErrorIf(S_OK != HttpListener::AddRequestIdToVar(this->requestId, this->requestSlot, stateData));
	ErrorIf(S_OK != ::SetIntProperty(stateData, L"state", this->state));
	if (this->error != S_OK)
	{
		ErrorIf(S_OK != ::SetIntProperty(stateData, L"error", this->error));
	}

	// call into JS dispatcher function from http.js
	
	v8::Handle<v8::Value> argv[] = { stateData };
	v8::Handle<v8::Value> v = this->denser->httpManager->callback->CallAsFunction(context->Global(), 1, argv);
	ErrorIf(S_OK != this->denser->log->Log(tryCatch));
	if (v.IsEmpty())
	{
		// Processing of HTTP body chunk failed. The error is non-fatal, issue a new HTTP request instead.
		hr = E_FAIL;
		ErrorIf(S_OK != this->denser->httpManager->AbortRequest(this->requestSlot, hr)); // if aborting of a request fails, consider this fatal
	}

	return S_OK;
Error:
	return E_FAIL;
}

PCWSTR HttpRequestHeadersEvent::httpVerbs[] = {
	NULL,
	NULL,
	NULL,
	L"OPTIONS",
	L"GET",
	L"HEAD",
	L"POST",
	L"PUT",
	L"DELETE",
	L"TRACE",
	L"CONNECT",
	L"TRACK",
	L"MOVE",
	L"COPY",
	L"POPFIND",
	L"PROPPATCH",
	L"MKCOL",
	L"LOCK",
	L"UNLOCK",
	L"SEARCH"
};

int HttpListener::httpRequestHeaderNumber = 41;

PCWSTR HttpListener::httpRequestHeaders[] = {
	L"Cache-Control",
	L"Connection",
	L"Date",
	L"Keep-Alive",
	L"Pragma",
	L"Trailer",
	L"Transfer-Encoding",
	L"Upgrade",
	L"Via",
	L"Warning",
	L"Allow",
	L"Content-Length",
	L"Content-Type",
	L"Content-Encoding",
	L"Content-Language",
	L"Content-Location",
	L"Content-MD5",
	L"Content-Range",
	L"Expires",
	L"Last-Modified",
	L"Accept",
	L"Accept-Charset",
	L"Accept-Encoding",
	L"Accept-Language",
	L"Authorization",
	L"Cookie",
	L"Expect",
	L"From",
	L"Host",
	L"If-Match",
	L"If-Modified-Since",
	L"If-None-Match",
	L"If-Range",
	L"If-Unmodified-Since",
	L"Max-Forwards",
	L"Proxy-Authorization",
	L"Referer",
	L"Range",
	L"Te",
	L"Translate",
	L"User-Agent"
};

int HttpListener::httpResponseHeaderNumber = 30;

PCWSTR HttpListener::httpResponseHeaders[] = {
	L"Cache-Control",
	L"Connection",
	L"Date",
	L"Keep-Alive",
	L"Pragma",
	L"Trailer",
	L"Transfer-Encoding",
	L"Upgrade",
	L"Via",
	L"Warning",
	L"Allow",
	L"Content-Length",
	L"Content-Type",
	L"Content-Encoding",
	L"Content-Language",
	L"Content-Location",
	L"Content-MD5",
	L"Content-Range",
	L"Expires",
	L"Last-Modified",
	L"Accept-Ranges",
	L"Age",
	L"Etag",
	L"Location",
	L"Proxy-Authenticate",
	L"Retry-After",
	L"Server",
	L"Set-Cookie",
	L"Vary",
	L"Www-Authenticate"
};

HttpRequestHeadersEvent::HttpRequestHeadersEvent(HTTP_IO_CONTEXT* ctx)
	: request((HTTP_REQUEST_V1*)this->ctx->buffer), ctx(ctx)
{
	ctx->buffer = NULL; // ownership of the buffer is taken
}

HttpRequestHeadersEvent::~HttpRequestHeadersEvent()
{
	free(this->request);
}

HRESULT HttpRequestHeadersEvent::Execute(v8::Handle<v8::Context> context)
{
	HRESULT hr;
	v8::TryCatch tryCatch;
	v8::HandleScope handleScope;
	v8::Context::Scope scope(context);

	// create JS representation of the HTTP request

	v8::Handle<v8::Object> httpRequest = v8::Object::New();
	ErrorIf(httpRequest.IsEmpty());
	v8::Handle<v8::Object> httpHeaders = v8::Object::New();
	ErrorIf(httpHeaders.IsEmpty());
	ErrorIf(!httpRequest->Set(v8::String::New("headers"), httpHeaders));

	// set requestId, URL, path, query, and verb
		
	ErrorIf(S_OK != HttpListener::AddRequestIdToVar(request->RequestId, this->ctx->requestSlot, httpRequest));
	ErrorIf(S_OK != ::SetIntProperty(httpRequest, L"state", ProcessingRequestHeaders));
	ErrorIf(S_OK != ::SetStringProperty(httpRequest, L"url", request->CookedUrl.pFullUrl, request->CookedUrl.FullUrlLength / sizeof(wchar_t)));
	ErrorIf(S_OK != ::SetStringProperty(httpRequest, L"path", request->CookedUrl.pAbsPath, request->CookedUrl.AbsPathLength / sizeof(wchar_t)));
	ErrorIf(S_OK != ::SetStringProperty(httpRequest, L"query", request->CookedUrl.pQueryString, request->CookedUrl.QueryStringLength / sizeof(wchar_t)));
	if (request->Verb == HttpVerbUnknown)
	{
		ErrorIf(!httpRequest->Set(v8::String::New("verb"), v8::String::New(request->pUnknownVerb, request->UnknownVerbLength)));
	}
	else
	{
		PCWSTR verb = HttpRequestHeadersEvent::httpVerbs[(int)request->Verb];
		ErrorIf(S_OK != ::SetStringProperty(httpRequest, L"verb", verb, -1));
	}

	// set known HTTP headers

	for (int i = 0; i < HttpListener::httpRequestHeaderNumber; i++)
	{
		HTTP_KNOWN_HEADER header = request->Headers.KnownHeaders[i];
		if (header.RawValueLength > 0)
		{
			ErrorIf(!httpHeaders->Set(
				v8::String::New((uint16_t*)HttpListener::httpRequestHeaders[i]), 
				v8::String::New(header.pRawValue, header.RawValueLength)));
		}
	}

	// set unknown HTTP headers

	for (int i = 0; i < request->Headers.UnknownHeaderCount; i++)
	{
		HTTP_UNKNOWN_HEADER header = request->Headers.pUnknownHeaders[i];
		ErrorIf(!httpHeaders->Set(
			v8::String::New(header.pName, header.NameLength), 
			v8::String::New(header.pRawValue, header.RawValueLength)));
	}

	// call into JS dispatcher function from http.js
	
	v8::Handle<v8::Value> argv[] = { httpRequest };
	v8::Handle<v8::Value> v = this->ctx->denser->httpManager->callback->CallAsFunction(context->Global(), 1, argv);
	ErrorIf(S_OK != this->ctx->denser->log->Log(tryCatch));
	if (v.IsEmpty())
	{
		// Processing of HTTP body chunk failed. The error is non-fatal, issue a new HTTP request instead.
		hr = E_FAIL;
		ErrorIf(S_OK != this->ctx->denser->httpManager->AbortRequest(this->ctx->requestSlot, hr)); // if aborting of a request fails, consider this fatal
	}

	return S_OK;
Error:
	return E_FAIL;
}

HttpListener::HttpListener(DWORD serverThreadCount)
	: urlCount(0), completionPort(NULL), serverThreadCount(serverThreadCount), serverThreads(NULL)	
{
	if (this->serverThreadCount == 0)
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		this->serverThreadCount = info.dwNumberOfProcessors * 2;
	}
}

HttpListener::~HttpListener()
{
	this->StopListen();
	if (this->completionPort != NULL)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}
	HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
}

HRESULT HttpListener::Initialize()
{
	HTTPAPI_VERSION version = HTTPAPI_VERSION_1;
	ErrorIf(S_OK != HttpInitialize(version, HTTP_INITIALIZE_SERVER, NULL));	
	ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, this->serverThreadCount)));		

	return S_OK;
Error:
	if (this->completionPort != NULL)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}
	return E_FAIL;
}

HRESULT HttpListener::AddRequestHandle(HANDLE requestHandle)
{
	ErrorIf(NULL == CreateIoCompletionPort(requestHandle, this->completionPort, NULL, 0));
	if (InterlockedIncrement(&this->urlCount) == 1)
	{
		ErrorIf(S_OK != this->StartListen());
	}

	return S_OK;
Error:
	return E_FAIL; 
}

HRESULT HttpListener::RequestHandleRemoved()
{
	if (InterlockedDecrement(&this->urlCount) == 0)
	{		
		return this->StopListen();
	}

	return S_OK;
}

void HttpListener::InitializeAsyncReceive(HTTP_IO_CONTEXT* ctx, HTTP_REQUEST_ID requestId)
{
	RtlZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

	ctx->state = ReadingRequestHeaders;

	if (ctx->denser->httpManager->requestHandle != NULL)
	{
		ULONG result = HttpReceiveHttpRequest(
			ctx->denser->httpManager->requestHandle, 
			requestId,
			0, 
			(PHTTP_REQUEST)ctx->buffer, 
			ctx->bufferLength, 
			NULL, 
			(OVERLAPPED*)ctx);

		switch (result) {
		default:
			ctx->state = Failed;
			break;
		case NO_ERROR:
		case ERROR_IO_PENDING:
			break;
		};
	}
}

void HttpListener::InitializeAsyncRequestBodyRead(HTTP_IO_CONTEXT* ctx)
{
	RtlZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

	ctx->state = ReadingRequestBody;

	if (ctx->denser->httpManager->requestHandle != NULL)
	{
		ULONG result = HttpReceiveRequestEntityBody(
			ctx->denser->httpManager->requestHandle,
			ctx->requestId,
			0,
			ctx->buffer,
			ctx->bufferLength,
			NULL,
			(OVERLAPPED*)ctx);

		switch (result) {
		default:
			ctx->state = Failed;
			break;
		case NO_ERROR:
		case ERROR_IO_PENDING:		
			break;
		case ERROR_HANDLE_EOF:
			ctx->state = WaitingForResponseHeaders;
		};
	}
}

HRESULT HttpListener::StartListen()
{
	ErrorIf(NULL != this->serverThreads);
	ErrorIf(NULL == (this->serverThreads = new HANDLE[this->serverThreadCount]));

	for (DWORD i = 0; i < this->serverThreadCount; i++)
	{
		this->serverThreads[i] = (HANDLE) _beginthreadex(NULL, 0, HttpListener::Server, this, 0, NULL);
	}

	return S_OK;
Error:
	if (this->serverThreads != NULL)
	{
		delete [] this->serverThreads;
		this->serverThreads = NULL;
	}
	return E_FAIL;
}

HRESULT HttpListener::StopListen()
{
	if (this->serverThreads != NULL)
	{
		for (DWORD i = 0; i < this->serverThreadCount; i++)
		{
			ErrorIf(0 == PostQueuedCompletionStatus(this->completionPort, 0, -1L, NULL));
		}
		WaitForMultipleObjects(this->serverThreadCount, this->serverThreads, TRUE, INFINITE);
		for (DWORD i = 0; i < this->serverThreadCount; i++)
		{
			CloseHandle(this->serverThreads[i]);
		}
		delete [] this->serverThreads;
		this->serverThreads = NULL;
	}

	return S_OK;
Error:
	return E_FAIL;
}

unsigned int WINAPI HttpListener::Server(void* arg)
{
	return ((HttpListener*)arg)->ServerImpl();
}

unsigned int WINAPI HttpListener::ServerImpl()
{
	unsigned int result = S_OK;
	BOOL shutdown = FALSE;
	DWORD bytesRead = 0;
	HTTP_IO_CONTEXT* ctx;
	ULONG_PTR pKey = NULL;
	DWORD error = 0;

	while (!shutdown) 
	{
		if (GetQueuedCompletionStatus(this->completionPort, &bytesRead, &pKey, (LPOVERLAPPED*)&ctx, INFINITE))
		{
			error = ERROR_SUCCESS;
		}
		else
		{
			error = GetLastError();
		}
		
		if (pKey == -1L)
		{
			// Shutdown requested from StopListen()
			shutdown = true;
			continue;
		}

		if (ctx == NULL)
		{
			// Unsuccessful completion, no data was dequeued, retry
			continue;
		}
		
		if (error == ERROR_SUCCESS)
		{
			// IO completion was successful			

			if (ctx->state == ReadingRequestHeaders)
			{
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_BYTES_RECEIVED, bytesRead);

				// HTTP headers of a new request were successfuly read, dispatch event to JS and wait for a request to read more data
				
				HTTP_REQUEST_V1* request = (HTTP_REQUEST_V1*)ctx->buffer;
				ctx->requestId = request->RequestId;
				ctx->state = ProcessingRequestHeaders;

				// HttpRequestHeadersEvent takes ownership of the buffer
				ctx->denser->loop->PostEvent((Event*)new HttpRequestHeadersEvent(ctx));
			}
			else if (ctx->state == ReadingRequestBody)
			{
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_BYTES_RECEIVED, bytesRead);

				// A piece of the HTTP request body was received, dispatch event to JS and wait for request to read more data

				ctx->state = ProcessingRequestBody;

				// HttpRequestBodyChunkEvent takes ownership of the buffer
				ctx->denser->loop->PostEvent((Event*)new HttpRequestBodyChunkEvent(ctx, bytesRead));
			}
			else if (ctx->state == ProcessingRequestHeaders || ctx->state == ProcessingRequestBody)
			{
				// received signal from JS code indicating code is ready to read [more of] the body
				
				ALLOC_BUFFER_OR_DIE(HTTP_BODY_BUFFER_SIZE)
				
				this->InitializeAsyncRequestBodyRead(ctx);
				if (ctx->state == WaitingForResponseHeaders)
				{
					// end of request has been reached, dispatch event to JS to that effect, then move on to waiting on next IO completion

					// HttpRequestBodyChunkEvent does not take ownership of the buffer
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, S_OK));
				}
				else if (ctx->state == Failed)
				{
					ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
					ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ABORTED);

					// Failure reading request body. Notify JS about arror and issue a new HTTP request.

					// ownership of the buffer remains with ctx
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, E_FAIL));

					this->InitializeAsyncReceive(ctx, NULL);
					if (ctx->state == Failed)
					{
						// Failure initiating a new async HTTP request. Consider this beyond rescue, shut down.
						result = E_FAIL;
						shutdown = true;
						continue;
					}			
				}
			}
			else if (ctx->state == WaitingForResponseHeaders) 
			{
				// received signal from JS code (via HttpListener::SendResponseHeaders) to send response headers

				this->InitializeAsyncSendResponseHeaders(ctx);
				if (ctx->state == Failed) 
				{
					// Failure sending response headers. Release resources, notify JS about arror and issue a new HTTP request.

					HttpListener::ReleaseHttpResponseV1(&ctx->response);
					ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
					ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ABORTED);					

					// ownership of the buffer remains with ctx
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, E_FAIL));

					ALLOC_BUFFER_OR_DIE(INITIAL_HTTP_HEADER_BUFFER_SIZE)

					this->InitializeAsyncReceive(ctx, NULL);
					if (ctx->state == Failed)
					{
						// Failure initiating a new async HTTP request. Consider this beyond rescue, shut down.
						result = E_FAIL;
						shutdown = true;
						continue;
					}			
				}
			}
			else if (ctx->state == WaitingForResponseBody) 
			{
				// received signal from JS code (via HttpListener::SendResponseBodyChunk) to send response body chunk

				this->InitializeAsyncSendResponseBody(ctx);
				if (ctx->state == Failed) 
				{
					// Failure sending response headers. Release resources, notify JS about arror and issue a new HTTP request.

					HttpListener::ReleaseHttpResponseChunk(&ctx->chunk);
					ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
					ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ABORTED);					

					// ownership of the buffer remains with ctx
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, E_FAIL));

					ALLOC_BUFFER_OR_DIE(INITIAL_HTTP_HEADER_BUFFER_SIZE)

					this->InitializeAsyncReceive(ctx, NULL);
					if (ctx->state == Failed)
					{
						// Failure initiating a new async HTTP request. Consider this beyond rescue, shut down.
						result = E_FAIL;
						shutdown = true;
						continue;
					}			
				}
			}
			else if (ctx->state == SendingResponseHeaders || ctx->state == SendingResponseBody)
			{
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_BYTES_SENT, bytesRead);

				// response headers or a chunk of response body have been sent

				if (ctx->state == SendingResponseHeaders)
				{
					HttpListener::ReleaseHttpResponseV1(&ctx->response);
				}
				else // ctx->state == SendingResponseBody
				{
					HttpListener::ReleaseHttpResponseChunk(&ctx->chunk);
				}

				if (ctx->closeResponse)
				{
					ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
					ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_COMPLETED);

					// there is no response body, we are done with the HTTP request/response; initiate a new HTTP request read using this slot

					ctx->state = Completed;
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, S_OK));

					ALLOC_BUFFER_OR_DIE(INITIAL_HTTP_HEADER_BUFFER_SIZE)

					this->InitializeAsyncReceive(ctx, NULL);
					if (ctx->state == Failed)
					{
						// Failure initiating a new async HTTP request. Consider this beyond rescue, shut down.
						result = E_FAIL;
						shutdown = true;
						continue;
					}			
				}
				else
				{
					// [more] response body is expected, send event to JS to indicate the host is ready to start sending it

					ctx->state = WaitingForResponseBody;
					
					// HttpStateTransitionEvent does not take ownership of the buffer
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, S_OK));
				}
			}
			else if (ctx->state == PendingAbort)
			{
				// An application handler for one of the prior events related to this request failed or the application explicitly 
				// asked for the HTTP request/response to be aborted. Issue a new request in its place. 

				// TODO, tjanczuk, should we send a 500 back to the client?

				ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ABORTED);

				// HttpStateTransitionEvent does not take ownership of the buffer
				ctx->state = Aborted;
				ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, ctx->error));

				ALLOC_BUFFER_OR_DIE(INITIAL_HTTP_HEADER_BUFFER_SIZE)

				this->InitializeAsyncReceive(ctx, NULL);
				if (ctx->state == Failed)
				{
					// Failure initiating a new async HTTP request. Consider this beyond rescue, shut down.
					result = E_FAIL;
					shutdown = true;
					continue;
				}			
			}
		}
		else if (error == ERROR_MORE_DATA)
		{
			// IO completion requires more buffer space to complete current operation. Reallocate buffer and retry operation.

			if (ctx->state == ReadingRequestHeaders)
			{
				HTTP_REQUEST_ID requestId = ((PHTTP_REQUEST)ctx->buffer)->RequestId;
				free(ctx->buffer);
				ctx->bufferLength = bytesRead;
				if (NULL == (ctx->buffer = malloc(ctx->bufferLength)))
				{
					result = ERROR_NOT_ENOUGH_MEMORY;
					shutdown = true;
					continue;
				}
				this->InitializeAsyncReceive(ctx, requestId);
			}
			else // ctx->state == ReadingRequestBody
			{
				// TODO tjanczuk is this code path ever hit? 
#if FALSE
				free(ctx->buffer);
				ctx->bufferLength = bytesRead;
				if (NULL == (ctx->buffer = malloc(ctx->bufferLength)))
				{
					result = ERROR_NOT_ENOUGH_MEMORY;
					shutdown = true;
					continue;
				}
				this->InitializeAsyncRequestBodyRead(ctx);
				if (ctx->state == Failed)
				{
					// Failure reading request body. Notify JS about an error before issuing a new HTTP request.

					// ownership of the buffer remains with ctx
					ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx->requestId, ctx->requestSlot, ctx->state, E_FAIL, this->callback));
				}
#else
				throw L"ERROR_MORE_DATA when reading HTTP request body - we should never be here";
#endif
			}

			if (ctx->state == Failed)
			{
				// A problem re-initializing a request after buffer re-allocation. Instead issue a new request.

				this->InitializeAsyncReceive(ctx, NULL);
				if (ctx->state == Failed)
				{
					result = E_FAIL;
					shutdown = true;
					continue;
				}
			}
		}
		else // unrecognized IO completion error condition
		{						
			if (ctx->state == ReadingRequestBody || ctx->state == SendingResponseHeaders || ctx->state == SendingResponseBody)
			{
				// The JavaScript code already has a represenation of the HTTP request. 
				// Signal error to the JavaScript handler, clean up and allocate state specific resources, and issue a new read request.

				ctx->denser->meter->DecrementMetric(HTTP_SERVER_REQUESTS_ACTIVE);
				ctx->denser->meter->IncrementMetric(HTTP_SERVER_REQUESTS_ABORTED);

				if (ctx->state == SendingResponseHeaders)
				{
					HttpListener::ReleaseHttpResponseV1(&ctx->response);
				}
				else if (ctx->state == SendingResponseBody)
				{
					HttpListener::ReleaseHttpResponseChunk(&ctx->chunk);
				}
				else // ctx->state == ReadingRequestBody
				{
					free(ctx->buffer);
					ctx->buffer = NULL;
				}

				ALLOC_BUFFER_OR_DIE(INITIAL_HTTP_HEADER_BUFFER_SIZE)

				// ownership of the buffer remains with ctx
				ctx->state = Failed;
				ctx->denser->loop->PostEvent((Event*)new HttpStateTransitionEvent(ctx, error));
			}

			// Attempt to issue a new read request read to replace the failed one. 
			this->InitializeAsyncReceive(ctx, NULL);
			if (ctx->state == Failed)
			{
				result = E_FAIL;
				shutdown = true;
				continue;
			}
		}
	}

	return result;
}

HRESULT HttpListener::AddRequestIdToVar(HTTP_REQUEST_ID requestId, ULONG requestSlot, v8::Handle<v8::Object> object)
{
	ErrorIf(S_OK != ::SetULONG64Property(object, L"requestId", requestId));	
	ErrorIf(S_OK != ::SetIntProperty(object, L"requestSlot", (int)requestSlot));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT HttpListener::SendResponseHeaders(HTTP_IO_CONTEXT* ctx, int statusCode, LPCWSTR reason, v8::Handle<v8::Object> headers, BOOL closeResponse)
{
	USHORT unknownHeaderSize = 0;

	// enforce HTTP processing state
	
	ErrorIf(WaitingForResponseHeaders != ctx->state);	
	
	// set status code and reason

	RtlZeroMemory(&ctx->response, sizeof(HTTP_RESPONSE));
	ctx->closeResponse = closeResponse;
	ctx->response.StatusCode = (USHORT)statusCode;
	ErrorIf(NULL == (ctx->response.pReason = 
		::AllocUnicodeToAscii(reason, -1, &ctx->response.ReasonLength)));

	// set response HTTP headers

	v8::Handle<v8::Array> headerNames = headers->GetOwnPropertyNames();
	ErrorIf(headerNames.IsEmpty());
	for (unsigned int h = 0; h < headerNames->Length(); h++)
	{
		v8::String::Value wHeaderName(headerNames->Get(h)->ToString());
		ErrorIf(!*wHeaderName);
		v8::String::Utf8Value aHeaderValue(headers->Get(headerNames->Get(h))->ToString());
		ErrorIf(!*aHeaderValue);

		// check if known header

		ctx->keepConnectionIntactAfterBodySend = false;
		BOOL knownHeader = false;
		for (int i = 0; i < HttpListener::httpResponseHeaderNumber; i++)
		{
			if (0 == _wcsicmp(HttpListener::httpResponseHeaders[i], (wchar_t*)*wHeaderName))
			{
				// Do not close TCP connection after sending the body if the Content-Length header is present 
				ctx->keepConnectionIntactAfterBodySend |= i == 11; 

				knownHeader = true;
				int len = strlen(*aHeaderValue);
				ErrorIf(NULL == (ctx->response.Headers.KnownHeaders[i].pRawValue = new char[len + 1]));
				memcpy((void*)ctx->response.Headers.KnownHeaders[i].pRawValue, *aHeaderValue, len + 1);
				ctx->response.Headers.KnownHeaders[i].RawValueLength = len;
				break;
			}
		}

		// check if unknown header

		if (!knownHeader)
		{
			if (unknownHeaderSize == 0)
			{
				unknownHeaderSize = 16;
				ErrorIf(NULL == (ctx->response.Headers.pUnknownHeaders = new HTTP_UNKNOWN_HEADER[unknownHeaderSize]));
			}
			else if (unknownHeaderSize == ctx->response.Headers.UnknownHeaderCount)
			{
				unknownHeaderSize *= 2;
				HTTP_UNKNOWN_HEADER *newUnknownHeaders;
				ErrorIf(NULL == (newUnknownHeaders = new HTTP_UNKNOWN_HEADER[unknownHeaderSize]));
				memcpy(newUnknownHeaders, ctx->response.Headers.pUnknownHeaders, ctx->response.Headers.UnknownHeaderCount * sizeof(HTTP_UNKNOWN_HEADER));
				delete [] ctx->response.Headers.pUnknownHeaders;
				ctx->response.Headers.pUnknownHeaders = newUnknownHeaders;
			}

			v8::String::Utf8Value aHeaderName(headerNames->Get(h)->ToString());
			ErrorIf(!*aHeaderName);
			int len = strlen(*aHeaderName);
			ErrorIf(NULL == (ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].pName = new char[len + 1]));
			memcpy((void*)ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].pName, *aHeaderName, len + 1);
			ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].NameLength = len;
			
			len = strlen(*aHeaderValue);
			ErrorIf(NULL == (ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].pRawValue = new char[len + 1]));
			memcpy((void*)ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].pRawValue, *aHeaderValue, len + 1);
			ctx->response.Headers.pUnknownHeaders[ctx->response.Headers.UnknownHeaderCount].RawValueLength = len;

			ctx->response.Headers.UnknownHeaderCount++;
		}
	}
	
	// "Pulse" the IO completion port to continue sending the response headers; control returns to ServerImpl running on the IO thread pool	

	ErrorIf(FALSE == PostQueuedCompletionStatus(
		this->completionPort,
		0,
		NULL,
		(OVERLAPPED*)ctx));

	return S_OK;
Error:
	HttpListener::ReleaseHttpResponseV1(&ctx->response);
	// TODO, tjanczu, if we are here we may be leaking HTTP request slots (e.g. when PostQueuedCompletionStatus fails)
	return E_FAIL;
}

void HttpListener::InitializeAsyncSendResponseHeaders(HTTP_IO_CONTEXT* ctx)
{
	RtlZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

	ctx->state = SendingResponseHeaders;
	
	if (ctx->denser->httpManager->requestHandle != NULL)
	{
		ULONG result = HttpSendHttpResponse(
			ctx->denser->httpManager->requestHandle,
			ctx->requestId,
			ctx->closeResponse ? 0 : HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
			(PHTTP_RESPONSE)&ctx->response,
			NULL,
			NULL,
			NULL,
			0,
			(OVERLAPPED*)ctx,
			NULL);

		switch (result) {
		default:
			ctx->state = Failed;
			break;
		case NO_ERROR:
		case ERROR_IO_PENDING:
			break;
		};
	}
}

void HttpListener::ReleaseHttpResponseV1(HTTP_RESPONSE* response)
{
	if (response->pReason)
	{
		delete[] response->pReason;
	}
	for (int i = 0; i < HttpListener::httpResponseHeaderNumber; i++)
	{
		if (response->Headers.KnownHeaders[i].pRawValue)
		{
			delete [] response->Headers.KnownHeaders[i].pRawValue;
		}
	}
	for (int i = 0; i < response->Headers.UnknownHeaderCount; i++)
	{
		if (response->Headers.pUnknownHeaders[i].pName)
		{
			delete [] response->Headers.pUnknownHeaders[i].pName;
		}
		if (response->Headers.pUnknownHeaders[i].pRawValue)
		{
			delete [] response->Headers.pUnknownHeaders[i].pRawValue;
		}
	}
	if (response->Headers.pUnknownHeaders)
	{
		delete [] response->Headers.pUnknownHeaders;
	}
	RtlZeroMemory(response, sizeof(HTTP_RESPONSE));
}

HRESULT HttpListener::SendResponseBodyChunk(HTTP_IO_CONTEXT* ctx, LPCWSTR chunk, BOOL closeResponse)
{
	USHORT size;

	// enforce HTTP processing state
	
	ErrorIf(WaitingForResponseBody != ctx->state);	

	// set status code and reason

	RtlZeroMemory(&ctx->chunk, sizeof(HTTP_DATA_CHUNK));
	ctx->chunk.DataChunkType = HttpDataChunkFromMemory;	
	if (chunk)
	{
		ErrorIf(NULL == (ctx->chunk.FromMemory.pBuffer = 
			(void*)::AllocUnicodeToAscii(chunk, -1, &size)));
		ctx->chunk.FromMemory.BufferLength = size;
		ctx->closeResponse = closeResponse;
	}
	else
	{
		ctx->closeResponse = true;
	}
	
	// "Pulse" the IO completion port to continue sending the response body chunk; control returns to ServerImpl running on an IO thread pool	

	ErrorIf(FALSE == PostQueuedCompletionStatus(
		this->completionPort,
		0,
		NULL,
		(OVERLAPPED*)ctx));

	return S_OK;
Error:
	HttpListener::ReleaseHttpResponseChunk(&ctx->chunk);
	// TODO, tjanczuk, if we are here we may be leaking HTTP request slots (e.g. when PostQueuedCompletionStatus fails)
	return E_FAIL;
}

HRESULT HttpListener::PumpRequest(HTTP_IO_CONTEXT* ctx)
{
	// enforce HTTP processing state
	
	ErrorIf(ProcessingRequestHeaders != ctx->state && ProcessingRequestBody != ctx->state);	

	// "Pulse" the IO completion port to continue reading the request; control returns to ServerImpl running on an IO thread pool	

	ErrorIf(FALSE == PostQueuedCompletionStatus(
		this->completionPort,
		0,
		NULL,
		(OVERLAPPED*)ctx));

	return S_OK;
Error:
	return E_FAIL;
}

void HttpListener::InitializeAsyncSendResponseBody(HTTP_IO_CONTEXT* ctx)
{
	RtlZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

	ctx->state = SendingResponseBody;

	if (ctx->denser->httpManager->requestHandle != NULL)
	{
		ULONG result = HttpSendResponseEntityBody(
			ctx->denser->httpManager->requestHandle,
			ctx->requestId,
			ctx->closeResponse ? (ctx->keepConnectionIntactAfterBodySend ? 0 : HTTP_SEND_RESPONSE_FLAG_DISCONNECT) : HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
			ctx->chunk.FromMemory.BufferLength > 0 ? 1 : 0,
			&ctx->chunk,
			NULL, 
			NULL,
			0,
			(OVERLAPPED*)ctx,
			NULL);

		switch (result) {
		default:
			ctx->state = Failed;
			break;
		case NO_ERROR:
		case ERROR_IO_PENDING:
			break;
		};
	}
}

void HttpListener::ReleaseHttpResponseChunk(HTTP_DATA_CHUNK* chunk)
{
	if (chunk->FromMemory.pBuffer)
	{
		delete [] chunk->FromMemory.pBuffer;
	}
	RtlZeroMemory(chunk, sizeof(HTTP_DATA_CHUNK));
}

HRESULT HttpListener::AbortRequest(HTTP_IO_CONTEXT* ctx, DWORD hresult)
{
	// Enforce HTTP processing state to ensure there are no pending async operations against the request slot and therefore 
	// avoid a race condition between IO completion of an async IO operation and the queued IO completion below. 
	// Effectively we are saying that control over an HTTP request remains either with the host or the application (mutually exclusive).
	
	ErrorIf(ProcessingRequestHeaders != ctx->state && ProcessingRequestBody != ctx->state
		&& WaitingForResponseHeaders != ctx->state && WaitingForResponseBody != ctx->state
		&& Failed != ctx->state);	

	ctx->state = PendingAbort;
	ctx->error = hresult;

	// "Pulse" the IO completion port to complete aborting the request; control returns to ServerImpl running on an IO thread pool	

	ErrorIf(FALSE == PostQueuedCompletionStatus(
		this->completionPort,
		0,
		NULL,
		(OVERLAPPED*)ctx));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT HttpListener::EnterShutdownMode()
{
	// Stop the IO completion threads from processing HTTP request/responses, but do not release memory
	// representing the HTTP request/responses in flight to allow for successful completion of queued up APCs. 

	this->StopListen();

	return S_OK;
}