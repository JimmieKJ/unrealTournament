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
	void Construct(const FArguments& InArgs, TSharedRef<class FSequencerTimeSliderController> InTimeSliderController)
	{
		SOverlay::Construct(InArgs);
		TimeSliderController = InTimeSliderController;
	}

	/** Controller for manipulating time */
	TSharedPtr<class FSequencerTimeSliderController> TimeSliderController;
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

void SVisualLoggerView::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<IVisualLoggerInterface> InVisualLoggerInterface)
{
	VisualLoggerInterface = InVisualLoggerInterface;
	AnimationOutlinerFillPercentage = .25f;
	VisualLoggerEvents = VisualLoggerInterface->GetVisualLoggerEvents();

	FVisualLoggerTimeSliderArgs TimeSliderArgs;
	TimeSliderArgs.ViewRange = InArgs._ViewRange;
	TimeSliderArgs.ClampMin = InArgs._ViewRange.Get().GetLowerBoundValue();
	TimeSliderArgs.ClampMax = InArgs._ViewRange.Get().GetUpperBoundValue();
	TimeSliderArgs.ScrubPosition = InArgs._ScrubPosition;

	TSharedRef<SScrollBar> ZoomScrollBar =
		SNew(SScrollBar)
		.Orientation(EOrientation::Orient_Horizontal)
		.Thickness(FVector2D(2.0f, 2.0f));
	ZoomScrollBar->SetState(0.0f, 1.0f);

	TSharedPtr<FSequencerTimeSliderController> TimeSliderController(new FSequencerTimeSliderController(TimeSliderArgs));
	TimeSliderController->SetExternalScrollbar(ZoomScrollBar);
	VisualLoggerInterface->SetTimeSliderController(TimeSliderController);

	// Create the top and bottom sliders
	const bool bMirrorLabels = true;
	TSharedRef<ITimeSlider> TopTimeSlider = SNew(STimeSlider, TimeSliderController.ToSharedRef()).MirrorLabels(bMirrorLabels);
	TSharedRef<ITimeSlider> BottomTimeSlider = SNew(STimeSlider, TimeSliderController.ToSharedRef()).MirrorLabels(bMirrorLabels);

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
							.VAlign(VAlign_Center)
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
						SNew(SInputCatcherOverlay, TimeSliderController.ToSharedRef())
						+ SOverlay::Slot()
						[
							MakeSectionOverlay(TimeSliderController.ToSharedRef(), InArgs._ViewRange, InArgs._ScrubPosition, false)
						]
						+ SOverlay::Slot()
						[
							SAssignNew(ScrollBox, SScrollBox)
							.ExternalScrollbar(ScrollBar)
							+ SScrollBox::Slot()
							[
								SAssignNew(TimelinesContainer, STimelinesContainer, SharedThis(this), TimeSliderController.ToSharedRef())
								.VisualLoggerInterface(VisualLoggerInterface)
							]
						]
						+ SOverlay::Slot()
						[
							MakeSectionOverlay(TimeSliderController.ToSharedRef(), InArgs._ViewRange, InArgs._ScrubPosition, true)
						]
						+ SOverlay::Slot()
						.VAlign(VAlign_Bottom)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							//.Visibility(EVisibility::HitTestInvisible)
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

		//ScrollBox->AddSlot()
		//[
		//	SAssignNew(TimelinesContainer, STimelinesContainer, SharedThis(this), TimeSliderController.ToSharedRef())
		//	.VisualLoggerInterface(VisualLoggerInterface)
		//];

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

TSharedRef<SWidget> SVisualLoggerView::MakeSectionOverlay(TSharedRef<FSequencerTimeSliderController> TimeSliderController, const TAttribute< TRange<float> >& ViewRange, const TAttribute<float>& ScrubPosition, bool bTopOverlay)
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
			SNew(SSequencerSectionOverlay, TimeSliderController)
			.DisplayScrubPosition(bTopOverlay)
			.DisplayTickLines(!bTopOverlay)
		];
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
	if (VisualLoggerInterface->GetTimeSliderController()->IsPanning())
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Cursor(EMouseCursor::Default);
}


#undef LOCTEXT_NAMESPACE
