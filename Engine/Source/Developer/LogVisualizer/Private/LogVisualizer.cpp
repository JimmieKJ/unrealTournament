// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "TimeSliderController.h"
#include "VisualLoggerRenderingActor.h"
#include "STimelinesContainer.h"
#if WITH_EDITOR
#	include "Editor/UnrealEd/Public/EditorComponents.h"
#	include "Editor/UnrealEd/Public/EditorReimportHandler.h"
#	include "Editor/UnrealEd/Public/TexAlignTools.h"
#	include "Editor/UnrealEd/Public/TickableEditorObject.h"
#	include "UnrealEdClasses.h"
#	include "Editor/UnrealEd/Public/Editor.h"
#	include "Editor/UnrealEd/Public/EditorViewportClient.h"
#endif

TSharedPtr< struct FLogVisualizer > FLogVisualizer::StaticInstance;
FColor FLogVisualizerColorPalette[] = {
	FColor(0xff00A480),
	FColorList::Aquamarine,
	FColorList::Cyan,
	FColorList::Brown,
	FColorList::Green,
	FColorList::Orange,
	FColorList::Magenta,
	FColorList::BrightGold,
	FColorList::NeonBlue,
	FColorList::MediumSlateBlue,
	FColorList::SpicyPink,
	FColorList::SpringGreen,
	FColorList::SteelBlue,
	FColorList::SummerSky,
	FColorList::Violet,
	FColorList::VioletRed,
	FColorList::YellowGreen,
	FColor(0xff62E200),
	FColor(0xff1F7B67),
	FColor(0xff62AA2A),
	FColor(0xff70227E),
	FColor(0xff006B53),
	FColor(0xff409300),
	FColor(0xff5D016D),
	FColor(0xff34D2AF),
	FColor(0xff8BF13C),
	FColor(0xffBC38D3),
	FColor(0xff5ED2B8),
	FColor(0xffA6F16C),
	FColor(0xffC262D3),
	FColor(0xff0F4FA8),
	FColor(0xff00AE68),
	FColor(0xffDC0055),
	FColor(0xff284C7E),
	FColor(0xff21825B),
	FColor(0xffA52959),
	FColor(0xff05316D),
	FColor(0xff007143),
	FColor(0xff8F0037),
	FColor(0xff4380D3),
	FColor(0xff36D695),
	FColor(0xffEE3B80),
	FColor(0xff6996D3),
	FColor(0xff60D6A7),
	FColor(0xffEE6B9E)
};


void FLogVisualizer::Initialize()
{
	StaticInstance = MakeShareable(new FLogVisualizer);
	Get().TimeSliderController = MakeShareable(new FVisualLoggerTimeSliderController(FVisualLoggerTimeSliderArgs()));

}

void FLogVisualizer::Shutdown()
{
	StaticInstance.Reset();
}

FLogVisualizer& FLogVisualizer::Get()
{
	return *StaticInstance;
}

void FLogVisualizer::Goto(float Timestamp, FName LogOwner)
{
	if (CurrentVisualizer.IsValid())
	{
		TArray<TSharedPtr<class STimeline> > TimeLines;
		CurrentVisualizer.Pin()->GetTimelines(TimeLines, false);
		for (TSharedPtr<class STimeline> Timeline : TimeLines)
		{
			if (Timeline->GetName() == LogOwner)
			{
				Timeline->GetOwner()->SetSelectionState(Timeline, true, true);
				break;
			}
		}
	}

	if (CurrentTimeLine.IsValid())
	{
		CurrentTimeLine.Pin()->Goto(Timestamp);
	}
}

void FLogVisualizer::GotoNextItem()
{
	if (CurrentTimeLine.IsValid())
	{
		CurrentTimeLine.Pin()->GotoNextItem();
	}
}

void FLogVisualizer::GotoPreviousItem()
{
	if (CurrentTimeLine.IsValid())
	{
		CurrentTimeLine.Pin()->GotoPreviousItem();
	}
}

FLinearColor FLogVisualizer::GetColorForCategory(int32 Index) const
{
	if (Index >= 0 && Index < sizeof(FLogVisualizerColorPalette) / sizeof(FLogVisualizerColorPalette[0]))
	{
		return FLogVisualizerColorPalette[Index];
	}

	static bool bReateColorList = false;
	static FColorList StaticColor;
	if (!bReateColorList)
	{
		bReateColorList = true;
		StaticColor.CreateColorMap();
	}
	return StaticColor.GetFColorByIndex(Index);
}

FLinearColor FLogVisualizer::GetColorForCategory(const FString& InFilterName) const
{
	static TArray<FString> Filters;
	int32 CategoryIndex = Filters.Find(InFilterName);
	if (CategoryIndex == INDEX_NONE)
	{
		CategoryIndex = Filters.Add(InFilterName);
	}

	return GetColorForCategory(CategoryIndex);
}

UWorld* FLogVisualizer::GetWorld(UObject* OptionalObject)
{
	UWorld* World = OptionalObject != nullptr ? GEngine->GetWorldFromContextObject(OptionalObject, false) : nullptr;
#if WITH_EDITOR
	if (!World && GIsEditor)
	{
		UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
		// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
		World = EEngine != nullptr && EEngine->PlayWorld != nullptr ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();

	}
	else 
#endif
	if (!World && !GIsEditor)
	{

		World = GEngine->GetWorld();
	}

	if (World == nullptr)
	{
		World = GWorld;
	}

	return World;
}

AActor* FLogVisualizer::GetVisualLoggerHelperActor()
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	for (TActorIterator<AVisualLoggerRenderingActor> It(World); It; ++It)
	{
		return *It;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoCollisionFail = true;
	SpawnInfo.Name = *FString::Printf(TEXT("VisualLoggerRenderingActor"));
	AActor* HelperActor = World->SpawnActor<AVisualLoggerRenderingActor>(SpawnInfo);

	return HelperActor;
}
