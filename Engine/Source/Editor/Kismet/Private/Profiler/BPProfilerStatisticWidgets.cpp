// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SBlueprintProfilerView.h"
#include "BPProfilerStatisticWidgets.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerViewTypesUI"

//////////////////////////////////////////////////////////////////////////
// BlueprintProfilerStatText

namespace BlueprintProfilerStatText
{
	const FName ColumnId_Name( "Name" );
	const FName ColumnId_InclusiveTime( "InclusiveTime" );
	const FName ColumnId_ExclusiveTime( "ExclusiveTime" );
	const FName ColumnId_MaxTime( "MaxTime" );
	const FName ColumnId_MinTime( "MinTime" );
	const FName ColumnId_Samples( "Samples" );
	const FName ColumnId_TotalTime( "TotalTime" );

	const FText ColumnText_Name( NSLOCTEXT("BlueprintProfilerViewUI", "Name", "Name") );
	const FText ColumnText_InclusiveTime( NSLOCTEXT("BlueprintProfilerViewUI", "InclusiveTime", "Inclusive Time (ms)") );
	const FText ColumnText_ExclusiveTime( NSLOCTEXT("BlueprintProfilerViewUI", "ExclusiveTime", "Exclusive Time (ms)") );
	const FText ColumnText_MaxTime( NSLOCTEXT("BlueprintProfilerViewUI", "MaxTime", "Max Time (ms)") );
	const FText ColumnText_MinTime( NSLOCTEXT("BlueprintProfilerViewUI", "MinTime", "Min Time (ms)") );
	const FText ColumnText_Samples( NSLOCTEXT("BlueprintProfilerViewUI", "Samples", "Samples") );
	const FText ColumnText_TotalTime( NSLOCTEXT("BlueprintProfilerViewUI", "TotalTime", "Total Time (s)") );
};

//////////////////////////////////////////////////////////////////////////
// SProfilerStatRow

TSharedRef<SWidget> SProfilerStatRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if (ColumnName == BlueprintProfilerStatText::ColumnId_Name)
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				ItemToEdit->GenerateNameWidget()
			];
	}
	else
	{
		return SNew(STextBlock)
			.Text(ItemToEdit->GetTextAttribute(ColumnName));
	}
}

void SProfilerStatRow::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FBPProfilerStatPtr InItemToEdit)
{
	check(InItemToEdit.IsValid());
	ItemToEdit = InItemToEdit;
	SMultiColumnTableRow<FBPProfilerStatPtr>::Construct( FSuperRowType::FArguments(), OwnerTableView );	
}

const FName SProfilerStatRow::GetStatName(const EBlueprintProfilerStat::Type StatId)
{
	switch(StatId)
	{
		case EBlueprintProfilerStat::Name:			return BlueprintProfilerStatText::ColumnId_Name;
		case EBlueprintProfilerStat::TotalTime:		return BlueprintProfilerStatText::ColumnId_TotalTime;
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnId_InclusiveTime;
		case EBlueprintProfilerStat::ExclusiveTime:	return BlueprintProfilerStatText::ColumnId_ExclusiveTime;
		case EBlueprintProfilerStat::MaxTime:		return BlueprintProfilerStatText::ColumnId_MaxTime;
		case EBlueprintProfilerStat::MinTime:		return BlueprintProfilerStatText::ColumnId_MinTime;
		case EBlueprintProfilerStat::Samples:		return BlueprintProfilerStatText::ColumnId_Samples;
		default:									return NAME_None;
	}
}

const FText SProfilerStatRow::GetStatText(const EBlueprintProfilerStat::Type StatId)
{
	switch(StatId)
	{
		case EBlueprintProfilerStat::Name:			return BlueprintProfilerStatText::ColumnText_Name;
		case EBlueprintProfilerStat::TotalTime:		return BlueprintProfilerStatText::ColumnText_TotalTime;
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnText_InclusiveTime;
		case EBlueprintProfilerStat::ExclusiveTime:	return BlueprintProfilerStatText::ColumnText_ExclusiveTime;
		case EBlueprintProfilerStat::MaxTime:		return BlueprintProfilerStatText::ColumnText_MaxTime;
		case EBlueprintProfilerStat::MinTime:		return BlueprintProfilerStatText::ColumnText_MinTime;
		case EBlueprintProfilerStat::Samples:		return BlueprintProfilerStatText::ColumnText_Samples;
		default:									return FText::GetEmpty();
	}
}

//////////////////////////////////////////////////////////////////////////
// FBPPerformanceData

int32 FBPPerformanceData::StatSampleSize = 20;

void FBPPerformanceData::AddEventTiming(const double NodeTiming)
{
	const double TimingInMs = NodeTiming * 1000;
	Samples.WriteNewElementInitialized() = TimingInMs;
	TotalTime += NodeTiming;
	NumSamples++;
	MinTime = FMath::Min<double>(MinTime, TimingInMs);
	MaxTime = FMath::Max<double>(MaxTime, TimingInMs);
	bDirty = true;
}

void FBPPerformanceData::Update()
{
	if (bDirty)
	{
		CachedExclusiveTime = 0.0;
		for (int32 Idx = 0; Idx < Samples.Num(); ++Idx)
		{
			CachedExclusiveTime += Samples(Idx);
		}
		CachedExclusiveTime /= GetSafeSampleCount();
		bDirty = false;
	}
}

void FBPPerformanceData::Reset()
{
	MaxTime = -MAX_dbl;
	MinTime = MAX_dbl;
	CachedExclusiveTime = 0.0;
	TotalTime = 0.0;
	NumSamples = 0;
	bDirty = false;
}

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStat

TAttribute<FText> FBPProfilerStat::GetTextAttribute(FName ColumnName) const
{
	if (ColumnName == BlueprintProfilerStatText::ColumnId_TotalTime)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetTotalTimeText);
	}
	else if (ColumnName == BlueprintProfilerStatText::ColumnId_InclusiveTime)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetInclusiveTimeText);
	}
	else if (ColumnName == BlueprintProfilerStatText::ColumnId_ExclusiveTime)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetExclusiveTimeText);
	}
	else if (ColumnName == BlueprintProfilerStatText::ColumnId_MaxTime)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetMaxTimeText);
	}
	else if (ColumnName == BlueprintProfilerStatText::ColumnId_MinTime)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetMinTimeText);
	}
	else if (ColumnName == BlueprintProfilerStatText::ColumnId_Samples)
	{
		return TAttribute<FText>(this, &FBPProfilerStat::GetSamplesText);
	}
	return TAttribute<FText>();
}

FText FBPProfilerStat::GetTotalTimeText() const
{
	// Covert back up to seconds for total time.
	return FText::AsNumber(GetTotalTime(), &SBlueprintProfilerView::GetNumberFormat());
}

FText FBPProfilerStat::GetInclusiveTimeText() const
{
	return FText::AsNumber(GetInclusiveTime(), &SBlueprintProfilerView::GetNumberFormat());
}

FText FBPProfilerStat::GetExclusiveTimeText() const
{
	return FText::AsNumber(GetExclusiveTime(), &SBlueprintProfilerView::GetNumberFormat());
}

FText FBPProfilerStat::GetMaxTimeText() const
{
	return FText::AsNumber(GetMaxTime(), &SBlueprintProfilerView::GetNumberFormat());
}

FText FBPProfilerStat::GetMinTimeText() const
{
	return FText::AsNumber(GetMinTime(), &SBlueprintProfilerView::GetNumberFormat());
}

FText FBPProfilerStat::GetSamplesText() const
{
	return FText::AsNumber(GetSampleCount());
}

double FBPProfilerStat::GetMaxTime() const
{
	double Result = 0.0;
	if (CachedChildren.Num())
	{
		Result += MaxInclusiveTime;
	}
	else if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetMaxTime();
	}
	return Result;
}

double FBPProfilerStat::GetMinTime() const
{
	double Result = 0.0;
	if (CachedChildren.Num())
	{
		Result += MinInclusiveTime;
	}
	else if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetMinTime();
	}
	return Result;
}

float FBPProfilerStat::GetTotalTime() const
{
	float Result = 0.f;
	if (CachedChildren.Num())
	{
		Result += InclusiveTotalTime;
	}
	else if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetTotalTime();
	}
	return Result;
}

float FBPProfilerStat::GetInclusiveTotalTime() const
{
	float Result = 0.f;
	if (CachedChildren.Num())
	{
		Result += InclusiveTotalTime;
	}
	else if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetTotalTime();
	}
	return Result;
}

float FBPProfilerStat::GetExclusiveTime() const
{
	return PerformanceStats.IsDataValid() ? PerformanceStats.GetExclusiveTime() : 0.f;
}

float FBPProfilerStat::GetInclusiveTime() const
{
	float Result = 0.f;
	Result += InclusiveTime;
	return Result;
}

float FBPProfilerStat::GetSampleCount() const
{
	float Result = 0.f;
	if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetSampleCount();
	}
	else
	{
		Result += InclusiveSamples;
	}
	return Result;
}

float FBPProfilerStat::GetSafeSampleCount() const
{
	float Result = 0.f;
	if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetSampleCount();
	}
	return Result != 0.f ? Result : 1.f;
}

FBPProfilerStatPtr FBPProfilerStat::FindEntryPoint(FScriptExecEvent& EventEntry)
{
	FBPProfilerStatPtr Result;
	if (EventEntry == ObjectContext)
	{
		Result = AsShared();
	}
	else
	{
		for (auto Iter : CachedChildren)
		{
			Result = Iter.Value->FindEntryPoint(EventEntry);
			if (Result.IsValid())
			{
				break;
			}
		}
	}
	return Result;
}

FBPProfilerStatPtr FBPProfilerStat::FindOrAddChildByContext(FScriptExecEvent& Context)
{
	FBPProfilerStatPtr& Result = CachedChildren.FindOrAdd(Context.GetObjectPtr());
	if (!Result.IsValid())
	{
		if (Context.IsBranch())
		{
			TSharedPtr<FBPProfilerBranchStat> Branch = MakeShareable(new FBPProfilerBranchStat(AsShared(), Context));
			Branch->CreateChildrenForPins();
			Result = StaticCastSharedPtr<FBPProfilerStat>(Branch);
		}
		else
		{
			Result = MakeShareable(new FBPProfilerStat(AsShared(), Context));
		}
	}
	return Result;
}

void FBPProfilerStat::SubmitData(class FScriptExecEvent& EventData)
{
	check (EventData == ObjectContext);
	if (EventData.CanSubmitData())
	{
		PerformanceStats.AddEventTiming(EventData.GetTime());
	}
}

TSharedRef<SWidget> FBPProfilerStat::GenerateNameWidget()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SImage)
			.Image(GetIcon())
			.ColorAndOpacity(GetIconColor())
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(5,0))
		[
			SNew(SHyperlink)
			.Text(StatName)
			.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
		//	.ToolTipText(GetToolTipText())
			.OnNavigate(this, &FBPProfilerStat::OnNavigateToObject)
		];
}

void FBPProfilerStat::GatherChildren(TArray<FBPProfilerStatPtr>& OutChildren)
{
	for (auto Child : CachedChildren)
	{
		OutChildren.Add(Child.Value);
	}
}

void FBPProfilerStat::UpdateProfilingStats()
{
	if (PerformanceStats.IsDataDirty() || !PerformanceStats.IsDataValid())
	{
		// Update local performance stats
		PerformanceStats.Update();
		const bool bIsLocalDataValid = PerformanceStats.IsDataValid();

		switch(ObjectContext.GetType())
		{
			case EScriptInstrumentation::Class:
			case EScriptInstrumentation::Instance:
			{
				// Reset stats
				InclusiveTime		= 0.0;
				InclusiveTotalTime	= 0.0;
				InclusiveSamples	= 0.0;
				// Re-Acquire data from children.
				for (auto Child : CachedChildren)
				{
					FBPProfilerStatPtr& ChildPtr = Child.Value;
					ChildPtr->UpdateProfilingStats();
					InclusiveTime += ChildPtr->GetInclusiveTime();
					InclusiveTotalTime += ChildPtr->GetInclusiveTotalTime();
					InclusiveSamples += !bIsLocalDataValid ? ChildPtr->GetInclusiveSampleCount() : 0;
				}
				MaxInclusiveTime = FMath::Max<double>(MaxInclusiveTime, InclusiveTime);
				MinInclusiveTime = FMath::Min<double>(MinInclusiveTime, InclusiveTime);
				break;
			}
			case EScriptInstrumentation::Event:
			{
				// Reset stats
				InclusiveTime		= 0.0;
				InclusiveTotalTime	= 0.0;
				InclusiveSamples	= bIsLocalDataValid ? PerformanceStats.GetSampleCount() : 0.0;
				// Re-Acquire data from children.
				for (auto Child : CachedChildren)
				{
					FBPProfilerStatPtr& ChildPtr = Child.Value;
					ChildPtr->UpdateProfilingStats();
					InclusiveTime += ChildPtr->GetInclusiveTime();
					InclusiveTotalTime += ChildPtr->GetInclusiveTotalTime();
					InclusiveSamples += !bIsLocalDataValid ? ChildPtr->GetInclusiveSampleCount() : 0;
				}
				MaxInclusiveTime = FMath::Max<double>(MaxInclusiveTime, InclusiveTime);
				MinInclusiveTime = FMath::Min<double>(MinInclusiveTime, InclusiveTime);
				InclusiveSamples	= bIsLocalDataValid ? PerformanceStats.GetSampleCount() : 0.0;

				break;
			}
			default:
			{
				// Reset stats
				InclusiveTime		= bIsLocalDataValid ? PerformanceStats.GetExclusiveTime() : 0.0;
				InclusiveTotalTime	= bIsLocalDataValid ? PerformanceStats.GetTotalTime() : 0.0;
				InclusiveSamples	= bIsLocalDataValid ? PerformanceStats.GetSampleCount() : 0.0;
				// Re-Acquire data from children.
				for (auto Child : CachedChildren)
				{
					FBPProfilerStatPtr& ChildPtr = Child.Value;
					ChildPtr->UpdateProfilingStats();
					InclusiveTime += ChildPtr->GetInclusiveTime();
					InclusiveTotalTime += ChildPtr->GetInclusiveTotalTime();
					InclusiveSamples += !bIsLocalDataValid ? ChildPtr->GetInclusiveSampleCount() : 0;
				}
				MaxInclusiveTime = FMath::Max<double>(MaxInclusiveTime, InclusiveTime);
				MinInclusiveTime = FMath::Min<double>(MinInclusiveTime, InclusiveTime);
				break;
			}
		}
	}
}

void FBPProfilerStat::UpdateStatName()
{
	switch(ObjectContext.GetType())
	{
		case EScriptInstrumentation::Instance:
		{
			TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
			if (ObjectPtr.IsValid())
			{
				if (const AActor* Actor = Cast<AActor>(ObjectPtr.Get()))
				{
					StatName = FText::FromString(Actor->GetActorLabel());
				}
				else
				{
					StatName = FText::FromString(ObjectPtr.Get()->GetName());
				}
			}
			break;
		}
		case EScriptInstrumentation::NodePin:
		{
			TWeakObjectPtr<const UEdGraphPin> Pin = ObjectContext.GetTypedObjectPtr<UEdGraphPin>();
			if (Pin.IsValid())
			{
				StatName = Pin.Get()->GetDisplayName();
			}
			break;
		}
		case EScriptInstrumentation::NodeEntry:
		case EScriptInstrumentation::Event:
		case EScriptInstrumentation::Function:
		case EScriptInstrumentation::Branch:
		case EScriptInstrumentation::Macro:
		{
			TWeakObjectPtr<const UEdGraphNode> Node = ObjectContext.GetTypedObjectPtr<UEdGraphNode>();
			if (Node.IsValid())
			{
				StatName = Node.Get()->GetNodeTitle(ENodeTitleType::ListView);
			}
			else
			{
				TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
				if (ObjectPtr.IsValid())
				{
					StatName = FText::FromString(ObjectPtr.Get()->GetName());
				}
			}
			break;
		}
		default:
		{
			TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
			if (ObjectPtr.IsValid())
			{
				StatName = FText::FromString(ObjectPtr.Get()->GetName());
			}
			break;
		}
	}
}

const FSlateBrush* FBPProfilerStat::GetIcon()
{
	switch(ObjectContext.GetType())
	{
		case EScriptInstrumentation::Class:		return FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPIcon_Normal"));
		case EScriptInstrumentation::Instance:	return FEditorStyle::GetBrush(TEXT("BlueprintProfiler.Actor"));
		case EScriptInstrumentation::NodePin:	return CachedChildren.Num() > 0 ? FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinConnected")) : FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinDisconnected"));
		default:
		{
			if (const UEdGraphNode* GraphNode = ObjectContext.GetTypedObjectPtr<UEdGraphNode>().Get())
			{
				if (GraphNode->ShowPaletteIconOnNode())
				{
					return FEditorStyle::GetBrush(GraphNode->GetPaletteIcon(IconColor));
				}
			}
			return FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPNode"));
		}
	}
}

FLinearColor FBPProfilerStat::GetIconColor() const
{
	switch(ObjectContext.GetType())
	{
		case EScriptInstrumentation::Class:		return FLinearColor( 0.46f, 0.54f, 0.81f );
		case EScriptInstrumentation::Event:		return FLinearColor( 0.91f, 0.16f, 0.16f );
		case EScriptInstrumentation::Function:	return FLinearColor( 0.46f, 0.54f, 0.95f );
		case EScriptInstrumentation::Macro:		return FLinearColor( 0.36f, 0.34f, 0.71f );
		case EScriptInstrumentation::Branch:	return FLinearColor( 1.f, 1.f, 1.f, 0.8f );
		default:								return FLinearColor( 1.f, 1.f, 1.f, 0.8f );
	}
}

const FText FBPProfilerStat::GetToolTip()
{
	switch(ObjectContext.GetType())
	{
		case EScriptInstrumentation::Class:		return LOCTEXT("NavigateToBlueprintLocationHyperlink_ToolTip", "Navigate to the Blueprint");
		case EScriptInstrumentation::Instance:	return LOCTEXT("NavigateToInstanceLocationHyperlink_ToolTip", "Navigate to the Object Instance");
		case EScriptInstrumentation::Event:		return LOCTEXT("NavigateToEventLocationHyperlink_ToolTip", "Navigate to the Event Node");
		case EScriptInstrumentation::Function:	return LOCTEXT("NavigateToFunctionLocationHyperlink_ToolTip", "Navigate to the Function Callsite");
		case EScriptInstrumentation::Macro:		return LOCTEXT("NavigateToMacroLocationHyperlink_ToolTip", "Navigate to the Macro Callsite");
		default:								return LOCTEXT("NavigateToUnknownLocationHyperlink_ToolTip", "Navigate to the Object");
	}
}

void FBPProfilerStat::OnNavigateToObject()
{
	switch(ObjectContext.GetType())
	{
		case EScriptInstrumentation::Class:
		{
			break;
		}
		case EScriptInstrumentation::Instance:
		{
			break;
		}
		case EScriptInstrumentation::Event:
		case EScriptInstrumentation::Function:
		case EScriptInstrumentation::Branch:
		case EScriptInstrumentation::Macro:
		default:
		{
			TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
			if (ObjectPtr.IsValid())
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObjectPtr.Get());
			}
			break;
		}
	}
}

bool FBPProfilerBranchStat::IsBranchSequential()
{
	TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
	bool bIsSequential = false;
	if (const UObject* Object = ObjectPtr.Get())
	{
		if (Object->IsA<UK2Node_ExecutionSequence>())
		{
			bIsSequential = true;
		}
	}
	return bIsSequential;
}

void FBPProfilerBranchStat::CreateChildrenForPins()
{
	TWeakObjectPtr<const UObject> ObjectPtr = ObjectContext.GetObjectPtr();
	bAreExecutionPathsSequential = IsBranchSequential();
	if (const UEdGraphNode* GraphNode = Cast<const UEdGraphNode>(ObjectPtr.Get()))
	{
		for (auto Pin : GraphNode->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				FBPProfilerStatPtr& Result = CachedChildren.FindOrAdd(Pin);
				if (!Result.IsValid())
				{
					FScriptExecEvent NewExitPin(Pin, ObjectContext);
					Result = MakeShareable(new FBPProfilerStat(AsShared(), NewExitPin));
				}
			}
		}
	}
}

FBPProfilerStatPtr FBPProfilerBranchStat::FindOrAddChildByContext(FScriptExecEvent& Context)
{
	FBPProfilerStatPtr Result;
	check (CachedExecPin.IsValid());
	if (FBPProfilerStatPtr* PinExecutionPath = CachedChildren.Find(CachedExecPin))
	{
		// Defer child creation down to the execution pin's children.
		// This is an artificial step to increase and manage the parent child relationship and execution depth
		// so we can navigate the stats correctly.
		FBPProfilerStatPtr& NewChild = (*PinExecutionPath)->CachedChildren.FindOrAdd(Context.GetObjectPtr());
		if (!NewChild.IsValid())
		{
			if (Context.IsBranch())
			{
				TSharedPtr<FBPProfilerBranchStat> Branch = MakeShareable(new FBPProfilerBranchStat(ParentStat, Context));
				Branch->CreateChildrenForPins();
				NewChild = StaticCastSharedPtr<FBPProfilerStat>(Branch);
			}
			else
			{
				NewChild = MakeShareable(new FBPProfilerStat(ParentStat, Context));
			}
		}
		Result = NewChild;
	}
	return Result;
}

void FBPProfilerBranchStat::UpdateProfilingStats()
{
	if (PerformanceStats.IsDataDirty() || !PerformanceStats.IsDataValid())
	{
		// Update local performance stats
		PerformanceStats.Update();
		// Reset stats
		const bool bIsLocalDataValid = PerformanceStats.IsDataValid();
		InclusiveTime		= bIsLocalDataValid ? PerformanceStats.GetExclusiveTime() : 0.0;
		InclusiveTotalTime	= bIsLocalDataValid ? PerformanceStats.GetTotalTime() : 0.0;
		InclusiveSamples	= bIsLocalDataValid ? PerformanceStats.GetSampleCount() : 0.0;
		// Find Inclusive data
		if (bAreExecutionPathsSequential)
		{
			for (auto Child : CachedChildren)
			{
				FBPProfilerStatPtr& ChildPtr = Child.Value;
				ChildPtr->UpdateProfilingStats();
				InclusiveTime += ChildPtr->GetInclusiveTime();
				InclusiveTotalTime += ChildPtr->GetInclusiveTotalTime();
			}
			MaxInclusiveTime = FMath::Max<double>(MaxInclusiveTime, InclusiveTime);
			MinInclusiveTime = FMath::Min<double>(MinInclusiveTime, InclusiveTime);
		}
		else
		{
			double WorstInclusiveTime = 0.0;
			double WorstInclusiveTotalTime = 0.0;
			for (auto Child : CachedChildren)
			{
				FBPProfilerStatPtr& ChildPtr = Child.Value;
				ChildPtr->UpdateProfilingStats();
				const double ChildInclusiveTime = ChildPtr->GetInclusiveTime();
				WorstInclusiveTime = FMath::Max<double>(WorstInclusiveTime, ChildInclusiveTime);
				WorstInclusiveTotalTime = FMath::Max<double>(WorstInclusiveTotalTime, ChildPtr->GetInclusiveTotalTime());
				// pick best/worst case path
				MaxInclusiveTime = FMath::Max<float>(MaxInclusiveTime, ChildInclusiveTime);
				MinInclusiveTime = FMath::Min<float>(MinInclusiveTime, ChildInclusiveTime);
			}
			InclusiveTime = WorstInclusiveTime;
		}
	}
}

double FBPProfilerBranchStat::GetMaxTime() const
{
	double Result = 0.0;
	if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetMaxTime();
	}
	Result += MaxInclusiveTime;
	return Result;
}

double FBPProfilerBranchStat::GetMinTime() const
{
	double Result = 0.0;
	if (PerformanceStats.IsDataValid())
	{
		Result += PerformanceStats.GetMinTime();
	}
	else
	{
		Result += MinInclusiveTime;
	}
	return Result;
}

void FBPProfilerBranchStat::SubmitData(FScriptExecEvent& EventData)
{
	check (EventData == ObjectContext);
	// Cache exec path
	CachedExecPin = EventData.GetGraphPinPtr();
	// Submit data
	if (EventData.CanSubmitData())
	{
		PerformanceStats.AddEventTiming(EventData.GetTime());
	}
}

#undef LOCTEXT_NAMESPACE
