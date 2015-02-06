// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateReflectorPrivatePCH.h"
#include "ISlateReflectorModule.h"

#if STATS
#include "StatsData.h"
#endif


#define LOCTEXT_NAMESPACE "SWidgetReflector"
#define WITH_EVENT_LOGGING 0

static const int32 MaxLoggedEvents = 100;


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


/* SWidgetReflector interface
 *****************************************************************************/

static FReply ToggleSlateStats()
{
	#if STATS
	FSelfRegisteringExec::StaticExec(nullptr, TEXT("stat slate"), *GLog);
	#endif

	return FReply::Handled();
}

extern SLATECORE_API int32 bFoldTick;


void SWidgetReflector::Construct( const FArguments& InArgs )
{
	LoggedEvents.Reserve(MaxLoggedEvents);

	bShowFocus = false;
	bIsPicking = false;

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
									.Text(LOCTEXT("AppScale", "Application Scale: ").ToString())
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
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(5.0f, 0.0f))
						[
							SNew(SButton)
							.Text(LOCTEXT("DisplayTextureAtlases", "Display Texture Atlases"))
							.OnClicked(this, &SWidgetReflector::HandleDisplayTextureAtlases)
						]
						+SHorizontalBox::Slot()
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
											.Text(LOCTEXT("ShowFocus", "Show Focus").ToString())
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
						SNew(SButton)
						#if STATS
						.Text( LOCTEXT("ToggleStats", "Toggle Stats") )
						#else
						.Text( LOCTEXT("ToggleStatsUnavailable", "Toggle Stats [see tooltip]") )
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("ToggleStatsUnavailableTooltip", "To enable STATS in standalone applications turn on <bCompileWithStatsWithoutEngine>false</bCompileWithStatsWithoutEngine> in BuildConfiguration.xml") )
							]
						)
						.IsEnabled(false)
						#endif
						.OnClicked_Static( &ToggleSlateStats )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f)
					[
						SNew(SButton)
						.OnClicked( this, &SWidgetReflector::CopyStatsToClipboard )
						.Text( LOCTEXT("CopyStatsToClipboard", "Copy Stats") )
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
				]

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
			
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
					MakeStatViewer()
					]
				
			]
	];
}

/* SCompoundWidget overrides
 *****************************************************************************/

void SWidgetReflector::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateStats();
}


void SWidgetReflector::OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply )
{
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


FString SWidgetReflector::HandlePickButtonText() const
{
	static const FString NotPicking = LOCTEXT("PickWidget", "Pick Widget").ToString();
	static const FString Picking = LOCTEXT("PickingWidget", "Picking (Esc to Stop)").ToString();

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


//
// STATS
//

/**
 * Holds the stat data for visualation. StatItems have an immutable
 * identity and a mutable value that changes from frame to frame.
 */
class FStatItem
{
public:
	
	FStatItem( FName InName, FText InDisplayText )
	: StatName( InName )
	, DisplayText( InDisplayText )
	, InclusiveAvgMs( 0.0f )
	{
	}

	FName GetStatName() const { return StatName; }
	FText GetDisplayName() const { return DisplayText; }	
	FText GetInclusiveAvgMsText() const
	{
		return InclusiveAvgMsText;
	}
	float GetInclusiveAvgMs() const { return InclusiveAvgMs; }
	void UpdateInclusiveAvgMs( float InMs )
	{
		static const auto FormattingOptions = FNumberFormattingOptions()
			.SetMinimumIntegralDigits(1)
			.SetMinimumFractionalDigits(3)
			.SetMaximumFractionalDigits(3);

		InclusiveAvgMs = InMs;
		InclusiveAvgMsText = FText::AsNumber(InMs, &FormattingOptions);
	}

private:

	const FName StatName;
	const FText DisplayText;

	FText InclusiveAvgMsText;
	float InclusiveAvgMs;
	
};

static const FName ColumnId_StatName("StatName");
static const FName ColumnId_InclusiveAvgMs("InclusiveAvgMs");
static const FName ColumnId_InclusiveAvgMsGraph("InclusiveAvgMsGraph");

TSharedRef<SWidget> SWidgetReflector::MakeStatViewer()
{
	return
		SAssignNew(StatsBorder, SBorder)
		.BorderImage( FCoreStyle::Get().GetBrush("NoBrush") )
		[
			// STATS LIST
			SAssignNew( StatsList, SListView< TSharedRef< FStatItem > > )
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
	#if STATS

	const FGameThreadHudData* ViewDataPtr = FHUDGroupGameThreadRenderer::Get().Latest;
	if (ViewDataPtr != nullptr)
	{
		// Stats are up to date; make sure the UI is full bright.
		StatsBorder->SetColorAndOpacity(FLinearColor::White);

		// If we don't get a report about a stat this frame, assume its value is 0;
		for (auto& StatItem : StatsItems)
		{
			StatItem->UpdateInclusiveAvgMs(0.0f);
		}

		// Update stats with latest values; add any stats if they are missing
		bool bNeedsRefresh = false;
		const FGameThreadHudData &ViewData = *ViewDataPtr;
		for (int32 GroupIndex = 0; GroupIndex < ViewData.HudGroups.Num(); ++GroupIndex)
		{
			const FHudGroup& HudGroup = ViewData.HudGroups[GroupIndex];
			const bool bHasHierarchy = !!HudGroup.HierAggregate.Num();
			const bool bHasFlat = !!HudGroup.FlatAggregate.Num();

			int32 Row = 0;
			for (const FComplexStatMessage& StatItem : HudGroup.FlatAggregate)
			{
				const TSharedRef<FStatItem>* ExistingItemPtr = StatsItems.FindByPredicate( [&StatItem]( const TSharedRef<FStatItem>& ItemRef )
				{
					return ItemRef->GetStatName() == StatItem.GetShortName();
				});

				const float InMs = FPlatformTime::ToMilliseconds(StatItem.GetValue_Duration(EComplexStatField::IncAve));

				// If stat does not exist...
				if (ExistingItemPtr == nullptr)
				{
					// ... add it.
					const TSharedRef<FStatItem> NewStatItem( new FStatItem( StatItem.GetShortName(), FText::FromString(StatItem.GetDescription() ) ) );
					StatsItems.Add( NewStatItem );
					ExistingItemPtr = &StatsItems.Last();

					// We have added items to the list, and it needs to be refreshed.
					bNeedsRefresh = true;
				}

				// Update stat values.
				(*ExistingItemPtr)->UpdateInclusiveAvgMs( InMs );
			}
		}

		// If we add widgets in tick, we must refresh the list.
		if (bNeedsRefresh)
		{
			StatsList->RequestListRefresh();
		}		
		else
		{
			int32 a = 5;
			if (a == 10){  }
		}
	}
	else
	{
		// We are now showing stale stats; provide visual hint.
		StatsBorder->SetColorAndOpacity(FLinearColor(1,1,1,0.75f));
		
		// If we don't get a report about a stat this frame, assume its value is 0;
		for (auto& StatItem : StatsItems)
		{
			StatItem->UpdateInclusiveAvgMs(0.0f);
		}
	}

	#endif
}

TSharedRef<ITableRow> SWidgetReflector::GenerateStatRow( TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	class SStatTableRow : public SMultiColumnTableRow< TSharedRef<FStatItem> >
	{
	
	public:
		typedef SMultiColumnTableRow< TSharedRef<FStatItem> > Super;
		
		SLATE_BEGIN_ARGS( SStatTableRow )
		{}
		SLATE_END_ARGS();

		void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FStatItem>& InStatItem )
		{
			StatItem = InStatItem;
			FSuperRowType::Construct( FTableRowArgs(), OwnerTable );
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
		{
			if (ColumnName == ColumnId_StatName)
			{
				// STAT NAME
				return
				SNew(STextBlock)
				.Text( StatItem->GetDisplayName() );
			}
			else if (ColumnName == ColumnId_InclusiveAvgMs)
			{
				// STAT NUMBER
				return
				SNew(SBox)
				.Padding(FMargin(5,0))
				[
					SNew(STextBlock)
					.TextStyle(FCoreStyle::Get(), "MonospacedText")
					.Text( this, &SStatTableRow::GetInclusiveAvgMsText )
				];
			}
			else if ( ColumnName == ColumnId_InclusiveAvgMsGraph )
			{
				// BAR GRAPH
				return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0,1)
				.FillWidth( TAttribute<float>(SharedThis(this), &SStatTableRow::BarGraph_GetFilledWidth) )
				[
					SNew(SImage)
					.Image( FCoreStyle::Get().GetBrush("WhiteBrush") )
					.ColorAndOpacity( this, &SStatTableRow::BarGraph_GetBarColor )
				]
				+SHorizontalBox::Slot()
				.Padding(0,1)
				.FillWidth( TAttribute<float>(SharedThis(this), &SStatTableRow::BarGraph_GetEmptyWidth) );
			}
			else
			{
				return SNew(SSpacer);
			}
		}

		private:

		FText GetInclusiveAvgMsText() const
		{
			return StatItem->GetInclusiveAvgMsText();
		}

		float BarGraph_GetFilledWidth() const
		{
			return StatItem->GetInclusiveAvgMs();
		}

		float BarGraph_GetEmptyWidth()  const
		{
			static const float MaxMs = 60.0f;
			return MaxMs - StatItem->GetInclusiveAvgMs();
		}

		FSlateColor BarGraph_GetBarColor() const
		{
			static const float RedlineMs = 30.0f;
			return FMath::Lerp( FLinearColor::Green, FLinearColor::Red, StatItem->GetInclusiveAvgMs()/RedlineMs );
		}

		TSharedPtr<FStatItem> StatItem;
	};

	return SNew(SStatTableRow, OwnerTable, StatItem);
}

FReply SWidgetReflector::CopyStatsToClipboard()
{
	FString ClipboardString;

	for (const TSharedRef<FStatItem>& StatItem : StatsItems)
	{
		ClipboardString += StatItem->GetDisplayName().ToString();
		ClipboardString += TEXT(", ");
		ClipboardString += StatItem->GetInclusiveAvgMsText().ToString();
		ClipboardString += TEXT("\r\n");
	}

	FPlatformMisc::ClipboardCopy( *ClipboardString );
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE


