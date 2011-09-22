#include "stdafx.h"

Denser::Denser(GUID programId, HttpListener* http, v8::Isolate* isolate, EventLoop* loop)
	: scriptError(S_OK), meter(NULL), programId(programId), http(http), next(NULL), result(S_OK), httpManager(NULL),
	script(NULL), initialized(FALSE), isolate(isolate), loop(loop), loopRefCountAdded(false)
{
	this->useContextIsolation = isolate != NULL;
	InitializeCriticalSection(&this->syncRoot);
}

Denser::~Denser()
{
	this->ReleaseResources();
	if (this->loop)
	{
		if (!this->useContextIsolation)
			delete this->loop;
		this->loop = NULL;
	}
	if (this->httpManager)
	{
		delete this->httpManager;
	}
	if (this->script)
	{
		delete [] this->script;
	}
	if (this->meter)
	{
		delete this->meter;
	}
	if (this->log)
	{
		delete this->log;
	}
	DeleteCriticalSection(&this->syncRoot);
}

void Denser::ReleaseResources()
{	
	if (this->initialized)
	{
		ENTER_CS(this->syncRoot)

		if (this->initialized) {
			this->isolate->Enter();
			this->context->Enter();

			for (int i = 0; i < this->modules.GetSize(); i++)
			{		
				delete this->modules[i];
			}
			this->modules.RemoveAll();
			if (this->loop && !this->useContextIsolation)
			{
				this->loop->ReleaseResources();
			}
			if (this->httpManager)
			{
				this->httpManager->ReleaseResources();
			}
			if (this->meter)
			{
				this->meter->ReleaseResources();
			}
			this->context->Exit();
			this->context.Dispose();
			this->context = v8::Persistent<v8::Context>();
			this->isolate->Exit();
			if (!this->useContextIsolation)
				this->isolate->Dispose();				
			this->isolate = NULL;
			this->initialized = FALSE;
		}

		LEAVE_CS(this->syncRoot)
	}
}

HRESULT Denser::InitializeRuntime()
{
	if (!this->useContextIsolation)
	{
		this->isolate = v8::Isolate::New();
	}
	v8::Isolate::Scope is(this->isolate);
	v8::HandleScope hs;
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
	global->Set(v8::String::New("require"), v8::FunctionTemplate::New(Denser::CommonJsRequire));
	global->SetInternalFieldCount(1);
	this->context = v8::Context::New(NULL, global);
	v8::Context::Scope cs(this->context);
	// http://code.google.com/p/v8/issues/detail?id=162
	v8::Handle<v8::Object>::Cast(this->context->Global()->GetPrototype())->SetInternalField(0, v8::External::New((void*)this));

	// TODO, tjanczuk, the max number of log entires be configurable
	this->log = new LogModule(MAX_LOG_ENTRIES);
	this->meter = new MeterModule(this);
	if (!this->useContextIsolation)
	{
		// TODO, tjanczuk, the max event loop length should be configurable
		this->loop = new EventLoop(MAX_EVENT_LOOP_LENGTH);	
	}
	this->loop->DenserAdded();
	this->loopRefCountAdded = true;
	this->initialized = TRUE;

	return S_OK;
}

void Denser::SetLastResult(HRESULT result)
{
	this->result = result;
	if (S_OK != result && this->loopRefCountAdded)
	{
		this->loopRefCountAdded = false;
		this->loop->DenserRemoved();
	}
}

HRESULT Denser::ExecuteScript(LPWSTR script)
{
	v8::Isolate::Scope is(this->isolate);
	v8::Context::Scope cs(this->context);
	v8::HandleScope hs;

	if (this->script)
	{
		delete this->script;
	}
	size_t size = wcslen(script);
	this->script = new WCHAR[size + 1];
	memcpy(this->script, script, (size + 1) * sizeof(WCHAR));
		
	v8::TryCatch tryCatch;	
	v8::Handle<v8::String> sh = v8::String::New((uint16_t*)this->script);
	v8::Handle<v8::Script> s = v8::Script::Compile(sh, NULL, NULL);
	v8::Handle<v8::Value> v = s->Run();
	ErrorIf(S_OK != this->log->Log(tryCatch));
	ErrorIf(v.IsEmpty());

	return S_OK;
Error:
	HRESULT hr = GetLastError();
    return hr == S_OK ? E_FAIL : hr;
}

v8::Handle<v8::Value> Denser::ConsoleWrite(const v8::Arguments& args)
{
	JS_ARGS

	v8::String::Value s(args[0]);
    if (*s != NULL)
	{		
		ErrorIf(S_OK != denser->log->Log((LPCWSTR)*s));
	}

Error:
    return v8::Undefined();
}

v8::Handle<v8::Value> Denser::PostEventDelayed(const v8::Arguments& args)
{
	// paramater checking happened in scheduler.js

	int dueTimeIn;
	Event* ev = NULL;

    JS_ARGS

	dueTimeIn = args[1]->Int32Value();
	ev = (Event*)new SimpleEvent(denser, args[0]->ToObject(), 0, NULL);
	ErrorIf(S_OK != denser->loop->PostEvent(denser, ev, dueTimeIn));
	
	return v8::Undefined();

Error:
	if (ev)
	{
		delete ev;
	}

    return v8::ThrowException(v8::String::New("Unable to register callback for delayed execution."));
}

v8::Handle<v8::Value> Denser::PostEventAsap(const v8::Arguments& args)
{
	// paramater checking happened in scheduler.js

    JS_ARGS

	SimpleEvent* ev = new SimpleEvent(denser, args[0]->ToObject(), 0, NULL);
	ErrorIf(S_OK != denser->loop->PostEvent(denser, (Event*)ev));

	return v8::Undefined();
Error:
	if (ev)
	{
		delete ev;
	}

	return v8::ThrowException(v8::String::New("Unable to register callback for future execution. The limit of the event queue length might have been reached."));
}

v8::Handle<v8::Value> Denser::HttpAddUrl(const v8::Arguments& args)
{
	v8::String::Value url(args[0]->ToString());
	HRESULT hr;

	JS_ARGS
	
	ErrorIf(S_OK != (hr = denser->httpManager->AddUrl((PCWSTR)(*url))));

	return v8::Undefined();
Error:
	CAtlString errorMessage;
	errorMessage.Format(_T("Cannot start listening on URL '%s'. Error code %x"), *url == NULL ? L"" : (LPWSTR)(*url), hr);
	return v8::ThrowException(v8::String::New((uint16_t*)(LPCWSTR)errorMessage));
}

v8::Handle<v8::Value> Denser::HttpRemoveUrl(const v8::Arguments& args)
{
	v8::String::Value url(args[0]->ToString());
	HRESULT hr;

	JS_ARGS
	
	ErrorIf(S_OK != (hr = denser->httpManager->RemoveUrl((PCWSTR)(*url))));

	return v8::Undefined();
Error:
	CAtlString errorMessage;
	errorMessage.Format(_T("Cannot stop listening on URL '%s'. Error code %x"), *url == NULL ? L"" : (LPWSTR)(*url), hr);
	return v8::ThrowException(v8::String::New((uint16_t*)(LPCWSTR)errorMessage));
}

v8::Handle<v8::Value> Denser::HttpSetHttpCallbacks(const v8::Arguments& args)
{
	JS_ARGS

	denser->httpManager->SetHttpCallbacks(args[0]->ToObject(), args[1]->ToObject());

	return v8::Undefined();
}

v8::Handle<v8::Value> Denser::HttpSendResponseHeaders(const v8::Arguments& args)
{
	ULONG requestSlot = (ULONG)args[0]->Int32Value();
	int statusCode = args[1]->Int32Value();
	v8::String::Value reason(args[2]->ToString());
	BOOL closeResponse = args[3]->BooleanValue();

	JS_ARGS // (requestSlot, statusCode, reason, closeResponse, headers)

	ErrorIf(S_OK != denser->httpManager->SendResponseHeaders(requestSlot, statusCode, (LPCWSTR)(*reason), args[4]->ToObject(), closeResponse));

	return v8::Undefined();
Error:
	return v8::ThrowException(v8::String::New("Error sending HTTP response headers."));
}

v8::Handle<v8::Value> Denser::HttpSendResponseBodyChunk(const v8::Arguments& args)
{
	ULONG requestSlot = (ULONG)args[0]->Int32Value();
	v8::String::Value chunk(args[1]->ToString());
	BOOL closeResponse = args[2]->BooleanValue();

	JS_ARGS // (requestSlot, chunk, closeResponse)

	ErrorIf(S_OK != denser->httpManager->SendResponseBodyChunk(requestSlot, (LPCWSTR)(*chunk), closeResponse));

	return v8::Undefined();
Error:
	return v8::ThrowException(v8::String::New("Error sending HTTP response body."));
}

v8::Handle<v8::Value> Denser::HttpPumpRequest(const v8::Arguments& args)
{
	JS_ARGS // (requestSlot)

	ULONG requestSlot = (ULONG)args[0]->Int32Value();

	ErrorIf(S_OK != denser->httpManager->PumpRequest(requestSlot));

	return v8::Undefined();
Error:
	return v8::ThrowException(v8::String::New("Error reading more of the HTTP request."));
}

v8::Handle<v8::Value> Denser::MeterGetUsage(const v8::Arguments& args)
{
	v8::Handle<v8::Object> stats;

	JS_ARGS

	ErrorIf(S_OK != denser->meter->GetUsage(stats));

	return stats;
Error:
	return v8::ThrowException(v8::String::New("Error reading more of the HTTP request."));
}

v8::Handle<v8::Value> Denser::CommonJsRequire(const v8::Arguments& args)
{
	JS_ARGS

	return denser->CommonJsRequireImpl(args);
}

v8::Handle<v8::Object> Denser::CommonJsRequireImpl(const v8::Arguments& args)
{
	v8::HandleScope hs;
	v8::String::Value moduleName(args[0]->ToString());
	v8::Handle<v8::Object> module;

    if (!*moduleName)
	{
		return v8::ThrowException(v8::String::New("'require' function was called with invalid parameters. The function requries a single string parameter with the module name."))->ToObject();		
	}

	ErrorIf(S_OK != this->GetOrCreateModule(args[0]->ToString(), module));

	return hs.Close(module);

Error:	
	CAtlString errorMessage;
	if (!*moduleName != NULL)
	{
		errorMessage.Format(_T("Unable to resolve module '%s'."), (LPCWSTR)(*moduleName));
	}
	else
	{
		errorMessage = _T("Unable to resolve module.");
	}

	return v8::ThrowException(v8::String::New((uint16_t*)(LPCWSTR)errorMessage))->ToObject();	
}

// This method is single threaded but re-entrant. It must handle circular module resolutions by returning top level
// object for a module that has not been fully resolved yet (see CommonJS module spec). 
HRESULT Denser::GetOrCreateModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{	
	v8::HandleScope hs;	
	v8::String::Value wname(name);

	if (!(module = this->FindModule(name)).IsEmpty())
	{
		return S_OK;
	}
	else if (_tcscmp(L"tracing", (LPCWSTR)*wname) == 0)
	{
		return this->CreateTracingModule(name, module);
	}
	else if (_tcscmp(L"scheduler", (LPCWSTR)*wname) == 0)
	{
		return this->CreateSchedulerModule(name, module);
	}
	else if (_tcscmp(L"http", (LPCWSTR)*wname) == 0)
	{
		return this->CreateHttpModule(name, module);
	}
	else if (_tcscmp(L"meter", (LPCWSTR)*wname) == 0)
	{
		return this->CreateMeterModule(name, module);
	}
	else
	{
		return this->CreateModuleFromFile(name, module);
	}
}

HRESULT Denser::CreateModuleFromFile(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{
	v8::String::Value wname(name);
	ModuleEntry* entry = NULL;

	// load script from file

	TCHAR currentDirectory[FILENAME_MAX];
	// TODO, tjanczuk, resolving a module name should be relative to the module requesting the resolution
	int length = GetCurrentDirectory(sizeof(currentDirectory), currentDirectory);
	if (length == 0 || length == 256)
	{
		return E_FAIL;
	}
	CAtlString moduleName;
	moduleName.Format(L"%s\\%s.js", currentDirectory, (LPCWSTR)*wname);		
	LPWSTR script = NULL;
	ErrorIf(NULL == (script = Denser::GetFileContentAsUnicode(moduleName)));	
	ErrorIf(S_OK != this->CreateModuleFromString(name, script, module));
	free(script);

	return S_OK;
Error:
	if (script != NULL)
	{
		free(script);
	}
	return E_FAIL;
}

HRESULT Denser::CreateModuleFromString(v8::Handle<v8::String> name, LPWSTR script, v8::Handle<v8::Object>& module)
{
	v8::TryCatch tryCatch;	
	ModuleEntry* entry = NULL;
	
	// create an empty stub of a module and register it to support cyclic references between module definitions

	ErrorIf(S_OK != ModuleEntry::Create(name, &entry));
	this->modules.Add(entry);

	// execute the module and extract its exports

	v8::Handle<v8::Object> global = v8::Context::GetCurrent()->Global();
	ErrorIf(global.IsEmpty());

	entry->module = v8::Persistent<v8::Object>::New(v8::Object::New());
	ErrorIf(entry->module.IsEmpty());

	v8::Handle<v8::String> exportsName = v8::String::New("exports");	
	v8::Handle<v8::Value> currentExports = global->Get(exportsName);
	global->Set(exportsName, entry->module);
	
	v8::Handle<v8::Script> s = v8::Script::Compile(v8::String::New((uint16_t*)script), NULL, NULL);
	v8::Handle<v8::Value> v = s->Run();
	ErrorIf(S_OK != this->log->Log(tryCatch));
	ErrorIf(v.IsEmpty());

	if (!currentExports.IsEmpty())
	{
		global->Set(exportsName, currentExports);
	}
	else
	{
		global->Delete(exportsName);
	}

	module = entry->module;

	return S_OK;
Error:
	if (entry != NULL)
	{
		this->modules.Remove(entry);
		delete entry;
	}
	if (!currentExports.IsEmpty())
	{
		global->Set(exportsName, currentExports);
	}
	return E_FAIL;
}

HRESULT Denser::CreateTracingModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{	
	LPWSTR script = NULL;

	ErrorIf(NULL == (script = Denser::LoadFileInResource(IDR_TRACINGMODULE, JSFILE)));

	v8::Handle<v8::Object> global = v8::Context::GetCurrent()->Global();
	ErrorIf(global.IsEmpty());
	v8::Handle<v8::String> importsName = v8::String::New("imports");
	ErrorIf(importsName.IsEmpty());
	v8::Handle<v8::Object> imports = v8::Object::New();
	ErrorIf(imports.IsEmpty());
	ErrorIf(!global->Set(importsName, imports));

	ErrorIf(!imports->Set(v8::String::New("write"), v8::FunctionTemplate::New(Denser::ConsoleWrite)->GetFunction()));
	ErrorIf(S_OK != this->CreateModuleFromString(name, script, module));

	ErrorIf(!global->Delete(importsName));
	delete[] script;

	return S_OK;
Error:
	if (script != NULL)
	{
		delete[] script;
	}
	if (!global.IsEmpty() && !importsName.IsEmpty())
	{
		global->Delete(importsName);
	}

	return E_FAIL;
}

HRESULT Denser::CreateMeterModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{	
	LPWSTR script = NULL;

	ErrorIf(NULL == (script = Denser::LoadFileInResource(IDR_METERMODULE, JSFILE)));

	v8::Handle<v8::Object> global = v8::Context::GetCurrent()->Global();
	ErrorIf(global.IsEmpty());
	v8::Handle<v8::String> importsName = v8::String::New("imports");
	ErrorIf(importsName.IsEmpty());
	v8::Handle<v8::Object> imports = v8::Object::New();
	ErrorIf(imports.IsEmpty());
	ErrorIf(!global->Set(importsName, imports));

	ErrorIf(!imports->Set(v8::String::New("getUsage"), v8::FunctionTemplate::New(Denser::MeterGetUsage)->GetFunction()));
	ErrorIf(S_OK != this->CreateModuleFromString(name, script, module));

	ErrorIf(!global->Delete(importsName));
	delete[] script;

	return S_OK;
Error:
	if (script != NULL)
	{
		delete[] script;
	}
	if (!global.IsEmpty() && !importsName.IsEmpty())
	{
		global->Delete(importsName);
	}

	return E_FAIL;
}


HRESULT Denser::CreateSchedulerModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{	
	LPWSTR script = NULL;

	ErrorIf(NULL == (script = Denser::LoadFileInResource(IDR_SCHEDULERMODULE, JSFILE)));

	v8::Handle<v8::Object> global = v8::Context::GetCurrent()->Global();
	ErrorIf(global.IsEmpty());
	v8::Handle<v8::String> importsName = v8::String::New("imports");
	ErrorIf(importsName.IsEmpty());
	v8::Handle<v8::Object> imports = v8::Object::New();
	ErrorIf(imports.IsEmpty());
	ErrorIf(!global->Set(importsName, imports));

	ErrorIf(!imports->Set(v8::String::New("postEventDelayed"), v8::FunctionTemplate::New(Denser::PostEventDelayed)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("postEventAsap"), v8::FunctionTemplate::New(Denser::PostEventAsap)->GetFunction()));
	ErrorIf(S_OK != this->CreateModuleFromString(name, script, module));

	ErrorIf(!global->Delete(importsName));
	delete[] script;

	return S_OK;
Error:
	if (script != NULL)
	{
		delete[] script;
	}
	if (!global.IsEmpty() && !importsName.IsEmpty())
	{
		global->Delete(importsName);
	}

	return E_FAIL;
}

HRESULT Denser::CreateHttpModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module)
{	
	v8::HandleScope hs;

	LPWSTR script = NULL;

	// TODO, tjanczuk, determine the max number of concurrent requests (make this configurable?)
	DWORD maxConcurrentRequests = MAX_CONCURRENT_REQUESTS;
	this->httpManager = new HttpManager(this, maxConcurrentRequests);

	ErrorIf(NULL == (script = Denser::LoadFileInResource(IDR_HTTPMODULE, JSFILE)));
	
	v8::Handle<v8::Object> global = v8::Context::GetCurrent()->Global();
	ErrorIf(global.IsEmpty());	
	v8::Handle<v8::Object> imports = v8::Object::New();
	ErrorIf(imports.IsEmpty());
	v8::Handle<v8::String> importsName = v8::String::New("imports");
	ErrorIf(importsName.IsEmpty());
	ErrorIf(!global->Set(importsName, imports));

	imports->Set(v8::String::New("addUrl"), v8::FunctionTemplate::New(Denser::HttpAddUrl)->GetFunction());
	imports->Set(v8::String::New("removeUrl"), v8::FunctionTemplate::New(Denser::HttpRemoveUrl)->GetFunction());
	imports->Set(v8::String::New("setHttpCallbacks"), v8::FunctionTemplate::New(Denser::HttpSetHttpCallbacks)->GetFunction());
	imports->Set(v8::String::New("sendHttpResponseHeaders"), v8::FunctionTemplate::New(Denser::HttpSendResponseHeaders)->GetFunction());
	imports->Set(v8::String::New("sendHttpResponseBodyChunk"), v8::FunctionTemplate::New(Denser::HttpSendResponseBodyChunk)->GetFunction());
	imports->Set(v8::String::New("pumpRequest"), v8::FunctionTemplate::New(Denser::HttpPumpRequest)->GetFunction());	

	ErrorIf(S_OK != this->CreateModuleFromString(name, script, module));

	ErrorIf(!global->Delete(importsName));
	delete[] script;

	return S_OK;
Error:
	if (this->httpManager != NULL)
	{
		delete this->httpManager;
		this->httpManager = NULL;
	}
	if (script != NULL)
	{
		delete[] script;
	}
	if (!global.IsEmpty() && !importsName.IsEmpty())
	{
		global->Delete(importsName);
	}

	return E_FAIL;
}

v8::Handle<v8::Object> Denser::FindModule(v8::Handle<v8::String> name)
{
	v8::String::Value wname(name);

	for (int i = 0; i < this->modules.GetSize(); i++)
	{
		v8::String::Value mname(this->modules[i]->name);
		if (_tcscmp((LPCWSTR)*wname, (LPCWSTR)*mname) == 0)
		{
			return this->modules[i]->module;
		}
	}

	return v8::Handle<v8::Object>();
}

HRESULT Denser::ModuleEntry::Create(v8::Handle<v8::String> name, ModuleEntry** newModule)
{
	if (newModule == NULL)
	{
		return E_INVALIDARG;
	}

	*newModule = NULL;

	ModuleEntry* m = new ModuleEntry();
	if (!m)
	{
		return E_OUTOFMEMORY;
	}

	m->name = v8::Persistent<v8::String>::New(name);
	if (m->name.IsEmpty())
	{
		delete m;
		return E_OUTOFMEMORY;
	}

	*newModule = m;

	return S_OK;
}

Denser::ModuleEntry::~ModuleEntry()
{	
	if (!this->name.IsEmpty())
	{
		this->name.Dispose();
		this->name = v8::Persistent<v8::String>();
	}
	if (!this->module.IsEmpty())
	{
		this->module.Dispose();
		this->module = v8::Persistent<v8::Object>();
	}
}

// TODO this code needs fixing; seems to only work for UTF-16 and ANSI, UTF-8 is excluded
LPWSTR Denser::GetFileContentAsUnicode(LPCWSTR fileName)
{
	HANDLE file = NULL;
    LPWSTR contents = NULL;
	DWORD lengthBytes;
	DWORD bytesRead;
	LPWSTR contentsRaw = NULL;

	ErrorIf(INVALID_HANDLE_VALUE == (file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)));
	ErrorIf(INVALID_FILE_SIZE == (lengthBytes = GetFileSize(file, NULL)));
    ErrorIf(NULL == (contentsRaw = (LPWSTR) calloc(lengthBytes + 3, 1)));
	ErrorIf(false == ReadFile(file, contentsRaw, lengthBytes, &bytesRead, NULL) || lengthBytes != bytesRead);

	byte * pRawBytes = (byte*)contentsRaw;

    ErrorIf((0xEF == *pRawBytes && 0xBB == *(pRawBytes+1) && 0xBF == *(pRawBytes+2)) ||
        0xFFFE == *contentsRaw ||
        0x0000 == *contentsRaw && 0xFEFF == *(contentsRaw+1));
	
	if (0xFEFF == *contentsRaw)
    {
        // unicode BE
        contents = contentsRaw;
    }
    else
    {
        // assume ANSI
        // note: some other encodings might fall wrongly in this category
        DWORD cAnsiChars = lengthBytes + 1;
        ErrorIf(NULL == (contents = (LPWSTR) calloc(cAnsiChars, sizeof(wchar_t))));
        ErrorIf(0 == MultiByteToWideChar(CP_ACP, 0, (LPCSTR)contentsRaw, cAnsiChars, (LPWSTR)contents, cAnsiChars));
        free((void*)contentsRaw);
    }

	CloseHandle(file);

    return contents;

Error:
	if (contentsRaw != NULL) 
	{
		free((void*)contentsRaw);
	}
	if (contents != NULL)
	{
		free((void*)contents);
	}
	if (file != NULL)
	{
		CloseHandle(file);
	}
	return NULL;
}

LPWSTR Denser::LoadFileInResource(int name, int type)
{
    HMODULE handle = GetModuleHandle(NULL);
    HRSRC rc = FindResource(handle, MAKEINTRESOURCE(name), MAKEINTRESOURCE(type));
    HGLOBAL rcData = LoadResource(handle, rc);
    size_t size = SizeofResource(handle, rc);
	char *resource = (char *)(LockResource(rcData));
	LPWSTR result = (LPWSTR)::AllocAsciiToUnicode(resource, size, NULL);
	
	return result;
}

void Denser::EventSourceAdded()
{
	this->loop->EventSourceAdded();
}

void Denser::EventSourceRemoved()
{
	this->loop->EventSourceRemoved();
}

HRESULT Denser::InitiateShutdown() 
{
	ENTER_CS(this->syncRoot)

	if (this->initialized)
	{
		// turn off HTTP
		if (this->httpManager != NULL)
		{
			this->httpManager->EnterShutdownMode();
		}

		// From now on, stop running JavaScript callbacks, but continue running the loop to drain the APC queue
		// EventLoop::RunLoop will exit once APCs have been drained. 
		if (this->loop)
		{
			this->loop->EnterShutdownMode(); 
		}
	}

	LEAVE_CS(this->syncRoot)

	return S_OK;
}
