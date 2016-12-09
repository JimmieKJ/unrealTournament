// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ScriptInstrumentationCapture.h"

//////////////////////////////////////////////////////////////////////////
// FScriptInstrumentedEvent

void FScriptInstrumentedEvent::SetData(EScriptInstrumentation::Type InEventType, const FString& InPathData, const int32 InCodeOffset)
{
	EventType = InEventType;
	PathData = InPathData;
	CodeOffset = InCodeOffset;
	Time = FPlatformTime::Seconds();
}

FName FScriptInstrumentedEvent::GetScopedFunctionName() const
{
	return FName(*FString::Printf(TEXT("%s::%s"), *FunctionClassScopeName.ToString(), *FunctionName.ToString()));
}

//////////////////////////////////////////////////////////////////////////
// FInstrumentationCaptureContext

bool FInstrumentationCaptureContext::UpdateContext(const FScriptInstrumentationSignal& InstrumentSignal, TArray<FScriptInstrumentedEvent>& InstrumentationQueue)
{
	enum EScopeChange
	{
		None,
		Push,
		Pop
	};

	if (InstrumentSignal.GetType() == EScriptInstrumentation::InlineEvent)
	{
		// This inline event needs to be processed using the next incoming event's function class scope,
		// so we need to store the data until the following UpdateContext call
		PendingInlineEventName = InstrumentSignal.GetFunctionName();
		return false;
	}
	else if (InstrumentSignal.GetType() != EScriptInstrumentation::Stop)
	{
		FContextScope SignalScope;
		SignalScope.ContextObject = InstrumentSignal.GetContextObject();
		SignalScope.ContextClass = InstrumentSignal.GetClass();
		SignalScope.ContextFunctionClassScope = InstrumentSignal.GetFunctionClassScope();

		const FContextScope& CurrentScope = ScopeStack.Num() > 0 ? ScopeStack.Last() : SignalScope;
	
		// Always push a new scope for an event
		EScopeChange ScopeChange = PendingInlineEventName != NAME_None ? EScopeChange::Push : EScopeChange::None;
		bool bIsStackEmpty = ScopeStack.Num() == 0;
		if (bIsStackEmpty || SignalScope.ContextObject != CurrentScope.ContextObject)
		{
			if (bIsStackEmpty || SignalScope.ContextClass != CurrentScope.ContextClass)
			{
				InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Class, SignalScope.ContextClass->GetFName(), InstrumentSignal.GetFunctionName(), SignalScope.ContextClass->GetPathName()));
			}

			InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Instance, SignalScope.ContextClass->GetFName(), InstrumentSignal.GetFunctionName(), SignalScope.ContextObject->GetPathName()));
			if (InstrumentSignal.GetType() == EScriptInstrumentation::PopState)
			{
				// An Instance change for a PopState signal indicates that we are returning to a previous scope
				ScopeChange = EScopeChange::Pop;
			}
			else
			{
				ScopeChange = EScopeChange::Push;
			}
		}
		else if (SignalScope.ContextFunctionClassScope != CurrentScope.ContextFunctionClassScope)
		{
			if (InstrumentSignal.GetType() != EScriptInstrumentation::Event && PendingInlineEventName == NAME_None)
			{
				InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::ClassScope, SignalScope.ContextFunctionClassScope->GetFName(), InstrumentSignal.GetFunctionName()));
			}
			ScopeChange = EScopeChange::Push;
		}

		switch (ScopeChange)
		{
			case EScopeChange::Push:
			{
				ScopeStack.Push(SignalScope);
			}
			break;
			case EScopeChange::Pop:
			{
				PopScope();
			}
			break;
		}

		if (PendingInlineEventName != NAME_None)
		{
			// The pending inline event needs to be added first to make sure our events are processed in the proper order.
			InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Event, InstrumentSignal.GetFunctionClassScope()->GetFName(), PendingInlineEventName));
			PendingInlineEventName = NAME_None;
		}
	}
	
	return true;
}

void FInstrumentationCaptureContext::ResetContext()
{
	ScopeStack.Empty();
}

void FInstrumentationCaptureContext::PopScope()
{
	if (ScopeStack.Num() > 0)
	{
		ScopeStack.Pop();
	}
}
