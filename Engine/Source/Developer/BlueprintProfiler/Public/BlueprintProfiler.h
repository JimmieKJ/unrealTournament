// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintProfilerModule.h"

//////////////////////////////////////////////////////////////////////////
// FBlueprintProfiler

class FBlueprintProfiler : public IBlueprintProfilerInterface
{
public:

	FBlueprintProfiler();
	virtual ~FBlueprintProfiler();

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	// Begin IBlueprintProfilerModule
	virtual bool IsProfilerEnabled() const override { return bProfilerActive; }
	virtual bool IsProfilingCaptureActive() const { return bProfilingCaptureActive; }
	virtual void ToggleProfilingCapture() override;
	virtual TArray<FBlueprintInstrumentedEvent>& GetProfilingData() override { return ProfilingData; }
	virtual void InstrumentEvent(const EScriptInstrumentationEvent& Event) override;
	// End IBlueprintProfilerModule

private:

	/** Reset Profiling Data */
	virtual void ResetProfilingData();

	/** Registers delegates to recieve profiling events */
	void RegisterDelegates(bool bEnabled);

	/** Handles pie events to enable/disable profiling captures in the editor */
	void BeginPIE(bool bIsSimulating);
	void EndPIE(bool bIsSimulating);

	/** Utility functon to extract script code offset safely */
	int32 GetScriptCodeOffset(const EScriptInstrumentationEvent& Event) const;

	/** Utility functon to extract object path safely */
	FString GetCurrentFunctionPath(const EScriptInstrumentationEvent& Event) const;

protected:

	/** Profiler active state */
	bool bProfilerActive;
	/** Profiling capture state */
	bool bProfilingCaptureActive;
	/** PIE Active */
	bool bPIEActive;
	/** Active Object */
	FKismetInstrumentContext ActiveContext;
	/** Profiling data */
	TArray<FBlueprintInstrumentedEvent> ProfilingData;
	
};