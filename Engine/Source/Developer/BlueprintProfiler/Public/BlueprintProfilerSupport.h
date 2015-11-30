// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "Core.h"
#include "Script.h"

//////////////////////////////////////////////////////////////////////////
// FBlueprintInstrumentedEvent

class FBlueprintInstrumentedEvent
{
public:

	FBlueprintInstrumentedEvent() {}

	/** Constructs a new script event */
	FBlueprintInstrumentedEvent(EScriptInstrumentation::Type InEventType, const FString& InPathData, const int32 InCodeOffset)
		: EventType(InEventType)
		, PathData(InPathData)
		, CodeOffset(InCodeOffset)
		, Time(FPlatformTime::Seconds())
	{
	}

	/** Set's the event data */
	void SetData(EScriptInstrumentation::Type InEventType, const FString& InPathData, const int32 InCodeOffset);

	/** Returns the type of script event */
	const EScriptInstrumentation::Type GetType() const { return EventType; }

	/** Returns the object path for the object that triggered this event */
	const FString& GetObjectPath() const { return PathData; }

	/** Returns the script offset at the time this event was generated */
	int32 GetScriptCodeOffset() const { return CodeOffset; }

	/** Returns the system time when this event was generated */
	double GetTime() const { return Time; }

	/** Returns if this event represents a change in active class/blueprint */
	bool IsNewClass() const { return EventType == EScriptInstrumentation::Class; }

	/** Returns if this event represents a change in active instance */
	bool IsNewInstance() const { return EventType == EScriptInstrumentation::Instance; }

	/** Returns if this event represents a node execution event */
	bool IsNodeTiming() const {	return EventType == EScriptInstrumentation::NodeEntry || EventType == EScriptInstrumentation::NodeExit;	}

protected:
	
	/** The event type */
	EScriptInstrumentation::Type EventType;
	/** The object path for the object emitting the event */
	FString PathData;
	/** The script code offset when this event was emitted */
	int32 CodeOffset;
	/** The recorded system time when this event was emitted */
	double Time;
};

//////////////////////////////////////////////////////////////////////////
// FKismetInstrumentContext

class FKismetInstrumentContext
{
public:

	/** Update the current execution context, and emit events to signal changes */
	void UpdateContext(const UObject* InContextObject, TArray<FBlueprintInstrumentedEvent>& ProfilingData);

	/** Reset the current event context */
	void ResetContext();

private:

	/** The current class/blueprint context */
	TWeakObjectPtr<const UClass> ContextClass;
	/** The current instance context */
	TWeakObjectPtr<const UObject> ContextObject;
};
