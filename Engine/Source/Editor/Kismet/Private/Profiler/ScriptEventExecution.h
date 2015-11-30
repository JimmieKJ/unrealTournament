// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

typedef TSharedPtr<class FBPProfilerStat> FBPProfilerStatPtr;
typedef TMap<TWeakObjectPtr<const UObject>, FBPProfilerStatPtr> BPProfilerWidgetCache;

//////////////////////////////////////////////////////////////////////////
// FScriptExecEvent

class FScriptExecEvent
{
public:

	FScriptExecEvent();
	FScriptExecEvent(TWeakObjectPtr<const UObject> InObjectPtr, const FScriptExecEvent& ContextToCopy);
	FScriptExecEvent(TWeakObjectPtr<const UObject> InObjectPtr, TWeakObjectPtr<const UObject> InMacroObjectPtr, const class FBlueprintInstrumentedEvent& InEvent, const int32 InPinIndex);

	bool operator == (const FScriptExecEvent& ContextIn) const;

	bool IsValid() const { return ContextObject.IsValid(); }

	/** Override the event object */
	void SetObjectPtr(TWeakObjectPtr<const UObject> InObjPtr) { ContextObject = InObjPtr; }

	/** Returns the event object */
	TWeakObjectPtr<const UObject> GetObjectPtr() { return ContextObject; }

	/** Returns the macro event object */
	TWeakObjectPtr<const UObject> GetMacroObjectPtr() { return MacroContextObject; }

	/** Returns the context object as the template type */
	template<typename UserType> TWeakObjectPtr<const UserType> GetTypedObjectPtr()
	{
		return TWeakObjectPtr<const UserType>(Cast<const UserType>(ContextObject.Get()));
	}

	/** Returns the graph pin the execution path used */
	TWeakObjectPtr<const UObject> GetGraphPinPtr();

	/** Returns the profiling event type */
	const EScriptInstrumentation::Type GetType() const { return EventType; }

	/** Overrides the profiling event type */
	void SetType(const EScriptInstrumentation::Type NewEventType) { EventType = NewEventType; }

	/** Returns the execution duration for this execution event */
	double GetTime() const { return EventTime; }

	/** Overrides the timing stat */
	void SetTime(double NewTime) { EventTime = NewTime; }

	/** Returns the call depth of this execution event */
	int32 GetCallDepth() const { return CallDepth; }

	/** Resets the call depth of this execution event */
	void ResetCallDepth() { CallDepth = 0; }

	/** Increments the call depth of this execution event */
	void IncrementDepth() { CallDepth++; }

	/** Merge start/end node events and calculate the elapsed time */
	void MergeContext(const FScriptExecEvent& TerminatorContext);

	/** Returns if this exec context requires event timings */
	bool CanSubmitData() const;

	/** Returns if this exec event represents a change in class/blueprint */
	bool IsClass() const { return EventType == EScriptInstrumentation::Class; }

	/** Returns if this exec event represents a change in instance */
	bool IsInstance() const { return EventType == EScriptInstrumentation::Instance; }

	/** Returns if this exec event represents the start of an event execution path */
	bool IsEvent() const { return EventType == EScriptInstrumentation::Event; }

	/** Returns if this event is a function callsite event */
	bool IsFunctionCallSite() const { return IsValid() ? ContextObject.Get()->IsA<UK2Node_CallFunction>() : false; }

	/** Returns if this event happened inside a macro instance */
	bool IsMacro() const { return MacroContextObject.IsValid(); }

	/** Returns if this event potentially multiple exit sites */
	bool IsBranch();

	/** Returns if this event signals the start of a node timing. */
	bool IsNodeEntry() const { return EventType == EScriptInstrumentation::NodeEntry; }

	/** Returns if this event signals the end of a node timing. */
	bool IsNodeExit() const { return EventType == EScriptInstrumentation::NodeExit; }

protected:

	/** Calculated call depth */
	int32 CallDepth;
	/** Discovered exec out pin count */
	int32 NumExecOutPins;
	/** Exec event type */
	EScriptInstrumentation::Type EventType;
	/** Script code offset at event */
	int32 ScriptCodeOffset;
	/** System time event occurred */
	double EventTime;
	/** Context object */
	TWeakObjectPtr<const UObject> ContextObject;
	/** Macro Context object */
	TWeakObjectPtr<const UObject> MacroContextObject;
	/** Exit pin index */
	int32 PinIndex;

};

//////////////////////////////////////////////////////////////////////////
// FEventObjectContext

struct FEventObjectContext
{
	bool IsValid() const { return ContextObject.IsValid(); }
	bool DoesRequireExecPin() const { return BlueprintClass.IsValid() && BlueprintClass.IsValid(); }
	const class UEdGraphNode* GetGraphNode() const { return ContextObject.IsValid() ? Cast<UEdGraphNode>(ContextObject.Get()) : nullptr; }

	TWeakObjectPtr<const UObject> ContextObject;
	TWeakObjectPtr<const UObject> MacroContextObject;
	TWeakObjectPtr<UBlueprintGeneratedClass> BlueprintClass;
	TWeakObjectPtr<UFunction> BlueprintFunction;
};

//////////////////////////////////////////////////////////////////////////
// FProfilerClassContext

class FProfilerClassContext
{
public:

	/** Returns that widget that represents the class blueprint */
	FBPProfilerStatPtr GetRootStat() { return ClassStat; }

	/** This analyses, conforms and submits profiling data */
	void ProcessExecutionPath(const TArray<FBlueprintInstrumentedEvent>& ProfilerData, const int32 ExecStart, const int32 ExecEnd);

private:

	/** Creates the root statisic for this class */
	void CreateClassStat();

	/** Use the current event stack to populate the event marker with a valid object */
	void FixupEventId();

	/** Remove duplicate entry profiling markers so the data is conformed enough to process */
	void RemoveDuplicateTraces();

	/** Merge node exit/entries into single events, Sort call depths of profiling events */
	void OptimiseEvents();

	/** Submit the conformed profiling data to the widget structure for display */
	void SubmitData();

	/** Create FScriptExecEvent from profiling event data and locate and cache the contextual UObject */
	FScriptExecEvent GenerateContextFromEvent(const FBlueprintInstrumentedEvent& InEvent);

private:

	/** Root blueprint stat, representing the root of all execution flows in this class */
	FBPProfilerStatPtr ClassStat;
	/** Stack of processed events ready for submission */
	TArray<FScriptExecEvent> ContextStack;
	/** Cached object path and code offset lookup to UObjects */
	TMap<const FString, struct FEventObjectContext> PathToObjectCache;
};
