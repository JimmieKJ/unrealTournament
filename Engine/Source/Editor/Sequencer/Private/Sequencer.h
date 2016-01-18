// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "EditorUndoClient.h"
#include "MovieSceneClipboard.h"

class FMenuBuilder;
class FMovieSceneSequenceInstance;
class IDetailKeyframeHandler;
class IMenu;
class ISequencerEditTool;
class ISequencerObjectChangeListener;
class IToolkitHost;
class SSequencer;
class UClass;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneSequence;
class UWorld;
class IMovieSceneSpawnRegister;


/**
 * Sequencer is the editing tool for MovieScene assets.
 */
class FSequencer
	: public ISequencer
	, public FGCObject
	, public FEditorUndoClient
	, public FTickableEditorObject
{
public:

	static bool IsSequencerEnabled();

	/**
	 * Initializes sequencer
	 *
	 * @param InitParams Initialization parameters.
	 * @param InObjectChangeListener The object change listener to use.
	 * @param InDetailKeyframeHandler The detail keyframe handler to use.
	 * @param TrackEditorDelegates Delegates to call to create auto-key handlers for this sequencer.
	 */
	void InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TSharedRef<IDetailKeyframeHandler>& InDetailKeyframeHandler, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates);

	/** Constructor */
	FSequencer();

	/** Destructor */
	virtual ~FSequencer();

public:

	//~ Begin FGCObject Interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

public:

	//~ Begin FTickableEditorObject Interface

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FSequencer, STATGROUP_Tickables); };

public:

	//~ Begin ISequencer Interface

	virtual void Close() override;
	virtual TSharedRef<SWidget> GetSequencerWidget() const override { return SequencerWidget.ToSharedRef(); }
	virtual UMovieSceneSequence* GetRootMovieSceneSequence() const override;
	virtual UMovieSceneSequence* GetFocusedMovieSceneSequence() const override;
	virtual void ResetToNewRootSequence(UMovieSceneSequence& NewSequence) override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetRootMovieSceneSequenceInstance() const override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetFocusedMovieSceneSequenceInstance() const override;
	virtual void FocusSequenceInstance( TSharedRef<FMovieSceneSequenceInstance> SequenceInstance ) override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetSequenceInstanceForSection(UMovieSceneSection& Section) const override;
	virtual bool GetAutoKeyEnabled() const override;
	virtual void SetAutoKeyEnabled(bool bAutoKeyEnabled) override;
	virtual bool GetKeyAllEnabled() const override;
	virtual void SetKeyAllEnabled(bool bKeyAllEnabled) override;
	virtual bool GetKeyInterpPropertiesOnly() const override;
	virtual void SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) override;
	virtual EMovieSceneKeyInterpolation GetKeyInterpolation() const override;
	virtual void SetKeyInterpolation(EMovieSceneKeyInterpolation) override;
	virtual bool IsRecordingLive() const override;
	virtual float GetCurrentLocalTime(UMovieSceneSequence& InMovieSceneSequence) override;
	virtual float GetGlobalTime() override;
	virtual void SetGlobalTime(float Time) override;
	virtual void SetPerspectiveViewportPossessionEnabled(bool bEnabled) override;
	virtual void SetPerspectiveViewportCameraCutEnabled(bool bEnabled) override;
	virtual FGuid GetHandleToObject(UObject* Object, bool bCreateHandleIfMissing = true) override;
	virtual ISequencerObjectChangeListener& GetObjectChangeListener() override;
	virtual void NotifyMovieSceneDataChanged() override;
	virtual void UpdateRuntimeInstances() override;
	virtual void AddSubSequence(UMovieSceneSequence* Sequence) override;
	virtual bool CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const override;
	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) override;
	virtual FSequencerSelection& GetSelection() override;
	virtual FSequencerSelectionPreview& GetSelectionPreview() override;
	virtual void NotifyMapChanged(UWorld* NewWorld, EMapChangeType MapChangeType) override;
	virtual FOnGlobalTimeChanged& OnGlobalTimeChanged() override { return OnGlobalTimeChangedDelegate; }
	virtual FGuid CreateBinding(UObject& InObject, const FString& InName) override;
	virtual UObject* GetPlaybackContext() const override;
	virtual void GetAllKeyedProperties(UObject& Object, TSet<UProperty*>& OutProperties) override;

	/** Set the global time directly, without performing any auto-scroll */
	void SetGlobalTimeDirectly(float Time);

	/** @return The current view range */
	virtual FAnimatedRange GetViewRange() const override;

	/** @return The current clamp range */
	FAnimatedRange GetClampRange() const;

	TRange<float> GetPlaybackRange() const;
	void SetPlaybackRange(TRange<float> InNewPlaybackRange);
	void SetStartPlaybackRange();
	void SetEndPlaybackRange();

public:

	/** Access the user-supplied settings object */
	USequencerSettings* GetSettings() const { return Settings; }

	bool IsPerspectiveViewportPossessionEnabled() const override { return bPerspectiveViewportPossessionEnabled; }
	bool IsPerspectiveViewportCameraCutEnabled() const override { return bPerspectiveViewportCameraCutEnabled; }

	/**
	 * Pops the current focused movie scene from the stack.  The parent of this movie scene will be come the focused one
	 */
	void PopToSequenceInstance( TSharedRef<FMovieSceneSequenceInstance> SequenceInstance );

	/** 
	 * Deletes the passed in sections
	 */
	void DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection> > & Sections);

	/**
	 * Deletes the currently selected in keys
	 */
	void DeleteSelectedKeys();

	/**
	 * Set interpolation modes
	 */
	void SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode);

	/**
	 * Is interpolation mode selected
	 */
	bool IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const;

	/**
	 * Snap the currently selected keys to frame
	 */
	void SnapToFrame();

	/**
 	 * Are there keys to snap? 
	 */
	bool CanSnapToFrame() const;

	/**
	 * @return Movie scene tools used by the sequencer
	 */
	const TArray<TSharedPtr<ISequencerTrackEditor>>& GetTrackEditors() const
	{
		return TrackEditors;
	}

	/**
	 * Attempts to add a new spawnable to the MovieScene for the specified asset or class
	 *
	 * @param	Object	The asset or class object to add a spawnable for
	 *
	 * @return	The spawnable guid for the spawnable, or an invalid Guid if we were not able to create a spawnable
	 */
	FGuid AddSpawnableForAssetOrClass( UObject* Object );

	/**
	 * Call when an asset is dropped into the sequencer. Will proprogate this
	 * to all tracks, and potentially consume this asset
	 * so it won't be added as a spawnable
	 *
	 * @param DroppedAsset		The asset that is dropped in
	 * @param TargetObjectGuid	Object to be targeted on dropping
	 * @return					If true, this asset should be consumed
	 */
	virtual bool OnHandleAssetDropped( UObject* DroppedAsset, const FGuid& TargetObjectGuid );
	
	/**
	 * Called to delete all moviescene data from a node
	 *
	 * @param NodeToBeDeleted	Node with data that should be deleted
	 */
	virtual void OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted );

	/**
	 * Copies properties from one actor to another.  Only properties that are different are copied.  This is used
	 * to propagate changed properties from a living actor in the scene to properties of a blueprint CDO actor
	 *
	 * @param	PuppetActor	The source actor (the puppet actor in the scene we're copying properties from)
	 * @param	TargetActor	The destination actor (data-only blueprint actor CDO)
	 */
	virtual void CopyActorProperties( AActor* PuppetActor, AActor* TargetActor ) const;

	/**
	 * Zooms to the edges of all currently selected sections
	 */
	void ZoomToSelectedSections();

	/**
	 * Gets the overlay fading animation curve lerp
	 */
	float GetOverlayFadeCurve() const;

	/** Gets the command bindings for the sequencer */
	TSharedPtr<FUICommandList> GetCommandBindings() { return SequencerCommandBindings; }

	/**
	 * Builds up the sequencer's "Add Track" menu.
	 *
	 * @param MenuBuilder The menu builder to add things to.
	 */
	void BuildAddTrackMenu(FMenuBuilder& MenuBuilder);

	/**
	 * Builds up the track menu for object binding nodes in the outliner
	 * 
	 * @param MenuBuilder	The track menu builder to add things to
	 * @param ObjectBinding	The object binding of the selected node
	 * @param ObjectClass	The class of the selected object
	 */
	void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass);

	/**
	 * Builds up the edit buttons for object binding nodes in the outliner
	 * 
	 * @param EditBox	    The edit box to add things to
	 * @param ObjectBinding	The object binding of the selected node
	 * @param ObjectClass	The class of the selected object
	 */
	void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass);

	/** IMovieScenePlayer interface */
	virtual void GetRuntimeObjects( TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const override;
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject) const override;
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override;
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual void AddOrUpdateMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd) override;
	virtual void RemoveMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove) override;
	virtual IMovieSceneSpawnRegister& GetSpawnRegister() override { return *SpawnRegister; }

	/** Called when an actor is dropped into Sequencer */
	void OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors );

	/** Functions to push on to the transport controls we use */
	FReply OnPlay(bool bTogglePlay=true);
	FReply OnRecord();
	FReply OnStepForward();
	FReply OnStepBackward();
	FReply OnStepToEnd();
	FReply OnStepToBeginning();
	FReply OnToggleLooping();

	bool IsLooping() const;

	/** Set the new global time, accounting for looping options */
	void SetGlobalTimeLooped(float InTime);

	EPlaybackMode::Type GetPlaybackMode() const;

	/** Called to determine whether a frame number is set so that frame numbers can be shown */
	bool CanShowFrameNumbers() const;

	/** @return The toolkit that this sequencer is hosted in (if any) */
	TSharedPtr<IToolkitHost> GetToolkitHost() const { return ToolkitHost.Pin(); }

	/** @return Whether or not this sequencer is used in the level editor */
	bool IsLevelEditorSequencer() const { return bIsEditingWithinLevelEditor; }

	/** Called to save the current movie scene */
	void SaveCurrentMovieScene();

	/** Called to add selected objects to the movie scene */
	void AddSelectedObjects();

	/** Called when a user executes the assign actor to track menu item */
	void AssignActor(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);
	void DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid ObjectBinding);

	/** Called when a user executes the delete node menu item */
	void DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted);
	void DeleteSelectedNodes();

	/** Called when a user executes the active node menu item */
	void ToggleNodeActive();
	bool IsNodeActive() const;

	/** Called when a user executes the locked node menu item */
	void ToggleNodeLocked();
	bool IsNodeLocked() const;

	/** Called when a user executes the set key time for selected keys */
	bool CanSetKeyTime() const;
	void SetKeyTime(const bool bUseFrames);
	void OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, const bool bUseFrames);

public:

	/** Copy the selected keys to the clipboard */
	void CopySelectedKeys();

	/** Copy the selected keys to the clipboard, then delete them as part of an undoable transaction */
	void CutSelectedKeys();

	/** Get the in-memory clipboard stack */
	const TArray<TSharedPtr<FMovieSceneClipboard>>& GetClipboardStack() const;

	/** Promote a clipboard to the top of the clipboard stack, and update its timestamp */
	void OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard);

public:

	/** Access the currently enabled edit tool */
	ISequencerEditTool& GetEditTool();

protected:

	/** Reset data about a movie scene when pushing or popping a movie scene. */
	void ResetPerMovieSceneData();

	/** Sets the actor CDO such that it is placed in front of the active perspective viewport camera, if we have one */
	static void PlaceActorInFrontOfCamera( AActor* ActorCDO );

	/** Update the time bounds to the focused movie scene */
	void UpdateTimeBoundsToFocusedMovieScene();

	/**
	 * Gets the far time boundaries of the currently edited movie scene
	 * If the scene has shots, it only takes the shot section boundaries
	 * Otherwise, it finds the furthest boundaries of all sections
	 */
	TRange<float> GetTimeBounds() const;
	
	/**
	 * Gets the time boundaries of the currently filtering shot sections.
	 * If there are no shot filters, an empty range is returned.
	 */
	TRange<float> GetFilteringShotsTimeBounds() const;

	/** @return The current scrub position */
	virtual float OnGetScrubPosition() const { return ScrubPosition; }

	/**
	 * Set the view range, growing the working range to accomodate, if necessary
	 * @param NewViewRange The new view range. Must be a finite range
	 * @param Interpolation How to interpolate to the new view range
	 */
	void SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation = EViewRangeInterpolation::Animated);

	/**
	 * Called when the clamp range is changed by the user
	 *
	 * @param	NewClampRange The new clapm range
	 */
	void OnClampRangeChanged( TRange<float> NewClampRange );

	/**
	 * Called when the scrub position is changed by the user
	 * This will stop any playback from happening
	 *
	 * @param NewScrubPosition	The new scrub position
	 */
	void OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing );

	/** Called when the user has begun scrubbing */
	void OnBeginScrubbing();

	/** Called when the user has finished scrubbing */
	void OnEndScrubbing();

	/** Called when the user has begun dragging the play range */
	void OnBeginPlaybackRangeDrag();

	/** Called when the user has finished dragging the play range */
	void OnEndPlaybackRangeDrag();

public:

	/** Put the sequencer in a horizontally autoscrolling state with the given rate */
	void StartAutoscroll(float UnitsPerS);

	/** Stop the sequencer from autoscrolling */
	void StopAutoscroll();

	/** Scroll the sequencer vertically by the specified number of slate units */
	void VerticalScroll(float ScrollAmountUnits);

protected:
	/**
	 * Update autoscroll mechanics as a result of a new time position
	 */
	void UpdateAutoScroll(float NewTime);

	/**
	 * Calculates the amount of encroachment the specified time has into the autoscroll region, if any
	 */
	TOptional<float> CalculateAutoscrollEncroachment(float NewTime, float ThresholdPercentage = 0.1f) const;

	/** Called to toggle auto-scroll on and off */
	void OnToggleAutoScroll();

	/**
	 * Whether auto-scroll is enabled.
	 *
	 * @return true if auto-scroll is enabled, false otherwise.
	 */
	bool IsAutoScrollEnabled() const;

	/** 
	 * Find the viewed sequence asset in the content browser
	 */
	void FindInContentBrowser();

	/**
	 * Get the asset we're currently editing, if applicable
	 */
	UObject* GetCurrentAsset() const;

protected:

	/**
	 * Populate the specified set with all UProperties that are currently keyed for the specified object and sequence instance, including any sub sequences
	 *
	 * @param Object		The object to get keyed properties for
	 * @param Instance		The sequence instance in which to look for the object bindings
	 * @param OutProperties	set to populate with properties
	 */
	void GetAllKeyedPropertiesForInstance(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties);

	/**
	 * Populate the specified set with all UProperties that are currently keyed for the specified object and sequence instance, excluding any sub sequences
	 *
	 * @param Object		The object to get keyed properties for
	 * @param Instance		The sequence instance in which to look for the object bindings
	 * @param OutProperties	set to populate with properties
	 */
	void GetKeyedProperties(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties);

protected:

	/** Called via UEditorEngine::GetActorRecordingStateEvent to check to see whether we need to record actor state */
	void GetActorRecordingState( bool& bIsRecording /* In+Out */ ) const;
	
	/** Called when a user executes the delete command to delete sections or keys */
	void DeleteSelectedItems();
	
	/** Transport controls */
	void TogglePlay();
	void PlayForward();
	void Rewind();
	void StepForward();
	void StepBackward();
	void StepToNextKey();
	void StepToPreviousKey();
	void StepToNextCameraKey();
	void StepToPreviousCameraKey();

	void ExpandAllNodesAndDescendants();
	void CollapseAllNodesAndDescendants();

	/** Expand or collapse selected nodes */
	void ToggleExpandCollapseNodes();

	/** Expand or collapse selected nodes and descendants*/
	void ToggleExpandCollapseNodesAndDescendants();

	/** Manually sets a key for the selected objects at the current time */
	void SetKey();

	/** Modeless Version of the String Entry Box */
	void GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted);
	
	/** Closes the popup created by GenericTextEntryModeless*/
	void CloseEntryPopupMenu();

	/** Trim a section to the left or right */
	void TrimSection(bool bTrimLeft);

	/** Split a section */
	void SplitSection();

	/** Generates command bindings for UI commands */
	void BindSequencerCommands();

	void ActivateSequencerEditorMode();

	void ActivateDetailKeyframeHandler();
	void DeactivateDetailKeyframeHandler();
	void OnPropertyEditorOpened();

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	void OnSectionSelectionChanged();

	void OnSelectedOutlinerNodesChanged();

	/** Called before the world is going to be saved. The sequencer puts everything back to its initial state. */
	void OnPreSaveWorld(uint32 SaveFlags, UWorld* World);

	/** Called after the world has been saved. The sequencer updates to the animated state. */
	void OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess);

	/** Called after a new level has been created. The sequencer editor mode needs to be enabled. */
	void OnNewCurrentLevel();

	/** Called after a map has been opened. The sequencer editor mode needs to be enabled. */
	void OnMapOpened(const FString& Filename, bool bLoadAsTemplate);

	/** Called before a PIE session begins. */
	void OnPreBeginPIE(bool bIsSimulating);

	/** Called after a PIE session ends. */
	void OnEndPIE(bool bIsSimulating);

	/** Updates a viewport client from camera cut data */
	void UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject) const;

	/** Is the sequencer widget focused? */
	bool IsSequencerWidgetFocused() const;

private:

	/** User-supplied settings object for this sequencer */
	USequencerSettings* Settings;

	TMap< TWeakObjectPtr<UMovieSceneSection>, TSharedRef<FMovieSceneSequenceInstance> > SequenceInstanceBySection;

	/** Command list for sequencer commands */
	TSharedRef<FUICommandList> SequencerCommandBindings;

	/** List of tools we own */
	TArray<TSharedPtr<ISequencerTrackEditor>> TrackEditors;

	/** Listener for object changes being made while this sequencer is open*/
	TSharedPtr<ISequencerObjectChangeListener> ObjectChangeListener;

	/** Listener for object changes being made while this sequencer is open*/
	TSharedPtr<IDetailKeyframeHandler> DetailKeyframeHandler;

	/** The runtime instance for the root movie scene */
	TSharedPtr<FMovieSceneSequenceInstance> RootMovieSceneSequenceInstance;

	/** Main sequencer widget */
	TSharedPtr<SSequencer> SequencerWidget;
	
	/** Spawn register for keeping track of what is spawned */
	TSharedPtr<IMovieSceneSpawnRegister> SpawnRegister;

	/** The asset editor that created this Sequencer if any */
	TWeakPtr<IToolkitHost> ToolkitHost;

	/**
	 * Stack of sequence instances.
	 *
	 * The first element is always the root instance.
	 * The last element is the focused instance.
	 */
	TArray<TSharedRef<FMovieSceneSequenceInstance>> SequenceInstanceStack;

	/** The time range target to be viewed */
	TRange<float> TargetViewRange;

	/** The last time range that was viewed */
	TRange<float> LastViewRange;

	/** The view range before zooming */
	TRange<float> ViewRangeBeforeZoom;

	/** The amount of autoscroll pan offset that is currently being applied */
	TOptional<float> AutoscrollOffset;

	/** The amount of autoscrub offset that is currently being applied */
	TOptional<float> AutoscrubOffset;

	/** Zoom smoothing curves */
	FCurveSequence ZoomAnimation;
	FCurveHandle ZoomCurve;

	/** Overlay fading curves */
	FCurveSequence OverlayAnimation;
	FCurveHandle OverlayCurve;

	/** Whether we are playing, recording, etc. */
	EMovieScenePlayerStatus::Type PlaybackState;

	/** The current scrub position */
	// @todo sequencer: Should use FTimespan or "double" for Time Cursor Position! (cascades)
	float ScrubPosition;

	bool bPerspectiveViewportPossessionEnabled;
	bool bPerspectiveViewportCameraCutEnabled;

	/** True if this sequencer is being edited within the level editor */
	bool bIsEditingWithinLevelEditor;

	/** Generic Popup Entry */
	TWeakPtr<IMenu> EntryPopupMenu;

	/** Stores a dirty bit for whether the sequencer tree (and other UI bits) may need to be refreshed.  We
	    do this simply to avoid refreshing the UI more than once per frame. (e.g. during live recording where
		the MovieScene data can change many times per frame.) */
	bool bNeedTreeRefresh;

	FSequencerSelection Selection;
	FSequencerSelectionPreview SelectionPreview;

	/** A delegate which is called any time the global time changes. */
	FOnGlobalTimeChanged OnGlobalTimeChangedDelegate;
};
