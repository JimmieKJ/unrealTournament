// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SVisualLoggerView.h"
#include "TimeSliderController.h"
#include "ITimeSlider.h"
#include "STimeSlider.h"
#include "SSearchBox.h"
#include "SSequencerSectionOverlay.h"
#include "STimelinesContainer.h"

#define LOCTEXT_NAMESPACE "SVisualLoggerFilters"

class SInputCatcherOverlay : public SOverlay
{
public:
	void Construct(const FArguments& InArgs, TSharedRef<class FVisualLoggerTimeSliderController> InTimeSliderController)
	{
		SOverlay::Construct(InArgs);
		TimeSliderController = InTimeSliderController;
	}

	/** Controller for manipulating time */
	TSharedPtr<class FVisualLoggerTimeSliderController> TimeSliderController;
private:
	/** SWidget Interface */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
};

FReply SInputCatcherOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return TimeSliderController->OnMouseButtonDown(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SInputCatcherOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return TimeSliderController->OnMouseButtonUp(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SInputCatcherOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TimeSliderController->OnMouseMove(SharedThis(this), MyGeometry, MouseEvent);
}

FReply SInputCatcherOverlay::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsLeftShiftDown() || MouseEvent.IsLeftControlDown())
	{
		return TimeSliderController->OnMouseWheel(SharedThis(this), MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

void SVisualLoggerView::GetTimelines(TArray<TSharedPtr<STimeline> >& OutList, bool bOnlySelectedOnes)
{
	OutList = bOnlySelectedOnes ? TimelinesContainer->GetSelectedNodes() : TimelinesContainer->GetAllNodes();
}

void SVisualLoggerView::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
	AnimationOutlinerFillPercentage = .25f;
	VisualLoggerEvents = FLogVisualizer::Get().GetVisualLoggerEvents();

	FVisualLoggerTimeSliderArgs TimeSliderArgs;
	TimeSliderArgs.ViewRange = InArgs._ViewRange;
	TimeSliderArgs.ClampMin = InArgs._ViewRange.Get().GetLowerBoundValue();
	TimeSliderArgs.ClampMax = InArgs._ViewRange.Get().GetUpperBoundValue();
	TimeSliderArgs.ScrubPosition = InArgs._ScrubPosition;
	FLogVisualizer::Get().GetTimeSliderController()->SetTimesliderArgs(TimeSliderArgs);

	TSharedRef<SScrollBar> ZoomScrollBar =
		SNew(SScrollBar)
		.Orientation(EOrientation::Orient_Horizontal)
		.Thickness(FVector2D(2.0f, 2.0f));
	ZoomScrollBar->SetState(0.0f, 1.0f);
	FLogVisualizer::Get().GetTimeSliderController()->SetExternalScrollbar(ZoomScrollBar);

	// Create the top and bottom sliders
	const bool bMirrorLabels = true;
	TSharedRef<ITimeSlider> TopTimeSlider = SNew(STimeSlider, FLogVisualizer::Get().GetTimeSliderController().ToSharedRef()).MirrorLabels(bMirrorLabels);
	TSharedRef<ITimeSlider> BottomTimeSlider = SNew(STimeSlider, FLogVisualizer::Get().GetTimeSliderController().ToSharedRef()).MirrorLabels(bMirrorLabels);

	TSharedRef<SScrollBar> ScrollBar =
		SNew(SScrollBar)
		.Thickness(FVector2D(2.0f, 2.0f));


	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();

	ChildSlot
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(SearchSplitter, SSplitter)
						.Orientation(Orient_Horizontal)
						.OnSplitterFinishedResizing(this, &SVisualLoggerView::OnSearchSplitterResized)
						+ SSplitter::Slot()
						.Value(0.25)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(FMargin(0))
							.AutoWidth()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Visibility_Lambda([]()->EVisibility{ return FCategoryFiltersManager::Get().GetSelectedObjects().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
								.Image(FLogVisualizerStyle::Get().GetBrush("Filters.FilterIcon"))
							]
							+ SHorizontalBox::Slot()
							.Padding(FMargin(0))
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								SAssignNew(ClassesComboButton, SComboButton)
								.Visibility_Lambda([this]()->EVisibility{ return TimelinesContainer.IsValid() && (TimelinesContainer->GetAllNodes().Num() > 1 || FCategoryFiltersManager::Get().GetSelectedObjects().Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed; })
								.ComboButtonStyle(FLogVisualizerStyle::Get(), "Filters.Style")
								.ForegroundColor(FLinearColor::White)
								.ContentPadding(0)
								.OnGetMenuContent(this, &SVisualLoggerView::MakeClassesFilterMenu)
								.ToolTipText(LOCTEXT("SetFilterByClasses", "Select classes to show"))
								.HasDownArrow(true)
								.ContentPadding(FMargin(1, 0))
								.ButtonContent()
								[
									SNew(STextBlock)
									.TextStyle(FLogVisualizerStyle::Get(), "Filters.Text")
									.Text(LOCTEXT("FilterClasses", "Classes"))
								]
							]
							+ SHorizontalBox::Slot()
							.Padding(FMargin(0))
							.HAlign(HAlign_Fill)
							.FillWidth(1)
							[
								SNew(SBox)
								.Padding(FMargin(0, 0, 4, 0))
								[
									// Search box for searching through the outliner
									SNew(SSearchBox)
									.OnTextChanged(this, &SVisualLoggerView::OnSearchChanged)
								]
							]
						]
						+ SSplitter::Slot()
						.Value(0.75)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							[
#if 0 //top time slider disabled to test idea with filter's search box
								SNew(SBorder)
								// @todo Sequencer Do not change the paddings or the sliders scrub widgets wont line up
								.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
								.BorderImage(FLogVisualizerStyle::Get().GetBrush("ToolPanel.GroupBorder"))
								.BorderBackgroundColor(FLinearColor(.50f, .50f, .50f, 1.0f))
								[
									TopTimeSlider
								]
#else
								SNew(SBox)
								.Padding(FMargin(0, 0, 4, 0))
								[
									SAssignNew(SearchBox, SSearchBox)
									.OnTextChanged(InArgs._OnFiltersSearchChanged)
									.HintText_Lambda([Settings]()->FText{return Settings->bSearchInsideLogs ? LOCTEXT("FiltersSearchHint", "Log Data Search") : LOCTEXT("FiltersSearchHint", "Log Category Search"); })
								]
#endif
							]
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0)
					[
						SNew(SInputCatcherOverlay, FLogVisualizer::Get().GetTimeSliderController().ToSharedRef())
						+ SOverlay::Slot()
						[
							MakeSectionOverlay(FLogVisualizer::Get().GetTimeSliderController().ToSharedRef(), InArgs._ViewRange, InArgs._ScrubPosition, false)
						]
						+ SOverlay::Slot()
						[
							SAssignNew(ScrollBox, SScrollBox)
							.ExternalScrollbar(ScrollBar)
							+ SScrollBox::Slot()
							[
								SAssignNew(TimelinesContainer, STimelinesContainer, SharedThis(this), FLogVisualizer::Get().GetTimeSliderController().ToSharedRef())
							]
						]
						+ SOverlay::Slot()
						[
							MakeSectionOverlay(FLogVisualizer::Get().GetTimeSliderController().ToSharedRef(), InArgs._ViewRange, InArgs._ScrubPosition, true)
						]
						+ SOverlay::Slot()
						.VAlign(VAlign_Bottom)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(TAttribute<float>(this, &SVisualLoggerView::GetAnimationOutlinerFillPercentage))
							[
								// Take up space but display nothing. This is required so that all areas dependent on time align correctly
								SNullWidget::NullWidget
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								ZoomScrollBar
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(TAttribute<float>(this, &SVisualLoggerView::GetAnimationOutlinerFillPercentage))
						[
							SNew(SSpacer)
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
						.FillWidth(1.0f)
						[
							SNew(SBorder)
							// @todo Sequencer Do not change the paddings or the sliders scrub widgets wont line up
							.Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
							.BorderImage(FLogVisualizerStyle::Get().GetBrush("ToolPanel.GroupBorder"))
							.BorderBackgroundColor(FLinearColor(.50f, .50f, .50f, 1.0f))
							[
								BottomTimeSlider
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					ScrollBar
				]
			]
		];
		
		SearchBox->SetText(FText::FromString(FCategoryFiltersManager::Get().GetSearchString()));
}

void SVisualLoggerView::SetAnimationOutlinerFillPercentage(float FillPercentage) 
{ 
	AnimationOutlinerFillPercentage = FillPercentage; 
}

void SVisualLoggerView::SetSearchString(FText SearchString) 
{ 
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(SearchString);
	}
}

void SVisualLoggerView::OnObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine)
{
	//FIXME: scroll to selected timeline (SebaK)
	//FWidgetPath WidgetPath;
	//if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(TimeLine.ToSharedRef(), WidgetPath))
	//{
	//	FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(TimeLine.ToSharedRef()).Get(FArrangedWidget::NullWidget);
	//	ScrollBox->ScrollDescendantIntoView(ArrangedWidget.Geometry, TimeLine, true);
	//}
}

void SVisualLoggerView::OnSearchSplitterResized()
{
	SSplitter::FSlot const& LeftSplitterSlot = SearchSplitter->SlotAt(0);
	SSplitter::FSlot const& RightSplitterSlot = SearchSplitter->SlotAt(1);

	SetAnimationOutlinerFillPercentage(LeftSplitterSlot.SizeValue.Get() / RightSplitterSlot.SizeValue.Get());
}

void SVisualLoggerView::OnSearchChanged(const FText& Filter)
{
	TimelinesContainer->OnSearchChanged(Filter);
}

TSharedRef<SWidget> SVisualLoggerView::MakeSectionOverlay(TSharedRef<FVisualLoggerTimeSliderController> TimeSliderController, const TAttribute< TRange<float> >& ViewRange, const TAttribute<float>& ScrubPosition, bool bTopOverlay)
{
	return
		SNew(SHorizontalBox)
		.Visibility(EVisibility::HitTestInvisible)
		+ SHorizontalBox::Slot()
		.FillWidth(TAttribute<float>(this, &SVisualLoggerView::GetAnimationOutlinerFillPercentage))
		[
			// Take up space but display nothing. This is required so that all areas dependent on time align correctly
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVisualLoggerSectionOverlay, TimeSliderController)
			.DisplayScrubPosition(bTopOverlay)
			.DisplayTickLines(!bTopOverlay)
		];
}

void SVisualLoggerView::ResetData()
{
	TimelinesContainer->ResetData();
}

void SVisualLoggerView::OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	TimelinesContainer->OnNewLogEntry(Entry);
}

void SVisualLoggerView::OnFiltersChanged()
{
	TimelinesContainer->OnFiltersChanged();
}

void SVisualLoggerView::OnFiltersSearchChanged(const FText& Filter)
{
	TimelinesContainer->OnFiltersSearchChanged(Filter);
}

FCursorReply SVisualLoggerView::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (FLogVisualizer::Get().GetTimeSliderController()->IsPanning())
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Cursor(EMouseCursor::Default);
}

TSharedRef<SWidget> SVisualLoggerView::MakeClassesFilterMenu()
{
	const TArray< TSharedPtr<class STimeline> >& AllTimelines = TimelinesContainer->GetAllNodes();

	FMenuBuilder MenuBuilder(true, NULL);

	TArray<FString> UniqueClasses;
	MenuBuilder.BeginSection(TEXT("Graphs"));
	for (TSharedPtr<class STimeline> CurrentTimeline : AllTimelines)
	{
		FString OwnerClassName = CurrentTimeline->GetOwnerClassName().ToString();
		if (UniqueClasses.Find(OwnerClassName) == INDEX_NONE)
		{
			FText LabelText = FText::FromString(OwnerClassName);
			MenuBuilder.AddMenuEntry(
				LabelText,
				FText::Format(LOCTEXT("FilterByClassPrefix", "Toggle {0} class"), LabelText),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateLambda([this, OwnerClassName]()
				{
				if (FCategoryFiltersManager::Get().MatchObjectName(OwnerClassName) && FCategoryFiltersManager::Get().GetSelectedObjects().Num() != 0)
					{
						FCategoryFiltersManager::Get().RemoveObjectFromSelection(OwnerClassName);
					}
					else
					{
						FCategoryFiltersManager::Get().SelectObject(OwnerClassName);
					}

					OnChangedClassesFilter();
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([OwnerClassName]()->bool
				{
					return FCategoryFiltersManager::Get().GetSelectedObjects().Find(OwnerClassName) != INDEX_NONE;
				}),
				FIsActionButtonVisible()),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
				);
			UniqueClasses.AddUnique(OwnerClassName);
		}
	}
	//show any classes from persistent data
	for (const FString& SelectedObj : FCategoryFiltersManager::Get().GetSelectedObjects())
	{
		if (UniqueClasses.Find(SelectedObj) == INDEX_NONE)
		{
			FText LabelText = FText::FromString(SelectedObj);
			MenuBuilder.AddMenuEntry(
			LabelText,
			FText::Format(LOCTEXT("FilterByClassPrefix", "Toggle {0} class"), LabelText),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateLambda([this, SelectedObj]()
			{
				if (FCategoryFiltersManager::Get().MatchObjectName(SelectedObj) && FCategoryFiltersManager::Get().GetSelectedObjects().Num() != 0)
				{
					FCategoryFiltersManager::Get().RemoveObjectFromSelection(SelectedObj);
				}
				else
				{
					FCategoryFiltersManager::Get().SelectObject(SelectedObj);
				}

				OnChangedClassesFilter();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([SelectedObj]()->bool
			{
				return FCategoryFiltersManager::Get().GetSelectedObjects().Find(SelectedObj) != INDEX_NONE;
			}),
			FIsActionButtonVisible()),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
			UniqueClasses.AddUnique(SelectedObj);
		}
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

void SVisualLoggerView::OnChangedClassesFilter()
{
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->SaveConfig();
	for (auto CurrentItem : TimelinesContainer->GetAllNodes())
	{
		CurrentItem->UpdateVisibility();
	}
}

#undef LOCTEXT_NAMESPACE
