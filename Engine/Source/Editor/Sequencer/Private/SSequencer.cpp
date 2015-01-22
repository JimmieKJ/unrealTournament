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
#include "SNumericEntryBox.h"


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

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
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

	virtual FVector2D ComputeDesiredSize() const override
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

	// Create a node tree which contains a tree of movie scene data to display in the sequence
	SequencerNodeTree = MakeShareable( new FSequencerNodeTree( InSequencer.Get() ) );

	CleanViewEnabled = InArgs._CleanViewEnabled;
	OnToggleCleanView = InArgs._OnToggleCleanView;

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
				.Padding( FMargin( 0.0f, 0.0f, 0.0f, 0.0f ) )
				.FillWidth( TAttribute<float>( this, &SSequencer::GetAnimationOutlinerFillPercentage ) )
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						MakeToolBar()
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.FillWidth(1.f)
					[
						// @todo Sequencer - Temp clean view button
						SNew( SCheckBox )
						.Visibility( this, &SSequencer::OnGetCleanViewVisibility )
						.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked( this, &SSequencer::OnGetCleanViewCheckState )
						.OnCheckStateChanged( this, &SSequencer::OnCleanViewChecked )
						[
							SNew( STextBlock )
							.Text( LOCTEXT("ToggleCleanView", "Clean View") )
						]
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(BreadcrumbTrail, SBreadcrumbTrail<FSequencerBreadcrumb>)
					.OnCrumbClicked(this, &SSequencer::OnCrumbClicked)
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
					SAssignNew( TrackArea, SSequencerTrackArea, Sequencer.Pin().ToSharedRef() )
					.ViewRange( InArgs._ViewRange )
					.OutlinerFillPercent( this, &SSequencer::GetAnimationOutlinerFillPercentage )
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
				[
					SNew( SSpacer )
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
		]
	];

	ResetBreadcrumbs();
}

TSharedRef<SWidget> SSequencer::MakeToolBar()
{
	FToolBarBuilder ToolBarBuilder( Sequencer.Pin()->GetCommandBindings(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true);
	
	ToolBarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleAutoKeyEnabled );

	ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);
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
			SNew( SNumericEntryBox<float> )
			.AllowSpin( true )
			.MinValue( 0 )
			.MinSliderValue( 0 )
			.MaxSliderValue( 1 )
			.MinDesiredValueWidth( 50 )
			.Delta( 0.05f )
			.ToolTipText( LOCTEXT( "SnappingIntervalToolTip", "Snapping interval" ) )
			.Value( this, &SSequencer::OnGetSnapInterval )
			.OnValueChanged( this, &SSequencer::OnSnapIntervalChanged ) 
		] );

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeSnapMenu()
{
	FMenuBuilder MenuBuilder( false, Sequencer.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection( "KeySnapping", LOCTEXT( "SnappingMenuKeyHeader", "Key Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeysToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeysToKeys );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "SectionSnapping", LOCTEXT( "SnappingMenuSectionHeader", "Section Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionsToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionsToSections );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlayTimeSnapping", LOCTEXT( "SnappingMenuPlayTimeHeader", "Play Time Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToInterval );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

SSequencer::~SSequencer()
{
}

void SSequencer::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	Sequencer.Pin()->Tick(InDeltaTime);

	const TArray< TSharedPtr<FMovieSceneTrackEditor> >& TrackEditors = Sequencer.Pin()->GetTrackEditors();
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(InCurrentTime);
	}
}

void SSequencer::UpdateLayoutTree()
{	
	// Update the node tree
	SequencerNodeTree->Update();
	
	TrackArea->Update( *SequencerNodeTree );

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

EVisibility SSequencer::OnGetCleanViewVisibility() const 
{
	return Sequencer.Pin()->IsLevelEditorSequencer() ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SSequencer::OnGetCleanViewCheckState() const
{
	return CleanViewEnabled.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SSequencer::OnCleanViewChecked( ECheckBoxState InState ) 
{
	OnToggleCleanView.ExecuteIfBound( InState == ECheckBoxState::Checked ? true : false );
}

TOptional<float> SSequencer::OnGetSnapInterval() const
{
	return GetDefault<USequencerSnapSettings>()->GetSnapInterval();
}

void SSequencer::OnSnapIntervalChanged( float InInterval )
{
	GetMutableDefault<USequencerSnapSettings>()->SetSnapInterval(InInterval);
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

	const TSet< TSharedRef<const FSequencerDisplayNode> >& SelectedNodes = SequencerNodeTree->GetSelectedNodes();
	FGuid TargetObjectGuid;
	// if exactly one object node is selected, we have a target object guid
	TSharedPtr<const FSequencerDisplayNode> DisplayNode;
	if (SelectedNodes.Num() == 1)
	{
		for (TSet< TSharedRef<const FSequencerDisplayNode> >::TConstIterator It(SelectedNodes); It; ++It)
		{
			DisplayNode = *It;
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
	const TSet< TSharedRef<const FSequencerDisplayNode> >& SelectedNodes = SequencerNodeTree->GetSelectedNodes();

	if( SelectedNodes.Num() > 0 )
	{
		const FScopedTransaction Transaction( LOCTEXT("UndoDeletingObject", "Delete Object from MovieScene") );

		FSequencer& SequencerRef = *Sequencer.Pin();

		for( auto It = SelectedNodes.CreateConstIterator(); It; ++It )
		{
			TSharedRef<const FSequencerDisplayNode> Node = *It;

			if( Node->IsVisible() )
			{
				// Delete everything in the entire node
				SequencerRef.OnRequestNodeDeleted( Node );
			}

		}
	}
}

#undef LOCTEXT_NAMESPACE
