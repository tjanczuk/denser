#pragma once

#include "stdafx.h"

// TODO, tjanczuk, make this configurable
#define WORKER_THREAD_STACK_SIZE 32 * 1024

class DenserManager
{
private:
	typedef struct {
		Denser* denser;
		WCHAR* script;
	} START_PROGRAM_CONTEXT;

	Denser* admin;
	Denser* root;
	HttpListener* listener;
	unsigned activeDenserCount;
	HANDLE completionEvent;
	bool isShuttingDown;
	CRITICAL_SECTION syncRoot;
	bool useContextIsolation;
	v8::Isolate *isolate;
	EventLoop *loop;

	HRESULT AddAdminImports(LPCWSTR config);
	HRESULT AddDenser(Denser* denser);
	HRESULT RemoveDenser(Denser* denser);
	static v8::Handle<v8::Value> LoadProgram(const v8::Arguments& args);	
	static v8::Handle<v8::Value> StartProgram(const v8::Arguments& args);	
	static v8::Handle<v8::Value> StopProgram(const v8::Arguments& args);	
	static v8::Handle<v8::Value> GetProgramSource(const v8::Arguments& args);	
	static v8::Handle<v8::Value> GetProgramStatus(const v8::Arguments& args);	
	static v8::Handle<v8::Value> GetProgramStats(const v8::Arguments& args);	
	static v8::Handle<v8::Value> GetProgramLog(const v8::Arguments& args);	
	static HRESULT StartProgramCore(START_PROGRAM_CONTEXT* ctx);	
	static v8::Handle<v8::Object> CreateProgramIdWithDenserExtension(Denser* admin, Denser* denserExtension, GUID programId);

	static unsigned int WINAPI DenserAdminWorker(void* arg);
	static unsigned int WINAPI DenserUserWorker(void* arg);

public:
	static DenserManager* singleton;

	DenserManager(bool useContextIsolation);
	~DenserManager();

	HRESULT Run(LPCWSTR config);
	void InitializeShutdown();
};
