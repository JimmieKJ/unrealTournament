// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Private/BlueprintProfilerPCH.h"

#include "BlueprintProfiler.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Script.h"
#endif

//////////////////////////////////////////////////////////////////////////
// FBlueprintProfiler

IMPLEMENT_MODULE(FBlueprintProfiler, BlueprintProfiler);

FBlueprintProfiler::FBlueprintProfiler()
	: bProfilerActive(false)
	, bProfilingCaptureActive(false)
	, bPIEActive(false)
{
}

FBlueprintProfiler::~FBlueprintProfiler()
{
	if (bProfilingCaptureActive)
	{
		ToggleProfilingCapture();
	}
}

void FBlueprintProfiler::StartupModule()
{
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddRaw(this, &FBlueprintProfiler::RegisterDelegates);
}

void FBlueprintProfiler::ShutdownModule()
{
	FBlueprintCoreDelegates::OnToggleScriptProfiler.RemoveAll(this);
}

void FBlueprintProfiler::ToggleProfilingCapture()
{
	// Toggle profiler state
	bProfilerActive = !bProfilerActive;
#if WITH_EDITOR
	bProfilingCaptureActive = bProfilerActive && bPIEActive;
#else
	bProfilingCaptureActive = bProfilerActive;
#endif // WITH_EDITOR
	// Broadcast capture state change so delegate consumers can update their state.
	FBlueprintCoreDelegates::OnToggleScriptProfiler.Broadcast(bProfilerActive);
}

void FBlueprintProfiler::InstrumentEvent(const EScriptInstrumentationEvent& Event)
{
//	if (bProfilingCaptureActive)
	{
		// Handle context switching events
		ActiveContext.UpdateContext(Event.GetContextObject(), ProfilingData);
		// Add instrumented event
		const int32 ScriptCodeOffset = GetScriptCodeOffset(Event);
		const FString CurrentFunctionPath = GetCurrentFunctionPath(Event);
		ProfilingData.Add(FBlueprintInstrumentedEvent(Event.GetType(), CurrentFunctionPath, ScriptCodeOffset));
	}
}

void FBlueprintProfiler::ResetProfilingData()
{
	ProfilingData.Reset();
	ActiveContext.ResetContext();
}

void FBlueprintProfiler::RegisterDelegates(bool bEnabled)
{
	if (bEnabled)
	{
#if WITH_EDITOR
		// Register for PIE begin and end events in the editor
		FEditorDelegates::BeginPIE.AddRaw(this, &FBlueprintProfiler::BeginPIE);
		FEditorDelegates::EndPIE.AddRaw(this, &FBlueprintProfiler::EndPIE);
#endif // WITH_EDITOR
		ResetProfilingData();
		// Start consuming profiling events for capture
		FBlueprintCoreDelegates::OnScriptProfilingEvent.AddRaw(this, &FBlueprintProfiler::InstrumentEvent);
	}
	else
	{
#if WITH_EDITOR
		// Unregister for PIE begin and end events in the editor
		FEditorDelegates::BeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);
#endif // WITH_EDITOR
		ResetProfilingData();
		// Stop consuming profiling events for capture
		FBlueprintCoreDelegates::OnScriptProfilingEvent.RemoveAll(this);
	}
}

void FBlueprintProfiler::BeginPIE(bool bIsSimulating)
{
#if WITH_EDITOR
	bPIEActive = true;
	ResetProfilingData();
	bProfilingCaptureActive = bProfilerActive && bPIEActive;
#endif // WITH_EDITOR
}

void FBlueprintProfiler::EndPIE(bool bIsSimulating)
{
#if WITH_EDITOR
	bPIEActive = false;
	bProfilingCaptureActive = bProfilerActive && bPIEActive;
#endif // WITH_EDITOR
}

int32 FBlueprintProfiler::GetScriptCodeOffset(const EScriptInstrumentationEvent& Event) const
{
	int32 ScriptCodeOffset = -1;
	if (Event.IsStackFrameValid())
	{
		const FFrame& StackFrame = Event.GetStackFrame();
		ScriptCodeOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1;
	}
	return ScriptCodeOffset;
}

FString FBlueprintProfiler::GetCurrentFunctionPath(const EScriptInstrumentationEvent& Event) const
{
	FString CurrentFunctionPath;
	if (Event.IsStackFrameValid())
	{
		const FFrame& StackFrame = Event.GetStackFrame();
		CurrentFunctionPath = StackFrame.Node->GetPathName();
	}
	return CurrentFunctionPath;
}
