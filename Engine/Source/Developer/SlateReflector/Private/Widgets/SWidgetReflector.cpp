// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateReflectorPrivatePCH.h"
#include "ISlateReflectorModule.h"
#include "SlateStats.h"

#define LOCTEXT_NAMESPACE "SWidgetReflector"
#define WITH_EVENT_LOGGING 0

extern SLATECORE_API int32 bFoldTick;
#if SLATE_STATS
extern SLATECORE_API int32 GSlateStatsFlatEnable;
extern SLATECORE_API int32 GSlateStatsFlatLogOutput;
extern SLATECORE_API int32 GSlateStatsHierarchyTrigger;
extern SLATECORE_API float GSlateStatsFlatIntervalWindowSec;
#endif
static const int32 MaxLoggedEvents = 100;

/**
 * Widget reflector user widget.
 */

/* Local helpers
 *****************************************************************************/

struct FLoggedEvent
{
	FLoggedEvent( const FInputEvent& InEvent, const FReplyBase& InReply )
		: Event(InEvent)
		, Handler(InReply.GetHandler())
		, EventText(InEvent.ToText())
		, HandlerText(InReply.GetHandler().IsValid() ? FText::FromString(InReply.GetHandler()->ToString()) : LOCTEXT("NullHandler", "null"))
	{ }

	FText ToText()
	{
		return FText::Format(NSLOCTEXT("","","{0}  |  {1}"), EventText, HandlerText);
	}
	
	FInputEvent Event;
	TWeakPtr<SWidget> Handler;
	FText EventText;
	FText HandlerText;
};

namespace WidgetReflectorImpl
{

/**
 * Widget reflector implementation
 */
class SWidgetReflector : public ::SWidgetReflector
{
	// The reflector uses a tree that observes FReflectorNodes.
	typedef STreeView<TSharedPtr<FReflectorNode>> SReflectorTree;

private:

	virtual void Construct( const FArguments& InArgs ) override;

	// SCompoundWidget overrides
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	// IWidgetReflector interface

	virtual void OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply ) override;

	virtual bool IsInPickingMode() const override
	{
		return bIsPicking;
	}

	virtual bool IsShowingFocus() const override
	{
		return bShowFocus;
	}

	virtual bool IsVisualizingLayoutUnderCursor() const override
	{
		return bIsPicking;
	}

	virtual void OnWidgetPicked() override
	{
		bIsPicking = false;
	}

	virtual bool ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const override;

	virtual void SetSourceAccessDelegate( FAccessSourceCode InDelegate ) override
	{
		SourceAccessDelegate = InDelegate;
	}

	virtual void SetAssetAccessDelegate(FAccessAsset InDelegate) override
	{
		AsseetAccessDelegate = InDelegate;
	}

	virtual void SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize ) override;
	virtual int32 Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId ) override;
	virtual int32 VisualizeCursorAndKeys(FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;

	/**
	 * Generates a tool tip for the given reflector tree node.
	 *
	 * @param InReflectorNode The node to generate the tool tip for.
	 * @return The tool tip widget.
	 */
	TSharedRef<SToolTip> GenerateToolTipForReflectorNode( TSharedPtr<FReflectorNode> InReflectorNode );

	/**
	 * Mark the provided reflector nodes such that they stand out in the tree and are visible.
	 *
	 * @param WidgetPathToObserve The nodes to mark.
	 */
	void VisualizeAsTree( const TArray< TSharedPtr<FReflectorNode> >& WidgetPathToVisualize );

	/**
	 * Draw the widget path to the picked widget as the widgets' outlines.
	 *
	 * @param InWidgetsToVisualize A widget path whose widgets' outlines to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId The maximum layer achieved in OutDrawElements so far.
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizePickAsRectangles( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId );

	/**
	 * Draw an outline for the specified nodes.
	 *
	 * @param InNodesToDraw A widget path whose widgets' outlines to draw.
	 * @param WindowGeometry The geometry of the window in which to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId the maximum layer achieved in OutDrawElements so far.
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizeSelectedNodesAsRectangles( const TArray<TSharedPtr<FReflectorNode>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId );

	/** Callback for changing the application scale slider. */
	void HandleAppScaleSliderChanged( float NewValue )
	{
		FSlateApplication::Get().SetApplicationScale(NewValue);
	}

	FReply HandleDisplayTextureAtlases();
	FReply HandleDisplayFontAtlases();

	/** Callback for getting the value of the application scale slider. */
	float HandleAppScaleSliderValue() const
	{
		return FSlateApplication::Get().GetApplicationScale();
	}

	/** Callback for checked state changes of the focus check box. */
	void HandleFocusCheckBoxCheckedStateChanged( ECheckBoxState NewValue );

	/** Callback for getting the checked state of the focus check box. */
	ECheckBoxState HandleFocusCheckBoxIsChecked() const
	{
		return bShowFocus ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Callback for getting the text of the frame rate text block. */
	FString HandleFrameRateText() const;

	/** Callback for clicking the pick button. */
	FReply HandlePickButtonClicked()
	{
		bIsPicking = !bIsPicking;

		if (bIsPicking)
		{
			bShowFocus = false;
		}

		return FReply::Handled();
	}

	/** Callback for getting the color of the pick button text. */
	FSlateColor HandlePickButtonColorAndOpacity() const
	{
		static const FName SelectionColor("SelectionColor");
		static const FName DefaultForeground("DefaultForeground");

		return bIsPicking
			? FCoreStyle::Get().GetSlateColor(SelectionColor)
			: FCoreStyle::Get().GetSlateColor(DefaultForeground);
	}

	/** Callback for getting the text of the pick button. */
	FText HandlePickButtonText() const;

	/** Callback for generating a row in the reflector tree view. */
	TSharedRef<ITableRow> HandleReflectorTreeGenerateRow( TSharedPtr<FReflectorNode> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the child items of the given reflector tree node. */
	void HandleReflectorTreeGetChildren( TSharedPtr<FReflectorNode> InWidgetGeometry, TArray<TSharedPtr<FReflectorNode>>& OutChildren );

	/** Callback for when the selection in the reflector tree has changed. */
	void HandleReflectorTreeSelectionChanged( TSharedPtr< FReflectorNode >, ESelectInfo::Type /*SelectInfo*/ )
	{
		SelectedNodes = ReflectorTree->GetSelectedItems();
	}

	TSharedRef<ITableRow> GenerateEventLogRow( TSharedRef<FLoggedEvent> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	TArray< TSharedRef<FLoggedEvent> > LoggedEvents;
	TSharedPtr< SListView< TSharedRef< FLoggedEvent > > > EventListView;
	TSharedPtr< SHorizontalBox > StatsToolsBox;
	TSharedPtr<SReflectorTree> ReflectorTree;

	TArray<TSharedPtr<FReflectorNode>> SelectedNodes;
	TArray<TSharedPtr<FReflectorNode>> ReflectorTreeRoot;
	TArray<TSharedPtr<FReflectorNode>> PickedPath;

	SSplitter::FSlot* WidgetInfoLocation;

	FAccessSourceCode SourceAccessDelegate;
	FAccessAsset AsseetAccessDelegate;

	bool bShowFocus;
	bool bIsPicking;

#if SLATE_STATS
	// STATS
	TSharedPtr<SBorder> StatsBorder;
	TArray< TSharedRef<class FStatItem> > StatsItems;
	TSharedPtr< SListView< TSharedRef<FStatItem> > > StatsList;

	TSharedRef<SWidget> MakeStatViewer();
	void UpdateStats();
	TSharedRef<ITableRow> GenerateStatRow(TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable);
#endif
private:
	// DEMO MODE
	bool bEnableDemoMode;
	double LastMouseClickTime;
	FVector2D CursorPingPosition;
};
















void SWidgetReflector::Construct( const FArguments& InArgs )
{
	LoggedEvents.Reserve(MaxLoggedEvents);

	bShowFocus = false;
	bIsPicking = false;

	bEnableDemoMode = false;
	LastMouseClickTime = -1;
	CursorPingPosition = FVector2D::ZeroVector;

	this->ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
					
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("AppScale", "Application Scale: "))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(100)
							.MaxDesiredWidth(250)
							[
								SNew(SSpinBox<float>)
									.Value(this, &SWidgetReflector::HandleAppScaleSliderValue)
									.MinValue(0.1f)
									.MaxValue(3.0f)
									.Delta(0.01f)
									.OnValueChanged(this, &SWidgetReflector::HandleAppScaleSliderChanged)
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SSpacer)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(5.0f, 0.0f))
						[
							SNew(SButton)
							.Text(LOCTEXT("DisplayTextureAtlases", "Display Texture Atlases"))
							.OnClicked(this, &SWidgetReflector::HandleDisplayTextureAtlases)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(5.0f, 0.0f))
						[
							SNew(SButton)
							.Text(LOCTEXT("DisplayFontAtlases", "Display Font Atlases"))
							.OnClicked(this, &SWidgetReflector::HandleDisplayFontAtlases)
						]
					]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						// Check box that controls LIVE MODE
						SNew(SCheckBox)
						.IsChecked(this, &SWidgetReflector::HandleFocusCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SWidgetReflector::HandleFocusCheckBoxCheckedStateChanged)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowFocus", "Show Focus"))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						// Check box that controls PICKING A WIDGET TO INSPECT
						SNew(SButton)
							.OnClicked(this, &SWidgetReflector::HandlePickButtonClicked)
							.ButtonColorAndOpacity(this, &SWidgetReflector::HandlePickButtonColorAndOpacity)
							[
								SNew(STextBlock)
									.Text(this, &SWidgetReflector::HandlePickButtonText)
							]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SCheckBox)
						.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
						.IsChecked_Lambda([]()
						{
							return bFoldTick == 0 ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
						})
						.OnCheckStateChanged_Lambda([]( const ECheckBoxState NewState )
						{
							bFoldTick = (NewState == ECheckBoxState::Checked) ? 1 : 0;
						})
						[
							SNew(SBox)
							.VAlign( VAlign_Center )
							.HAlign( HAlign_Center )
							[
								SNew(STextBlock)	
								.Text( LOCTEXT("ToggleTickFolding", "Fold Tick") )
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SCheckBox)
						.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([&]()
						{
							return bEnableDemoMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([&](const ECheckBoxState NewState)
						{
							bEnableDemoMode = (NewState == ECheckBoxState::Checked) ? true : false;
						})
						[
							SNew(SBox)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("EnableDemoMode", "Demo Mode"))
							]
						]
					]
#if SLATE_STATS
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SCheckBox)
						.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
						.IsChecked_Static([]
						{
							return GSlateStatsFlatEnable == 0 ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
						})
						.OnCheckStateChanged_Lambda( [=]( const ECheckBoxState NewState )
						{
							GSlateStatsFlatEnable = (NewState == ECheckBoxState::Checked) ? 1 : 0;
							StatsToolsBox->SetVisibility(GSlateStatsFlatEnable != 0 ? EVisibility::Visible : EVisibility::Collapsed);
						})
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("ToggleStatsTooltip", "Enables flat stats view.") )
							]
						)
						[
							SNew(SBox)
							.VAlign( VAlign_Center )
							.HAlign( HAlign_Center )
							[
								SNew(STextBlock)	
								.Text( LOCTEXT("ToggleStats", "Toggle Stats") )
							]
						]
					]
#else
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SCheckBox)
						.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("ToggleStatsUnavailableTooltip", "To enable slate stats, compile with SLATE_STATS defined to one (see SlateStats.h).") )
							]
						)
						[
							SNew(SBox)
							.VAlign( VAlign_Center )
							.HAlign( HAlign_Center )
							[
								SNew(STextBlock)	
								.Text( LOCTEXT("ToggleStatsUnavailable", "Toggle Stats [see toolip]") )
							]
						]
						.IsEnabled(false)
					]
#endif
				]
#if SLATE_STATS
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(StatsToolsBox, SHorizontalBox)
					.Visibility(EVisibility::Collapsed)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SCheckBox)
						.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
						.IsChecked_Static([]
						{
							return GSlateStatsFlatLogOutput == 0 ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
						})
						.OnCheckStateChanged_Static([]( const ECheckBoxState NewState )
						{
							GSlateStatsFlatLogOutput = (NewState == ECheckBoxState::Checked) ? 1 : 0;
						})
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("LogStatsTooltip", "Enables outputting stats to the log at the given interval.") )
							]
						)
						[
							SNew(SBox)
							.VAlign( VAlign_Center )
							.HAlign( HAlign_Center )
							[
								SNew(STextBlock)	
								.Text( LOCTEXT("ToggleLogStats", "Log Stats") )
							]
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SButton)
						.OnClicked_Static([]
						{
							GSlateStatsHierarchyTrigger = 1;
							return FReply::Handled();
						})
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("CaptureStatsHierarchyTooltip", "When clicked, the next rendered frame will capture hierarchical stats and save them to file in the Saved/ folder with the following name: SlateHierarchyStats-<timestamp>.csv") )
							]
						)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("CaptureHierarchy", "Capture Hierarchy"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("StatsSamplingIntervalTooltip", "the interval (in seconds) to integrate stats before updating the averages.") )
							]
						)
						.Text(LOCTEXT("StatsSampleWindow", "Sampling Interval: "))
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SSpinBox<float>)
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("StatsSamplingIntervalTooltip", "the interval (in seconds) to integrate stats before updating the stats.") )
							]
						)
						.Value_Static([]
						{
							return GSlateStatsFlatIntervalWindowSec;
						})
						.MinValue(0.1f)
						.MaxValue(15.0f)
						.Delta(0.1f)
						.OnValueChanged_Static([](float NewValue)
						{
							GSlateStatsFlatIntervalWindowSec = NewValue;
						})
					]
				]
#endif
				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						// The tree view that shows all the info that we capture.
						SAssignNew(ReflectorTree, SReflectorTree)
						.ItemHeight(24.0f)
						.TreeItemsSource(&ReflectorTreeRoot)
						.OnGenerateRow(this, &SWidgetReflector::HandleReflectorTreeGenerateRow)
						.OnGetChildren(this, &SWidgetReflector::HandleReflectorTreeGetChildren)
						.OnSelectionChanged(this, &SWidgetReflector::HandleReflectorTreeSelectionChanged)
						.HeaderRow
						(
							SNew(SHeaderRow)

							+ SHeaderRow::Column("WidgetName")
							.DefaultLabel(LOCTEXT("WidgetName", "Widget Name"))
							.FillWidth(0.65f)

							+ SHeaderRow::Column("ForegroundColor")
							.FixedWidth(24.0f)
							.VAlignHeader(VAlign_Center)
							.HeaderContent()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ForegroundColor", "FG"))
								.ToolTipText(LOCTEXT("ForegroundColorToolTip", "Foreground Color"))
							]

							+ SHeaderRow::Column("Visibility")
							.DefaultLabel(LOCTEXT("Visibility", "Visibility" ))
							.FixedWidth(125.0f)

							+ SHeaderRow::Column("WidgetInfo")
							.DefaultLabel(LOCTEXT("WidgetInfo", "Widget Info" ))
							.FillWidth(0.25f)

							+ SHeaderRow::Column("Address")
							.DefaultLabel( LOCTEXT("Address", "Address") )
							.FillWidth( 0.10f )
						)
					]

				#if WITH_EVENT_LOGGING
				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(EventListView, SListView<TSharedRef<FLoggedEvent>>)
							.ListItemsSource( &LoggedEvents )
							.OnGenerateRow(this, &SWidgetReflector::GenerateEventLogRow)
					]
				#endif //WITH_EVENT_LOGGING
#if SLATE_STATS			
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeStatViewer()
					]
#endif
			]
	];

#if SLATE_STATS			
	if (StatsToolsBox.IsValid())
	{
		StatsToolsBox->SetVisibility(GSlateStatsFlatEnable != 0 ? EVisibility::Visible : EVisibility::Collapsed);
	}
#endif
}

/* SCompoundWidget overrides
 *****************************************************************************/

void SWidgetReflector::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
#if SLATE_STATS
	UpdateStats();
#endif
}


void SWidgetReflector::OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply )
{
	if ( Event.IsPointerEvent() )
	{
		const FPointerEvent& PtrEvent = static_cast<const FPointerEvent&>(Event);
		if (PtrEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			LastMouseClickTime = FSlateApplication::Get().GetCurrentTime();
			CursorPingPosition = PtrEvent.GetScreenSpacePosition();
		}
	}

	#if WITH_EVENT_LOGGING
		if (LoggedEvents.Num() >= MaxLoggedEvents)
		{
			LoggedEvents.Empty();
		}

		LoggedEvents.Add(MakeShareable(new FLoggedEvent(Event, InReply)));
		EventListView->RequestListRefresh();
		EventListView->RequestScrollIntoView(LoggedEvents.Last());
	#endif //WITH_EVENT_LOGGING
}


/* IWidgetReflector overrides
 *****************************************************************************/

bool SWidgetReflector::ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const
{
	return ((SelectedNodes.Num() > 0) && (ReflectorTreeRoot.Num() > 0) && (ReflectorTreeRoot[0]->Widget.Pin() == ThisWindow));
}


void SWidgetReflector::SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize )
{
	ReflectorTreeRoot.Empty();

	if (InWidgetsToVisualize.IsValid())
	{
		ReflectorTreeRoot.Add(FReflectorNode::NewTreeFrom(InWidgetsToVisualize.Widgets[0]));
		PickedPath.Empty();

		FReflectorNode::FindWidgetPath( ReflectorTreeRoot, InWidgetsToVisualize, PickedPath );
		VisualizeAsTree(PickedPath);
	}
	
	ReflectorTree->RequestTreeRefresh();
}


int32 SWidgetReflector::Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId )
{
	const bool bAttemptingToVisualizeReflector = InWidgetsToVisualize.ContainsWidget(ReflectorTree.ToSharedRef());

	if (!InWidgetsToVisualize.IsValid())
	{
		TSharedPtr<SWidget> WindowWidget = ReflectorTreeRoot[0]->Widget.Pin();
		TSharedPtr<SWindow> Window = StaticCastSharedPtr<SWindow>(WindowWidget);

		return VisualizeSelectedNodesAsRectangles(SelectedNodes, Window.ToSharedRef(), OutDrawElements, LayerId);
	}

	if (!bAttemptingToVisualizeReflector)
	{
		SetWidgetsToVisualize(InWidgetsToVisualize);

		return VisualizePickAsRectangles(InWidgetsToVisualize, OutDrawElements, LayerId);
	}		

	return LayerId;
}

int32 SWidgetReflector::VisualizeCursorAndKeys(FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (bEnableDemoMode)
	{
		static const float ClickFadeTime = 0.5f;
		static const float PingScaleAmount = 3.0f;
		static const FName CursorPingBrush("DemoRecording.CursorPing");
		const TSharedPtr<SWindow> WindowBeingDrawn = OutDrawElements.GetWindow();

		// Normalized animation value for the cursor ping between 0 and 1.
		const float AnimAmount = (FSlateApplication::Get().GetCurrentTime() - LastMouseClickTime) / ClickFadeTime;

		if (WindowBeingDrawn.IsValid() && AnimAmount <= 1.0f)
		{
			const FVector2D CursorPosDesktopSpace = CursorPingPosition;
			const FVector2D CursorSize = FSlateApplication::Get().GetCursorSize();
			const FVector2D PingSize = CursorSize*PingScaleAmount*FCurveHandle::ApplyEasing(AnimAmount, ECurveEaseFunction::QuadOut);
			const FLinearColor PingColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f - FCurveHandle::ApplyEasing(AnimAmount, ECurveEaseFunction::QuadIn));

			FGeometry CursorHighlightGeometry = FGeometry::MakeRoot(PingSize, FSlateLayoutTransform(CursorPosDesktopSpace - PingSize / 2));
			CursorHighlightGeometry.AppendTransform(Inverse(WindowBeingDrawn->GetLocalToScreenTransform()));

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				CursorHighlightGeometry.ToPaintGeometry(),
				FCoreStyle::Get().GetBrush(CursorPingBrush),
				OutDrawElements.GetWindow()->GetClippingRectangleInWindow(),
				ESlateDrawEffect::None,
				PingColor
				);
		}
	}
	
	return LayerId;
}


/* SWidgetReflector implementation
 *****************************************************************************/

TSharedRef<SToolTip> SWidgetReflector::GenerateToolTipForReflectorNode( TSharedPtr<FReflectorNode> InReflectorNode )
{
	return SNew(SToolTip)
		[
			SNew(SReflectorToolTipWidget)
				.WidgetInfoToVisualize(InReflectorNode)
		];
}


void SWidgetReflector::VisualizeAsTree( const TArray<TSharedPtr<FReflectorNode>>& WidgetPathToVisualize )
{
	const FLinearColor TopmostWidgetColor(1.0f, 0.0f, 0.0f);
	const FLinearColor LeafmostWidgetColor(0.0f, 1.0f, 0.0f);

	for (int32 WidgetIndex = 0; WidgetIndex<WidgetPathToVisualize.Num(); ++WidgetIndex)
	{
		TSharedPtr<FReflectorNode> CurWidget = WidgetPathToVisualize[WidgetIndex];

		// Tint the item based on depth in picked path
		const float ColorFactor = static_cast<float>(WidgetIndex)/WidgetPathToVisualize.Num();
		CurWidget->Tint = FMath::Lerp(TopmostWidgetColor, LeafmostWidgetColor, ColorFactor);

		// Make sure the user can see the picked path in the tree.
		ReflectorTree->SetItemExpansion(CurWidget, true);
	}

	ReflectorTree->RequestScrollIntoView(WidgetPathToVisualize.Last());
	ReflectorTree->SetSelection(WidgetPathToVisualize.Last());
}


int32 SWidgetReflector::VisualizePickAsRectangles( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	const FLinearColor TopmostWidgetColor(1.0f, 0.0f, 0.0f);
	const FLinearColor LeafmostWidgetColor(0.0f, 1.0f, 0.0f);

	for (int32 WidgetIndex = 0; WidgetIndex < InWidgetsToVisualize.Widgets.Num(); ++WidgetIndex)
	{
		const FArrangedWidget& WidgetGeometry = InWidgetsToVisualize.Widgets[WidgetIndex];
		const float ColorFactor = static_cast<float>(WidgetIndex)/InWidgetsToVisualize.Widgets.Num();
		const FLinearColor Tint(1.0f - ColorFactor, ColorFactor, 0.0f, 1.0f);

		// The FGeometry we get is from a WidgetPath, so it's rooted in desktop space.
		// We need to APPEND a transform to the Geometry to essentially undo this root transform
		// and get us back into Window Space.
		// This is nonstandard so we have to go through some hoops and a specially exposed method 
		// in FPaintGeometry to allow appending layout transforms.
		FPaintGeometry WindowSpaceGeometry = WidgetGeometry.Geometry.ToPaintGeometry();
		WindowSpaceGeometry.AppendTransform(TransformCast<FSlateLayoutTransform>(Inverse(InWidgetsToVisualize.TopLevelWindow->GetPositionInScreen())));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			WindowSpaceGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			InWidgetsToVisualize.TopLevelWindow->GetClippingRectangleInWindow(),
			ESlateDrawEffect::None,
			FMath::Lerp(TopmostWidgetColor, LeafmostWidgetColor, ColorFactor)
		);
	}

	return LayerId;
}


int32 SWidgetReflector::VisualizeSelectedNodesAsRectangles( const TArray<TSharedPtr<FReflectorNode>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId )
{
	for (int32 NodeIndex = 0; NodeIndex < InNodesToDraw.Num(); ++NodeIndex)
	{
		const TSharedPtr<FReflectorNode>& NodeToDraw = InNodesToDraw[NodeIndex];
		const FLinearColor Tint(0.0f, 1.0f, 0.0f);

		// The FGeometry we get is from a WidgetPath, so it's rooted in desktop space.
		// We need to APPEND a transform to the Geometry to essentially undo this root transform
		// and get us back into Window Space.
		// This is nonstandard so we have to go through some hoops and a specially exposed method 
		// in FPaintGeometry to allow appending layout transforms.
		FPaintGeometry WindowSpaceGeometry = NodeToDraw->Geometry.ToPaintGeometry();
		WindowSpaceGeometry.AppendTransform(TransformCast<FSlateLayoutTransform>(Inverse(VisualizeInWindow->GetPositionInScreen())));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			WindowSpaceGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			VisualizeInWindow->GetClippingRectangleInWindow(),
			ESlateDrawEffect::None,
			NodeToDraw->Tint
		);
	}

	return LayerId;
}


/* SWidgetReflector callbacks
 *****************************************************************************/

FReply SWidgetReflector::HandleDisplayTextureAtlases()
{
	static const FName SlateReflectorModuleName("SlateReflector");
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayTextureAtlasVisualizer();
	return FReply::Handled();
}

FReply SWidgetReflector::HandleDisplayFontAtlases()
{
	static const FName SlateReflectorModuleName("SlateReflector");
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayFontAtlasVisualizer();
	return FReply::Handled();
}

void SWidgetReflector::HandleFocusCheckBoxCheckedStateChanged( ECheckBoxState NewValue )
{
	bShowFocus = NewValue != ECheckBoxState::Unchecked;

	if (bShowFocus)
	{
		bIsPicking = false;
	}
}


FString SWidgetReflector::HandleFrameRateText() const
{
	FString MyString;
#if 0 // the new stats system does not support this
	MyString = FString::Printf(TEXT("FPS: %0.2f (%0.2f ms)"), (float)( 1.0f / FPSCounter.GetAverage()), (float)FPSCounter.GetAverage() * 1000.0f);
#endif

	return MyString;
}


FText SWidgetReflector::HandlePickButtonText() const
{
	static const FText NotPicking = LOCTEXT("PickWidget", "Pick Widget");
	static const FText Picking = LOCTEXT("PickingWidget", "Picking (Esc to Stop)");

	return bIsPicking ? Picking : NotPicking;
}


TSharedRef<ITableRow> SWidgetReflector::HandleReflectorTreeGenerateRow( TSharedPtr<FReflectorNode> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SReflectorTreeWidgetItem, OwnerTable)
		.WidgetInfoToVisualize(InReflectorNode)
		.ToolTip(GenerateToolTipForReflectorNode(InReflectorNode))
		.SourceCodeAccessor(SourceAccessDelegate)
		.AssetAccessor(AsseetAccessDelegate);
}


void SWidgetReflector::HandleReflectorTreeGetChildren(TSharedPtr<FReflectorNode> InWidgetGeometry, TArray<TSharedPtr<FReflectorNode>>& OutChildren)
{
	OutChildren = InWidgetGeometry->ChildNodes;
}


TSharedRef<ITableRow> SWidgetReflector::GenerateEventLogRow( TSharedRef<FLoggedEvent> InLoggedEvent, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedRef<FLoggedEvent>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(InLoggedEvent->ToText())
	];
}

#if SLATE_STATS

//
// STATS
//

class FStatItem
{
public:
	
	FStatItem(FSlateStatCycleCounter* InCounter)
	: Counter(InCounter)
	{
		UpdateValues();
	}

	FText GetStatName() const { return StatName; }
	FText GetInclusiveAvgMsText() const { return InclusiveAvgMsText; }
	float GetInclusiveAvgMs() const { return InclusiveAvgMs; }
	void UpdateValues()
	{
		StatName = FText::FromName(Counter->GetName());
		InclusiveAvgMsText = FText::AsNumber(Counter->GetLastComputedAverageInclusiveTime(), &FNumberFormattingOptions().SetMinimumIntegralDigits(1).SetMinimumFractionalDigits(3).SetMaximumFractionalDigits(3));
		InclusiveAvgMs = (float)(Counter->GetLastComputedAverageInclusiveTime());
	}
private:
	FSlateStatCycleCounter* Counter;
	FText StatName;
	FText InclusiveAvgMsText;
	float InclusiveAvgMs;
};

static const FName ColumnId_StatName("StatName");
static const FName ColumnId_InclusiveAvgMs("InclusiveAvgMs");
static const FName ColumnId_InclusiveAvgMsGraph("InclusiveAvgMsGraph");

TSharedRef<SWidget> SWidgetReflector::MakeStatViewer()
{
	// the list of registered counters must remain constant throughout program execution.
	// As long as all counters are declared globally this will be true.
	for (auto Stat : FSlateStatCycleCounter::GetRegisteredCounters())
	{
		StatsItems.Add(MakeShareable(new FStatItem(Stat)));
	}

	return
		SAssignNew(StatsBorder, SBorder)
		.BorderImage( FCoreStyle::Get().GetBrush("NoBrush") )
		.Visibility_Lambda([]
		{
			return GSlateStatsFlatEnable > 0 ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			// STATS LIST
			SAssignNew( StatsList, SListView< TSharedRef<FStatItem> > )
			.OnGenerateRow( this, &SWidgetReflector::GenerateStatRow )
			.ListItemsSource( &StatsItems )
			.HeaderRow
			(
				// STATS HEADER
				SNew(SHeaderRow)
				
				+SHeaderRow::Column(ColumnId_StatName)
				.FillWidth( 5.0f )
				.HAlignCell(HAlign_Right)
				.DefaultLabel( LOCTEXT("Stats_StatNameColumn", "Statistic") )

				+SHeaderRow::Column(ColumnId_InclusiveAvgMs)
				.FixedWidth(80.0f)
				.HAlignCell(HAlign_Right)
				.DefaultLabel( LOCTEXT("Stats_InclusiveAvgMsColumn", "AvgTime (ms)") )

				+SHeaderRow::Column(ColumnId_InclusiveAvgMsGraph)
				.FillWidth( 7.0f )
				.DefaultLabel( LOCTEXT("Stats_InclusiveAvgMsGraphColumn", " ") )
			)
		];
}

void SWidgetReflector::UpdateStats()
{
	if (FSlateStatCycleCounter::AverageInclusiveTimesWereUpdatedThisFrame())
	{
		for (auto& StatsItem : StatsItems)
		{
			StatsItem->UpdateValues();
		}
		//StatsList->RequestListRefresh();
	}
}


class SStatTableRow : public SMultiColumnTableRow< TSharedRef<FSlateStatCycleCounter> >
{
	
public:
	SLATE_BEGIN_ARGS( SStatTableRow )
	{}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FStatItem>& InStatItem )
	{
		StatItem = InStatItem;
		FSuperRowType::Construct( FTableRowArgs(), OwnerTable );
	}

	float GetValue() const { return 6.0f; }

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:
	TSharedPtr<FStatItem> StatItem;
};

TSharedRef<SWidget> SStatTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ColumnId_StatName)
	{
		// STAT NAME
		return
			SNew(STextBlock)
			.Text_Lambda([=] { return StatItem->GetStatName(); });
	}
	else if (ColumnName == ColumnId_InclusiveAvgMs)
	{
		// STAT NUMBER
		return
			SNew(SBox)
			.Padding(FMargin(5, 0))
			[
				SNew(STextBlock)
				.TextStyle(FCoreStyle::Get(), "MonospacedText")
				.Text_Lambda([=] { return StatItem->GetInclusiveAvgMsText(); })
			];
	}
	else if (ColumnName == ColumnId_InclusiveAvgMsGraph)
	{
		// BAR GRAPH
		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 1)
			.FillWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([=] { return StatItem->GetInclusiveAvgMs(); })))
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity_Lambda([=] { return FMath::Lerp(FLinearColor::Green, FLinearColor::Red, StatItem->GetInclusiveAvgMs() / 30.0f); })
			]
			+ SHorizontalBox::Slot()
			.Padding(0, 1)
			.FillWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([=] { return 60.0f - StatItem->GetInclusiveAvgMs(); })))
			;
	}
	else
	{
		return SNew(SSpacer);
	}
}

TSharedRef<ITableRow> SWidgetReflector::GenerateStatRow( TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SStatTableRow, OwnerTable, StatItem);
}

#endif // SLATE_STATS

} // namespace WidgetReflectorImpl

TSharedRef<SWidgetReflector> SWidgetReflector::New()
{
  return MakeShareable( new WidgetReflectorImpl::SWidgetReflector() );
}

#undef LOCTEXT_NAMESPACE


