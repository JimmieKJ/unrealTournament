// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Script.h"
#include "BlueprintProfilerSupport.h"

//////////////////////////////////////////////////////////////////////////
// IBlueprintProfilerInterface

class IBlueprintProfilerInterface : public IModuleInterface
{
public:

	/** Returns true if the profiler is enabled */
	virtual bool IsProfilerEnabled() const { return false; }

	/** Returns true if the profiler is enabled and actively capturing events */
	virtual bool IsProfilingCaptureActive() const { return false; }

	/** Toggles the profiling event capture state */
	virtual void ToggleProfilingCapture() {}

	/** Returns the current profiling event data */
	virtual TArray<FBlueprintInstrumentedEvent>& GetProfilingData() = 0;

	/** Instruments and add a profiling event to the data */
	virtual void InstrumentEvent(const EScriptInstrumentationEvent& Event) {}

};
