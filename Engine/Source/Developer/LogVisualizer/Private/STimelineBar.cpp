// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "STimelineBar.h"
#include "TimeSliderController.h"
#include "STimelinesContainer.h"
#include "VisualLoggerCameraController.h"
#if WITH_EDITOR
#	include "Editor/UnrealEd/Public/EditorComponents.h"
#	include "Editor/UnrealEd/Public/EditorReimportHandler.h"
#	include "Editor/UnrealEd/Public/TexAlignTools.h"
#	include "Editor/UnrealEd/Public/TickableEditorObject.h"
#	include "UnrealEdClasses.h"
#	include "Editor/UnrealEd/Public/Editor.h"
#	include "Editor/UnrealEd/Public/EditorViewportClient.h"
#endif

FReply STimelineBar::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TimelineOwner.Pin()->OnMouseButtonDown(MyGeometry, MouseEvent);
	return TimeSliderController->OnMouseButtonDown(SharedThis(this), MyGeometry, MouseEvent);
}

FReply STimelineBar::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TimelineOwner.Pin()->OnMouseButtonUp(MyGeometry, MouseEvent);

	FReply  Replay = TimeSliderController->OnMouseButtonUp(SharedThis(this), MyGeometry, MouseEvent);
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SnapScrubPosition(TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get());
	}

	return Replay;
}

FReply STimelineBar::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TimeSliderController->OnMouseMove(SharedThis(this), MyGeometry, MouseEvent);
}

FReply STimelineBar::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	auto &Entries = TimelineOwner.Pin()->GetEntries();
	SnapScrubPosition(TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get());
	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (World && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Entries.IsValidIndex(CurrentItemIndex))
	{
		FVector CurrentLocation = Entries[CurrentItemIndex].Entry.Location;

		FVector Extent(150);
		bool bFoundActor = false;
		FName OwnerName = Entries[CurrentItemIndex].OwnerName;
		for (FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor->GetFName() == OwnerName)
			{
				FVector Orgin;
				Actor->GetActorBounds(false, Orgin, Extent);
				bFoundActor = true;
				break;
			}
		}

		
		const float DefaultCameraDistance = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->DefaultCameraDistance;
		Extent = Extent.Size() < DefaultCameraDistance ? FVector(1) * DefaultCameraDistance : Extent;

#if WITH_EDITOR
		UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EEngine != NULL)
		{
			for (auto ViewportClient : EEngine->AllViewportClients)
			{
				ViewportClient->FocusViewportOnBox(FBox::BuildAABB(CurrentLocation, Extent));
			}
		}
		else if (AVisualLoggerCameraController::IsEnabled(World) && AVisualLoggerCameraController::Instance.IsValid() && AVisualLoggerCameraController::Instance->GetSpectatorPawn())
		{
			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(AVisualLoggerCameraController::Instance->Player);
			if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport)
			{
				FViewport* Viewport = LocalPlayer->ViewportClient->Viewport;

				FBox BoundingBox = FBox::BuildAABB(CurrentLocation, Extent);
				const FVector Position = BoundingBox.GetCenter();
				float Radius = BoundingBox.GetExtent().Size();

				FViewportCameraTransform ViewTransform;
				ViewTransform.TransitionToLocation(Position, SharedThis(this), true);

				float NewOrthoZoom;
				const float AspectRatio = 1.777777f;
				uint32 MinAxisSize = (AspectRatio > 1.0f) ? Viewport->GetSizeXY().Y : Viewport->GetSizeXY().X;
				float Zoom = Radius / (MinAxisSize / 2.0f);

				NewOrthoZoom = Zoom * (Viewport->GetSizeXY().X*15.0f);
				NewOrthoZoom = FMath::Clamp<float>(NewOrthoZoom, 250, MAX_FLT);
				ViewTransform.SetOrthoZoom(NewOrthoZoom);

				AVisualLoggerCameraController::Instance->GetSpectatorPawn()->TeleportTo(ViewTransform.GetLocation(), ViewTransform.GetRotation(), false, true);
			}
		}
#endif
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void STimelineBar::GotoNextItem(int32 MoveDistance)
{
	int32 NewItemIndex = CurrentItemIndex;
	auto &Entries = TimelineOwner.Pin()->GetEntries();
	int32 Index = 0;
	while (true)
	{
		NewItemIndex++;
		if (Entries.IsValidIndex(NewItemIndex))
		{
			auto& CurrentEntryItem = Entries[NewItemIndex];
			if (TimelineOwner.Pin()->IsEntryHidden(CurrentEntryItem) == false && ++Index == MoveDistance)
			{
				break;
			}
		}
		else
		{
			NewItemIndex = FMath::Clamp(NewItemIndex, 0, Entries.Num() - 1);
			break;
		}
	}

	if (NewItemIndex != CurrentItemIndex)
	{
		float NewTimeStamp = Entries[NewItemIndex].Entry.TimeStamp;
		SnapScrubPosition(NewItemIndex);
	}
}

void STimelineBar::GotoPreviousItem(int32 MoveDistance)
{
	int32 NewItemIndex = CurrentItemIndex;
	auto &Entries = TimelineOwner.Pin()->GetEntries();
	int32 Index = 0;
	while (true)
	{
		NewItemIndex--;
		if (Entries.IsValidIndex(NewItemIndex))
		{
			auto& CurrentEntryItem = Entries[NewItemIndex];
			if (TimelineOwner.Pin()->IsEntryHidden(CurrentEntryItem) == false && ++Index == MoveDistance)
			{
				break;
			}
		}
		else
		{
			NewItemIndex = FMath::Clamp(NewItemIndex, 0, Entries.Num() - 1);
			break;
		}
	}

	if (NewItemIndex != CurrentItemIndex)
	{
		float NewTimeStamp = Entries[NewItemIndex].Entry.TimeStamp;
		SnapScrubPosition(NewItemIndex);
	}
}

FReply STimelineBar::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply ReturnValue = FReply::Unhandled();
	if (TimelineOwner.Pin()->IsSelected() == false)
	{
		return ReturnValue;
	}

	if (CurrentItemIndex != INDEX_NONE)
	{
		TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
		float RangeSize = LocalViewRange.Size<float>();

		const FKey Key = InKeyEvent.GetKey();
		int32 NewItemIndex = CurrentItemIndex;
		auto &Entries = TimelineOwner.Pin()->GetEntries();
		if (Key == EKeys::Home)
		{
			for (int32 Index = 0; Index < Entries.Num(); Index++)
			{
				if (TimelineOwner.Pin()->IsEntryHidden(Entries[Index]) == false)
				{
					NewItemIndex = Index;
					break;
				}
			}
			if (NewItemIndex != CurrentItemIndex)
			{
				SnapScrubPosition(NewItemIndex);
			}
			ReturnValue = FReply::Handled();
		}
		else if (Key == EKeys::End)
		{
			for (int32 Index = Entries.Num()-1; Index >= 0; Index--)
			{
				if (TimelineOwner.Pin()->IsEntryHidden(Entries[Index]) == false)
				{
					NewItemIndex = Index;
					break;
				}
			}
			if (NewItemIndex != CurrentItemIndex)
			{
				SnapScrubPosition(NewItemIndex);
			}
			ReturnValue = FReply::Handled();
		}
		else if (Key == EKeys::Left)
		{
			GotoPreviousItem(InKeyEvent.IsLeftControlDown() ? InKeyEvent.IsLeftShiftDown() ? 20 : 10 : 1);
			ReturnValue = FReply::Handled();
		}
		else if (Key == EKeys::Right)
		{
			GotoNextItem(InKeyEvent.IsLeftControlDown() ? InKeyEvent.IsLeftShiftDown() ? 20 : 10 : 1);
			ReturnValue = FReply::Handled();
		}
	}

	return ReturnValue;
}

int32 STimelineBar::GetClosestItem(float Time) const
{
	int32 BestItemIndex = INDEX_NONE;
	float BestDistance = MAX_FLT;
	auto &Entries = TimelineOwner.Pin()->GetEntries();
	for (int32 Index = 0; Index < Entries.Num(); Index++)
	{
		auto& CurrentEntryItem = Entries[Index];

		if (TimelineOwner.Pin()->IsEntryHidden(CurrentEntryItem))
		{
			continue;
		}
		TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
		const float CurrentDist = FMath::Abs(CurrentEntryItem.Entry.TimeStamp - Time);
		if (CurrentDist < BestDistance)
		{
			BestDistance = CurrentDist;
			BestItemIndex = Index;
		}
	}

	const float CurrentDist = Entries.IsValidIndex(CurrentItemIndex) ? FMath::Abs(Entries[CurrentItemIndex].Entry.TimeStamp - Time) : MAX_FLT;

	if (BestItemIndex != INDEX_NONE && CurrentDist > BestDistance)
	{
		return BestItemIndex;
	}

	return CurrentItemIndex;
}

void STimelineBar::SnapScrubPosition(float ScrubPosition)
{
	uint32 BestItemIndex = GetClosestItem(ScrubPosition);
	if (BestItemIndex != INDEX_NONE)
	{
		auto &Entries = TimelineOwner.Pin()->GetEntries();
		const float CurrentTime = Entries[BestItemIndex].Entry.TimeStamp;
		if (CurrentItemIndex != BestItemIndex)
		{
			CurrentItemIndex = BestItemIndex;
			FLogVisualizer::Get().GetVisualLoggerEvents().OnItemSelectionChanged.ExecuteIfBound(Entries[BestItemIndex]);
		}
		TimeSliderController->CommitScrubPosition(CurrentTime, false);
	}
}

void STimelineBar::SnapScrubPosition(int32 NewItemIndex)
{
	if (NewItemIndex != INDEX_NONE)
	{
		auto &Entries = TimelineOwner.Pin()->GetEntries();
		const float CurrentTime = Entries[NewItemIndex].Entry.TimeStamp;
		if (CurrentItemIndex != NewItemIndex)
		{
			CurrentItemIndex = NewItemIndex;
			FLogVisualizer::Get().GetVisualLoggerEvents().OnItemSelectionChanged.ExecuteIfBound(Entries[NewItemIndex]);
		}
		TimeSliderController->CommitScrubPosition(CurrentTime, false);
	}
}

void STimelineBar::Construct(const FArguments& InArgs, TSharedPtr<FVisualLoggerTimeSliderController> InTimeSliderController, TSharedPtr<STimeline> InTimelineOwner)
{
	TimeSliderController = InTimeSliderController;
	TimelineOwner = InTimelineOwner;
	CurrentItemIndex = INDEX_NONE;

	TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
}

FVector2D STimelineBar::ComputeDesiredSize(float) const
{
	return FVector2D(5000.0f, 20.0f);
}

void STimelineBar::OnSelect()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
	CurrentItemIndex = INDEX_NONE;
}

void STimelineBar::OnDeselect()
{
	CurrentItemIndex = INDEX_NONE;
}

int32 STimelineBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	//@TODO: Optimize it like it was with old LogVisualizer, to draw everything much faster (SebaK)
	int32 RetLayerId = LayerId;

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
	float LocalScrubPosition = TimeSliderController->GetTimeSliderArgs().ScrubPosition.Get();

	float ViewRange = LocalViewRange.Size<float>();
	float PixelsPerInput = ViewRange > 0 ? AllottedGeometry.Size.X / ViewRange : 0;
	float CurrentScrubLinePos = (LocalScrubPosition - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;
	float BoxWidth = FMath::Max(0.08f * PixelsPerInput, 4.0f);

	// Draw a region around the entire section area
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		FLogVisualizerStyle::Get().GetBrush("Sequencer.SectionArea.Background"),
		MyClippingRect,
		ESlateDrawEffect::None,
		TimelineOwner.Pin()->IsSelected() ? FLinearColor(.2f, .2f, .2f, 0.5f) : FLinearColor(.1f, .1f, .1f, 0.5f)
		);

	const FSlateBrush* FillImage = FLogVisualizerStyle::Get().GetBrush("LogVisualizer.LogBar.EntryDefault");
	static const FColor CurrentTimeColor(140, 255, 255, 255);
	static const FColor ErrorTimeColor(255, 0, 0, 255);
	static const FColor WarningTimeColor(255, 255, 0, 255);
	static const FColor SelectedBarColor(255, 255, 255, 255);
	const FSlateBrush* SelectedFillImage = FLogVisualizerStyle::Get().GetBrush("LogVisualizer.LogBar.Selected");

	const ESlateDrawEffect::Type DrawEffects = ESlateDrawEffect::None;// bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TArray<float> ErrorTimes;
	TArray<float> WarningTimes;
	auto &Entries = TimelineOwner.Pin()->GetEntries();
	int32 EntryIndex = 0;

	while (EntryIndex < Entries.Num())
	{
		const FVisualLogEntry& Entry = Entries[EntryIndex].Entry;
		if (Entry.TimeStamp < LocalViewRange.GetLowerBoundValue() || Entry.TimeStamp > LocalViewRange.GetUpperBoundValue())
		{
			EntryIndex++;
			continue;
		}

		if (TimelineOwner.Pin()->IsEntryHidden(Entries[EntryIndex]))
		{
			EntryIndex++;
			continue;
		}

		// find bar width, connect all contiguous bars to draw them as one geometry (rendering optimization)
		const float StartPos = (Entry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput - 2;
		float EndPos = (Entry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput + 2;
		int32 StartIndex = EntryIndex;
		float LastEndX = MAX_FLT;
		for (; StartIndex < Entries.Num(); ++StartIndex)
		{
			const FVisualLogEntry& CurrentEntry = Entries[StartIndex].Entry;
			if (CurrentEntry.TimeStamp < LocalViewRange.GetLowerBoundValue() || CurrentEntry.TimeStamp > LocalViewRange.GetUpperBoundValue())
			{
				break;
			}

			if (TimelineOwner.Pin()->IsEntryHidden(Entries[StartIndex]))
			{
				continue;
			}

			const TArray<FVisualLogLine>& LogLines = CurrentEntry.LogLines;
			bool bAddedWarning = false;
			bool bAddedError = false;
			for (const FVisualLogLine& CurrentLine : LogLines)
			{
				if (CurrentLine.Verbosity <= ELogVerbosity::Error && !bAddedError)
				{
					ErrorTimes.AddUnique(CurrentEntry.TimeStamp);
					bAddedError = true;
				}
				else if (CurrentLine.Verbosity == ELogVerbosity::Warning && !bAddedWarning)
				{
					WarningTimes.AddUnique(CurrentEntry.TimeStamp);
					bAddedWarning = true;
				}
				if (bAddedError && bAddedWarning)
				{
					break;
				}
			}

			const float CurrentStartPos = (CurrentEntry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput - 2;
			if (CurrentStartPos > EndPos)
			{
				break;
			}
			EndPos = (CurrentEntry.TimeStamp - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput + 2;
		}

		if (EndPos - StartPos > 0)
		{
			const float BarWidth = (EndPos - StartPos);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				RetLayerId,
				AllottedGeometry.ToPaintGeometry(
				FVector2D(StartPos, 0.0f),
				FVector2D(BarWidth, AllottedGeometry.Size.Y)),
				FillImage,
				MyClippingRect,
				DrawEffects,
				CurrentTimeColor
				);
		}
		EntryIndex = StartIndex;
	}

	if (WarningTimes.Num()) RetLayerId++;
	for (auto CurrentTime : WarningTimes)
	{
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 3, 0.0f),
			FVector2D(6, AllottedGeometry.Size.Y)),
			FillImage,
			MyClippingRect,
			DrawEffects,
			WarningTimeColor
			);
	}

	if (ErrorTimes.Num()) RetLayerId++;
	for (auto CurrentTime : ErrorTimes)
	{
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 3, 0.0f),
			FVector2D(6, AllottedGeometry.Size.Y)),
			FillImage,
			MyClippingRect,
			DrawEffects,
			ErrorTimeColor
			);
	}

	int32 BestItemIndex = GetClosestItem(LocalScrubPosition);

	if (BestItemIndex != INDEX_NONE && TimelineOwner.Pin()->IsSelected())
	{
		const auto &BestItemEntry = TimelineOwner.Pin()->GetEntries()[BestItemIndex];
		if (BestItemIndex != CurrentItemIndex)
		{
			FLogVisualizer::Get().GetVisualLoggerEvents().OnItemSelectionChanged.ExecuteIfBound(BestItemEntry);
			CurrentItemIndex = BestItemIndex;
		}

		float CurrentTime = BestItemEntry.Entry.TimeStamp;
		float LinePos = (CurrentTime - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++RetLayerId,
			AllottedGeometry.ToPaintGeometry(
			FVector2D(LinePos - 2, 0.0f),
			FVector2D(4, AllottedGeometry.Size.Y)),
			SelectedFillImage,
			MyClippingRect,
			ESlateDrawEffect::None,
			SelectedBarColor
			);
	}

	return RetLayerId;
}
