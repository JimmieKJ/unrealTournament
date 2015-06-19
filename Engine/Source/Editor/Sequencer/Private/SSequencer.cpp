// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"

#include "SSequencer.h"
#include "Editor/SequencerWidgets/Public/SequencerWidgetsModule.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "MovieSceneTrackEditor.h"
#include "SSequencerSectionOverlay.h"
#include "SSequencerTrackArea.h"
#include "SequencerNodeTree.h"
#include "TimeSliderController.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "AssetSelection.h"
#include "K2Node_PlayMovieScene.h"
#include "MovieSceneShotSection.h"
#include "CommonMovieSceneTools.h"
#include "SSearchBox.h"
#include "SNumericDropDown.h"
#include "EditorWidgetsModule.h"


#define LOCTEXT_NAMESPACE "Sequencer"


/**
 * The shot filter overlay displays the overlay needed to filter out widgets based on
 * which shots are actively in use.
 */
class SShotFilterOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SShotFilterOverlay) {}
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FSequencer> InSequencer)
	{
		ViewRange = InArgs._ViewRange;
		Sequencer = InSequencer;
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{
		if (Sequencer.Pin()->IsShotFilteringOn())
		{
			TArray< TWeakObjectPtr<UMovieSceneSection> > FilterShots = Sequencer.Pin()->GetFilteringShotSections();
			// if there are filtering shots, continuously update the cached filter zones
			// we do this in tick, and cache it in order to make animation work properly
			CachedFilteredRanges.Empty();
			for (int32 i = 0; i < FilterShots.Num(); ++i)
			{
				UMovieSceneSection* Filter = FilterShots[i].Get();
				CachedFilteredRanges.Add(Filter->GetRange());
			}
		}
	}
	
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override
	{
		float Alpha = Sequencer.Pin()->GetOverlayFadeCurve();
		
		if (Alpha > 0.f)
		{
			FTimeToPixel TimeToPixelConverter = FTimeToPixel(AllottedGeometry, ViewRange.Get());
		
			TRange<float> TimeBounds = TRange<float>(TimeToPixelConverter.PixelToTime(0),
				TimeToPixelConverter.PixelToTime(AllottedGeometry.Size.X));

			TArray< TRange<float> > OverlayRanges = ComputeOverlayRanges(TimeBounds, CachedFilteredRanges);

			for (int32 i = 0; i < OverlayRanges.Num(); ++i)
			{
				float LowerBound = TimeToPixelConverter.TimeToPixel(OverlayRanges[i].GetLowerBoundValue());
				float UpperBound = TimeToPixelConverter.TimeToPixel(OverlayRanges[i].GetUpperBoundValue());
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(LowerBound, 0), FVector2D(UpperBound - LowerBound, AllottedGeometry.Size.Y)),
					FEditorStyle::GetBrush("Sequencer.ShotFilter"),
					MyClippingRect,
					ESlateDrawEffect::None,
					FLinearColor(1.f, 1.f, 1.f, Alpha)
				);
			}
		}

		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(100, 100);
	}

private:
	/**
	 * Given a range of time bounds, find what ranges still should be
	 * filtered based on shot filters
	 */
	TArray< TRange<float> > ComputeOverlayRanges(TRange<float> TimeBounds, TArray< TRange<float> > RangesToSubtract) const
	{
		TArray< TRange<float> > FilteredRanges;
		FilteredRanges.Add(TimeBounds);
		// @todo Sequencer Optimize - This is O(n^2)
		// However, n is likely to stay very low, and the average case is likely O(n)
		// Also, the swapping of TArrays in this loop could use some heavy optimization as well
		for (int32 i = 0; i < RangesToSubtract.Num(); ++i)
		{
			TRange<float>& CurrentRange = RangesToSubtract[i];

			// ignore ranges that don't overlap with the time bounds
			if (CurrentRange.Overlaps(TimeBounds))
			{
				TArray< TRange<float> > NewFilteredRanges;
				for (int32 j = 0; j < FilteredRanges.Num(); ++j)
				{
					TArray< TRange<float> > SubtractedRanges = TRange<float>::Difference(FilteredRanges[j], CurrentRange);
					NewFilteredRanges.Append(SubtractedRanges);
				}
				FilteredRanges = NewFilteredRanges;
			}
		}
		return FilteredRanges;
	}

private:
	/** The current minimum view range */
	TAttribute< TRange<float> > ViewRange;
	/** The main sequencer interface */
	TWeakPtr<FSequencer> Sequencer;
	/** Cached set of ranges that are being filtering currently */
	TArray< TRange<float> > CachedFilteredRanges;
};


void SSequencer::Construct( const FArguments& InArgs, TSharedRef< class FSequencer > InSequencer )
{
	Sequencer = InSequencer;
	bIsActiveTimerRegistered = false;

	// Create a node tree which contains a tree of movie scene data to display in the sequence
	SequencerNodeTree = MakeShareable( new FSequencerNodeTree( InSequencer.Get() ) );

	FSequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<FSequencerWidgetsModule>( "SequencerWidgets" );

	FTimeSliderArgs TimeSliderArgs;
	TimeSliderArgs.ViewRange = InArgs._ViewRange;
	TimeSliderArgs.OnViewRangeChanged = InArgs._OnViewRangeChanged;
	TimeSliderArgs.ScrubPosition = InArgs._ScrubPosition;
	TimeSliderArgs.OnScrubPositionChanged = InArgs._OnScrubPositionChanged;

	TSharedRef<FSequencerTimeSliderController> TimeSliderController( new FSequencerTimeSliderController( TimeSliderArgs ) );
	
	bool bMirrorLabels = false;
	// Create the top and bottom sliders
	TSharedRef<ITimeSlider> TopTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderController, bMirrorLabels );

	bMirrorLabels = true;
	TSharedRef<ITimeSlider> BottomTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderController, bMirrorLabels);

	SAssignNew( TrackArea, SSequencerTrackArea, Sequencer.Pin().ToSharedRef() )
		.ViewRange( InArgs._ViewRange )
		.OutlinerFillPercent( this, &SSequencer::GetAnimationOutlinerFillPercentage );

	ChildSlot
	[
		SNew(SBorder)
		.Padding(2)
		.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					MakeToolBar()
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SSequencerCurveEditorToolBar, TrackArea->GetCurveEditor()->GetCommands() )
					.Visibility( this, &SSequencer::GetCurveEditorToolBarVisibility )
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				// @todo Sequencer Do not change the paddings or the sliders scrub widgets wont line up
				.Padding( FMargin(0) )
				.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
				.VAlign(VAlign_Center)
				[
					SNew( SBox )
					.Padding( FMargin( 0,0,4,0) )
					[
						// Search box for searching through the outliner
						SNew( SSearchBox )
						.OnTextChanged( this, &SSequencer::OnOutlinerSearchChanged )
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SBorder )
					// @todo Sequencer Do not change the paddings or the sliders scrub widgets wont line up
					.Padding( FMargin( 0.0f, 2.0f, 0.0f, 0.0f ) )
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
					[
						TopTimeSlider
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew( SOverlay )
				+ SOverlay::Slot()
				[
					MakeSectionOverlay( TimeSliderController, InArgs._ViewRange, InArgs._ScrubPosition, false )
				]
				+ SOverlay::Slot()
				[
					TrackArea.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					SNew( SHorizontalBox )
					.Visibility( EVisibility::HitTestInvisible )
					+ SHorizontalBox::Slot()
					.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
					[
						// Take up space but display nothing. This is required so that all areas dependent on time align correctly
						SNullWidget::NullWidget
					]
					+ SHorizontalBox::Slot()
					.FillWidth( 1.0f )
					[
						SNew(SShotFilterOverlay, Sequencer)
						.ViewRange( InArgs._ViewRange )
					]
				]
				+ SOverlay::Slot()
				[
					MakeSectionOverlay( TimeSliderController, InArgs._ViewRange, InArgs._ScrubPosition, true )
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
				.HAlign(HAlign_Center)
				[
					MakeTransportControls()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SBorder )
					// @todo Sequencer Do not change the paddings or the sliders scrub widgets wont line up
					.Padding( FMargin( 0.0f, 0.0f, 0.0f, 2.0f ) )
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
					[
						BottomTimeSlider
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
				[
					SNew( SSpacer )
				]
				+ SHorizontalBox::Slot()
				.FillWidth( 1.0f )
				[
					SAssignNew( BreadcrumbTrail, SBreadcrumbTrail<FSequencerBreadcrumb> )
					.Visibility(this, &SSequencer::GetBreadcrumbTrailVisibility)
					.OnCrumbClicked( this, &SSequencer::OnCrumbClicked )
				]
			]
		]
	];

	ResetBreadcrumbs();
}

TSharedRef<SWidget> SSequencer::MakeToolBar()
{
	FToolBarBuilder ToolBarBuilder( Sequencer.Pin()->GetCommandBindings(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true);
	
	ToolBarBuilder.BeginSection("Base Commands");
	{
		ToolBarBuilder.SetLabelVisibility(EVisibility::Visible);
		ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleAutoKeyEnabled );

		if ( Sequencer.Pin()->IsLevelEditorSequencer() )
		{
			ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleCleanView );
		}
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Snapping");
	{
		TArray<SNumericDropDown<float>::FNamedValue> SnapValues;
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandth", "0.001" ), LOCTEXT( "SnapDescription_OneThousandth", "Set snap to 1/1000th" ) ) );
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.01f, LOCTEXT( "Snap_OneHundredth", "0.01" ), LOCTEXT( "SnapDescription_OneHundredth", "Set snap to 1/100th" ) ) );
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.1f, LOCTEXT( "Snap_OneTenth", "0.1" ), LOCTEXT( "SnapDescription_OneTenth", "Set snap to 1/10th" ) ) );
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 1.0f, LOCTEXT( "Snap_One", "1" ), LOCTEXT( "SnapDescription_One", "Set snap to 1" ) ) );
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 10.0f, LOCTEXT( "Snap_Ten", "10" ), LOCTEXT( "SnapDescription_Ten", "Set snap to 10" ) ) );
		SnapValues.Add( SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundred", "100" ), LOCTEXT( "SnapDescription_OneHundred", "Set snap to 100" ) ) );

		ToolBarBuilder.SetLabelVisibility( EVisibility::Collapsed );
		ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleIsSnapEnabled, NAME_None, TAttribute<FText>( FText::GetEmpty() ) );
		ToolBarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP( this, &SSequencer::MakeSnapMenu ),
			LOCTEXT( "SnapOptions", "Options" ),
			LOCTEXT( "SnapOptionsToolTip", "Snapping Options" ),
			TAttribute<FSlateIcon>(),
			true );
		ToolBarBuilder.AddWidget(
			SNew( SBox )
			.VAlign( VAlign_Center )
			[
				SNew( SNumericDropDown<float> )
				.DropDownValues( SnapValues )
				.LabelText( LOCTEXT( "TimeSnapLabel", "Time" ) )
				.ToolTipText( LOCTEXT( "TimeSnappingIntervalToolTip", "Time snapping interval" ) )
				.Value( this, &SSequencer::OnGetTimeSnapInterval )
				.OnValueChanged( this, &SSequencer::OnTimeSnapIntervalChanged )
			] );
		ToolBarBuilder.AddWidget(
			SNew( SBox )
			.VAlign( VAlign_Center )
			[
				SNew( SNumericDropDown<float> )
				.DropDownValues( SnapValues )
				.LabelText( LOCTEXT( "ValueSnapLabel", "Value" ) )
				.ToolTipText( LOCTEXT( "ValueSnappingIntervalToolTip", "Curve value snapping interval" ) )
				.Value( this, &SSequencer::OnGetValueSnapInterval )
				.OnValueChanged( this, &SSequencer::OnValueSnapIntervalChanged )
			] );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Curve Editor");
	{
		ToolBarBuilder.SetLabelVisibility( EVisibility::Visible );
		ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleShowCurveEditor );
		ToolBarBuilder.SetLabelVisibility( EVisibility::Collapsed );
		ToolBarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP( this, &SSequencer::MakeCurveEditorMenu ),
			LOCTEXT( "CurveEditorOptions", "Options" ),
			LOCTEXT( "CurveEditorOptionsToolTip", "Curve Editor Options" ),
			TAttribute<FSlateIcon>(),
			true );
	}

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeSnapMenu()
{
	FMenuBuilder MenuBuilder( false, Sequencer.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection( "KeySnapping", LOCTEXT( "SnappingMenuKeyHeader", "Key Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToKeys );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "SectionSnapping", LOCTEXT( "SnappingMenuSectionHeader", "Section Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionTimesToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionTimesToSections );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlayTimeSnapping", LOCTEXT( "SnappingMenuPlayTimeHeader", "Play Time Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToInterval );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "CurveSnapping", LOCTEXT( "SnappingMenuCurveHeader", "Curve Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapCurveValueToInterval );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeCurveEditorMenu()
{
	FMenuBuilder MenuBuilder( true, Sequencer.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection( "CurveVisibility", LOCTEXT( "CurveEditorMenuCurveVisibilityHeader", "Curve Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetAllCurveVisibility );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetSelectedCurveVisibility );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetAnimatedCurveVisibility );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "Options", LOCTEXT( "CurveEditorMenuOptionsHeader", "Editor Options" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowCurveEditorCurveToolTips );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeTransportControls()
{
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );
	TSharedRef<FSequencer> SequencerPinned = Sequencer.Pin().ToSharedRef();

	FTransportControlArgs TransportControlArgs;
	TransportControlArgs.OnBackwardEnd.BindSP( SequencerPinned, &FSequencer::OnStepToBeginning );
	TransportControlArgs.OnBackwardStep.BindSP( SequencerPinned, &FSequencer::OnStepBackward );
	TransportControlArgs.OnForwardPlay.BindSP( SequencerPinned, &FSequencer::OnPlay );
	TransportControlArgs.OnForwardStep.BindSP( SequencerPinned, &FSequencer::OnStepForward );
	TransportControlArgs.OnForwardEnd.BindSP( SequencerPinned, &FSequencer::OnStepToEnd );
	TransportControlArgs.OnToggleLooping.BindSP( SequencerPinned, &FSequencer::OnToggleLooping );
	TransportControlArgs.OnGetLooping.BindSP( SequencerPinned, &FSequencer::IsLooping );
	TransportControlArgs.OnGetPlaybackMode.BindSP( SequencerPinned, &FSequencer::GetPlaybackMode );

	return EditorWidgetsModule.CreateTransportControl( TransportControlArgs );
}

SSequencer::~SSequencer()
{
}

void SSequencer::RegisterActiveTimerForPlayback()
{
	if (!bIsActiveTimerRegistered)
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSequencer::EnsureSlateTickDuringPlayback));
	}
}

EActiveTimerReturnType SSequencer::EnsureSlateTickDuringPlayback(double InCurrentTime, float InDeltaTime)
{
	if (Sequencer.IsValid())
	{
		auto PlaybackStatus = Sequencer.Pin()->GetPlaybackStatus();
		if (PlaybackStatus == EMovieScenePlayerStatus::Playing || PlaybackStatus == EMovieScenePlayerStatus::Recording)
		{
			return EActiveTimerReturnType::Continue;
		}
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SSequencer::UpdateLayoutTree()
{	
	// Update the node tree
	SequencerNodeTree->Update();
	
	TrackArea->Update( SequencerNodeTree );

	SequencerNodeTree->UpdateCachedVisibilityBasedOnShotFiltersChanged();
}

void SSequencer::UpdateBreadcrumbs(const TArray< TWeakObjectPtr<class UMovieSceneSection> >& FilteringShots)
{
	TSharedRef<FMovieSceneInstance> FocusedMovieSceneInstance = Sequencer.Pin()->GetFocusedMovieSceneInstance();

	if (BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::ShotType)
	{
		BreadcrumbTrail->PopCrumb();
	}

	if( BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::MovieSceneType && BreadcrumbTrail->PeekCrumb().MovieSceneInstance.Pin() != FocusedMovieSceneInstance )
	{
		// The current breadcrumb is not a moviescene so we need to make a new breadcrumb in order return to the parent moviescene later
		BreadcrumbTrail->PushCrumb( FText::FromString(FocusedMovieSceneInstance->GetMovieScene()->GetName()), FSequencerBreadcrumb( FocusedMovieSceneInstance ) );
	}

	if (Sequencer.Pin()->IsShotFilteringOn())
	{
		TAttribute<FText> CrumbString;
		if (FilteringShots.Num() == 1)
		{
			CrumbString = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequencer::GetShotSectionTitle, FilteringShots[0].Get()));
		}
		else 
		{
			CrumbString = LOCTEXT("MultipleShots", "Multiple Shots");
		}

		BreadcrumbTrail->PushCrumb(CrumbString, FSequencerBreadcrumb());
	}

	SequencerNodeTree->UpdateCachedVisibilityBasedOnShotFiltersChanged();
}

void SSequencer::ResetBreadcrumbs()
{
	BreadcrumbTrail->ClearCrumbs();
	BreadcrumbTrail->PushCrumb(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequencer::GetRootMovieSceneName)), FSequencerBreadcrumb(Sequencer.Pin()->GetRootMovieSceneInstance()));
}

void SSequencer::OnOutlinerSearchChanged( const FText& Filter )
{
	SequencerNodeTree->FilterNodes( Filter.ToString() );
}

TSharedRef<SWidget> SSequencer::MakeSectionOverlay( TSharedRef<FSequencerTimeSliderController> TimeSliderController, const TAttribute< TRange<float> >& ViewRange, const TAttribute<float>& ScrubPosition, bool bTopOverlay )
{
	return
		SNew( SHorizontalBox )
		.Visibility( EVisibility::HitTestInvisible )
		+ SHorizontalBox::Slot()
		.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
		[
			// Take up space but display nothing. This is required so that all areas dependent on time align correctly
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		[
			SNew( SSequencerSectionOverlay, TimeSliderController )
			.DisplayScrubPosition( bTopOverlay )
			.DisplayTickLines( !bTopOverlay )
		];
}

float SSequencer::OnGetTimeSnapInterval() const
{
	return GetDefault<USequencerSettings>()->GetTimeSnapInterval();
}

void SSequencer::OnTimeSnapIntervalChanged( float InInterval )
{
	GetMutableDefault<USequencerSettings>()->SetTimeSnapInterval(InInterval);
}

float SSequencer::OnGetValueSnapInterval() const
{
	return GetDefault<USequencerSettings>()->GetCurveValueSnapInterval();
}

void SSequencer::OnValueSnapIntervalChanged( float InInterval )
{
	GetMutableDefault<USequencerSettings>()->SetCurveValueSnapInterval( InInterval );
}


void SSequencer::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Add drop validity cue
}


void SSequencer::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Clear drop validity cue
}


FReply SSequencer::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bool bIsDragSupported = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid() && (
		Operation->IsOfType<FAssetDragDropOp>() ||
		Operation->IsOfType<FClassDragDropOp>() ||
		Operation->IsOfType<FUnloadedClassDragDropOp>() ||
		Operation->IsOfType<FActorDragDropGraphEdOp>() ) )
	{
		bIsDragSupported = true;
	}

	return bIsDragSupported ? FReply::Handled() : FReply::Unhandled();
}


FReply SSequencer::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bool bWasDropHandled = false;

	// @todo sequencer: Get rid of hard-code assumptions about dealing with ACTORS at this level?

	// @todo sequencer: We may not want any actor-specific code here actually.  We need systems to be able to
	// register with sequencer to support dropping assets/classes/actors, or OTHER types!

	// @todo sequencer: Handle drag and drop from other FDragDropOperations, including unloaded classes/asset and external drags!

	// @todo sequencer: Consider allowing drops into the level viewport to add to the MovieScene as well.
	//		- Basically, when Sequencer is open it would take over drops into the level and auto-add puppets for these instead of regular actors
	//		- This would let people drag smoothly and precisely into the view to drop assets/classes into the scene

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if ( !Operation.IsValid() )
	{

	}
	else if ( Operation->IsOfType<FAssetDragDropOp>() )
	{
		const auto& DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

		OnAssetsDropped( *DragDropOp );

		bWasDropHandled = true;
	}
	else if( Operation->IsOfType<FClassDragDropOp>() )
	{
		const auto& DragDropOp = StaticCastSharedPtr<FClassDragDropOp>( Operation );

		OnClassesDropped( *DragDropOp );

		bWasDropHandled = true;
	}
	else if( Operation->IsOfType<FUnloadedClassDragDropOp>() )
	{
		const auto& DragDropOp = StaticCastSharedPtr<FUnloadedClassDragDropOp>( Operation );

		OnUnloadedClassesDropped( *DragDropOp );

		bWasDropHandled = true;
	}
	else if( Operation->IsOfType<FActorDragDropGraphEdOp>() )
	{
		const auto& DragDropOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>( Operation );

		OnActorsDropped( *DragDropOp );

		bWasDropHandled = true;
	}

	return bWasDropHandled ? FReply::Handled() : FReply::Unhandled();
}

FReply SSequencer::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) 
{
	// A toolkit tab is active, so direct all command processing to it
	if( Sequencer.Pin()->GetCommandBindings()->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSequencer::OnAssetsDropped( const FAssetDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *Sequencer.Pin();

	bool bObjectAdded = false;
	bool bSpawnableAdded = false;
	TArray< UObject* > DroppedObjects;
	bool bAllAssetsWereLoaded = true;
	for( auto CurAssetData = DragDropOp.AssetData.CreateConstIterator(); CurAssetData; ++CurAssetData )
	{
		const FAssetData& AssetData = *CurAssetData;

		UObject* Object = AssetData.GetAsset();
		if ( Object != NULL )
		{
			DroppedObjects.Add( Object );
		}
		else
		{
			bAllAssetsWereLoaded = false;
		}
	}

	const TSet< TSharedRef<const FSequencerDisplayNode> >* SelectedNodes = Sequencer.Pin()->GetSelection()->GetSelectedOutlinerNodes();
	FGuid TargetObjectGuid;
	// if exactly one object node is selected, we have a target object guid
	TSharedPtr<const FSequencerDisplayNode> DisplayNode;
	if (SelectedNodes->Num() == 1)
	{
		for (TSharedRef<const FSequencerDisplayNode> SelectedNode : *SelectedNodes )
		{
			DisplayNode = SelectedNode;
		}
		if (DisplayNode.IsValid() && DisplayNode->GetType() == ESequencerNode::Object)
		{
			TSharedPtr<const FObjectBindingNode> ObjectBindingNode = StaticCastSharedPtr<const FObjectBindingNode>(DisplayNode);
			TargetObjectGuid = ObjectBindingNode->GetObjectBinding();
		}
	}

	for( auto CurObjectIter = DroppedObjects.CreateConstIterator(); CurObjectIter; ++CurObjectIter )
	{
		UObject* CurObject = *CurObjectIter;

		if (!SequencerRef.OnHandleAssetDropped(CurObject, TargetObjectGuid))
		{
			FGuid NewGuid = SequencerRef.AddSpawnableForAssetOrClass( CurObject, NULL );
			bSpawnableAdded = NewGuid.IsValid();
		}
		bObjectAdded = true;
	}


	if( bSpawnableAdded )
	{
		SequencerRef.SpawnOrDestroyPuppetObjects( SequencerRef.GetFocusedMovieSceneInstance() );
	}

	if( bObjectAdded )
	{
		// Update the sequencers view of the movie scene data when any object is added
		SequencerRef.NotifyMovieSceneDataChanged();
	}
}


void SSequencer::OnClassesDropped( const FClassDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *Sequencer.Pin();

	bool bSpawnableAdded = false;
	for( auto ClassIter = DragDropOp.ClassesToDrop.CreateConstIterator(); ClassIter; ++ClassIter )
	{
		UClass* Class = ( *ClassIter ).Get();
		if( Class != NULL )
		{
			UObject* Object = Class->GetDefaultObject();

			FGuid NewGuid = SequencerRef.AddSpawnableForAssetOrClass( Object, NULL );

			bSpawnableAdded = NewGuid.IsValid();

		}
	}

	if( bSpawnableAdded )
	{
		SequencerRef.SpawnOrDestroyPuppetObjects( SequencerRef.GetFocusedMovieSceneInstance() );

		// Update the sequencers view of the movie scene data
		SequencerRef.NotifyMovieSceneDataChanged();
	}
}

void SSequencer::OnUnloadedClassesDropped( const FUnloadedClassDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *Sequencer.Pin();
	bool bSpawnableAdded = false;
	for( auto ClassDataIter = DragDropOp.AssetsToDrop->CreateConstIterator(); ClassDataIter; ++ClassDataIter )
	{
		auto& ClassData = *ClassDataIter;

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>( NULL, *ClassData.AssetName );
		if( Object == NULL )
		{
			Object = FindObject<UObject>(NULL, *FString::Printf(TEXT("%s.%s"), *ClassData.GeneratedPackageName, *ClassData.AssetName));
		}

		if( Object == NULL )
		{
			// Load the package.
			GWarn->BeginSlowTask( LOCTEXT("OnDrop_FullyLoadPackage", "Fully Loading Package For Drop"), true, false );
			UPackage* Package = LoadPackage(NULL, *ClassData.GeneratedPackageName, LOAD_NoRedirects );
			if( Package != NULL )
			{
				Package->FullyLoad();
			}
			GWarn->EndSlowTask();

			Object = FindObject<UObject>(Package, *ClassData.AssetName);
		}

		if( Object != NULL )
		{
			// Check to see if the dropped asset was a blueprint
			if(Object->IsA(UBlueprint::StaticClass()))
			{
				// Get the default object from the generated class.
				Object = Cast<UBlueprint>(Object)->GeneratedClass->GetDefaultObject();
			}
		}

		if( Object != NULL )
		{
			FGuid NewGuid = SequencerRef.AddSpawnableForAssetOrClass( Object, NULL );
			bSpawnableAdded = NewGuid.IsValid();
		}
	}

	if( bSpawnableAdded )
	{
		SequencerRef.SpawnOrDestroyPuppetObjects( SequencerRef.GetFocusedMovieSceneInstance() );

		SequencerRef.NotifyMovieSceneDataChanged();
	}
}


void SSequencer::OnActorsDropped( FActorDragDropGraphEdOp& DragDropOp )
{
	Sequencer.Pin()->OnActorsDropped( DragDropOp.Actors );
}

void SSequencer::OnCrumbClicked(const FSequencerBreadcrumb& Item)
{
	if (Item.BreadcrumbType != FSequencerBreadcrumb::ShotType)
	{
		if( Sequencer.Pin()->GetFocusedMovieSceneInstance() == Item.MovieSceneInstance.Pin() ) 
		{
			// then do zooming
		}
		else
		{
			Sequencer.Pin()->PopToMovieScene( Item.MovieSceneInstance.Pin().ToSharedRef() );
		}

		Sequencer.Pin()->FilterToShotSections( TArray< TWeakObjectPtr<UMovieSceneSection> >() );
	}
	else
	{
		// refilter what we already have
		Sequencer.Pin()->FilterToShotSections(Sequencer.Pin()->GetFilteringShotSections());
	}
}

FText SSequencer::GetRootMovieSceneName() const
{
	return FText::FromString(Sequencer.Pin()->GetRootMovieScene()->GetName());
}

FText SSequencer::GetShotSectionTitle(UMovieSceneSection* ShotSection) const
{
	return Cast<UMovieSceneShotSection>(ShotSection)->GetTitle();
}

void SSequencer::DeleteSelectedNodes()
{
	const TSet< TSharedRef<const FSequencerDisplayNode> >* SelectedNodes = Sequencer.Pin()->GetSelection()->GetSelectedOutlinerNodes();

	if( SelectedNodes->Num() > 0 )
	{
		const FScopedTransaction Transaction( LOCTEXT("UndoDeletingObject", "Delete Object from MovieScene") );

		FSequencer& SequencerRef = *Sequencer.Pin();

		for( auto SelectedNode : *SelectedNodes )
		{
			if( SelectedNode->IsVisible() )
			{
				// Delete everything in the entire node
				SequencerRef.OnRequestNodeDeleted( SelectedNode );
			}
		}
	}
}

EVisibility SSequencer::GetBreadcrumbTrailVisibility() const
{
	return Sequencer.Pin()->IsLevelEditorSequencer() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SSequencer::GetCurveEditorToolBarVisibility() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditor() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
