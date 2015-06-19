// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "STimelinesContainer.h"
#include "STimeline.h"
#include "STimelineBar.h"
#include "TimeSliderController.h"
#include "SVisualLoggerReport.h"

#define LOCTEXT_NAMESPACE "STimelinesContainer"

TSharedRef<SWidget> STimelinesContainer::MakeTimeline(TSharedPtr<class SVisualLoggerView> InVisualLoggerView, TSharedPtr<class FVisualLoggerTimeSliderController> InTimeSliderController, const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	TSharedPtr<STimeline> NewTimeline;

	ContainingBorder->AddSlot()
		[
			SAssignNew(NewTimeline, STimeline, InVisualLoggerView, InTimeSliderController, SharedThis(this), Entry)
			.OnItemSelectionChanged(FLogVisualizer::Get().GetVisualLoggerEvents().OnItemSelectionChanged)
			.OnGetMenuContent(this, &STimelinesContainer::GetRightClickMenuContent)
		];

	TimelineItems.Add(NewTimeline.ToSharedRef());

	return NewTimeline.ToSharedRef();
}

TSharedRef<SWidget> STimelinesContainer::GetRightClickMenuContent()
{
	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.BeginSection("VisualLogReports", LOCTEXT("VisualLogReports", "VisualLog Reports"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("GenarateReport", "Genarate Report"),
			LOCTEXT("GenarateReportTooltip", "Genarate report from Visual Log events."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &STimelinesContainer::GenerateReport))
			);
	}
	MenuBuilder.EndSection();

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

void STimelinesContainer::SetSelectionState(TSharedPtr<class STimeline> AffectedNode, bool bSelect, bool bDeselectOtherNodes)
{
	if (bSelect)
	{
		if (bDeselectOtherNodes)
		{
			// empty current selection set unless multiple selecting
			SelectedNodes.Empty();
		}

		if (AffectedNode.IsValid())
		{
			SelectedNodes.Add(AffectedNode);
			AffectedNode->OnSelect();
		}
		FLogVisualizer::Get().GetVisualLoggerEvents().OnObjectSelectionChanged.ExecuteIfBound(AffectedNode);
	}
	else if (AffectedNode.IsValid())
	{
		// Not selecting so remove the node from the selection set
		SelectedNodes.Remove(AffectedNode);
		AffectedNode->OnDeselect();
	}
}

bool STimelinesContainer::IsNodeSelected(TSharedPtr<class STimeline> Node) const
{
	return SelectedNodes.Contains(Node);
}

void STimelinesContainer::ChangeSelection(class TSharedPtr<class STimeline> Timeline, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsLeftShiftDown() == false)
	{
		if (MouseEvent.IsLeftControlDown())
		{
			SetSelectionState(Timeline, !Timeline->IsSelected(), false);
		}
		else
		{
			SetSelectionState(Timeline, true, true);
		}
	}
	else
	{
		if (SelectedNodes.Num() == 0 && TimelineItems.Num())
		{
			SetSelectionState(TimelineItems[0], true, true);
		}

		TSharedPtr<class STimeline> LastSelected = SelectedNodes.Num() ? SelectedNodes[SelectedNodes.Num() - 1] : nullptr;
		if (LastSelected.IsValid())
		{
			bool bStartedSelection = false;
			for (auto& CurrentItem : TimelineItems)
			{
				if (CurrentItem == LastSelected || CurrentItem == Timeline)
				{
					if (!bStartedSelection)
					{
						bStartedSelection = true;
					}
					else
					{
						bStartedSelection = false;
						break;
					}
				}
				if (bStartedSelection)
				{
					SetSelectionState(CurrentItem, true, false);
				}
			}
		}
		SetSelectionState(Timeline, true, false);
	}
}

FReply STimelinesContainer::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return TimeSliderController->OnMouseButtonDown(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply STimelinesContainer::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return TimeSliderController->OnMouseButtonUp(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply STimelinesContainer::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return TimeSliderController->OnMouseMove(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply STimelinesContainer::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsLeftControlDown() || MouseEvent.IsLeftShiftDown())
	{
		return TimeSliderController->OnMouseWheel(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply STimelinesContainer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::A && InKeyEvent.IsLeftControlDown())
	{
		for (auto& CurrentTimeline : TimelineItems)
		{
			SetSelectionState(CurrentTimeline, true, false);
		}

		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Platform_Delete && SelectedNodes.Num() > 0)
	{
		TWeakPtr<class STimeline>  NotSelectedOne;
		for (auto & CurrentNode : SelectedNodes)
		{
			TSharedPtr<class STimeline> LastSelected = SelectedNodes[SelectedNodes.Num() - 1];
			bool bFoundSelectedOne = false;
			for (auto& CurrentItem : TimelineItems)
			{
				if (IsNodeSelected(CurrentItem) == false)
				{
					NotSelectedOne = CurrentItem;
				}
				if (LastSelected == CurrentItem)
				{
					if (bFoundSelectedOne && NotSelectedOne.IsValid())
					{
						break;
					}
					bFoundSelectedOne = true;
				}
			}
			TimelineItems.Remove(CurrentNode);
			ContainingBorder->RemoveSlot(CurrentNode.ToSharedRef());
		}

		SetSelectionState(NotSelectedOne.Pin(), true, true);
		return FReply::Handled();
	}
#if 0 //disable movement between timelines for now
	else if (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Down)
	{
		TSharedPtr<class STimeline> PreviousTimeline;
		TSharedPtr<class STimeline> LastSelected = SelectedNodes[SelectedNodes.Num() - 1];
		for (int32 Index = 0; Index < TimelineItems.Num(); ++Index)
		{
			auto& CurrentItem = TimelineItems[Index];
			if (LastSelected == CurrentItem)
			{
				if (InKeyEvent.GetKey() == EKeys::Up && PreviousTimeline.IsValid())
				{
					SetSelectionState(PreviousTimeline, true, true);
				}
				else if (InKeyEvent.GetKey() == EKeys::Down)
				{
					// let's find next visible time line
					if (TimelineItems.IsValidIndex(Index + 1))
					{
						for (int32 i = Index + 1; i < TimelineItems.Num(); ++i)
						{
							if (TimelineItems[i]->GetVisibility() == EVisibility::Visible)
							{
								SetSelectionState(TimelineItems[i], true, true);
								break;
							}
						}
					}
				}
				break;
			}

			if (CurrentItem->GetVisibility() == EVisibility::Visible)
			{
				PreviousTimeline = CurrentItem;
			}
		}
		return FReply::Handled();
	}
#endif
	return FReply::Unhandled();
}

void STimelinesContainer::ResetData()
{
	for (auto CurrentItem : TimelineItems)
	{
		ContainingBorder->RemoveSlot(CurrentItem.ToSharedRef());
	}
	TimelineItems.Reset();

	CachedMinTime = FLT_MAX;
	CachedMaxTime = 0;
	TimeSliderController->SetClampRange(0, 5);
	TimeSliderController->SetTimeRange(0, 5);
}

void STimelinesContainer::Construct(const FArguments& InArgs, TSharedRef<class SVisualLoggerView> InVisualLoggerView, TSharedRef<FVisualLoggerTimeSliderController> InTimeSliderController)
{
	TimeSliderController = InTimeSliderController;
	VisualLoggerView = InVisualLoggerView;

	ChildSlot
		[
			SNew(SBorder)
			.Padding(0)
			.VAlign(VAlign_Top)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(ContainingBorder, SVerticalBox)
			]
		];

	CachedMinTime = FLT_MAX;
	CachedMaxTime = 0;

}

void STimelinesContainer::OnSearchChanged(const FText& Filter)
{
	for (auto CurrentItem : TimelineItems)
	{
		CurrentItem->OnSearchChanged(Filter);
	}
}

void STimelinesContainer::OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	bool bCreateNew = true;
	for (auto CurrentItem : TimelineItems)
	{
		if (CurrentItem->GetName() == Entry.OwnerName)
		{
			CurrentItem->AddEntry(Entry);
			bCreateNew = false;
			break;
		}
	}

	if (bCreateNew)
	{
		MakeTimeline(VisualLoggerView, TimeSliderController, Entry);
	}

	CachedMinTime = CachedMinTime < Entry.Entry.TimeStamp ? CachedMinTime : Entry.Entry.TimeStamp;
	CachedMaxTime = CachedMaxTime > Entry.Entry.TimeStamp ? CachedMaxTime : Entry.Entry.TimeStamp;

	TRange<float> LocalViewRange = TimeSliderController->GetTimeSliderArgs().ViewRange.Get();
	const float CurrentMin = TimeSliderController->GetTimeSliderArgs().ClampMin.Get().GetValue();
	const float CurrentMax = TimeSliderController->GetTimeSliderArgs().ClampMax.Get().GetValue();
	float ZoomLevel = LocalViewRange.Size<float>() / (CurrentMax - CurrentMin);

	TimeSliderController->GetTimeSliderArgs().ClampMin = CachedMinTime;
	TimeSliderController->GetTimeSliderArgs().ClampMax = CachedMaxTime + 0.1;
	if ( FMath::Abs(ZoomLevel - 1) <= SMALL_NUMBER)
	{
		TimeSliderController->GetTimeSliderArgs().ViewRange = TRange<float>(TimeSliderController->GetTimeSliderArgs().ClampMin.Get().GetValue(), TimeSliderController->GetTimeSliderArgs().ClampMax.Get().GetValue());
	}
}

void STimelinesContainer::OnFiltersChanged()
{
	for (auto CurrentItem : TimelineItems)
	{
		CurrentItem->OnFiltersChanged();
	}
}

void STimelinesContainer::OnFiltersSearchChanged(const FText& Filter)
{
	for (auto CurrentItem : TimelineItems)
	{
		CurrentItem->OnFiltersSearchChanged(Filter);
	}
}

void STimelinesContainer::GenerateReport()
{
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.ClientSize(FVector2D(720, 768))
		.Title(NSLOCTEXT("LogVisualizerReport", "WindowTitle", "Log Visualizer Report"))
		[
			SNew(SVisualLoggerReport, SelectedNodes, VisualLoggerView)
		];

	FSlateApplication::Get().AddWindow(NewWindow);
}

#undef LOCTEXT_NAMESPACE
