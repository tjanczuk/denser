#pragma once

#include "stdafx.h"

class Denser;

class Event
{
public:
	bool ignorable;

	Event() : ignorable(true) {}
	virtual ~Event() {}

	virtual HRESULT Execute(v8::Handle<v8::Context> context) = 0;
};

class SimpleEvent : Event
{
private:
	v8::Persistent<v8::Object> func;
	int argc;
	v8::Persistent<v8::Value>* argv;
	Denser* denser;

public:
	SimpleEvent(Denser* denser, v8::Handle<v8::Object> func, int argc, v8::Handle<v8::Value>* argv);
	virtual ~SimpleEvent();

	virtual HRESULT Execute(v8::Handle<v8::Context> context);
};

class MeterModule;
class Denser;

class EventLoop
{
private:
	struct EventContext
	{
		Event* ev;
		Denser* denser;
		HANDLE timer;
		EventContext *next, *previous;

		EventContext(Event* ev, Denser* denser) : ev(ev), denser(denser), timer(NULL), next(NULL), previous(NULL) {}
	};

	LONG maxPendingEvents;
	LONG eventSourceCount;
	LONG pendingEventCount;
	LONG denserCount;
	BOOL shutdownMode;
	HANDLE signalEvent;
	EventContext *pendingDelayedEvents;

	static void APIENTRY OnExecuteDelayedEvent(LPVOID ctx, DWORD dwTimerLowValue, DWORD dwTimerHighValue);
	static void WINAPI OnExecuteEvent(DWORD ctx);
	void Execute(Event* ev);
	BOOL ContinueRunning();	
	void StopLoopIfNeeded();

public:
	HANDLE threadHandle;

	EventLoop(LONG maxPendingEvents);
	~EventLoop();

	HRESULT PostEvent(Denser* denser, Event *ev);
	HRESULT PostEvent(Denser* denser, Event *ev, LONG delayMilliseconds);
	HRESULT RunLoop();	
	void EventSourceAdded();
	void EventSourceRemoved();
	void DenserAdded();
	void DenserRemoved();
	HRESULT EnterShutdownMode();
	void ReleaseResources();
};