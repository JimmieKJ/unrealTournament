// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Public/Profiler/TracePath.h"

//////////////////////////////////////////////////////////////////////////
// FScriptInstrumentedEvent

class FScriptInstrumentedEvent
{
public:

	FScriptInstrumentedEvent() {}

	bool operator == (const FScriptInstrumentedEvent& OtherIn) const;

	FScriptInstrumentedEvent(EScriptInstrumentation::Type InEventType, const FString& InPathData)
		: EventType(InEventType)
		, PathData(InPathData)
		, CodeOffset(-1)
		, Time(FPlatformTime::Seconds())
	{
	}

	FScriptInstrumentedEvent(EScriptInstrumentation::Type InEventType, const FName InFunctionName, const int32 InCodeOffset)
		: EventType(InEventType)
		, FunctionName(InFunctionName)
		, CodeOffset(InCodeOffset)
		, Time(FPlatformTime::Seconds())
	{
	}

	/** Set's the event data */
	void SetData(EScriptInstrumentation::Type InEventType, const FString& InPathData, const int32 InCodeOffset);

	/** Returns the type of script event */
	const EScriptInstrumentation::Type GetType() const { return EventType; }

	/** Override the event type */
	void OverrideType(const EScriptInstrumentation::Type NewType) { EventType = NewType; }

	/** Returns the object path for the object that triggered this event */
	const FString& GetObjectPath() const { return PathData; }

	/** Returns the function name the event occurred in */
	FName GetFunctionName() const { return FunctionName; }

	/** Returns the script offset at the time this event was generated */
	int32 GetScriptCodeOffset() const { return CodeOffset; }

	/** Returns the system time when this event was generated */
	double GetTime() const { return Time; }

	/** Returns if this event represents a change in active class/blueprint */
	bool IsNewClass() const { return EventType == EScriptInstrumentation::Class; }

	/** Returns if this event represents a change in active instance */
	bool IsNewInstance() const { return EventType == EScriptInstrumentation::Instance; }

	/** Returns if this event represents a change in active event */
	bool IsEvent() const { return EventType == EScriptInstrumentation::Event||EventType == EScriptInstrumentation::ResumeEvent; }

	/** Returns if this event represents a active event resuming execution */
	bool IsResumeEvent() const { return EventType == EScriptInstrumentation::ResumeEvent; }

	/** Returns if this event represents a node execution event */
	bool IsNodeTiming() const {	return EventType == EScriptInstrumentation::NodeEntry || EventType == EScriptInstrumentation::NodeExit;	}

protected:
	
	/** The event type */
	EScriptInstrumentation::Type EventType;
	/** The owning function name */
	FName FunctionName;
	/** The object path for the object emitting the event */
	FString PathData;
	/** The script code offset when this event was emitted */
	int32 CodeOffset;
	/** The recorded system time when this event was emitted */
	double Time;
};

//////////////////////////////////////////////////////////////////////////
// FInstrumentationCaptureContext

class FInstrumentationCaptureContext
{
public:

	/** Update the current execution context, and emit events to signal changes */
	void UpdateContext(const UObject* InContextObject, TArray<FScriptInstrumentedEvent>& InstrumentationQueue);

	/** Reset the current event context */
	void ResetContext();

private:

	/** The current class/blueprint context */
	TWeakObjectPtr<const UClass> ContextClass;
	/** The current instance context */
	TWeakObjectPtr<const UObject> ContextObject;
};
