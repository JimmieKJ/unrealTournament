// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace SequencerLayoutConstants
{
	/** The amount to indent child nodes of the layout tree */
	const float IndentAmount = 25.0f;
	/** Padding between each node */
	const float NodePadding = 3.0f;
	/** Height of each object node */
	const float ObjectNodeHeight = 25.0f;
	/** Height of each section area if there are no sections (note: section areas may be larger than this if they have children. This is the height of a section area with no children or all children hidden) */
	const float SectionAreaDefaultHeight = 15.0f;
	/** Height of each key area */
	const float KeyAreaHeight = 15.0f;
	/** Height of each category node */
	const float CategoryNodeHeight = 15.0f;
}

/**
 * The kind of breadcrumbs that sequencer uses
 */
struct FSequencerBreadcrumb
{
	enum Type
	{
		ShotType,
		MovieSceneType,
	};

	/** The type of breadcrumb this is */
	FSequencerBreadcrumb::Type BreadcrumbType;
	/** The movie scene this may point to */
	class TWeakPtr<FMovieSceneInstance> MovieSceneInstance;

	FSequencerBreadcrumb( TSharedRef<class FMovieSceneInstance> InMovieSceneInstance )
		: BreadcrumbType(FSequencerBreadcrumb::MovieSceneType)
		, MovieSceneInstance(InMovieSceneInstance) {}

	FSequencerBreadcrumb()
		: BreadcrumbType(FSequencerBreadcrumb::ShotType) {}
};

/**
 * Main sequencer UI widget
 */
class SSequencer : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnToggleBoolOption, bool )
	SLATE_BEGIN_ARGS( SSequencer )
		: _ViewRange( TRange<float>( 0.0f, 5.0f ) )
		, _ScrubPosition( 1.0f )
	{}
		/** The current view range (seconds) */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
		/** The current scrub position in (seconds) */
		SLATE_ATTRIBUTE( float, ScrubPosition )
		/** Whether the clean sequencer view is enabled */
		SLATE_ATTRIBUTE( bool, CleanViewEnabled )
		/** Called when the user changes the view range */
		SLATE_EVENT( FOnViewRangeChanged, OnViewRangeChanged )
		/** Called when the user changes the scrub position */
		SLATE_EVENT( FOnScrubPositionChanged, OnScrubPositionChanged )
		/** Called when the user toggles auto-key */
		SLATE_EVENT( FOnToggleBoolOption, OnToggleCleanView )
	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs, TSharedRef< class FSequencer > InSequencer );

	~SSequencer();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Updates the layout node tree from movie scene data */
	void UpdateLayoutTree();


	/**
	 * Updates the breadcrumbs from a change in the shot filter state
	 *
	 * @param FilteringShots A list of shots that are now filtering, or none if filtering is off
	 */
	void UpdateBreadcrumbs(const TArray< TWeakObjectPtr<class UMovieSceneSection> >& FilteringShots);
	void ResetBreadcrumbs();

	/** Deletes selected nodes out of the sequencer node tree */
	void DeleteSelectedNodes();

private:
	/** Makes the toolbar for the outline section. */
	TSharedRef<SWidget> MakeToolBar();

	/** Makes the snapping menu for the toolbar. */
	TSharedRef<SWidget> MakeSnapMenu();
	
	/** @return The visibility of the clean view button */
	EVisibility OnGetCleanViewVisibility() const;

	/**
	 * @return The state of the auto-key check box
	 */
	ECheckBoxState OnGetCleanViewCheckState() const;
	
	/**
	 * Called when the auto-key checkbox state changes
	 * 
	 * @param InState	The new state
	 */
	void OnCleanViewChecked( ECheckBoxState InState );	

	/**
	* @return The value of the current snap interval.
	*/
	TOptional<float> OnGetSnapInterval() const;

	/**
	* Called when the snap interval slider changes.
	*
	* @param InInterval	The new snap interval
	*/
	void OnSnapIntervalChanged(float InInterval);

	/**
	 * Called when the outliner search terms change                                                              
	 */
	void OnOutlinerSearchChanged( const FText& Filter );

	/**
	 * @return The fill percentage of the animation outliner
	 * @todo Sequencer Make this user adjustable
	 */
	float GetAnimationOutlinerFillPercentage() const { return .25f; }

	/**
	 * Creates an overlay for various components of the time slider that should be displayed on top or bottom of the section area
	 * 
	 * @param TimeSliderController	Time controller to route input to for adjustment of the time view
	 * @param ViewRange		The current visible time range
	 * @param ScrubPosition		The current scrub position
	 * @param bTopOverlay		Whether or not the overlay is displayed on top or bottom of the section view
	 */
	TSharedRef<SWidget> MakeSectionOverlay( TSharedRef<class FSequencerTimeSliderController> TimeSliderController, const TAttribute< TRange<float> >& ViewRange, const TAttribute<float>& ScrubPosition, bool bTopOverlay );

	/** SWidget interface */
	/** @todo Sequencer Basic drag and drop support.  Doesn't belong here most likely */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/**
	 * Called when one or more assets are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the asset(s) that were dropped
	 */
	void OnAssetsDropped( const class FAssetDragDropOp& DragDropOp );
	
	/**
	 * Called when one or more classes are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the class(es) that were dropped
	 */
	void OnClassesDropped( const class FClassDragDropOp& DragDropOp );
	
	/**
	 * Called when one or more unloaded classes are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the unloaded class(es) that were dropped
	 */
	void OnUnloadedClassesDropped( const class FUnloadedClassDragDropOp& DragDropOp );
	
	/**
	 * Called when one or more actors are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the actor(s) that was dropped
	 */
	void OnActorsDropped( class FActorDragDropGraphEdOp& DragDropOp ); 
	
	/** Called when a breadcrumb is clicked on in the sequencer */
	void OnCrumbClicked(const FSequencerBreadcrumb& Item);

	/** Gets the root movie scene name */
	FText GetRootMovieSceneName() const;

	/** Gets the title of the passed in shot section */
	FText GetShotSectionTitle(UMovieSceneSection* ShotSection) const;

private:
	/** Section area widget */
	TSharedPtr<class SSequencerTrackArea> TrackArea;
	/** Sequencer node tree for movie scene data */
	TSharedPtr<class FSequencerNodeTree> SequencerNodeTree;
	/** The breadcrumb trail widget for this sequencer */
	TSharedPtr< class SBreadcrumbTrail<FSequencerBreadcrumb> > BreadcrumbTrail;
	/** Delegate to call when clean view is toggled */
	FOnToggleBoolOption OnToggleCleanView;
	/** Whether the clean sequencer view is enabled */
	TAttribute<bool> CleanViewEnabled;
	/** The main sequencer interface */
	TWeakPtr<FSequencer> Sequencer;
};
