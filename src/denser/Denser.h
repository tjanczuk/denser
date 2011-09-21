#pragma once

#include "stdafx.h"

#define MAX_EVENT_LOOP_LENGTH 1024
#define MAX_LOG_ENTRIES 100
#define MAX_CONCURRENT_REQUESTS 10

class Denser
{
private:	

	class ModuleEntry
	{
	public:
		v8::Persistent<v8::String> name;
		v8::Persistent<v8::Object> module;

		~ModuleEntry();
		static HRESULT Create(v8::Handle<v8::String> name, ModuleEntry** newModule);
	};
	
	CSimpleArray<ModuleEntry*> modules;		
	HRESULT scriptError;	
	CRITICAL_SECTION syncRoot;

	v8::Handle<v8::Value> GetUndefined();
	HRESULT GetOrCreateModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateTracingModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateSchedulerModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateMeterModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateHttpModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateSqlServerModule(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateModuleFromFile(v8::Handle<v8::String> name, v8::Handle<v8::Object>& module);
	HRESULT CreateModuleFromString(v8::Handle<v8::String> name, LPWSTR script, v8::Handle<v8::Object>& module);
	v8::Handle<v8::Object> FindModule(v8::Handle<v8::String> name);	

public:

	BOOL initialized;
	GUID programId;
	EventLoop* loop;
	MeterModule *meter;
	HttpListener* http;
	HttpManager* httpManager;
	v8::Persistent<v8::Context> context;
	v8::Isolate* isolate;
	Denser* next; // used by DenserManager to create a linked list in DenserManager::root
	HRESULT result;
	LPWSTR script;
	LogModule* log;

	Denser(GUID programId, HttpListener* http);
	~Denser();

	static LPWSTR LoadFileInResource(int name, int type);
	static LPWSTR GetFileContentAsUnicode(LPCWSTR fileName);
	HRESULT InitializeRuntime();
	HRESULT ExecuteScript(LPWSTR script);
	HRESULT ExecuteEventLoop();
	void EventSourceAdded();
	void EventSourceRemoved();
	HRESULT InitiateShutdown();
	void ReleaseResources();

	static v8::Handle<v8::Value> ConsoleWrite(const v8::Arguments& args);
	static v8::Handle<v8::Value> PostEventDelayed(const v8::Arguments& args);	
	static v8::Handle<v8::Value> PostEventAsap(const v8::Arguments& args);
	static v8::Handle<v8::Value> CommonJsRequire(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpAddUrl(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpRemoveUrl(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpSetHttpCallbacks(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpSendResponseHeaders(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpSendResponseBodyChunk(const v8::Arguments& args);
	static v8::Handle<v8::Value> HttpPumpRequest(const v8::Arguments& args);
	static v8::Handle<v8::Value> MeterGetUsage(const v8::Arguments& args);
	v8::Handle<v8::Object> CommonJsRequireImpl(const v8::Arguments& args);
};

#define DENSER_FROM_VAR(var, denser) \
	{\
	    v8::Local<v8::Object> self = v8::Local<v8::Object>::Cast(v8::Context::GetCurrent()->Global()->GetPrototype());\
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));\
		void* ptr = wrap->Value();\
		denser = static_cast<Denser*>(ptr);\
	};\

#define JS_ARGS \
	Denser* denser = NULL;\
	DENSER_FROM_VAR(args, denser);\
	v8::Isolate::Scope is(denser->isolate);\
	v8::Context::Scope cs(denser->context);\
	v8::HandleScope hs;
