#include "stdafx.h"

// TODO, tjanczuk, find a better way to measure thread times: http://blog.kalmbachnet.de/?postid=28, http://blogs.msdn.com/b/oldnewthing/archive/2009/03/02/9453318.aspx

PCWSTR MeterModule::metricName[] = {
	L"httpServerRequestsCompleted", //	HTTP_SERVER_REQUESTS_COMPLETED,
	L"httpServerRequestsAborted", // HTTP_SERVER_REQUESTS_ABORTED,
	L"httpServerRequestsActive", // HTTP_SERVER_REQUESTS_ACTIVE,
	L"httpServerBytesReceived", // HTTP_SERVER_BYTES_RECEIVED,
	L"httpServerBytesSent" // HTTP_SERVER_BYTES_SENT,
};

MeterModule::MeterModule(v8::Handle<v8::Context> context, GUID programId) 
	: threadCycleTime(0), startThreadCycleTime(0), context(v8::Persistent<v8::Context>::New(context)), programId(programId), thread(NULL)
{
	RtlZeroMemory(this->metric, sizeof(ULONG64) * METRIC_SIZE);
	HANDLE process = GetCurrentProcess();
	DuplicateHandle(process, GetCurrentThread(), process, &this->thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
}

MeterModule::~MeterModule() 
{
	if (this->thread)
	{
		CloseHandle(this->thread);
	}
}

void MeterModule::ReleaseResources()
{
	if (!this->context.IsEmpty())
	{
		this->context.Dispose();
		this->context = v8::Persistent<v8::Context>();
	}
}

HRESULT MeterModule::GetUsage(v8::Handle<v8::Object>& stats)
{
	return this->GetUsage(this->context, true, stats);
}

HRESULT MeterModule::GetUsage(v8::Handle<v8::Context> context, bool getInProgressStats, v8::Handle<v8::Object>& stats)
{
	v8::HandleScope handleScope;
	v8::Context::Scope scope(context);
	v8::Handle<v8::Object> result = v8::Object::New();
	v8::Handle<v8::Object> cpu = v8::Object::New();
	v8::Handle<v8::Object> http = v8::Object::New();
	ULONG64 applicationThreadCycleTime, totalThreadCycleTime;

	// Create stats object

	ErrorIf(result.IsEmpty());
	ErrorIf(cpu.IsEmpty());
	ErrorIf(http.IsEmpty());
	ErrorIf(S_OK != ::SetGUIDProperty(result, L"programId", this->programId));
	ErrorIf(S_OK != ::SetVarProperty(result, L"cpu", cpu));
	ErrorIf(S_OK != ::SetVarProperty(result, L"http", http));	

	// Calculate thread cycle time

	ErrorIf(S_OK != this->GetCurrentThreadCycleTime(getInProgressStats, &applicationThreadCycleTime, &totalThreadCycleTime));
	ErrorIf(S_OK != ::SetULONG64Property(cpu, L"applicationCpuCycles", applicationThreadCycleTime));
	ErrorIf(S_OK != ::SetULONG64Property(cpu, L"threadCpuCycles", totalThreadCycleTime));

	// Add enumerated metrics

	for (int i = 0; i < METRIC_SIZE; i++)
	{
		ErrorIf(S_OK != ::SetULONG64Property(http, MeterModule::metricName[i], this->metric[i]));
	}

	stats = handleScope.Close(result);

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT MeterModule::StartThreadTimeMeter()
{
	ErrorIf(0 != this->startThreadCycleTime);

	ErrorIf(0 == QueryThreadCycleTime(this->thread, &this->startThreadCycleTime));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT MeterModule::StopThreadTimeMeter()
{
	ULONG64 nowCycles;

	ErrorIf(0 == this->startThreadCycleTime);

	ErrorIf(0 == QueryThreadCycleTime(this->thread, &nowCycles));
	this->threadCycleTime += nowCycles - this->startThreadCycleTime;
	this->startThreadCycleTime = 0;

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT MeterModule::GetCurrentThreadCycleTime(bool getInProgressStats, ULONG64 *applicationTime, ULONG64 *totalTime)
{
	ErrorIf(NULL == applicationTime);
	ErrorIf(NULL == totalTime);

	ErrorIf(0 == QueryThreadCycleTime(this->thread, totalTime));

	if (this->startThreadCycleTime == 0 || !getInProgressStats)
	{
		*applicationTime = this->threadCycleTime;
	}
	else
	{
		*applicationTime = this->threadCycleTime + *totalTime - this->startThreadCycleTime;
	}

	return S_OK;
Error:
	return E_FAIL;
}

void MeterModule::IncrementMetric(DENSER_METRIC id, ULONG64 value)
{
	InterlockedExchangeAdd(&this->metric[id], value);
}

void MeterModule::IncrementMetric(DENSER_METRIC id)
{
	InterlockedIncrement(&this->metric[id]);
}

void MeterModule::DecrementMetric(DENSER_METRIC id)
{
	InterlockedDecrement(&this->metric[id]);
}