// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintProfilerPCH.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "BlueprintProfiler"

//////////////////////////////////////////////////////////////////////////
// FBlueprintProfiler

IMPLEMENT_MODULE(FBlueprintProfiler, BlueprintProfiler);

DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_ProfilerTickCost, STATGROUP_BlueprintProfiler);
DECLARE_CYCLE_STAT(TEXT("Processing Events"), STAT_ProfilerInstrumentationCost, STATGROUP_BlueprintProfiler);
DECLARE_CYCLE_STAT(TEXT("Blueprint Lookup"), STAT_BlueprintLookupCost, STATGROUP_BlueprintProfiler);

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
	#if WITH_EDITOR
	SCOPE_CYCLE_COUNTER(STAT_ProfilerInstrumentationCost);

	const UObject* ContextObject = Event.GetContextObject();
	if (ContextObject && !ContextObject->HasAnyFlags(RF_Transient))
	#endif
	{
		// Handle context switching events
		CaptureContext.UpdateContext(Event.GetContextObject(), InstrumentationEventQueue);
		// Add instrumented event
		int32 ScriptCodeOffset = -1;
		FName CurrentFunctionName = Event.GetEventName();
		if (Event.IsStackFrameValid())
		{
			const FFrame& StackFrame = Event.GetStackFrame();
			ScriptCodeOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1;
			CurrentFunctionName = StackFrame.Node->GetFName();
		}
		InstrumentationEventQueue.Add(FScriptInstrumentedEvent(Event.GetType(), CurrentFunctionName, ScriptCodeOffset));
		// Reset context on event end
		if (Event.GetType() == EScriptInstrumentation::Stop)
		{
			CaptureContext.ResetContext();
		}
	}
}

#if WITH_EDITOR

TSharedPtr<FBlueprintExecutionContext> FBlueprintProfiler::GetBlueprintContext(const FString& BlueprintClassPath)
{
	SCOPE_CYCLE_COUNTER(STAT_BlueprintLookupCost);
	TSharedPtr<FBlueprintExecutionContext>& Result = PathToBlueprintContext.FindOrAdd(BlueprintClassPath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FBlueprintExecutionContext>(new FBlueprintExecutionContext);
		// Map the blueprint and initialise the context.
		if (Result->InitialiseContext(BlueprintClassPath))
		{
			// Register delegate to handle updates
			Result->GetBlueprint().Get()->OnCompiled().AddRaw(this, &FBlueprintProfiler::RemoveAllBlueprintReferences);
		}
		else
		{
			Result.Reset();
			PathToBlueprintContext.Remove(BlueprintClassPath);
		}
	}
	return Result;
}

TSharedPtr<FScriptExecutionNode> FBlueprintProfiler::GetProfilerDataForNode(const UEdGraphNode* GraphNode)
{
	TSharedPtr<FScriptExecutionNode> Result;
	if (GraphNode)
	{
		if (UBlueprint* Blueprint = GraphNode->GetTypedOuter<UBlueprint>())
		{
			if (TSharedPtr<FBlueprintExecutionContext>* BlueprintContext = PathToBlueprintContext.Find(Blueprint->GeneratedClass->GetPathName()))
			{
				Result = (*BlueprintContext)->GetProfilerDataForNode(GraphNode);
			}
		}
	}
	return Result;
}

bool FBlueprintProfiler::HasDataForInstance(const UObject* Instance) const
{
	bool bHasData = false;
	if (Instance)
	{
		if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Instance->GetClass()))
		{
			if (const TSharedPtr<FBlueprintExecutionContext>* Result = PathToBlueprintContext.Find(BlueprintClass->GetPathName()))
			{
				FName InstanceName(*Instance->GetPathName());
				bHasData = (*Result)->HasProfilerDataForInstance(InstanceName);
			}
		}
	}
	return bHasData;
}

void FBlueprintProfiler::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_ProfilerTickCost);
	ProcessEventProfilingData();
}

bool FBlueprintProfiler::IsTickable() const
{
	return InstrumentationEventQueue.Num() > 0;
}

void FBlueprintProfiler::ProcessEventProfilingData()
{
	struct FEventRange
	{
		FEventRange()
			: StartIdx(0)
			, StopIdx(0)
		{
		}

		bool IsRangeValid() const { return StopIdx > StartIdx; }

		TSharedPtr<FBlueprintExecutionContext> BlueprintContext;
		FName InstanceName;
		int32 StartIdx;
		int32 StopIdx;
	};
	TArray<FEventRange> ScriptEventRanges;
	TSet<TSharedPtr<FBlueprintExecutionContext>> DirtyContexts;
	// Iterate through the events and batch into single script executions
	for (int32 EventIdx = 0; EventIdx < InstrumentationEventQueue.Num(); ++EventIdx)
	{
		FScriptInstrumentedEvent& CurrEvent = InstrumentationEventQueue[EventIdx];
		switch (CurrEvent.GetType())
		{
			case EScriptInstrumentation::Class:
			{
				if ((EventIdx + 2) < InstrumentationEventQueue.Num())
				{
					const FScriptInstrumentedEvent& InstanceEvent = InstrumentationEventQueue[EventIdx+1];
					const FScriptInstrumentedEvent& Event = InstrumentationEventQueue[EventIdx+2];
					// Check if this is a new event and handle context switch
					if (InstanceEvent.IsNewInstance() && Event.IsEvent())
					{
						FEventRange NewEventRange;
						NewEventRange.BlueprintContext = GetBlueprintContext(CurrEvent.GetObjectPath());
						NewEventRange.InstanceName = MapBlueprintInstance(NewEventRange.BlueprintContext, InstanceEvent.GetObjectPath());
						NewEventRange.StartIdx = EventIdx;
						ScriptEventRanges.Push(NewEventRange);
					}
				}
				InstrumentationEventQueue.RemoveAt(EventIdx, 2, false);
				break;
			}
			case EScriptInstrumentation::Event:
			{
				// Nested events such as calls from event dispatchers.
				check(ScriptEventRanges.Num() > 0);
				FEventRange& LastEventRange = ScriptEventRanges.Last();
				FEventRange NewEventRange;
				NewEventRange.BlueprintContext = LastEventRange.BlueprintContext;
				NewEventRange.InstanceName = LastEventRange.InstanceName;
				NewEventRange.StartIdx = EventIdx;
				ScriptEventRanges.Push(NewEventRange);
				break;
			}
			case EScriptInstrumentation::Stop:
			{
				const int32 NumEventRanges = ScriptEventRanges.Num();
				if (ScriptEventRanges.Num() > 0)
				{
					FEventRange& EventRangeToProcess = ScriptEventRanges.Last();
					EventRangeToProcess.StopIdx = EventIdx;
					// Create and validate new event.
					if (EventRangeToProcess.IsRangeValid())
					{
						TSharedPtr<FScriptEventPlayback> EventToProcess;
						// Find any resumed contexts if required
						if (InstrumentationEventQueue[EventRangeToProcess.StartIdx].IsResumeEvent())
						{
							if (TSharedPtr<FScriptEventPlayback>* SuspendedEvent = SuspendedEvents.Find(EventRangeToProcess.InstanceName))
							{
								EventToProcess = *SuspendedEvent;
								SuspendedEvents.Remove(EventRangeToProcess.InstanceName);
							}
						}
						else
						{
							EventToProcess = (MakeShareable(new FScriptEventPlayback(EventRangeToProcess.BlueprintContext, EventRangeToProcess.InstanceName)));
						}
						check(EventToProcess.IsValid());
						if (EventToProcess->Process(InstrumentationEventQueue, EventRangeToProcess.StartIdx, EventRangeToProcess.StopIdx))
						{
							const int32 NumEventsToRemove = (EventRangeToProcess.StopIdx - EventRangeToProcess.StartIdx) + 1;
							InstrumentationEventQueue.RemoveAt(EventRangeToProcess.StartIdx, NumEventsToRemove, false);
							EventIdx -= NumEventsToRemove;
							DirtyContexts.Add(EventRangeToProcess.BlueprintContext);
							if (EventToProcess->IsSuspended())
							{
								SuspendedEvents.Add(EventRangeToProcess.InstanceName) = EventToProcess;
							}
						}
					}
					ScriptEventRanges.Pop();
				}
				break;
			}
		}
	}
	// Update dirty contexts
	for (auto Context : DirtyContexts)
	{
		Context->UpdateConnectedStats();
	}
}

void FBlueprintProfiler::RemoveAllBlueprintReferences(UBlueprint* Blueprint)
{
	if (Blueprint)
	{
		const FString BlueprintClassPath = Blueprint->GeneratedClass->GetPathName();
		if (PathToBlueprintContext.Contains(BlueprintClassPath))
		{
			// Tear down the blueprint context.
			PathToBlueprintContext.Remove(BlueprintClassPath);
			// Remove compilation hook, this will get added again if the blueprint is profiled
			Blueprint->OnCompiled().RemoveAll(this);
			// Broadcast changes to stats structure
			GraphLayoutChangedDelegate.Broadcast(Blueprint);
		}
	}
}

#endif // WITH_EDITOR

void FBlueprintProfiler::ResetProfilingData()
{
	InstrumentationEventQueue.Reset();
	CaptureContext.ResetContext();
#if WITH_EDITOR
	PathToBlueprintContext.Reset();
#endif // WITH_EDITOR
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

FName FBlueprintProfiler::MapBlueprintInstance(TSharedPtr<FBlueprintExecutionContext> BlueprintContext, const FString& InstancePath)
{
	FName InstanceName(*InstancePath);
	TWeakObjectPtr<const UObject> Instance = BlueprintContext->GetInstance(InstanceName);

	if (Instance.IsValid() && !BlueprintContext->HasProfilerDataForInstance(InstanceName))
	{
		// Create new instance node
		FScriptExecNodeParams InstanceNodeParams;
		InstanceNodeParams.NodeName = InstanceName;
		InstanceNodeParams.ObservedObject = Instance.Get();
		InstanceNodeParams.NodeFlags = EScriptExecutionNodeFlags::Instance;
		const AActor* Actor = Cast<AActor>(Instance.Get());
		InstanceNodeParams.DisplayName = Actor ? FText::FromString(Actor->GetActorLabel()) : FText::FromString(Instance.Get()->GetName());
		InstanceNodeParams.Tooltip = LOCTEXT("NavigateToInstanceHyperlink_ToolTip", "Navigate to the Instance");
		InstanceNodeParams.IconColor = FLinearColor(1.f, 1.f, 1.f, 0.8f);
		InstanceNodeParams.Icon = const_cast<FSlateBrush*>(FEditorStyle::GetBrush(TEXT("BlueprintProfiler.Actor")));
		TSharedPtr<FScriptExecutionInstance> InstanceNode = MakeShareable<FScriptExecutionInstance>(new FScriptExecutionInstance(InstanceNodeParams));
		// Link to parent blueprint entry
		TSharedPtr<FScriptExecutionBlueprint> BlueprintNode = BlueprintContext->GetBlueprintExecNode();
		BlueprintNode->AddInstance(InstanceNode);
		InstanceNode->SetParentNode(BlueprintNode);
		// Fill out events from the blueprint root node
		for (int32 NodeIdx = 0; NodeIdx < BlueprintNode->GetNumChildren(); ++NodeIdx)
		{
			InstanceNode->AddChildNode(BlueprintNode->GetChildByIndex(NodeIdx));
		}
	}
	return InstanceName;
}

#undef LOCTEXT_NAMESPACE
