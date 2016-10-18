// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "EditorUndoClient.h"
#include "MovieSceneClipboard.h"
#include "MovieScenePossessable.h"
#include "SequencerLabelManager.h"
#include "LevelEditor.h"

class ACineCameraActor;
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
class FSequencerNodeTree;

struct ISequencerHotspot;
struct FTransformData;


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

	/** Constructor */
	FSequencer();

	/** Virtual destructor */
	virtual ~FSequencer();

public:

	/**
	 * Initializes sequencer
	 *
	 * @param InitParams Initialization parameters.
	 * @param InObjectChangeListener The object change listener to use.
	 * @param InDetailKeyframeHandler The detail keyframe handler to use.
	 * @param TrackEditorDelegates Delegates to call to create auto-key handlers for this sequencer.
	 */
	void InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TSharedRef<IDetailKeyframeHandler>& InDetailKeyframeHandler, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates);

	/** @return The current view range */
	virtual FAnimatedRange GetViewRange() const override;
	virtual void SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation = EViewRangeInterpolation::Animated) override;

	/** @return The current clamp range */
	FAnimatedRange GetClampRange() const;
	void SetClampRange(TRange<float> InNewClampRange);

public:

	/**
	 * Get the selection range.
	 *
	 * @return The selection range.
	 * @see SetSelectionRange, SetSelectionRangeEnd, SetSelectionRangeStart
	 */
	TRange<float> GetSelectionRange() const;

	/**
	 * Set the selection selection range.
	 *
	 * @param Range The new range to set.
	 * @see GetSelectionRange, SetSelectionRangeEnd, SetSelectionRangeStart
	 */
	void SetSelectionRange(TRange<float> Range);
	
	/**
	 * Set the selection range's end position to the current global time.
	 *
	 * @see GetSelectionRange, SetSelectionRange, SetSelectionRangeStart
	 */
	void SetSelectionRangeEnd();
	
	/**
	 * Set the selection range's start position to the current global time.
	 *
	 * @see GetSelectionRange, SetSelectionRange, SetSelectionRangeEnd
	 */
	void SetSelectionRangeStart();

	/** Clear and reset the selection range. */
	void ResetSelectionRange();

	/** Select all keys that fall into the current selection range. */
	void SelectKeysInSelectionRange();

public:

	/**
	 * Get the playback range.
	 *
	 * @return Playback range.
	 * @see SetPlaybackRange, SetPlaybackRangeEnd, SetPlaybackRangeStart
	 */
	TRange<float> GetPlaybackRange() const;

	/**
	 * Set the playback range.
	 *
	 * @param Range The new range to set.
	 * @see GetPlaybackRange, SetPlaybackRangeEnd, SetPlaybackRangeStart
	 */
	void SetPlaybackRange(TRange<float> Range);
	
	/**
	 * Set the playback range's end position to the current global time.
	 *
	 * @see GetPlaybackRange, SetPlaybackRange, SetPlaybackRangeStart
	 */
	void SetPlaybackRangeEnd()
	{
		SetPlaybackRange(TRange<float>(GetPlaybackRange().GetLowerBoundValue(), GetGlobalTime()));
	}

	/**
	 * Set the playback range's start position to the current global time.
	 *
	 * @see GetPlaybackRange, SetPlaybackRange, SetPlaybackRangeStart
	 */
	void SetPlaybackRangeStart()
	{
		SetPlaybackRange(TRange<float>(GetGlobalTime(), GetPlaybackRange().GetUpperBoundValue()));
	}

public:

	void ResetViewRange();
	void ZoomViewRange(float InZoomDelta);
	void ZoomInViewRange();
	void ZoomOutViewRange();

public:

	/** Access the user-supplied settings object */
	USequencerSettings* GetSettings() const
	{
		return Settings;
	}

	/** Gets the tree of nodes which is used to populate the animation outliner. */
	TSharedRef<FSequencerNodeTree> GetNodeTree()
	{
		return NodeTree;
	}

	bool IsPerspectiveViewportPossessionEnabled() const override
	{
		return bPerspectiveViewportPossessionEnabled;
	}

	bool IsPerspectiveViewportCameraCutEnabled() const override
	{
		return bPerspectiveViewportCameraCutEnabled;
	}

	/**
	 * Pops the current focused movie scene from the stack.  The parent of this movie scene will be come the focused one
	 */
	void PopToSequenceInstance( TSharedRef<FMovieSceneSequenceInstance> SequenceInstance );

	/** Deletes the passed in sections. */
	void DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection> > & Sections);

	/** Deletes the currently selected in keys. */
	void DeleteSelectedKeys();

	/** Set interpolation modes. */
	void SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode);

	/** Is interpolation mode selected. */
	bool IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const;

	/** Snap the currently selected keys to frame. */
	void SnapToFrame();

	/** Are there keys to snap? */
	bool CanSnapToFrame() const;

	/**
	 * @return Movie scene tools used by the sequencer
	 */
	const TArray<TSharedPtr<ISequencerTrackEditor>>& GetTrackEditors() const
	{
		return TrackEditors;
	}

public:

	/**
	 * Converts the specified possessable GUID to a spawnable
	 *
	 * @param	PossessableGuid		The guid of the possessable to convert
	 */
	void ConvertToSpawnable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted);

	/**
	 * Converts the specified spawnable GUID to a possessable
	 *
	 * @param	SpawnableGuid		The guid of the spawnable to convert
	 */
	void ConvertToPossessable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted);

	/**
	 * Converts all the currently selected nodes to be spawnables, if possible
	 */
	void ConvertSelectedNodesToSpawnables();

	/**
	 * Converts all the currently selected nodes to be possessables, if possible
	 */
	void ConvertSelectedNodesToPossessables();

protected:

	/**
	 * Attempts to add a new spawnable to the MovieScene for the specified asset, class, or actor
	 *
	 * @param	Object	The asset, class, or actor to add a spawnable for
	 *
	 * @return	The spawnable ID, or invalid ID on failure
	 */
	FGuid AddSpawnable( UObject& Object );

	/**
	 * Setup a new spawnable object with some default tracks and keys
	 *
	 * @param	Guid		The ID of the spawnable to setup defaults for
	 * @param	Transform	The default transform for the spawnable
	 */
	void SetupDefaultsForSpawnable( const FGuid& Guid, const FTransformData& Transform );

public:

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
	 * @return true if anything was deleted, otherwise false.
	 */
	virtual bool OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted );

	/** Zooms to the edges of all currently selected sections. */
	void ZoomToSelectedSections();

	/** Gets the overlay fading animation curve lerp. */
	float GetOverlayFadeCurve() const;

	/** Gets the command bindings for the sequencer */
	virtual TSharedPtr<FUICommandList> GetCommandBindings(ESequencerCommandBindings Type = ESequencerCommandBindings::Sequencer) const override
	{
		if (Type == ESequencerCommandBindings::Sequencer)
		{
			return SequencerCommandBindings;
		}

		return SequencerSharedBindings;
	}

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

	/** Called when an actor is dropped into Sequencer */
	void OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors );

	/** Generates and attaches transport control widgets to the main level editor viewports */
	void AttachTransportControlsToViewports();
	/** Purges all transport control widgets from the main level editor viewports */
	void DetachTransportControlsFromViewports();

	/** Gets the color and opacity of a transport control */
	FLinearColor GetTransportControlsColorAndOpacity(TSharedRef<SWidget> TransportWidget) const;

	/** Gets the visibility of a transport control based on the viewport it's attached to */
	EVisibility GetTransportControlVisibility(TSharedPtr<ILevelViewport> LevelViewport) const;

	/** Extend the level editor viewport menu */
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);

	/** Extend the level viewport context menu */
	void AddLevelViewportMenuExtender();
	
	/** Remove a previously registered extender for the level viewport context menu */
	void RemoveLevelViewportMenuExtender();

	/** Get the extender for the level viewport menu */
	TSharedRef<FExtender> GetLevelViewportExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors);
	
	/** Add level editor actions */
	void BindLevelEditorCommands();
	
	/** Remove level editor actions */
	void UnbindLevelEditorCommands();

	void RecordSelectedActors();
	
	/** Create a menu entry we can use to toggle the transport controls */
	void CreateTransportToggleMenuEntry(FMenuBuilder& MenuBuilder);

	/** Functions to push on to the transport controls we use */
	FReply OnPlay(bool bTogglePlay=true, float InPlayRate=1.f);
	FReply OnRecord();
	FReply OnStepForward();
	FReply OnStepBackward();
	FReply OnStepToEnd();
	FReply OnStepToBeginning();
	FReply OnToggleLooping();
	FReply SetPlaybackEnd();
	FReply SetPlaybackStart();
	FReply JumpToPreviousKey();
	FReply JumpToNextKey();

	/** Get the visibility of the record button */
	EVisibility GetRecordButtonVisibility() const;

	/** Delegate handler for recording starting */
	void HandleRecordingStarted(UMovieSceneSequence* Sequence);

	/** Delegate handler for recording finishing */
	void HandleRecordingFinished(UMovieSceneSequence* Sequence);

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

	/** @return Whether to show the curve editor or not */
	void SetShowCurveEditor(bool bInShowCurveEditor);
	bool GetShowCurveEditor() const { return bShowCurveEditor; }

	/** Called to save the current movie scene */
	void SaveCurrentMovieScene();

	/** Called to save the current movie scene under a new name */
	void SaveCurrentMovieSceneAs();

	/** Called when a user executes the assign actor to track menu item */
	void AssignActor(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);
	void DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid ObjectBinding);

	/** Called when a user executes the delete node menu item */
	void DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted);
	void DeleteSelectedNodes();

	/** Called when a user executes the copy track menu item */
	void CopySelectedTracks(TArray<TSharedPtr<FSequencerTrackNode>>& TrackNodes);
	void ExportTracksToText(TArray<UMovieSceneTrack*> TrackToExport, /*out*/ FString& ExportedText);

	/** Called when a user executes the paste track menu item */
	void PasteCopiedTracks(TArray<TSharedPtr<FSequencerObjectBindingNode>>& ObjectNodes);
	void ImportTracksFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneTrack*>& ImportedTrack);

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

	FSequencerLabelManager& GetLabelManager()
	{
		return LabelManager;
	}

	/** Select keys belonging to a section at the key time */
	void SelectTrackKeys(TWeakObjectPtr<UMovieSceneSection> Section, float KeyTime, bool bAddToSelection, bool bToggleSelection);

	/** Updates the external selection to match the current sequencer selection. */
	void SynchronizeExternalSelectionWithSequencerSelection();

	/** Updates the sequencer selection to match the current external selection. */
	void SynchronizeSequencerSelectionWithExternalSelection();

public:

	/** Copy the selection, whether it's keys or tracks */
	void CopySelection();

	/** Cut the selection, whether it's keys or tracks */
	void CutSelection();

	/** Copy the selected keys to the clipboard */
	void CopySelectedKeys();

	/** Copy the selected keys to the clipboard, then delete them as part of an undoable transaction */
	void CutSelectedKeys();

	/** Get the in-memory clipboard stack */
	const TArray<TSharedPtr<FMovieSceneClipboard>>& GetClipboardStack() const;

	/** Promote a clipboard to the top of the clipboard stack, and update its timestamp */
	void OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard);

	/** Discard all changes to the current movie scene. */
	void DiscardChanges();

	/** Create camera and set it as the current camera cut. */
	void CreateCamera();

	/** Called when a new camera is added */
	void NewCameraAdded(ACineCameraActor* NewCamera, FGuid CameraGuid, bool bLockToCamera);

	/** Attempts to automatically fix up broken actor references in the current scene. */
	void FixActorReferences();

	/** Moves all time data for the current scene onto a valid frame. */
	void FixFrameTiming();

	/** Imports the animation from an fbx file. */
	void ImportFBX();

	/** Exports the animation to an fbx file. */
	void ExportFBX();

public:
	
	/** Access the currently active track area edit tool */
	const ISequencerEditTool* GetEditTool() const;

	/** Get the current active hotspot */
	TSharedPtr<ISequencerHotspot> GetHotspot() const;

	/** Set the hotspot to something else */
	void SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot);

protected:

	/** The current hotspot that can be set from anywhere to initiate drags */
	TSharedPtr<ISequencerHotspot> Hotspot;

public:

	/** Put the sequencer in a horizontally auto-scrolling state with the given rate */
	void StartAutoscroll(float UnitsPerS);

	/** Stop the sequencer from auto-scrolling */
	void StopAutoscroll();

	/** Scroll the sequencer vertically by the specified number of slate units */
	void VerticalScroll(float ScrollAmountUnits);

public:

	//~ FGCObject Interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

public:

	//~ FTickableEditorObject Interface

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FSequencer, STATGROUP_Tickables); };

public:

	//~ ISequencer Interface

	virtual void Close() override;
	virtual TSharedRef<SWidget> GetSequencerWidget() const override;
	virtual UMovieSceneSequence* GetRootMovieSceneSequence() const override;
	virtual UMovieSceneSequence* GetFocusedMovieSceneSequence() const override;
	virtual void ResetToNewRootSequence(UMovieSceneSequence& NewSequence) override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetRootMovieSceneSequenceInstance() const override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetFocusedMovieSceneSequenceInstance() const override;
	virtual void FocusSequenceInstance( UMovieSceneSubSection& InSubSection ) override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetSequenceInstanceForSection(UMovieSceneSection& Section) const override;
	virtual bool HasSequenceInstanceForSection(UMovieSceneSection& Section) const override;
	virtual EAutoKeyMode GetAutoKeyMode() const override;
	virtual void SetAutoKeyMode(EAutoKeyMode AutoKeyMode) override;
	virtual bool GetKeyAllEnabled() const override;
	virtual void SetKeyAllEnabled(bool bKeyAllEnabled) override;
	virtual bool GetKeyInterpPropertiesOnly() const override;
	virtual void SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) override;
	virtual EMovieSceneKeyInterpolation GetKeyInterpolation() const override;
	virtual void SetKeyInterpolation(EMovieSceneKeyInterpolation) override;
	virtual bool GetInfiniteKeyAreas() const override;
	virtual void SetInfiniteKeyAreas(bool bInfiniteKeyAreas) override;
	virtual bool IsRecordingLive() const override;
	virtual float GetCurrentLocalTime(UMovieSceneSequence& InMovieSceneSequence) override;
	virtual float GetGlobalTime() const override;
	virtual void SetGlobalTime(float Time, ESnapTimeMode SnapTimeMode = ESnapTimeMode::STM_None, bool bRestarted = false) override;
	virtual void SetGlobalTimeDirectly(float Time, ESnapTimeMode SnapTimeMode = ESnapTimeMode::STM_None, bool bRestarted = false) override;
	virtual void SetPerspectiveViewportPossessionEnabled(bool bEnabled) override;
	virtual void SetPerspectiveViewportCameraCutEnabled(bool bEnabled) override;
	virtual void RenderMovie(UMovieSceneSection* InSection) const override;
	virtual void EnterSilentMode() override { ++SilentModeCount; }
	virtual void ExitSilentMode() override { --SilentModeCount; ensure(SilentModeCount >= 0); }
	virtual bool IsInSilentMode() const override { return SilentModeCount != 0; }
	virtual FGuid GetHandleToObject(UObject* Object, bool bCreateHandleIfMissing = true) override;
	virtual ISequencerObjectChangeListener& GetObjectChangeListener() override;
protected:
	virtual void NotifyMovieSceneDataChangedInternal() override;
public:
	virtual void NotifyMovieSceneDataChanged( EMovieSceneDataChangeType DataChangeType ) override;
	virtual void UpdateRuntimeInstances() override;
	virtual void UpdatePlaybackRange() override;
	virtual void AddActors(const TArray<TWeakObjectPtr<AActor> >& InActors) override;
	virtual void AddSubSequence(UMovieSceneSequence* Sequence) override;
	virtual bool CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const override;
	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) override;
	virtual FSequencerSelection& GetSelection() override;
	virtual FSequencerSelectionPreview& GetSelectionPreview() override;
	virtual void NotifyMapChanged(UWorld* NewWorld, EMapChangeType MapChangeType) override;
	virtual FOnGlobalTimeChanged& OnGlobalTimeChanged() override { return OnGlobalTimeChangedDelegate; }
	virtual FOnMovieSceneDataChanged& OnMovieSceneDataChanged() override { return OnMovieSceneDataChangedDelegate; }
	virtual FOnSelectionChangedObjectGuids& GetSelectionChangedObjectGuids() override { return OnSelectionChangedObjectGuidsDelegate; }
	virtual FGuid CreateBinding(UObject& InObject, const FString& InName) override;
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;
	virtual void GetAllKeyedProperties(UObject& Object, TSet<UProperty*>& OutProperties) override;
	virtual FOnActorAddedToSequencer& OnActorAddedToSequencer() override;
	virtual FOnPreSave& OnPreSave() override;
	virtual FOnActivateSequence& OnActivateSequence() override;
	virtual FOnCameraCut& OnCameraCut() override;
	virtual TSharedRef<INumericTypeInterface<float>> GetNumericTypeInterface() override;
	virtual TSharedRef<INumericTypeInterface<float>> GetZeroPadNumericTypeInterface() override;
	virtual TSharedRef<SWidget> MakeTransportControls(bool bExtended) override;
	virtual TSharedRef<SWidget> MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange) override;
	virtual void SetViewportTransportControlsVisibility(bool bVisible) override;
	virtual UObject* FindSpawnedObjectOrTemplate(const FGuid& BindingId) const override;
	virtual FGuid MakeNewSpawnable(UObject& SourceObject) override;
	
public:

	// IMovieScenePlayer interface

	virtual void GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray<TWeakObjectPtr<UObject>>& OutObjects) const override;
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override;
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override;
	virtual void AddOrUpdateMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd) override;
	virtual void RemoveMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove) override;
	virtual IMovieSceneSpawnRegister& GetSpawnRegister() override { return *SpawnRegister; }
	virtual bool IsPreview() const override { return SilentModeCount != 0; }
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
	 * Called when the clamp range is changed by the user
	 *
	 * @param	NewClampRange The new clamp range
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

	/** Called when the user has begun dragging the playback range */
	void OnPlaybackRangeBeginDrag();

	/** Called when the user has finished dragging the playback range */
	void OnPlaybackRangeEndDrag();

	/** Called when the user has begun dragging the selection range */
	void OnSelectionRangeBeginDrag();

	/** Called when the user has finished dragging the selection range */
	void OnSelectionRangeEndDrag();

protected:

	/**
	 * Update auto-scroll mechanics as a result of a new time position
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

	/** Find the viewed sequence asset in the content browser. */
	void FindInContentBrowser();

	/** Get the asset we're currently editing, if applicable. */
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

	/** Get all the keys for the current sequencer selection */
	virtual void GetKeysFromSelection(TUniquePtr<ISequencerKeyCollection>& KeyCollection) override;

	/** Find the nearest keyframe */
	float FindNearestKey(float NewScrubPosition) override;

protected:

	/** Called via UEditorEngine::GetActorRecordingStateEvent to check to see whether we need to record actor state */
	void GetActorRecordingState( bool& bIsRecording /* In+Out */ ) const;
	
	/** Called when a user executes the delete command to delete sections or keys */
	void DeleteSelectedItems();
	
	/** Transport controls */
	void TogglePlay();
	void PlayForward();
	void Rewind();
	void ShuttleForward();
	void ShuttleBackward();
	void Pause();
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
	void BindCommands();

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

	/** Called after a level has been added */
	void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);

	/** Called after a level has been removed */
	void OnLevelRemoved(ULevel* InLevel, UWorld* InWorld);

	/** Called after a new level has been created. The sequencer editor mode needs to be enabled. */
	void OnNewCurrentLevel();

	/** Called after a map has been opened. The sequencer editor mode needs to be enabled. */
	void OnMapOpened(const FString& Filename, bool bLoadAsTemplate);

	/** Called when new actors are dropped in the viewport. */
	void OnNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors);

	/** Called before a PIE session begins. */
	void OnPreBeginPIE(bool bIsSimulating);

	/** Called after a PIE session ends. */
	void OnEndPIE(bool bIsSimulating);

	/** Called after PIE session ends and maps have been cleaned up */
	void OnEndPlayMap();

	/** Updates a viewport client from camera cut data */
	void UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject, bool bJumpCut) const;

	/** Internal conversion function that doesn't perform expensive reset/update tasks */
	FMovieSceneSpawnable* ConvertToSpawnableInternal(FGuid PossessableGuid);

	/** Internal conversion function that doesn't perform expensive reset/update tasks */
	FMovieScenePossessable* ConvertToPossessableInternal(FGuid SpawnableGuid);

	/** Internal function to render movie for a given start/end time */
	void RenderMovieInternal(float InStartTime, float InEndTime) const;

	/** Handles adding a new folder to the outliner tree. */
	void OnAddFolder();

	/** Handles the actor selection changing externally .*/
	void OnActorSelectionChanged( UObject* );

	/** Create set playback start transport control */
	TSharedRef<SWidget> OnCreateTransportSetPlaybackStart();

	/** Create jump to previous key transport control */
	TSharedRef<SWidget> OnCreateTransportJumpToPreviousKey();

	/** Create jump to next key transport control */
	TSharedRef<SWidget> OnCreateTransportJumpToNextKey();

	/** Create set playback end transport control */
	TSharedRef<SWidget> OnCreateTransportSetPlaybackEnd();

	/** Select all keys in a display node that fall into the current selection range. */
	void SelectKeysInSelectionRange(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const TRange<float>& SelectionRange);
	/** Create record transport control */
	TSharedRef<SWidget> OnCreateTransportRecord();

	/** Possess PIE viewports with the specified camera settings (a mirror of level viewport possession, but for game viewport clients) */
	void PossessPIEViewports(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut);

private:
	/** Performs any post-tick rendering work needed when moving through scenes */
	void PostTickRenderStateFixup();

	/** User-supplied settings object for this sequencer */
	USequencerSettings* Settings;

	TMap< TWeakObjectPtr<UMovieSceneSection>, TSharedRef<FMovieSceneSequenceInstance> > SequenceInstanceBySection;

	/** Command list for sequencer commands (Sequencer widgets only). */
	TSharedRef<FUICommandList> SequencerCommandBindings;

	/** Command list for sequencer commands (shared by non-Sequencer). */
	TSharedRef<FUICommandList> SequencerSharedBindings;

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

	/** The playback rate */
	float PlayRate;

	/** The shuttle multiplier */
	float ShuttleMultiplier;

	bool bPerspectiveViewportPossessionEnabled;
	bool bPerspectiveViewportCameraCutEnabled;

	/** True if this sequencer is being edited within the level editor */
	bool bIsEditingWithinLevelEditor;

	bool bShowCurveEditor;

	/** Generic Popup Entry */
	TWeakPtr<IMenu> EntryPopupMenu;

	/** Stores a dirty bit for whether the sequencer tree (and other UI bits) may need to be refreshed.  We
	    do this simply to avoid refreshing the UI more than once per frame. (e.g. during live recording where
		the MovieScene data can change many times per frame.) */
	bool bNeedTreeRefresh;

	/** When true, the runtime instances need to be updated next frame. */
	bool bNeedInstanceRefresh;

	/** Stores the playback status to be restored on refresh. */
	EMovieScenePlayerStatus::Type StoredPlaybackState;

	/** A flag which indicates Close() was called on this Sequencer. */ 
	bool bWasClosed;

	FSequencerLabelManager LabelManager;
	FSequencerSelection Selection;
	FSequencerSelectionPreview SelectionPreview;

	/** Represents the tree of nodes to display in the animation outliner. */
	TSharedRef<FSequencerNodeTree> NodeTree;

	/** A delegate which is called any time the global time changes. */
	FOnGlobalTimeChanged OnGlobalTimeChangedDelegate;

	/** A delegate which is called any time the movie scene data is changed. */
	FOnMovieSceneDataChanged OnMovieSceneDataChangedDelegate;

	/** A delegate which is called any time the sequencer selection changes. */
	FOnSelectionChangedObjectGuids OnSelectionChangedObjectGuidsDelegate;

	/** A map of all the transport controls to viewports that this sequencer has made */
	TMap< TSharedPtr<class ILevelViewport>, TSharedPtr<class SWidget> > TransportControls;

	FOnActorAddedToSequencer OnActorAddedToSequencerEvent;
	FOnCameraCut OnCameraCutEvent;
	FOnPreSave OnPreSaveEvent;
	FOnActivateSequence OnActivateSequenceEvent;

	int32 SilentModeCount;
	
	/** Menu extender for level viewports */
	FLevelEditorModule::FLevelEditorMenuExtender ViewMenuExtender;
	FDelegateHandle LevelEditorExtenderDelegateHandle;

	/** When true the sequencer selection is being updated from changes to the external selection. */
	bool bUpdatingSequencerSelection;

	/** When true the external selection is being updated from changes to the sequencer selection. */
	bool bUpdatingExternalSelection;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	double OldMaxTickRate;

	struct FCachedViewTarget
	{
		/** The player controller we're possessing */
		TWeakObjectPtr<APlayerController> PlayerController;
		/** The view target it was pointing at before we took over */
		TWeakObjectPtr<AActor> ViewTarget;
	};
	
	/** Cached array of view targets that were set before we possessed the player controller with a camera from sequencer */
	TArray<FCachedViewTarget> PrePossessionViewTargets;

	FDelegateHandle LevelViewportExtenderHandle;

	/** Attribute used to retrieve the playback context for this frame */
	TAttribute<UObject*> PlaybackContextAttribute;

	/** Cached playback context for this frame */
	TWeakObjectPtr<UObject> CachedPlaybackContext;

	/** Attribute used to retrieve event contexts */
	TAttribute<TArray<UObject*>> EventContextsAttribute;

	/** Event contexts retrieved from the above attribute once per frame */
	TArray<TWeakObjectPtr<UObject>> CachedEventContexts;
};
