#pragma once

#include "stdafx.h"

typedef enum {
	HTTP_SERVER_REQUESTS_COMPLETED,
	HTTP_SERVER_REQUESTS_ABORTED,
	HTTP_SERVER_REQUESTS_ACTIVE,
	HTTP_SERVER_BYTES_RECEIVED,
	HTTP_SERVER_BYTES_SENT,
	METRIC_SIZE // This must be the last entry
} DENSER_METRIC;

class MeterModule {
private:
	ULONG64 threadCycleTime, startThreadCycleTime;	
	ULONG64 metric[METRIC_SIZE];
	static PCWSTR metricName[];
	HANDLE thread;
	Denser* denser;

public:
	MeterModule(Denser* denser); 
	~MeterModule();

	HRESULT GetUsage(v8::Handle<v8::Object>& stats);
	HRESULT GetUsage(v8::Handle<v8::Context> context, bool getInProgressStats, v8::Handle<v8::Object>& stats);
	HRESULT StartThreadTimeMeter();
	HRESULT StopThreadTimeMeter();
	HRESULT GetCurrentThreadCycleTime(bool getInProgessStats, ULONG64 *applicationTime, ULONG64 *totalTime);
	void IncrementMetric(DENSER_METRIC id);
	void IncrementMetric(DENSER_METRIC id, ULONG64 value);
	void DecrementMetric(DENSER_METRIC id);
	void ReleaseResources();
};