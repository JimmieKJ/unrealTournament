// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniquePtr.h"
#include "ISequencerEditTool.h"
#include "SequencerCommonHelpers.h"


class FActorDragDropGraphEdOp;
class FAssetDragDropOp;
class FClassDragDropOp;
class FEditPropertyChain;
class FMovieSceneSequenceInstance;
class FSequencer;
class FSequencerNodeTree;
class FUnloadedClassDragDropOp;
class IDetailsView;
class UMovieSceneSection;
class SSequencerCurveEditor;
class SSequencerTrackArea;
class SSequencerTrackOutliner;
class SSequencerTreeView;
struct FSectionHandle;
struct FPaintPlaybackRangeArgs;

namespace SequencerLayoutConstants
{
	/** The amount to indent child nodes of the layout tree */
	const float IndentAmount = 8.0f;

	/** Height of each object node */
	const float ObjectNodeHeight = 20.0f;

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
	TWeakPtr<FMovieSceneSequenceInstance> MovieSceneInstance;

	FSequencerBreadcrumb(TSharedRef<FMovieSceneSequenceInstance> InMovieSceneInstance)
		: BreadcrumbType(FSequencerBreadcrumb::MovieSceneType)
		, MovieSceneInstance(InMovieSceneInstance)
	{ }

	FSequencerBreadcrumb()
		: BreadcrumbType(FSequencerBreadcrumb::ShotType)
	{ }
};


/**
 * Main sequencer UI widget
 */
class SSequencer
	: public SCompoundWidget
	, public FGCObject
	, public FNotifyHook
{
public:

	DECLARE_DELEGATE_OneParam( FOnToggleBoolOption, bool )
	SLATE_BEGIN_ARGS( SSequencer )
		: _ScrubPosition( 1.0f )
	{ }
		/** The current view range (seconds) */
		SLATE_ATTRIBUTE( FAnimatedRange, ViewRange )

		/** The current clamp range (seconds) */
		SLATE_ATTRIBUTE( FAnimatedRange, ClampRange )

		/** The playback range */
		SLATE_ATTRIBUTE( TRange<float>, PlaybackRange )
		
		/** Called when the user changes the playback range */
		SLATE_EVENT( FOnRangeChanged, OnPlaybackRangeChanged )

		/** The current scrub position in (seconds) */
		SLATE_ATTRIBUTE( float, ScrubPosition )

		/** Called when the user changes the view range */
		SLATE_EVENT( FOnViewRangeChanged, OnViewRangeChanged )

		/** Called when the user changes the clamp range */
		SLATE_EVENT( FOnRangeChanged, OnClampRangeChanged )

		/** Called when the user has begun scrubbing */
		SLATE_EVENT( FSimpleDelegate, OnBeginScrubbing )

		/** Called when the user has finished scrubbing */
		SLATE_EVENT( FSimpleDelegate, OnEndScrubbing )

		/** Called when the user changes the scrub position */
		SLATE_EVENT( FOnScrubPositionChanged, OnScrubPositionChanged )

		/** Called to populate the add combo button in the toolbar. */
		SLATE_EVENT( FOnGetAddMenuContent, OnGetAddMenuContent )

		/** Extender to use for the add menu. */
		SLATE_ARGUMENT( TSharedPtr<FExtender>, AddMenuExtender )
	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer);

	void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings);
	
	~SSequencer();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector )
	{
		Collector.AddReferencedObject( Settings );
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Updates the layout node tree from movie scene data */
	void UpdateLayoutTree();

	/** Causes the widget to register an empty active timer that persists until Sequencer playback stops */
	void RegisterActiveTimerForPlayback();

	/** Updates the breadcrumbs from a change in the shot filter state. */
	void UpdateBreadcrumbs();
	void ResetBreadcrumbs();

	/** Step to next and previous keyframes */
	void StepToNextKey();
	void StepToPreviousKey();
	void StepToNextCameraKey();
	void StepToPreviousCameraKey();
	void StepToKey(bool bStepToNextKey, bool bCameraOnly);

	/** Whether the user is selecting. Ignore selection changes from the level when the user is selecting. */
	void SetUserIsSelecting(bool bUserIsSelectingIn)
	{
		bUserIsSelecting = bUserIsSelectingIn;
	}

	bool UserIsSelecting()
	{
		return bUserIsSelecting;
	}

	/** Called when the save button is clicked */
	void OnSaveMovieSceneClicked();

	/** Access the tree view for this sequencer */
	TSharedPtr<SSequencerTreeView> GetTreeView() const;

	/** Access the currently active edit tool */
	ISequencerEditTool& GetEditTool() const
	{
		return *EditTool;
	}

	/** Generate a helper structure that can be used to transform between phsyical space and virtual space in the track area */
	FVirtualTrackArea GetVirtualTrackArea() const;

	/** Get an array of section handles for the given set of movie scene sections */
	TArray<FSectionHandle> GetSectionHandles(const TSet<TWeakObjectPtr<UMovieSceneSection>>& DesiredSections) const;

	/** @return a numeric type interface that will parse and display numbers as frames and times correctly */
	TSharedRef<INumericTypeInterface<float>> GetNumericTypeInterface();

public:

	// FNotifyHook overrides

	void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged);

protected:

	/** Update the details view from the currently selected keys and sections. */
	void UpdateDetailsView();

protected:

	// SWidget interface

	// @todo Sequencer Basic drag and drop support. Doesn't belong here most likely.
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

private:
	
	/** Handles checking whether the details view is enabled. */
	bool HandleDetailsViewEnabled() const;

	/** Handles determining the visibility of the details view selection tip. */
	EVisibility HandleDetailsViewTipVisibility() const;

	/** Handles determining the visibility of the details view. */
	EVisibility HandleDetailsViewVisibility() const;

	/** Handles key selection changes. */
	void HandleKeySelectionChanged();

	/** Handles section selection changes. */
	void HandleSectionSelectionChanged();

	/** Check whether the specified edit tool is enabled */
	bool IsEditToolEnabled(FName InIdentifier);

	/** Empty active timer to ensure Slate ticks during Sequencer playback */
	EActiveTimerReturnType EnsureSlateTickDuringPlayback(double InCurrentTime, float InDeltaTime);	

	/** Makes the toolbar. */
	TSharedRef<SWidget> MakeToolBar();

	/** Makes the add menu for the toolbar. */
	TSharedRef<SWidget> MakeAddMenu();

	/** Makes the general menu for the toolbar. */
	TSharedRef<SWidget> MakeGeneralMenu();

	/** Makes the snapping menu for the toolbar. */
	TSharedRef<SWidget> MakeSnapMenu();

	/** Makes and configures a set of the standard UE transport controls. */
	TSharedRef<SWidget> MakeTransportControls();

	/**
	* @return The value of the current time snap interval.
	*/
	float OnGetTimeSnapInterval() const;

	/**
	* Called when the time snap interval changes.
	*
	* @param InInterval	The new time snap interval
	*/
	void OnTimeSnapIntervalChanged(float InInterval);

	/**
	* @return The value of the current value snap interval.
	*/
	float OnGetValueSnapInterval() const;

	/**
	* Called when the value snap interval changes.
	*
	* @param InInterval	The new value snap interval
	*/
	void OnValueSnapIntervalChanged( float InInterval );

	/**
	 * Called when the outliner search terms change                                                              
	 */
	void OnOutlinerSearchChanged( const FText& Filter );

	/**
	 * @return The fill percentage of the animation outliner
	 */
	float GetColumnFillCoefficient(int32 ColumnIndex) const
	{
		return ColumnFillCoefficients[ColumnIndex];
	}

	/** Get the amount of space that the outliner spacer should fill */
	float GetOutlinerSpacerFill() const;

	/** Get the visibility of the curve area */
	EVisibility GetCurveEditorVisibility() const;

	/** Get the visibility of the track area */
	EVisibility GetTrackAreaVisibility() const;

	/**
	 * Called when one or more assets are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the asset(s) that were dropped
	 */
	void OnAssetsDropped(const FAssetDragDropOp& DragDropOp);
	
	/**
	 * Called when one or more classes are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the class(es) that were dropped
	 */
	void OnClassesDropped(const FClassDragDropOp& DragDropOp);
	
	/**
	 * Called when one or more unloaded classes are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the unloaded class(es) that were dropped
	 */
	void OnUnloadedClassesDropped(const FUnloadedClassDragDropOp& DragDropOp);
	
	/**
	 * Called when one or more actors are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the actor(s) that was dropped
	 */
	void OnActorsDropped(FActorDragDropGraphEdOp& DragDropOp); 
	
	/**
	* Delegate used when actor selection changes in the level
	*
	*/	
	void OnActorSelectionChanged( UObject* obj );

	/** Called when a breadcrumb is clicked on in the sequencer */
	void OnCrumbClicked(const FSequencerBreadcrumb& Item);

	/** Gets the root movie scene name */
	FText GetRootAnimationName() const;

	/** Gets the title of the passed in shot section */
	FText GetShotSectionTitle(UMovieSceneSection* ShotSection) const;

	/** Gets whether or not the breadcrumb trail should be visible. */
	EVisibility GetBreadcrumbTrailVisibility() const;

	/** Gets whether or not the curve editor toolbar should be visible. */
	EVisibility GetCurveEditorToolBarVisibility() const;

	/** Gets whether or not the bottom time slider should be visible. */
	EVisibility GetBottomTimeSliderVisibility() const;

	/** Gets whether or not the time range should be visible. */
	EVisibility GetTimeRangeVisibility() const;

	/** Gets whether or not to show frame numbers. */
	bool ShowFrameNumbers() const;

	/** Called when a column fill percentage is changed by a splitter slot. */
	void OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex);

	/** Called when the curve editor is shown or hidden */
	void OnCurveEditorVisibilityChanged();

	/** Gets paint options for painting the playback range on sequencer */
	FPaintPlaybackRangeArgs GetSectionPlaybackRangeArgs() const;

private:

	/** Holds the details view. */
	TSharedPtr<IDetailsView> DetailsView;

	/** Section area widget */
	TSharedPtr<SSequencerTrackArea> TrackArea;

	/** Outliner widget */
	TSharedPtr<SSequencerTrackOutliner> TrackOutliner;

	/** The curve editor. */
	TSharedPtr<SSequencerCurveEditor> CurveEditor;

	/** Sequencer node tree for movie scene data */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;

	/** The breadcrumb trail widget for this sequencer */
	TSharedPtr<SBreadcrumbTrail<FSequencerBreadcrumb>> BreadcrumbTrail;

	/** The sequencer tree view responsible for the outliner and track areas */
	TSharedPtr<SSequencerTreeView> TreeView;

	/** The main sequencer interface */
	TWeakPtr<FSequencer> Sequencer;

	/** Cached settings provided to the sequencer itself on creation */
	USequencerSettings* Settings;

	/** The fill coefficients of each column in the grid. */
	float ColumnFillCoefficients[2];

	/** Whether the active timer is currently registered */
	bool bIsActiveTimerRegistered;

	/** Whether the user is selecting. Ignore selection changes from the level when the user is selecting. */
	bool bUserIsSelecting;

	/** The current edit tool */
	TUniquePtr<ISequencerEditTool> EditTool;

	/** Extender to use for the 'add' menu */
	TSharedPtr<FExtender> AddMenuExtender;

	/** Numeric type interface used for converting parsing and generating strings from numbers */
	TSharedPtr<INumericTypeInterface<float>> NumericTypeInterface;

	FOnGetAddMenuContent OnGetAddMenuContent;
};
