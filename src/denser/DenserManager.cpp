#include "stdafx.h"

DenserManager* DenserManager::singleton = NULL;

DenserManager::DenserManager(bool useContextIsolation)
	: admin(NULL), root(NULL), activeDenserCount(0), completionEvent(NULL), isShuttingDown(false), 
	useContextIsolation(useContextIsolation), isolate(NULL), loop(NULL)
{
	InitializeCriticalSection(&this->syncRoot);
}

DenserManager::~DenserManager()
{
	if (this->listener)
	{
		delete this->listener;
		this->listener = NULL;
	}
	if (this->completionEvent)
	{
		CloseHandle(this->completionEvent);
	}
	while (this->root)
	{
		Denser* tmp = this->root;
		this->root = this->root->next;
		delete tmp;
	}
	if (this->loop)
	{
		delete this->loop;
		this->loop = NULL;
	}
	if (this->isolate)
	{
		this->isolate->Dispose();
		this->isolate = NULL;
	}
	DeleteCriticalSection(&this->syncRoot);
}

HRESULT DenserManager::AddAdminImports(LPCWSTR config)
{
	v8::Isolate::Scope is(this->admin->isolate);
	v8::Context::Scope cs(this->admin->context);
	v8::HandleScope handleScope;
	v8::Handle<v8::Object> programId;
	v8::Local<v8::Object> global = v8::Context::GetCurrent()->Global();	
	v8::Local<v8::Object> imports = v8::Object::New();
	ErrorIf(S_OK != ::SetVarProperty(global, L"host", imports));
	ErrorIf((programId = DenserManager::CreateProgramIdWithDenserExtension(this->admin, this->admin, this->admin->programId)).IsEmpty());
	ErrorIf(S_OK != ::SetVarProperty(imports, L"managerProgram", programId));
	ErrorIf(S_OK != ::SetStringProperty(imports, L"config", config, -1));
	ErrorIf(!imports->Set(v8::String::New("loadProgram"), v8::FunctionTemplate::New(DenserManager::LoadProgram)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("startProgram"), v8::FunctionTemplate::New(DenserManager::StartProgram)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("stopProgram"), v8::FunctionTemplate::New(DenserManager::StopProgram)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("getProgramSource"), v8::FunctionTemplate::New(DenserManager::GetProgramSource)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("getProgramStatus"), v8::FunctionTemplate::New(DenserManager::GetProgramStatus)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("getProgramStats"), v8::FunctionTemplate::New(DenserManager::GetProgramStats)->GetFunction()));
	ErrorIf(!imports->Set(v8::String::New("getProgramLog"), v8::FunctionTemplate::New(DenserManager::GetProgramLog)->GetFunction()));

	return S_OK;
Error:
	return E_FAIL;
}

v8::Handle<v8::Value> DenserManager::GetProgramSource(const v8::Arguments& args)
{
	JS_ARGS

	Denser* denserProgram = NULL;

	// extract the Denser instance that is the target of this request
	DENSER_FROM_VAR(args, denserProgram) 

	ErrorIf(NULL == denserProgram->script);
	v8::Handle<v8::String> result = v8::String::New((uint16_t*)denserProgram->script);

	return result;
Error:
	return v8::ThrowException(v8::String::New("Unable to get program source code."));
}

v8::Handle<v8::Value> DenserManager::GetProgramStatus(const v8::Arguments& args)
{
	JS_ARGS

	Denser* denserProgram = NULL;

	// extract the Denser instance that is the target of this request
	DENSER_FROM_VAR(args, denserProgram) 

	CAtlString status;

	if (denserProgram->initialized)
	{
		status.AppendFormat(L"{\"state\":\"running\"}");
	}
	else
	{
		status.AppendFormat(L"{\"state\":\"finished\",\"result\":%d}", denserProgram->result);
	}

	v8::Handle<v8::String> result = v8::String::New((uint16_t*)(PCWSTR)status);
	ErrorIf(result.IsEmpty());

	return result;
Error:
	return v8::ThrowException(v8::String::New("Unable to get program status."));
}

v8::Handle<v8::Value> DenserManager::GetProgramStats(const v8::Arguments& args)
{
	JS_ARGS

	Denser* denserProgram = NULL;

	// extract the Denser instance that is the target of this request
	DENSER_FROM_VAR(args, denserProgram) 
	
	ErrorIf(NULL == denserProgram->meter);
	v8::Handle<v8::Object> result;
	ErrorIf(S_OK != denserProgram->meter->GetUsage(denser->context, false, result));

	return result;
Error:
	return v8::ThrowException(v8::String::New("Unable to get program statistics."));
}

v8::Handle<v8::Value> DenserManager::GetProgramLog(const v8::Arguments& args)
{
	JS_ARGS

	Denser* denserProgram = NULL;

	// extract the Denser instance that is the target of this request
	DENSER_FROM_VAR(args, denserProgram) 

	ErrorIf(NULL == denserProgram->log);
	v8::Handle<v8::Object> result;
	ErrorIf(S_OK != denserProgram->log->GetLog(result));

	return result;
Error:
	return v8::ThrowException(v8::String::New("Unable to get program log."));
}

unsigned int WINAPI DenserManager::DenserAdminWorker(void* arg)
{
	LPCWSTR config = (LPCWSTR)arg;
	LPWSTR script = NULL;
	GUID programId;
	bool timerStarted = false;
	bool denserAdded = false;

	if (DenserManager::singleton->useContextIsolation)
	{
		ErrorIf(NULL == (DenserManager::singleton->isolate = v8::Isolate::New()));
		ErrorIf(NULL == (DenserManager::singleton->loop = new EventLoop(MAX_EVENT_LOOP_LENGTH)));
	}

	RtlZeroMemory(&programId, sizeof(programId));
	DenserManager::singleton->admin = new Denser(programId, DenserManager::singleton->listener, DenserManager::singleton->isolate, DenserManager::singleton->loop);	
	ErrorIf(S_OK != DenserManager::singleton->AddDenser(DenserManager::singleton->admin));
	denserAdded = true;
	ErrorIf(S_OK != DenserManager::singleton->admin->InitializeRuntime());
	ErrorIf(S_OK != DenserManager::singleton->AddAdminImports(config));
	ErrorIf(NULL == (script = Denser::LoadFileInResource(IDR_MANAGERMODULE, JSFILE)));
	ErrorIf(S_OK != DenserManager::singleton->admin->meter->StartThreadTimeMeter());
	timerStarted = true;
	ErrorIf(S_OK != DenserManager::singleton->admin->ExecuteScript(script));
	ErrorIf(S_OK != DenserManager::singleton->admin->meter->StopThreadTimeMeter());
	timerStarted = false;
	delete [] script;
	script = NULL;
	HRESULT result = DenserManager::singleton->admin->loop->RunLoop();
	DenserManager::singleton->admin->ReleaseResources();
	DenserManager::singleton->RemoveDenser(DenserManager::singleton->admin);
		
	return result;
Error:
	if (script)
	{
		delete [] script;
	}
	if (timerStarted)
	{
		DenserManager::singleton->admin->meter->StopThreadTimeMeter();
	}
	if (DenserManager::singleton->admin && !denserAdded)
	{
		delete DenserManager::singleton->admin;
		DenserManager::singleton->admin = NULL;
	}
	SetEvent(DenserManager::singleton->completionEvent);
	return E_FAIL;
}

unsigned int WINAPI DenserManager::DenserUserWorker(void* arg)
{
	START_PROGRAM_CONTEXT* ctx = (START_PROGRAM_CONTEXT*)arg;
	bool timerStarted = false;
	bool denserAdded = false;	
	HRESULT result = S_OK;

	ErrorIf(S_OK != DenserManager::singleton->AddDenser(ctx->denser));
	DenserManager::singleton->RemoveDenser(NULL); // decreases ref count increased in DenserManager::StartProgramCore
	denserAdded = true;

	ErrorIf(S_OK != ctx->denser->InitializeRuntime());
	ErrorIf(S_OK != ctx->denser->meter->StartThreadTimeMeter());
	timerStarted = true;		
	ErrorIf(S_OK != ctx->denser->ExecuteScript(ctx->script));
	ErrorIf(S_OK != ctx->denser->meter->StopThreadTimeMeter());
	timerStarted = false;
	delete [] ctx->script;
	ctx->script = NULL;
	if (!DenserManager::singleton->useContextIsolation)
	{
		// we are running denser-specific event loop on a denser-dedicated thread 

		result = ctx->denser->loop->RunLoop();
		ctx->denser->ReleaseResources();
		DenserManager::singleton->RemoveDenser(ctx->denser);
	}
	else
	{
		// there is a singleton event loop that will be processing further events

		result = ctx->denser->result;
		DenserManager::singleton->RemoveDenser(ctx->denser);
	}
		
	delete ctx;

	return result;
Error:
	if (ctx)
	{
		if (ctx->script)
		{
			delete[] ctx->script;
		}
		if (timerStarted)
		{
			ctx->denser->meter->StopThreadTimeMeter();
		}
		if (!denserAdded)
		{
			delete ctx->denser;
		}
		else 
		{
			ctx->denser->ReleaseResources();
			DenserManager::singleton->RemoveDenser(NULL); // decreases ref count to avoid programs that failed on startup preventing system shutdown
			if (ctx->denser->result == S_OK)
			{
				ctx->denser->result = E_FAIL;
			}
			if (ctx->denser->log) 
			{
				ctx->denser->log->Log(L"Program failed during startup. One possible reason is an unhandled exception. Try adding more tracing to the program code to diagnose the failure.");
			}
		}
		delete ctx;
	}
	return E_FAIL;
}

HRESULT DenserManager::Run(LPCWSTR config) 
{
	ErrorIf(NULL != DenserManager::singleton);
	DenserManager::singleton = this;

	this->listener = new HttpListener(0); // TODO, tjanczuk, make IO thread pool size configurable; now we use proc count times 2
	ErrorIf(S_OK != this->listener->Initialize());
	this->completionEvent = CreateEvent(NULL, false, false, NULL);

	ErrorIf(-1L == _beginthreadex(NULL, WORKER_THREAD_STACK_SIZE, DenserManager::DenserAdminWorker, (void*)config, 0, NULL));

	WaitForSingleObjectEx(this->completionEvent, INFINITE, false);

	HRESULT result = S_OK;
	
	while (this->root) 
	{
		Denser* current = this->root;
		if (S_OK != current->result)
		{
			//_tprintf(_T("Thread exited with result 0x%x"), current->result);
			if (S_OK == result) 
			{
				result = current->result;
			}
		}
		this->root = this->root->next;
		delete current;
	}

	v8::V8::Dispose();
	return result;
Error:
	v8::V8::Dispose();
	return E_FAIL;
}

// This function is only ever called from the admin thread
HRESULT DenserManager::AddDenser(Denser* denser)
{
	if (denser)
	{
		denser->next = this->root;
		this->root = denser;
	}
	InterlockedIncrement(&this->activeDenserCount); // this needs to be synchronized with RemoveDenser
	return S_OK;
}

// This is called from the admin thread or from the custom program worker threads
HRESULT DenserManager::RemoveDenser(Denser* denser)
{
	if (0 == InterlockedDecrement(&this->activeDenserCount))
	{
		SetEvent(this->completionEvent);
	}
	return S_OK;
}

void DenserManager::InitializeShutdown()
{
	if (!this->isShuttingDown)
	{
		ENTER_CS(this->syncRoot)

		if (!this->isShuttingDown)
		{
			this->isShuttingDown = true;
			Denser* current = this->root;
			while (current)
			{
				current->InitiateShutdown();
				current = current->next;
			}
		}

		LEAVE_CS(this->syncRoot)
	}
}

v8::Handle<v8::Value> DenserManager::LoadProgram(const v8::Arguments& args)
{
	JS_ARGS

	LPWSTR programSource = NULL;

	v8::String::Value fileName(args[0]->ToString());
	ErrorIf(NULL == (programSource = Denser::GetFileContentAsUnicode((LPCWSTR)(*fileName))));
	v8::Handle<v8::String> result = v8::String::New((uint16_t*)programSource);
	free(programSource);

	return result;
Error:
	if (programSource)
	{
		free(programSource);
	}
	CAtlString errorMessage;
	if (*fileName)
	{
		errorMessage.AppendFormat(L"Error loading program %s.", (LPCWSTR)(*fileName));
	}
	else
	{
		errorMessage.Append(L"Error loading program.");
	}

	return v8::ThrowException(v8::String::New((uint16_t*)(LPCWSTR)errorMessage));
}

v8::Handle<v8::Object> DenserManager::CreateProgramIdWithDenserExtension(Denser* admin, Denser* denserExtension, GUID programId)
{
	v8::HandleScope handleScope;
	v8::Context::Scope scope(admin->context);
	v8::Handle<v8::ObjectTemplate> resultTemplate = v8::ObjectTemplate::New();
	resultTemplate->SetInternalFieldCount(1);
	v8::Handle<v8::Object> result = resultTemplate->NewInstance();
	result->SetInternalField(0, v8::External::New(denserExtension));
	ErrorIf(S_OK != ::SetGUIDProperty(result, L"programId", programId));

	return handleScope.Close(result);
Error:
	v8::Handle<v8::Object> empty;
	return handleScope.Close(empty);
}

v8::Handle<v8::Value> DenserManager::StopProgram(const v8::Arguments& args)
{
	JS_ARGS

	Denser* denserProgram = NULL;

	// extract the Denser instance that is the target of this request
	DENSER_FROM_VAR(args, denserProgram) 

	if (denserProgram->initialized)
	{
		ErrorIf(S_OK != denserProgram->InitiateShutdown());
	}

	return v8::Undefined();
Error:
	return v8::ThrowException(v8::String::New("Unable to initialize program shutdown."));
}

v8::Handle<v8::Value> DenserManager::StartProgram(const v8::Arguments& args)
{
	JS_ARGS
		
	START_PROGRAM_CONTEXT* ctx = new START_PROGRAM_CONTEXT;
	GUID programId;    

	RtlZeroMemory(ctx, sizeof(*ctx));	

	v8::Handle<v8::Object> result;
	v8::String::Value script(args[0]->ToString());

	ErrorIf(DenserManager::singleton->isShuttingDown);	
	ErrorIf(S_OK != CoCreateGuid(&programId));	
	int len = wcslen((WCHAR*)*script) + 1;
	ErrorIf(NULL == (ctx->script = new WCHAR[len]));
	memcpy(ctx->script, *script, len * sizeof WCHAR);
	ctx->denser = new Denser(programId, DenserManager::singleton->listener, DenserManager::singleton->isolate, DenserManager::singleton->loop);	
	ErrorIf((result = DenserManager::CreateProgramIdWithDenserExtension(denser, ctx->denser, programId)).IsEmpty());
	ErrorIf(S_OK != DenserManager::StartProgramCore(ctx));

	return result;
Error:
	if (ctx)
	{
		if (ctx->script)
		{
			delete ctx->script;
		}
		if (ctx->denser)
		{
			delete ctx->denser;
		}
		delete ctx;
	}

	return v8::ThrowException(v8::String::New("Error starting a new program."));
}

HRESULT DenserManager::StartProgramCore(START_PROGRAM_CONTEXT* ctx)
{
	ENTER_CS(DenserManager::singleton->syncRoot)

	// Increase ref count to prevent early termination due to race condition between worker thread start and amin exit
	// If activation is successful, this refcount is decreased from DenserManager::DenserUserWorker
	DenserManager::singleton->AddDenser(NULL); 
	ErrorIf(DenserManager::singleton->isShuttingDown);
	if (DenserManager::singleton->useContextIsolation)
	{
		// each program runs on the same thread in the same v8::Isolate, but in different v8::Context

		DenserManager::DenserUserWorker(ctx);
	}
	else
	{
		// each program runs in its own thread within its own v8::Isolate 

		ErrorIf(-1L ==  _beginthreadex(NULL, WORKER_THREAD_STACK_SIZE, DenserManager::DenserUserWorker, (void*)ctx, 0, NULL));
	}
	
	LEAVE_CS(DenserManager::singleton->syncRoot)

	return S_OK;
Error:
	DenserManager::singleton->RemoveDenser(NULL); // decrease ref count since the new denser did not start
	return E_FAIL;
}