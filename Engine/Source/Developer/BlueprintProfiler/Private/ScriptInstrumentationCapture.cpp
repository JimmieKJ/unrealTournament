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

//////////////////////////////////////////////////////////////////////////
// FInstrumentationCaptureContext

void FInstrumentationCaptureContext::UpdateContext(const UObject* InContextObject, TArray<FScriptInstrumentedEvent>& InstrumentationQueue)
{
	// Handle instance context switch
	if (ContextObject != InContextObject && InContextObject)
	{
		// Handle class context switch
		if (const UClass* NewClass = InContextObject->GetClass())
		{
			if (NewClass != ContextClass)
			{
				InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Class, NewClass->GetPathName()));
				ContextClass = NewClass;
			}
		}
		// Handle instance change
		InstrumentationQueue.Add(FScriptInstrumentedEvent(EScriptInstrumentation::Instance, InContextObject->GetPathName()));
		ContextObject = InContextObject;
	}
}

void FInstrumentationCaptureContext::ResetContext()
{
	ContextClass.Reset();
	ContextObject.Reset();
}