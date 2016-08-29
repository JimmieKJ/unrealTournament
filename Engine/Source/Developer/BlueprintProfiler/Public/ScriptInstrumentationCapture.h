// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Public/Profiler/TracePath.h"

//////////////////////////////////////////////////////////////////////////
// FScriptInstrumentedEvent

class FScriptInstrumentedEvent
{
public:

	FScriptInstrumentedEvent() {}

	FScriptInstrumentedEvent(EScriptInstrumentation::Type InEventType, const FName InFunctionClassScopeName, const FString& InPathData)
		: EventType(InEventType)
		, FunctionClassScopeName(InFunctionClassScopeName)
		, PathData(InPathData)
		, CodeOffset(-1)
		, Time(FPlatformTime::Seconds())
	{
	}

	FScriptInstrumentedEvent(EScriptInstrumentation::Type InEventType, const FName InFunctionClassScopeName, const FName InFunctionName, const int32 InCodeOffset = INDEX_NONE)
		: EventType(InEventType)
		, FunctionClassScopeName(InFunctionClassScopeName)
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
	const FString& GetBlueprintClassPath() const { return PathData; }

	/** Returns the object path for the object that triggered this event */
	const FString& GetInstancePath() const { return PathData; }

	/** Returns the function name the signal was emitted from */
	FName GetFunctionName() const { return FunctionName; }

	/** Returns the scoped function name signal was emitted from */
	FName GetScopedFunctionName() const;

	/** Returns the function scope name the signal was emitted from */
	FName GetFunctionClassScopeName() const { return FunctionClassScopeName; }

	/** Returns the script offset at the time this event was generated */
	int32 GetScriptCodeOffset() const { return CodeOffset; }

	/** Returns the system time when this event was generated */
	double GetTime() const { return Time; }

	/** Overrides the system time for this event */
	void OverrideTime(const double OverrideTime) { Time = OverrideTime; }

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

	/** Returns if this event represents a node entry event */
	bool IsNodeEntry() const { return EventType == EScriptInstrumentation::NodeEntry || EventType == EScriptInstrumentation::NodeDebugSite; }

	/** Returns if this event represents a node exit event */
	bool IsNodeExit() const { return EventType == EScriptInstrumentation::NodeExit; }

protected:
	
	/** The event type */
	EScriptInstrumentation::Type EventType;
	/** The function class scope name, this indicates the owning class of the current function */
	FName FunctionClassScopeName;
	/** The owning function name */
	FName FunctionName;
	/** The object / blueprint class path for the object emitting the event */
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
	void UpdateContext(const FScriptInstrumentationSignal& InstrumentSignal, TArray<FScriptInstrumentedEvent>& InstrumentationQueue);

	/** Reset the current event context */
	void ResetContext();

private:

	/** The current class/blueprint context */
	TWeakObjectPtr<const UClass> ContextClass;
	/** The current function class scope */
	TWeakObjectPtr<const UClass> ContextFunctionClassScope;
	/** The current instance context */
	TWeakObjectPtr<const UObject> ContextObject;
	/** The current event context */
	FName ActiveEvent;
};
