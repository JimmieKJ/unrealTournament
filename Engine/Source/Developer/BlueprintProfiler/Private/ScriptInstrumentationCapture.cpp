// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintProfilerPCH.h"

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

void FInstrumentationCaptureContext::UpdateContext(const FScriptInstrumentationSignal& InstrumentSignal, TArray<FScriptInstrumentedEvent>& InstrumentationQueue)
{
	// Handle instance context switch
	if (const UObject* NewContextObject = InstrumentSignal.GetContextObject())
	{
		if (NewContextObject != ContextObject)
		{
			// Handle class context switch
			if (const UClass* NewClass = InstrumentSignal.GetClass())
			{
				if (NewClass != ContextClass)
				{
					InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Class, InstrumentSignal.GetFunctionClassScope()->GetFName(), NewClass->GetPathName()));
					ContextClass = NewClass;
				}
			}
			// Handle instance change
			InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Instance, ContextClass->GetFName(), NewContextObject->GetPathName()));
			ContextObject = NewContextObject;
		}
		else
		{
			// Handle function class scope changes
			if (ContextFunctionClassScope != InstrumentSignal.GetFunctionClassScope())
			{
				ContextFunctionClassScope = InstrumentSignal.GetFunctionClassScope();
				InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::ClassScope, ContextFunctionClassScope->GetFName(), InstrumentSignal.GetFunctionName()));
			}
		}
	}
}

void FInstrumentationCaptureContext::ResetContext()
{
	ContextClass.Reset();
	ContextFunctionClassScope.Reset();
	ContextObject.Reset();
}