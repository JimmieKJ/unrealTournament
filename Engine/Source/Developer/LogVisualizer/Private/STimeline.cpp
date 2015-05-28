// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "STimeline.h"
#include "STimelineBar.h"
#include "TimeSliderController.h"
#include "STimelinesContainer.h"
#include "LogVisualizerSettings.h"

class STimelineLabelAnchor : public SMenuAnchor
{
public:
	void Construct(const FArguments& InArgs, TSharedPtr<class STimeline> InTimelineOwner)
	{
		SMenuAnchor::Construct(InArgs);
		TimelineOwner = InTimelineOwner;
	}

private:
	/** SWidget Interface */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TWeakPtr<class STimeline> TimelineOwner;
};

FReply STimelineLabelAnchor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && TimelineOwner.IsValid() && TimelineOwner.Pin()->IsSelected())
	{
		SetIsOpen(!IsOpen());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


STimeline::~STimeline()
{
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->OnSettingChanged().RemoveAll(this);
}

FReply STimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Owner->ChangeSelection(SharedThis(this), MouseEvent);

	return FReply::Unhandled();
}

FReply STimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

bool STimeline::IsSelected() const
{
	// Ask the tree if we are selected
	return Owner->IsNodeSelected(SharedThis(const_cast<STimeline*>(this)));
}

void STimeline::OnSelect()
{
	if (TimelineBar.IsValid())
	{
		TimelineBar->OnSelect();
	}
}

void STimeline::OnDeselect()
{
	if (TimelineBar.IsValid())
	{
		TimelineBar->OnDeselect();
	}
}

void STimeline::UpdateVisibility()
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const bool bJustIgnore = Settings->bIgnoreTrivialLogs && Entries.Num() <= Settings->TrivialLogsThreshold;
	const bool bVisibleByOwnerClasses = FCategoryFiltersManager::Get().MatchObjectName(OwnerClassName.ToString());
	const bool bIsCollapsed = bJustIgnore || HiddenEntries.Num() == Entries.Num() || (SearchFilter.Len() > 0 && Name.ToString().Find(SearchFilter) == INDEX_NONE);

	SetVisibility(bIsCollapsed || !bVisibleByOwnerClasses ? EVisibility::Collapsed : EVisibility::Visible);
	if (bIsCollapsed)
	{
		Owner->SetSelectionState(SharedThis(this), false, false);
	}
}

void STimeline::UpdateVisibilityForItems()
{
	HiddenEntries.Reset();
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const FString QuickSearchStrng = FCategoryFiltersManager::Get().GetSearchString(); 

	for (auto& CurrentEntry : Entries)
	{
		UpdateVisibilityForEntry(CurrentEntry, QuickSearchStrng, Settings->bSearchInsideLogs);
	}
}

void STimeline::OnFiltersSearchChanged(const FText& Filter)
{
	OnFiltersChanged();
}

void STimeline::OnFiltersChanged()
{
	UpdateVisibilityForItems();
	UpdateVisibility();
}

void STimeline::OnSearchChanged(const FText& Filter)
{
	SearchFilter = Filter.ToString();
	UpdateVisibility();
}

bool STimeline::IsEntryHidden(const FVisualLogDevice::FVisualLogEntryItem& EntryItem) const
{
	return HiddenEntries.Contains(&EntryItem);
}

void STimeline::HandleLogVisualizerSettingChanged(FName InName)
{
	UpdateVisibility();
}

void STimeline::UpdateVisibilityForEntry(FVisualLogDevice::FVisualLogEntryItem& CurrentEntry, const FString& SearchString, bool bSearchInsideLogs)
{
	TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
	FVisualLoggerHelpers::GetCategories(CurrentEntry.Entry, OutCategories);
	bool bHasValidCategories = false;
	for (FVisualLoggerCategoryVerbosityPair& Categoryair : OutCategories)
	{
		bHasValidCategories = bHasValidCategories || FCategoryFiltersManager::Get().MatchCategoryFilters(Categoryair.CategoryName.ToString(), Categoryair.Verbosity);
	}

	if (bSearchInsideLogs && bHasValidCategories && SearchString.Len() > 0)
	{
		bool bMatchSearchString = false;
		for (const FVisualLogLine& CurrentLine : CurrentEntry.Entry.LogLines)
		{
			if (CurrentLine.Line.Find(SearchString) != INDEX_NONE || CurrentLine.Category.ToString().Find(SearchString) != INDEX_NONE)
			{
				bMatchSearchString = true;
				break;
			}
		}
		if (!bMatchSearchString)
		{
			for (const FVisualLogEvent& CurrentEvent : CurrentEntry.Entry.Events)
			{
				if (CurrentEvent.Name.Find(SearchString) != INDEX_NONE)
				{
					bMatchSearchString = true;
					break;
				}
			}
		}

		if (!bMatchSearchString)
		{
			HiddenEntries.AddUnique(&CurrentEntry);
		}
	}
	else
	{
		if (bHasValidCategories == false)
		{
			HiddenEntries.AddUnique(&CurrentEntry);
		}
	}
}

void STimeline::AddEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry) 
{ 
	Entries.Add(Entry); 

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	
	UpdateVisibilityForEntry(Entries[Entries.Num() - 1], FCategoryFiltersManager::Get().GetSearchString(), Settings->bSearchInsideLogs);
	UpdateVisibility();

	if (Settings->bStickToRecentData && IsSelected())
	{
		TimelineBar->SnapScrubPosition(Entry.Entry.TimeStamp);
	}
}

void STimeline::Construct(const FArguments& InArgs, TSharedPtr<SVisualLoggerView> VisualLoggerView, TSharedPtr<FVisualLoggerTimeSliderController> TimeSliderController, TSharedPtr<STimelinesContainer> InContainer, const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	OnGetMenuContent = InArgs._OnGetMenuContent;

	Owner = InContainer;
	Name = Entry.OwnerName;
	OwnerClassName = Entry.OwnerClassName;
	
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->OnSettingChanged().AddRaw(this, &STimeline::HandleLogVisualizerSettingChanged);


	ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 4, 0, 0))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillWidth(TAttribute<float>(VisualLoggerView.Get(), &SVisualLoggerView::GetAnimationOutlinerFillPercentage))
			[
				SAssignNew(PopupAnchor, STimelineLabelAnchor, SharedThis(this))
				.OnGetMenuContent(OnGetMenuContent)
				[
					SNew(SBorder)
					.HAlign(HAlign_Fill)
					.Padding(FMargin(0, 0, 4, 0))
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					[
						SNew(SBorder)
						.VAlign(VAlign_Center)
						.BorderImage(this, &STimeline::GetBorder)
						.Padding(FMargin(4, 0, 2, 0))
						[
							// Search box for searching through the outliner
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(FMargin(0, 0, 0, 0))
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(STextBlock)
								.Text( FText::FromName(Name) )
								.ShadowOffset(FVector2D(1.f, 1.f))
							]
							+ SVerticalBox::Slot()
							.Padding(FMargin(0, 0, 0, 0))
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(STextBlock)
								.Text(FText::FromName(OwnerClassName))
								.TextStyle(FLogVisualizerStyle::Get(), TEXT("Sequencer.ClassNAme"))
							]
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 4, 0, 0))
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.Padding(FMargin(0, 0, 0, 0))
				.HAlign(HAlign_Left)
				[
					// Search box for searching through the outliner
					SAssignNew(TimelineBar, STimelineBar, TimeSliderController, SharedThis(this))
				]
			]
		];

	AddEntry(Entry);
}

const FSlateBrush* STimeline::GetBorder() const
{
	if (IsSelected())
	{
		return FLogVisualizerStyle::Get().GetBrush("ToolBar.Button.Hovered");
	}
	else
	{
		return FLogVisualizerStyle::Get().GetBrush("ToolBar.Button.Normal");
	}
}

void STimeline::Goto(float ScrubPosition) 
{ 
	TimelineBar->SnapScrubPosition(ScrubPosition); 
}

void STimeline::GotoNextItem() 
{ 
	TimelineBar->GotoNextItem(); 
}

void STimeline::GotoPreviousItem() 
{ 
	TimelineBar->GotoPreviousItem(); 
}
