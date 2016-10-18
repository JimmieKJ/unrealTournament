// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Public/BlueprintEditor.h"

#include "AdvancedPreviewScene.h"

#include "PersonaModule.h"
#include "PersonaCommands.h"

//////////////////////////////////////////////////////////////////////////
// FPersona

class UBlendProfile;
class USkeletalMesh;
class SAnimationEditorViewportTabBody;

/**
 * Persona asset editor (extends Blueprint editor)
 */
class FPersona : public FBlueprintEditor
{
public:
	/**
	 * Edits the specified character asset(s)
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InitSkeleton			The skeleton to edit.  If specified, Blueprint must be NULL.
	 * @param	InitAnimBlueprint		The blueprint object to start editing.  If specified, Skeleton and AnimationAsset must be NULL.
	 * @param	InitAnimationAsset		The animation asset to edit.  If specified, Blueprint must be NULL.
	 * @param	InitMesh				The mesh asset to edit.  If specified, Blueprint must be NULL.	 
	 */
	void InitPersona(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class USkeleton* InitSkeleton, class UAnimBlueprint* InitAnimBlueprint, class UAnimationAsset* InitAnimationAsset, class USkeletalMesh* InitMesh);

public:
	FPersona();

	virtual ~FPersona();

	TSharedPtr<SDockTab> OpenNewDocumentTab(class UAnimationAsset* InAnimAsset);

	void ExtendDefaultPersonaToolbar();

	/** Set the current preview viewport for Persona */
	void SetViewport(TWeakPtr<class SAnimationEditorViewportTabBody> NewViewport);
	/** Set Sequence browser */
	void SetSequenceBrowser(class SAnimationSequenceBrowser* SequenceBrowser);

	/** Refresh viewport */
	void RefreshViewport();

	/** Set the animation asset to preview **/
	void SetPreviewAnimationAsset(UAnimationAsset* AnimAsset, bool bEnablePreview=true);
	UObject* GetPreviewAnimationAsset() const;
	UObject* GetAnimationAssetBeingEdited() const;

	/** Update the inspector that displays information about the current selection*/
	void UpdateSelectionDetails(UObject* Object, const FText& ForcedTitle);

	void SetDetailObject(UObject* Obj);

	virtual void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated) override;

	/** Get the preview DebugSkelMeshComponent */
	UDebugSkelMeshComponent* GetPreviewMeshComponent() const;
	/** Get the AnimInstance associated with the preview DEbugSkelMeshComponent */
	UAnimInstance* GetPreviewAnimInstance() const;

	// Gets the skeleton being edited/viewed by this Persona instance
	USkeleton* GetSkeleton() const;
	UObject* GetSkeletonAsObject() const;

	// Gets the skeletal mesh being edited/viewed by this Persona instance
	USkeletalMesh* GetMesh() const;

	// Gets the physics asset being edited/viewed by this Persona instance
	UPhysicsAsset* GetPhysicsAsset() const;

	// Gets the Anim Blueprint being edited/viewed by this Persona instance
	UAnimBlueprint* GetAnimBlueprint() const;

	// Sets the current preview mesh
	void SetPreviewMesh(USkeletalMesh* NewPreviewMesh);

	// Validate preview attached assets on skeleton and supplied skeletal mesh, notifying user if any are removed
	void ValidatePreviewAttachedAssets(USkeletalMesh* SkeletalMeshToUse);

	// Thunks for SContentReference
	UObject* GetMeshAsObject() const;
	UObject* GetPhysicsAssetAsObject() const;
	UObject* GetAnimBlueprintAsObject() const;

	// Sets the selected bone to preview
	void SetSelectedBone(class USkeleton* Skeleton, const FName& BoneName, bool bRebroadcast = true );

	/** Helper method to clear out any selected bones */
	void ClearSelectedBones();

	/** Sets the selected socket to preview */
	void SetSelectedSocket( const FSelectedSocketInfo& SocketInfo, bool bRebroadcast = true );

	/** Duplicates and selects the socket specified (used when Alt-Dragging in the viewport ) */
	void DuplicateAndSelectSocket( const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName = FName() );

	/** Clears the selected socket (removes both the rotate/transform widget and clears the details panel) */
	void ClearSelectedSocket();

	/** Rename socket to new name */
	void RenameSocket( USkeletalMeshSocket* Socket, const FName& NewSocketName );

	/** Reparent socket to new parent */
	void ChangeSocketParent( USkeletalMeshSocket* Socket, const FName& NewSocketParent );

	/** Clears the selected wind actor */
	void ClearSelectedWindActor();

	/** Clears the selected anim graph node */
	void ClearSelectedAnimGraphNode();

	/** Clears the selection (both sockets and bones). Also broadcasts this */
	void DeselectAll();

	/* Open details panel for the skeletal preview mesh */
	FReply OnClickEditMesh();

	/** Generates a unique socket name from the input name, by changing the FName's number */
	FName GenerateUniqueSocketName( FName InName );

	/** Function to tell you if a socket name already exists in a given array of sockets */
	bool DoesSocketAlreadyExist( const class USkeletalMeshSocket* InSocket, const FText& InSocketName, const TArray< USkeletalMeshSocket* >& InSocketArray ) const;

	/** Creates a new skeletal mesh component with the right master pose component set */
	USkeletalMeshComponent*	CreateNewSkeletalMeshComponent();

	/** Handler for a user removing a mesh in the loaded meshes panel */
	void RemoveAdditionalMesh(USkeletalMeshComponent* MeshToRemove);

	/** Cleans up any additional meshes that have been loaded */
	void ClearAllAdditionalMeshes();

	/** Adds to the viewport all the attached preview objects that the current skeleton and mesh contains */
	void AddPreviewAttachedObjects();

	/** Attaches an object to the preview component using the supplied attach name, returning whether it was successfully attached or not */
	bool AttachObjectToPreviewComponent( UObject* Object, FName AttachTo, FPreviewAssetAttachContainer* PreviewAssetContainer = NULL );

	/**Removes a currently attached object from the preview component */
	void RemoveAttachedObjectFromPreviewComponent(UObject* Object, FName AttachedTo);

	/** Get the viewport scene component for the specified attached asset */
	USceneComponent* GetComponentForAttachedObject(UObject* Asset, FName AttachedTo);

	/** Removes attached components from the preview component. (WARNING: There is a possibility that this function will
	 *  remove the wrong component if 2 of the same type (same UObject) are attached at the same location!
	 *
	 * @param bRemovePreviewAttached	Specifies whether all attached components are removed or just the ones
	 *									that haven't come from the target skeleton.
	 */
	void RemoveAttachedComponent(bool bRemovePreviewAttached = true);

	/** Destroy the supplied component (and its children) */
	void CleanupComponent(USceneComponent* Component);

	/** Returns the editors preview scene */
	FAdvancedPreviewScene& GetPreviewScene() { return PreviewScene; }

	/** Gets called when it's clicked via mode tab - Reinitialize the mode **/
	void ReinitMode();

	/** Called when an asset is imported into the editor */
	void OnPostImport(UFactory* InFactory, UObject* InObject);

	/** Called when the blend profile tab selects a profile */
	void SetSelectedBlendProfile(UBlendProfile* InBlendProfile);

	/** Check whether this Persona instance is recording */
	bool IsRecording() const;

	/** Stop recording in this Persona instance */
	void StopRecording();

	/** Get the currently recording animation */
	UAnimSequence* GetCurrentRecording() const;

	/** Get the currently recording animation time */
	float GetCurrentRecordingTime() const;

	/** Toggle playback of the current animation */
	void TogglePlayback();

public:
	//~ Begin IToolkit Interface
	virtual FName GetToolkitContextFName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;	
	//~ End IToolkit Interface

	/** Saves all animation assets related to a skeleton */
	void SaveAnimationAssets_Execute();
	bool CanSaveAnimationAssets() const;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Animation/Persona"));
	}
	
	/** Returns a pointer to the Blueprint object we are currently editing, as long as we are editing exactly one */
	virtual UBlueprint* GetBlueprintObj() const override;

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

	/** Returns the image brush to use for each modes dirty marker */
	const FSlateBrush* GetDirtyImageForMode(FName Mode) const;

	TSharedRef<SWidget> GetPreviewEditor() { return PreviewEditor.ToSharedRef(); }
	/** Refresh Preview Instance Track Curves **/
	void RefreshPreviewInstanceTrackCurves();

	void RecompileAnimBlueprintIfDirty();

	/* Reference Pose Handler */
	bool IsShowReferencePoseEnabled() const;
	bool CanShowReferencePose() const;
	void ShowReferencePose(bool bReferencePose);

	/* Handle error checking for additive base pose */
	bool ShouldDisplayAdditiveScaleErrorMessage();

	/** Validates curve use */
	void TestSkeletonCurveNamesForUse() const;

protected:
	bool IsPreviewAssetEnabled() const;
	bool CanPreviewAsset() const;
	FText GetPreviewAssetTooltip() const;

	//~ Begin FBlueprintEditor Interface
	//virtual void CreateDefaultToolbar() override;
	virtual void CreateDefaultCommands() override;
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList);
	virtual void OnSelectBone() override;
	virtual bool CanSelectBone() const override;
	virtual void OnAddPosePin() override;
	virtual bool CanAddPosePin() const override;
	virtual void OnRemovePosePin() override;
	virtual bool CanRemovePosePin() const override;
	virtual void Compile() override;
	virtual void OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor) override;
	virtual void OnConvertToSequenceEvaluator() override;
	virtual void OnConvertToSequencePlayer() override;
	virtual void OnConvertToBlendSpaceEvaluator() override;
	virtual void OnConvertToBlendSpacePlayer() override;
	virtual void OnConvertToPoseBlender() override;
	virtual void OnConvertToPoseByName() override;
	virtual bool IsInAScriptingMode() const override;
	virtual void OnOpenRelatedAsset() override;
	virtual void GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const override;
	virtual void CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints) override;
	virtual FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* InGraph) const override;
	virtual bool IsEditable(UEdGraph* InGraph) const override;
	virtual FText GetGraphDecorationString(UEdGraph* InGraph) const override;
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled = false) override;
	//~ End FBlueprintEditor Interface

	//~ Begin IAssetEditorInstance Interface
	virtual void FocusWindow(UObject* ObjectToFocusOn = NULL) override;
	//~ End IAssetEditorInstance Interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	//~ Begin FAssetEditorToolkit Interface
	virtual void FindInContentBrowser_Execute() override;
	// End of FAssetEditorToolkit

	// Generic Command handlers
	void OnCommandGenericDelete();

	// Pose watch handler
	void OnTogglePoseWatch();
protected:
	// 
	TSharedPtr<SDockTab> OpenNewAnimationDocumentTab(UObject* InAnimAsset);

	// Creates an editor widget for a specified animation document and returns the document link 
	TSharedPtr<SWidget> CreateEditorWidgetForAnimDocument(UObject* InAnimAsset, FString& OutDocumentLink);

	/** Callback when an object has been reimported, and whether it worked */
	void OnPostReimport(UObject* InObject, bool bSuccess);

	/** Refreshes the viewport and current editor mode if the passed in object is open in the editor */
	void ConditionalRefreshEditor(UObject* InObject);

	/** Clear up Preview Mesh's AnimNotifyStates. Called when undo or redo**/
	void ClearupPreviewMeshAnimNotifyStates();

	/** Handler connected to open skeleton so we get notified when it changes */
	void OnSkeletonHierarchyChanged();

protected:
	//~ IToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

protected:
	USkeleton* TargetSkeleton;
	TWeakObjectPtr<UObject> CachedPreviewAsset;

public:
	class UDebugSkelMeshComponent* PreviewComponent;

	// Array of loaded additional meshes
	TArray<USkeletalMeshComponent*> AdditionalMeshes;

	// The animation document currently being edited
	mutable TWeakObjectPtr<UObject> SharedAnimAssetBeingEdited;
	TWeakPtr<SDockTab> SharedAnimDocumentTab;

public:
	/** Viewport widget */
	TWeakPtr<class SAnimationEditorViewportTabBody> Viewport;

	// Property changed delegate
	FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate OnPropertyChangedHandle;
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	/** Shared data between modes - for now only used for viewport **/
	FPersonaModeSharedData ModeSharedData;

	/** holding this pointer to refresh persona mesh detials tab when LOD is changed **/
	class IDetailLayoutBuilder* PersonaMeshDetailLayout;
private:
	// called when animation asset has been changed
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnAnimChangedMulticaster, UAnimationAsset* )
	// Called when the preview mesh has been changed
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnPreviewMeshChangedMulticaster, USkeletalMesh* )
	// Called when a socket is selected
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnSelectSocket, const struct FSelectedSocketInfo& )
	// Called when a bone is selected
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnSelectBone, const FName& )
	// Called when a blend profile is selected in the blend profile tab
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnSelectBlendProfile, UBlendProfile*)
	// Called when the preview viewport is created
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnCreateViewport, TWeakPtr<SAnimationEditorViewportTabBody> )
public:
	// Called when Persona refreshes
	typedef FSimpleMulticastDelegate::FDelegate FOnPersonaRefresh;

	/** Registers a delegate to be called when Persona is Refreshed */
	void RegisterOnPersonaRefresh(const FOnPersonaRefresh& Delegate)
	{
		OnPersonaRefresh.Add(Delegate);
	}

	/** Unregisters refresh delegate */
	void UnregisterOnPersonaRefresh(SWidget* Widget)
	{
		OnPersonaRefresh.RemoveAll(Widget);
	}

	// anim changed 
	typedef FOnAnimChangedMulticaster::FDelegate FOnAnimChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	void RegisterOnAnimChanged(const FOnAnimChanged& Delegate)
	{
		OnAnimChanged.Add(Delegate);
	}

	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnAnimChanged(SWidget* Widget)
	{
		OnAnimChanged.RemoveAll(Widget);
	}

	// Called after an undo is performed to give child widgets a chance to refresh
	typedef FSimpleMulticastDelegate::FDelegate FOnPostUndo;

	/** Registers a delegate to be called after an Undo operation */
	void RegisterOnPostUndo(const FOnPostUndo& Delegate)
	{
		OnPostUndo.Add(Delegate);
	}

	/** Unregisters a delegate to be called after an Undo operation */
	void UnregisterOnPostUndo(SWidget* Widget)
	{
		OnPostUndo.RemoveAll(Widget);
	}

	// preview mesh changed 
	typedef FOnPreviewMeshChangedMulticaster::FDelegate FOnPreviewMeshChanged;

	/** Registers a delegate to be called when the preview mesh is changed */
	void RegisterOnPreviewMeshChanged(const FOnPreviewMeshChanged& Delegate)
	{
		OnPreviewMeshChanged.Add(Delegate);
	}

	/** Unregisters a delegate to be called when the preview mesh is changed */
	void UnregisterOnPreviewMeshChanged(SWidget* Widget)
	{
		OnPreviewMeshChanged.RemoveAll(Widget);
	}

	// Bone selected
	typedef FOnSelectBone::FDelegate FOnBoneSelected;

	/** Registers a delegate to be called when the selected bone is changed */
	void RegisterOnBoneSelected( const FOnBoneSelected& Delegate )
	{
		OnBoneSelected.Add( Delegate );
	}

	/** Unregisters a delegate to be called when the selected bone is changed */
	void UnregisterOnBoneSelected( SWidget* Widget )
	{
		OnBoneSelected.RemoveAll( Widget );
	}

	// Socket selected
	typedef FOnSelectSocket::FDelegate FOnSocketSelected;

	/** Registers a delegate to be called when the selected socket is changed */
	void RegisterOnSocketSelected( const FOnSocketSelected& Delegate )
	{
		OnSocketSelected.Add( Delegate );
	}

	/** Unregisters a delegate to be called when the selected socket is changed */
	void UnregisterOnSocketSelected(SWidget* Widget)
	{
		OnSocketSelected.RemoveAll( Widget );
	}

	typedef FOnSelectBlendProfile::FDelegate FOnBlendProfileSelected;

	void RegisterOnBlendProfileSelected(const FOnBlendProfileSelected& Delegate)
	{
		OnBlendProfileSelected.Add(Delegate);
	}

	void UnregisterOnBlendProfileSelected(SWidget* Widget)
	{
		OnBlendProfileSelected.RemoveAll(Widget);
	}

	// Called when selection is cleared
	typedef FSimpleMulticastDelegate::FDelegate FOnAllDeselected;

	/** Registers a delegate to be called when all bones/sockets are deselected */
	void RegisterOnDeselectAll(const FOnAllDeselected& Delegate)
	{
		OnAllDeselected.Add( Delegate );
	}

	/** Unregisters a delegate to be called when all bones/sockets are deselected */
	void UnregisterOnDeselectAll(SWidget* Widget)
	{
		OnAllDeselected.RemoveAll( Widget );
	}

	// Called when the skeleton tree has been changed (socket added/deleted/etc)
	typedef FSimpleMulticastDelegate::FDelegate FOnSkeletonTreeChanged;

	/** Registers a delegate to be called when the skeleton tree has changed */
	void RegisterOnChangeSkeletonTree(const FOnSkeletonTreeChanged& Delegate)
	{
		OnSkeletonTreeChanged.Add( Delegate );
	}

	/** Unregisters a delegate to be called when all bones/sockets are deselected */
	void UnregisterOnChangeSkeletonTree(SWidget* Widget)
	{
		OnSkeletonTreeChanged.RemoveAll( Widget );
	}

	// Called when the notifies of the current animation are changed
	typedef FSimpleMulticastDelegate::FDelegate FOnAnimNotifiesChanged;

	/** Registers a delegate to be called when the skeleton anim notifies have been changed */
	void RegisterOnChangeAnimNotifies(const FOnAnimNotifiesChanged& Delegate)
	{
		OnAnimNotifiesChanged.Add( Delegate );
	}

	/** Unregisters a delegate to be called when the skeleton anim notifies have been changed */
	void UnregisterOnChangeAnimNotifies(SWidget* Widget)
	{
		OnAnimNotifiesChanged.RemoveAll( Widget );
	}

	/** Delegate for when the skeletons animation notifies have been changed */
	FSimpleMulticastDelegate OnAnimNotifiesChanged;

	// Called when the curve panel is changed / updated
	typedef FSimpleMulticastDelegate::FDelegate FOnCurvesChanged;

	/** Registers delegate for changing / updating of curves panel */
	void RegisterOnChangeCurves(const FOnCurvesChanged& Delegate)
	{
		OnCurvesChanged.Add(Delegate);
	}

	/** Unregisters delegate for changing / updating of curves panel */
	void UnregisterOnChangeCurves(SWidget* Widget)
	{
		OnCurvesChanged.RemoveAll(Widget);
	}

	/** Delegate for changing / updating of curves panel */
	FSimpleMulticastDelegate OnCurvesChanged;

	// Called when the track curve is changed / updated
	typedef FSimpleMulticastDelegate::FDelegate FOnTrackCurvesChanged;

	/** Registers delegate for changing / updating of track curves panel */
	void RegisterOnChangeTrackCurves(const FOnTrackCurvesChanged& Delegate)
	{
		OnTrackCurvesChanged.Add(Delegate);
	}

	/** Unregisters delegate for changing / updating of track curves panel */
	void UnregisterOnChangeTrackCurves(SWidget* Widget)
	{
		OnTrackCurvesChanged.RemoveAll(Widget);
	}

	// Viewport Created
	typedef FOnCreateViewport::FDelegate FOnViewportCreated;

	/** Registers a delegate to be called when the preview viewport is created */
	void RegisterOnCreateViewport(const FOnViewportCreated& Delegate)
	{
		OnViewportCreated.Add( Delegate );
	}

	/** Unregisters a delegate to be called when the preview viewport is created */
	void UnregisterOnCreateViewport(SWidget* Widget)
	{
		OnViewportCreated.RemoveAll( Widget );
	}

	// Called when generic delete happens
	typedef FSimpleMulticastDelegate::FDelegate FOnDeleteGeneric;

	/** Registers a delegate to be called when Persona receives a generic delete command */
	void RegisterOnGenericDelete(const FOnDeleteGeneric& Delegate)
	{
		OnGenericDelete.Add(Delegate);
	}

	/** Unregisters a delegate to be called when Persona receives a generic delete command */
	void UnregisterOnGenericDelete(SWidget* Widget)
	{
		OnGenericDelete.RemoveAll(Widget);
	}

	/** Broadcasts section changes */
	FSimpleMulticastDelegate OnSectionsChanged;

	// Called when a section is changed
	typedef FSimpleMulticastDelegate::FDelegate FOnSectionsChanged;

	// Register a delegate to be called when a montage section changes
	void RegisterOnSectionsChanged(const FOnSectionsChanged& Delegate)
	{
		OnSectionsChanged.Add(Delegate);
	}

	// Unregister a delegate to be called when a montage section changes
	void UnregisterOnSectionsChanged(SWidget* Widget)
	{
		OnSectionsChanged.RemoveAll(Widget);
	}

	// Called when the notifies of the current animation are changed
	typedef FSimpleMulticastDelegate::FDelegate FOnLODChanged;

	/** Registers a delegate to be called when the skeleton anim notifies have been changed */
	void RegisterOnLODChanged(const FOnLODChanged& Delegate)
	{
		OnLODChanged.Add(Delegate);
	}

	/** Unregisters a delegate to be called when the skeleton anim notifies have been changed */
	void UnregisterOnLODChanged(SWidget* Widget)
	{
		OnLODChanged.RemoveAll(Widget);
	}

	/** Delegate for when the skeletons animation notifies have been changed */
	FSimpleMulticastDelegate OnLODChanged;

	/** Apply Compression to list of animations */
	void ApplyCompression(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences);
	/** Export to FBX files of the list of animations */
	void ExportToFBX(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences);
	/** Add looping interpolation to the list of animations */
	void AddLoopingInterpolation(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences);

protected:
	/** Undo Action**/
	void UndoAction();
	/** Redo Action **/
	void RedoAction();

protected:

	/** Called when persona is refreshed through an external action (reimport etc) */
	FSimpleMulticastDelegate OnPersonaRefresh;

	/** Delegate called after an undo operation for child widgets to refresh */
	FSimpleMulticastDelegate OnPostUndo;	

	/**	Broadcasts whenever the animation changes */
	FOnAnimChangedMulticaster OnAnimChanged;

	/** Broadcasts whenever the preview mesh changes */
	FOnPreviewMeshChangedMulticaster OnPreviewMeshChanged;
	
	/** Delegate for when a socket is selected by clicking its hit point */
	FOnSelectSocket OnSocketSelected;

	/** Delegate for when a bone is selected by clicking its hit point */
	FOnSelectBone OnBoneSelected;

	/** Delegate for when a blend profile is selected in the blend profile tab */
	FOnSelectBlendProfile OnBlendProfileSelected;

	/** Delegate for clearing the current skeleton bone/socket selection */
	FSimpleMulticastDelegate OnAllDeselected;

	/** Delegate for when the skeleton tree has changed (e.g. a socket has been duplicated in the viewport) */
	FSimpleMulticastDelegate OnSkeletonTreeChanged;

	/** Delegate for when the preview viewport is created */
	FOnCreateViewport OnViewportCreated;

	/** Delegate for changing / updating of track curves panel */
	FSimpleMulticastDelegate OnTrackCurvesChanged;

	/**
	 * Delegate for handling generic delete command
	 * Register to this if you want the generic delete command (delete key) from the editor. Also
	 * remember that the user may be using another part of the editor when you get the broadcast so
	 * make sure not to delete unless the user intends it.
	 */
	FSimpleMulticastDelegate OnGenericDelete;

	/** Persona toolbar builder class */
	TSharedPtr<class FPersonaToolbar> PersonaToolbar;

private:

	/** Recording animation functions **/
	void RecordAnimation();
	bool IsRecordAvailable() const;
	FSlateIcon GetRecordStatusImage() const;
	FText GetRecordStatusTooltip() const;
	FText GetRecordStatusLabel() const;
	FText GetRecordMenuLabel() const;

	/** Animation menu functions **/
	void OnApplyCompression();
	void OnExportToFBX();
	void OnAddLoopingInterpolation();
	bool HasValidAnimationSequencePlaying() const;
	/** Return true if currently in the given mode */
	bool IsInPersonaMode(const FName InPersonaMode) const;

	/** Animation Editing Features **/
	void OnSetKey();
	bool CanSetKey() const;
	void OnSetKeyCompleted();
	void OnApplyRawAnimChanges();
	bool CanApplyRawAnimChanges() const;
	
	/** Change skeleton preview mesh functions */
	void ChangeSkeletonPreviewMesh();
	bool CanChangeSkeletonPreviewMesh() const;

	/** Remove unused bones from skeleton functions */
	void RemoveUnusedBones();
	bool CanRemoveBones() const;

	// tool bar actions
	void OnAnimNotifyWindow();
	void OnRetargetManager();
	void OnReimportMesh();
	void OnImportAsset(enum EFBXImportType DefaultImportType);
	void OnReimportAnimation();
	void OnAssetCreated(const TArray<UObject*> NewAssets);
	TSharedRef< SWidget > GenerateCreateAssetMenu( USkeleton* Skeleton ) const;
	void FillCreateAnimationMenu(FMenuBuilder& MenuBuilder) const;
	void FillCreatePoseAssetMenu(FMenuBuilder& MenuBuilder) const;
	void CreateAnimation(const TArray<UObject*> NewAssets, int32 Option);
	void CreatePoseAsset(const TArray<UObject*> NewAssets, int32 Option);

	/** Extend menu and toolbar */
	void ExtendMenu();
	/** update skeleton ref pose based on current preview mesh */
	void UpdateSkeletonRefPose();
	/** set preview mesh internal use only. The mesh should be verified by now.  */
	void SetPreviewMeshInternal(USkeletalMesh* NewPreviewMesh);

	/** Returns the editor objects that are applicable for our current mode (e.g mesh, animation etc) */
	TArray<UObject*> GetEditorObjectsForMode(FName Mode) const;

	/** Called immediately prior to a blueprint compilation */
	void OnBlueprintPreCompile(UBlueprint* BlueprintToCompile);

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Preview scene for the editor */
	FAdvancedPreviewScene PreviewScene;

	/** Brush to use as a dirty marker for assets */
	const FSlateBrush* AssetDirtyBrush;

	/** Preview instance inspector widget */
	TSharedPtr<class SWidget> PreviewEditor;

	/** Sequence Browser **/
	TWeakPtr<class SAnimationSequenceBrowser> SequenceBrowser;

	/** Handle to the registered OnPropertyChangedHandle delegate */
	FDelegateHandle OnPropertyChangedHandleDelegateHandle;

	/** Last Cached LOD value of Preview Mesh Component */
	int32 LastCachedLODForPreviewComponent;

	/** Handle additive anim scale validation */
	bool bDoesAdditiveRefPoseHaveZeroScale;
	FGuid RefPoseGuid;

	static const FName PreviewSceneSettingsTabId;
};
