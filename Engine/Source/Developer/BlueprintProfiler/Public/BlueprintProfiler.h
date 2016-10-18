// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintProfilerModule.h"
#include "ScriptInstrumentationCapture.h"

#if WITH_EDITOR
#include "ScriptInstrumentationPlayback.h"
#endif

//////////////////////////////////////////////////////////////////////////
// FBlueprintProfiler

class FBlueprintProfiler : public IBlueprintProfilerInterface, public FTickableEditorObject
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
	virtual void InstrumentEvent(const FScriptInstrumentationSignal& Event) override;

#if WITH_EDITOR
	virtual void AddInstrumentedBlueprint(UBlueprint* InstrumentedBlueprint) override;
	virtual FOnBPStatGraphLayoutChanged& GetGraphLayoutChangedDelegate() { return GraphLayoutChangedDelegate; }
	virtual TSharedPtr<FBlueprintExecutionContext> GetBlueprintContext(const FString& BlueprintClassPath) override;
	virtual TSharedPtr<FScriptExecutionNode> GetProfilerDataForNode(const UEdGraphNode* GraphNode) override;
	virtual TSharedPtr<FScriptExecutionBlueprint> GetProfilerDataForBlueprint(const UBlueprint* Blueprint) override;
	virtual bool HasDataForInstance(const UObject* Instance) const override;
	virtual void ProcessEventProfilingData() override;
#endif
	// End IBlueprintProfilerModule

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT( FBlueprintProfiler, STATGROUP_Tickables ); }

private:

	/** Reset Profiling Data */
	virtual void ResetProfilingData();

	/** Registers delegates to recieve profiling events */
	void RegisterDelegates(bool bEnabled);

	/** Handles pie events to enable/disable profiling captures in the editor */
	void BeginPIE(bool bIsSimulating);
	void EndPIE(bool bIsSimulating);

#if WITH_EDITOR

	/** Removes all blueprint references from data */
	void RemoveAllBlueprintReferences(UBlueprint* Blueprint);

#endif

protected:

	/** Multcast delegate to notify of statistic structural changes for events like compilation */
	FOnBPStatGraphLayoutChanged GraphLayoutChangedDelegate;

	/** Current instrumentation context */
	FInstrumentationCaptureContext CaptureContext;
	/** Raw instrumentation data */
	TArray<FScriptInstrumentedEvent> InstrumentationEventQueue;

	/** Profiler active state */
	bool bProfilerActive;
	/** Profiling capture state */
	bool bProfilingCaptureActive;

#if WITH_EDITOR
	/** PIE Active */
	bool bPIEActive;
	/** Suspended script events, the key is the latent LinkId */
	TMap<FName, TMap<int32, TSharedPtr<FScriptEventPlayback>>> SuspendedEvents;
	/** Cached object path and code offset lookup to UObjects */
	TMap<FString, TSharedPtr<FBlueprintExecutionContext>> PathToBlueprintContext;
	/** The profiler connection factory */
	TSharedPtr<struct FGraphPanelPinConnectionFactory> ConnectionFactory;
#endif
	
};
