// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"
#include "ScriptEventExecution.h"
#include "BPProfilerStatisticWidgets.h"

//////////////////////////////////////////////////////////////////////////
// FScriptExecEvent

FScriptExecEvent::FScriptExecEvent()
	: EventTime(0.0)
{
}

FScriptExecEvent::FScriptExecEvent(TWeakObjectPtr<const UObject> InObjectPtr, const FScriptExecEvent& ContextToCopy)
	: CallDepth(ContextToCopy.CallDepth)
	, NumExecOutPins(1)
	, EventType(EScriptInstrumentation::NodePin)
	, ScriptCodeOffset(-1)
	, EventTime(0.0)
	, ContextObject(InObjectPtr)
	, PinIndex(ContextToCopy.PinIndex)
{
}

FScriptExecEvent::FScriptExecEvent(TWeakObjectPtr<const UObject> InObjectPtr, TWeakObjectPtr<const UObject> InMacroObjectPtr, const FBlueprintInstrumentedEvent& InEvent, const int32 InPinIndex)
	: CallDepth(0)
	, NumExecOutPins(-1)
	, EventType(InEvent.GetType())
	, ScriptCodeOffset(InEvent.GetScriptCodeOffset())
	, EventTime(InEvent.GetTime())
	, ContextObject(InObjectPtr)
	, MacroContextObject(InMacroObjectPtr)
	, PinIndex(InPinIndex)
{
}

bool FScriptExecEvent::operator == (const FScriptExecEvent& ContextIn) const
{
	return EventType == ContextIn.EventType && ContextObject == ContextIn.ContextObject;
}

bool FScriptExecEvent::CanSubmitData() const
{
	return	EventType != EScriptInstrumentation::Class && 
			EventType != EScriptInstrumentation::Instance;
}

bool FScriptExecEvent::IsBranch()
{
	if (NumExecOutPins < 0)
	{
		NumExecOutPins = 0;
		// Cache num exec pins
		if (const UEdGraphNode* GraphNode = Cast<UEdGraphNode>(ContextObject.Get())) 
		{
			NumExecOutPins = 0;
			for (auto Pin : GraphNode->Pins)
			{
				if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					NumExecOutPins++;
				}
			}
		}
		else
		{
			// for non node contexts just flag that we tried to discover the pins.
			NumExecOutPins = 1;
		}
	}
	return NumExecOutPins > 1;
}

TWeakObjectPtr<const UObject> FScriptExecEvent::GetGraphPinPtr()
{
	TWeakObjectPtr<const UObject> Result;
	if (PinIndex >= 0)
	{
		if (const UEdGraphNode* Node = Cast<const UEdGraphNode>(ContextObject.Get()))
		{
			Result = Node->Pins[PinIndex];
		}
	}
	return Result;
}

void FScriptExecEvent::MergeContext(const FScriptExecEvent& TerminatorContext)
{
	EventTime = TerminatorContext.GetTime() - EventTime;
	PinIndex = TerminatorContext.PinIndex;
}

//////////////////////////////////////////////////////////////////////////
// FProfilerClassContext

void FProfilerClassContext::ProcessExecutionPath(const TArray<FBlueprintInstrumentedEvent>& ProfilerData, const int32 ExecStart, const int32 ExecEnd)
{
	// Process profiling events for a single execution path
	for (int32 Idx = ExecStart; Idx <= ExecEnd; ++Idx)
	{
		// Cursory check for end submission conditions
		switch(ProfilerData[Idx].GetType())
		{
			case EScriptInstrumentation::Class:
			{
				ContextStack.SetNum(0, false);
				ContextStack.Add(GenerateContextFromEvent(ProfilerData[Idx]));
				// Ensure the root stat exists for this blueprint.
				if (!ClassStat.IsValid())
				{
					CreateClassStat();
				}
				break;
			}
			case EScriptInstrumentation::Instance:
			{
				ContextStack.SetNum(1, false);
				ContextStack.Add(GenerateContextFromEvent(ProfilerData[Idx]));
				// Ensure the root stat exists for this blueprint.
				if (!ClassStat.IsValid())
				{
					CreateClassStat();
				}
				break;
			}
			case EScriptInstrumentation::Event:
			{
				ContextStack.SetNum(2, false);
				ContextStack.Add(GenerateContextFromEvent(ProfilerData[Idx]));
				break;
			}
			case EScriptInstrumentation::Stop:
			{
				ContextStack.Add(GenerateContextFromEvent(ProfilerData[Idx]));
				FixupEventId();
				RemoveDuplicateTraces();
				OptimiseEvents();
				SubmitData();
				return;
			}
			default:
			{
				// Generate the new execution event
				ContextStack.Add(GenerateContextFromEvent(ProfilerData[Idx]));
				break;
			}
		}
	}
}

void FProfilerClassContext::FixupEventId()
{
	FScriptExecEvent* EventContext = nullptr;
	FScriptExecEvent* NodeContext = nullptr;
	for (int32 Idx = 0; Idx < ContextStack.Num(); ++Idx)
	{
		FScriptExecEvent& CurrContext = ContextStack[Idx];
		if (!EventContext && CurrContext.IsEvent())
		{
			EventContext = &CurrContext;
		}
		else if (!NodeContext && CurrContext.IsNodeEntry())
		{
			NodeContext = &CurrContext;
			break;
		}
	}
	if (EventContext && NodeContext)
	{
		EventContext->SetObjectPtr(NodeContext->GetObjectPtr());
	}
}

void FProfilerClassContext::RemoveDuplicateTraces()
{
	// Remove duplicate traces and reset call depths
	for (int32 Idx = 0; (Idx+1) < ContextStack.Num(); ++Idx)
	{
		ContextStack[Idx].ResetCallDepth();
		if (ContextStack[Idx] == ContextStack[Idx+1])
		{
			ContextStack.RemoveAt(Idx, 1, false);
			Idx--;
		}
	}
}

void FProfilerClassContext::OptimiseEvents()
{
	// flatten call graphs updating depth
	for (int32 Idx = 0; Idx < ContextStack.Num(); ++Idx)
	{
		switch(ContextStack[Idx].GetType())
		{
			case EScriptInstrumentation::Class:
			case EScriptInstrumentation::Instance:
			{
				// Artificial events that provide a const offset for all node timings.
				for (int32 ChildIdx = Idx + 1; ChildIdx < ContextStack.Num(); ++ChildIdx)
				{
					ContextStack[ChildIdx].IncrementDepth();
				}
				break;
			}
			case EScriptInstrumentation::NodeExit:
			{
				TWeakObjectPtr<const UObject> ObjKey = ContextStack[Idx].GetObjectPtr();
				int32 PairedEntryIdx = Idx-1;
				// Find entry point
				for (; PairedEntryIdx >= 0; --PairedEntryIdx)
				{
					if (ContextStack[PairedEntryIdx].GetObjectPtr() == ObjKey)
					{
						break;
					}
					else
					{
						ContextStack[PairedEntryIdx].IncrementDepth();
					}
				}
				// Merge paired nodes and remove the exit node.
				if (PairedEntryIdx > 0)
				{
					ContextStack[PairedEntryIdx].MergeContext(ContextStack[Idx]);
					ContextStack.RemoveAt(Idx, 1, false);
					Idx--;
				}
				break;
			}
			case EScriptInstrumentation::Stop:
			{
				int32 PairedEntryIdx = Idx-1;
				// Find entry point
				for (; PairedEntryIdx >= 0; --PairedEntryIdx)
				{
					if (ContextStack[PairedEntryIdx].IsEvent())
					{
						break;
					}
					else
					{
						ContextStack[PairedEntryIdx].IncrementDepth();
					}
				}
				// Merge paired nodes and remove the exit node.
				if (PairedEntryIdx > 0)
				{
					ContextStack[PairedEntryIdx].MergeContext(ContextStack[Idx]);
					ContextStack.RemoveAt(Idx, 1, false);
					Idx--;
				}
				break;
			}
		}
	}
	// Interpret graph node event types, and handle branch depths
	for (int32 Idx = 0; Idx < ContextStack.Num(); ++Idx)
	{
		if (ContextStack[Idx].IsNodeEntry())
		{
			if (ContextStack[Idx].IsMacro())
			{
				// Collapse all macro nodes to just the instance for now.
				// Further work will include ensuring the BPGC has valid pins to lookup for macros.
				ContextStack[Idx].SetType(EScriptInstrumentation::Macro);
				int32 NumToRemove = 0;
				double MacroTiming = 0.0;
				// Merge node timings
				for (int32 MacroIdx = Idx; MacroIdx < ContextStack.Num(); ++MacroIdx)
				{
					if (ContextStack[Idx].GetMacroObjectPtr() != ContextStack[MacroIdx].GetMacroObjectPtr())
					{
						break;
					}
					NumToRemove++;
					MacroTiming += ContextStack[MacroIdx].GetTime();
				}
				if (NumToRemove > 1)
				{
					ContextStack[Idx].SetTime(MacroTiming);
					ContextStack.RemoveAt(Idx+1, NumToRemove, false);
				}
			}
			else if (ContextStack[Idx].IsFunctionCallSite())
			{
				ContextStack[Idx].SetType(EScriptInstrumentation::Function);
			}
			else if (ContextStack[Idx].IsBranch())
			{
				ContextStack[Idx].SetType(EScriptInstrumentation::Branch);
				const int32 BranchDepth = ContextStack[Idx].GetCallDepth();
				TWeakObjectPtr<const UObject> BranchNodePtr = ContextStack[Idx].GetObjectPtr();
				int32 NodeDepth = BranchDepth;
				// Decrement branch children depths
				for (int32 BranchChildIdx = Idx + 1; BranchChildIdx < ContextStack.Num() && ContextStack[BranchChildIdx].GetCallDepth() >= BranchDepth; ++BranchChildIdx)
				{
					if (ContextStack[BranchChildIdx].GetObjectPtr() != BranchNodePtr)
					{
						ContextStack[BranchChildIdx].IncrementDepth();
					}
					else
					{
						break;
					}
				}
			}
		}
	}
}

void FProfilerClassContext::CreateClassStat()
{
	if (!ClassStat.IsValid())
	{
		// Determine class from class or instance event.
		for (int32 Idx = 0; Idx < ContextStack.Num(); ++Idx)
		{
			if (ContextStack[Idx].IsClass())
			{
				FBPProfilerStatPtr Root;
				ClassStat = MakeShareable(new FBPProfilerStat(Root, ContextStack[Idx]));
				break;
			}
			else if (ContextStack[Idx].IsInstance())
			{
				if (const UObject* InstanceObject = ContextStack[Idx].GetObjectPtr().Get())
				{
					if (UClass* InstanceClass = InstanceObject->GetClass())
					{
						FBlueprintInstrumentedEvent FakeClassEvent(EScriptInstrumentation::Class, InstanceClass->GetPathName(), -1);
						FScriptExecEvent FakeExecContext = GenerateContextFromEvent(FakeClassEvent);
						FBPProfilerStatPtr Root;
						ClassStat = MakeShareable(new FBPProfilerStat(Root, FakeExecContext));
						break;
					}
				}
			}
		}
	}
}

void FProfilerClassContext::SubmitData()
{
	check(ClassStat.IsValid());
	if (ContextStack.Num())
	{
		// Submit profiling data now it is fully conformed.
		FBPProfilerStatPtr CurrentParentStat = ClassStat->FindEntryPoint(ContextStack[0]);
		if (CurrentParentStat.IsValid())
		{
			CurrentParentStat->SubmitData(ContextStack[0]);
			int32 CurrentCallDepth = ContextStack[0].GetCallDepth() + 1;
			FBPProfilerStatPtr CurrentStat;
			// Debug Extras
			FScriptExecEvent ParentContext = CurrentParentStat->GetObjectContext();

			for (int32 Idx = 1; Idx < ContextStack.Num(); ++Idx)
			{
				FScriptExecEvent& NextContext = ContextStack[Idx];
				// Adjust call depth if required
				if (NextContext.GetCallDepth() > CurrentCallDepth)
				{
					CurrentParentStat = CurrentStat;
					ParentContext = CurrentParentStat->GetObjectContext();
					CurrentCallDepth++;
				}
				else if(NextContext.GetCallDepth() < CurrentCallDepth)
				{
					while(NextContext.GetCallDepth() < CurrentCallDepth)
					{
						CurrentParentStat = CurrentParentStat->GetParent();
						ParentContext = CurrentParentStat->GetObjectContext();
						CurrentCallDepth = CurrentParentStat->GetCallDepth() + 1;
					}
				}
				// Submit Data
				const bool bBranchNode = NextContext.IsBranch();
				CurrentStat = CurrentParentStat->FindOrAddChildByContext(NextContext);
				check (CurrentStat.IsValid());
				CurrentStat->SubmitData(NextContext);
			}
			// Update stat data tree.
			ClassStat->UpdateProfilingStats();
		}
	}
}

FScriptExecEvent FProfilerClassContext::GenerateContextFromEvent(const FBlueprintInstrumentedEvent& Event)
{
	// Try the cache first, using the object path and script offset.
	const FString ObjectPath = FString::Printf( TEXT( "%s:%i" ), *Event.GetObjectPath(), Event.GetScriptCodeOffset());
	FEventObjectContext& Result = PathToObjectCache.FindOrAdd(ObjectPath);
	int32 DiscoveredPinIndex = -1;
	// Fall back to more exhaustive search, but this should only happen once per object.
	if (!Result.IsValid())
	{
		if (Event.IsNewClass())
		{
			// Attempt to locate the blueprint
			if (const UObject* ObjectPtr = FindObject<UObject>(nullptr, *Event.GetObjectPath()))
			{
				if (const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ObjectPtr))
				{
					if (const UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy))
					{
						Result.ContextObject = Blueprint;
					}
				}
			}
		}
		else if (Event.IsNewInstance())
		{
			// attempt to locate the instance
			if (const UObject* ObjectPtr = FindObject<UObject>(nullptr, *Event.GetObjectPath()))
			{
				// Get Outer world
				if (UWorld* ObjectWorld = ObjectPtr->GetTypedOuter<UWorld>())
				{
					if (ObjectWorld->WorldType == EWorldType::PIE)
					{
						FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
						if (UWorld* EditorWorld = EditorWorldContext.World())
						{
							for (auto LevelIter : EditorWorld->GetLevels())
							{
								if (UObject* EditorObject = FindObject<UObject>(LevelIter, *ObjectPtr->GetName()))
								{
									Result.ContextObject = EditorObject;
									break;
								}
							}
						}
					}
					else
					{
						Result.ContextObject = ObjectPtr;
					}
				}
			}
		}
		else
		{
			// Attempt to locate graph node
			TArray<FString> ObjectPaths;
			Event.GetObjectPath().ParseIntoArray(ObjectPaths, TEXT( ":" ));

			if (ObjectPaths.Num() > 0)
			{
				if (UBlueprintGeneratedClass* BlueprintClass = FindObject<UBlueprintGeneratedClass>(nullptr, *ObjectPaths[0]))
				{
					Result.BlueprintClass = BlueprintClass;
					if (UFunction* BlueprintFunction = BlueprintClass->FindFunctionByName(FName(*ObjectPaths.Last())))
					{
						Result.BlueprintFunction = BlueprintFunction;
						if (const UEdGraphNode* GraphNode = BlueprintClass->GetDebugData().FindSourceNodeFromCodeLocation(BlueprintFunction, Event.GetScriptCodeOffset(), true))
						{
							TArray<UEdGraphNode*> MacroInstanceNodes;
							BlueprintClass->GetDebugData().FindMacroInstanceNodesFromCodeLocation(BlueprintFunction, Event.GetScriptCodeOffset(), MacroInstanceNodes);
							// We need to do more work to ensure the executable path in macros can be followed so disabling for now.
							if (MacroInstanceNodes.Num())
							{
								Result.ContextObject = BlueprintClass->GetDebugData().FindMacroSourceNodeFromCodeLocation(BlueprintFunction, Event.GetScriptCodeOffset());
								Result.MacroContextObject = MacroInstanceNodes[0];
							}
							else
							{
								Result.ContextObject = GraphNode;
							}
						}
					}
				}
			}
		}
	}
	// Find exit pin index
	if (Result.DoesRequireExecPin())
	{
		if (UEdGraphPin* GraphPin = Result.BlueprintClass.Get()->GetDebugData().FindExecPinFromCodeLocation(Result.BlueprintFunction.Get(), Event.GetScriptCodeOffset()))
		{
			DiscoveredPinIndex = Result.GetGraphNode()->GetPinIndex(GraphPin);
		}
	}
	FScriptExecEvent NewContext(Result.ContextObject, Result.MacroContextObject, Event, DiscoveredPinIndex);
	return NewContext;
}
