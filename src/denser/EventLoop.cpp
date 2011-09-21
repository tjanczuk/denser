#include "stdafx.h"

SimpleEvent::SimpleEvent(v8::Handle<v8::Object> func, int argc, v8::Handle<v8::Value>* argv)
	: Event(), argc(argc), argv(NULL)
{
	this->func = v8::Persistent<v8::Object>::New(func);
	if (argc > 0)
	{
		argv = new v8::Handle<v8::Value>[argc];
		for (int i = 0; i < argc; i++)
		{
			this->argv[i] = v8::Persistent<v8::Value>::New(argv[i]);
		}
	}
}

SimpleEvent::~SimpleEvent()
{
	if (!this->func.IsEmpty())
	{
		this->func.Dispose();
		this->func = v8::Persistent<v8::Object>();
	}

	if (this->argv)
	{
		for (int i = 0; i < this->argc; i++)
		{
			this->argv[i].Dispose();
		}			
		delete[] this->argv;
		this->argv = NULL;
	}
}

HRESULT SimpleEvent::Execute(v8::Handle<v8::Context> context)
{
	if (!context.IsEmpty())
	{
		v8::HandleScope hs;
		v8::Context::Scope scope(context);
		v8::TryCatch tryCatch;
		v8::Handle<v8::Object> global = context->Global();
		v8::Handle<v8::Value> v = this->func->CallAsFunction(global, this->argc, this->argv);
		if (v.IsEmpty())
		{
			return E_FAIL;
		}
		else
		{
			return S_OK;
		}
	}
	else
	{
		return E_FAIL;
	}
}

EventLoop::EventLoop(LONG maxPendingEvents, Denser* denser)
	: maxPendingEvents(maxPendingEvents), eventSourceCount(0), 
	pendingEventCount(0), denser(denser), shutdownMode(FALSE), pendingDelayedEvents(NULL)
{
	// Store the current thread handle (assumed to be the same as the apartment thread of the associated script context)
	// for the purpose of scheduling APCs for that thread in PostEvent.
	HANDLE process = GetCurrentProcess();
	DuplicateHandle(process, GetCurrentThread(), process, &this->threadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
	this->signalEvent = CreateEvent(NULL, false, false, NULL);
	this->lastEventResult = !denser->context.IsEmpty() ? S_OK : E_FAIL;
}

EventLoop::~EventLoop()
{
	this->ReleaseResources();
}

void EventLoop::ReleaseResources()
{
	if (this->signalEvent)
	{
		CloseHandle(this->signalEvent);
		this->signalEvent = NULL;
	}
	if (this->threadHandle)
	{
		CloseHandle(this->threadHandle);
		this->threadHandle = NULL;
	}
}

void APIENTRY EventLoop::OnExecuteDelayedEvent(LPVOID ctx, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	// This is an APC callback of SetWaitableTimer that executes in the appartment thread of the script context.

	EventContext* ec = (EventContext*)ctx;
	
	CloseHandle(ec->timer);	

	// Remove EventContext from EventLoop::pendingDelayedEvents

	if (ec->previous != NULL)
	{
		ec->previous->next = ec->next;
	}
	else
	{
		ec->loop->pendingDelayedEvents = ec->next;
	}
	if (ec->next != NULL)
	{
		ec->next->previous = ec->previous;
	}
	
	InterlockedIncrement(&ec->loop->pendingEventCount);
	EventLoop::OnExecuteEvent((DWORD)ctx);
}

void WINAPI EventLoop::OnExecuteEvent(DWORD ctx)
{
	EventContext* ec = (EventContext*)ctx;

	EventLoop* loop = ec->loop;
	
	if (loop->lastEventResult == S_OK && !loop->shutdownMode)
	{
		// This check was to avoid executing further events after an event execution failed. This can occur as a result of 
		// a scheduling race condition between queued up APCs and resuming the wait in RunLoop. The condition is short lived
		// as we expect the RunLoop to exit as soon as it is signalled. 

		v8::Isolate::Scope is(loop->denser->isolate);
		loop->denser->meter->StartThreadTimeMeter();		
		loop->lastEventResult = ec->ev->Execute(loop->denser->context);
		loop->denser->meter->StopThreadTimeMeter();
	}
	else if (!ec->ev->ignorable)
	{
		v8::Isolate::Scope is(loop->denser->isolate);
		ec->ev->Execute(loop->denser->context);
	}

	{
		v8::Isolate::Scope is(loop->denser->isolate);
		v8::Context::Scope cs(loop->denser->context);
		delete ec->ev;
	}
	delete ec;	

	InterlockedDecrement(&loop->pendingEventCount);

	loop->StopLoopIfNeeded();
}

void EventLoop::StopLoopIfNeeded()
{
	if (!this->ContinueRunning())
	{
		// The event loop should no longer be running. Signal the main loop in RunLoop to reevalue its condition.

		SetEvent(this->signalEvent);
	}
}

HRESULT EventLoop::PostEvent(Event *ev)
{
	EventContext* ec = NULL;
	bool increasedPendingCount = false;

	ErrorIf(this->lastEventResult != S_OK);
	ErrorIf(ev == NULL);
	ec = new EventContext(ev, this);
	increasedPendingCount = true;
	ErrorIf(InterlockedIncrement(&this->pendingEventCount) > this->maxPendingEvents);
	ErrorIf(0 == QueueUserAPC(EventLoop::OnExecuteEvent, this->threadHandle, (ULONG_PTR)ec));

	return S_OK;
Error:
	if (increasedPendingCount)
	{
		InterlockedDecrement(&this->pendingEventCount);
	}
	if (ec)
	{
		delete ec;
	}
	return E_FAIL;
}

HRESULT EventLoop::PostEvent(Event *ev, LONG delayInMilliseconds)
{
	EventContext* ec = NULL;
	LARGE_INTEGER dueTime;

	ErrorIf(this->lastEventResult != S_OK);
	ErrorIf(ev == NULL);
	ec = new EventContext(ev, this);
	dueTime.QuadPart = -(LONGLONG)delayInMilliseconds * 10000;	
	ErrorIf(NULL == (ec->timer = CreateWaitableTimer(NULL, FALSE, NULL)));	
	ErrorIf(false == SetWaitableTimer(ec->timer, &dueTime, 0, EventLoop::OnExecuteDelayedEvent, ec, FALSE));

	// This method is only ever called from the single thread associated with the script context.
	// Therefore there is no need to synchronize access around pendingDelayedEvents.

	if (this->pendingDelayedEvents != NULL)
	{
		this->pendingDelayedEvents->previous = ec;
	}
	ec->next = this->pendingDelayedEvents;
	this->pendingDelayedEvents = ec;

	return S_OK;
Error:
	if (ec)
	{
		if (ec->timer)
		{
			CloseHandle(ec->timer);
		}
		delete ec;
	}
	return E_FAIL;
}

void EventLoop::EventSourceAdded()
{
	this->eventSourceCount++;
}

void EventLoop::EventSourceRemoved()
{
	this->eventSourceCount--;
	this->StopLoopIfNeeded();
}

HRESULT EventLoop::RunLoop()
{
	DWORD waitResult = 0;

	while (this->ContinueRunning())
	{
		// This is an alertable wait which allows queued APCs to execute on the current thread during the wait. 
		// When this event is signalled, it means the conditions may exist that would cause the loop to terminate (e.g. no more events to process).
		// The 20 second delay is a trip wire to re-evaluate whether the loop should continue running independent of the signal (software has bugs).

		waitResult = WaitForSingleObjectEx(this->signalEvent, 20000, true);
	}

	// Cancel and clean up delayed events that have not been activated yet
	while (this->pendingDelayedEvents != NULL)
	{
		CloseHandle(this->pendingDelayedEvents->timer);
		delete this->pendingDelayedEvents->ev;
		EventContext *current = this->pendingDelayedEvents;
		this->pendingDelayedEvents = this->pendingDelayedEvents->next;
		delete current;
	}

	return this->lastEventResult;
}

BOOL EventLoop::ContinueRunning()
{
	// For the event loop to continue running previous events must have been processed without errors, and one of the following conditions must hold:
	// - there are events waiting to be executed in the APC queue, or
	// - we are _not_ in the host imposed shutdown mode and one of the following is true:
	// -- there are active event sources that may generate events (e.g. HTTP is listening for messages)
	// -- there are delayed events scheduled for the future 

	return this->lastEventResult == S_OK && 
		(this->pendingEventCount > 0 || 
			(!this->shutdownMode && (this->eventSourceCount > 0 || this->pendingDelayedEvents != NULL)));
}

HRESULT EventLoop::EnterShutdownMode()
{
	if (this->signalEvent)
	{
		this->shutdownMode = TRUE;
		SetEvent(this->signalEvent);
	}

	return S_OK;
}