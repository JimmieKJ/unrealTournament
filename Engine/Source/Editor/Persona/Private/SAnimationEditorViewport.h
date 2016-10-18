// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "SEditorViewport.h"

//////////////////////////////////////////////////////////////////////////
// EAnimationPlaybackSpeeds

namespace EAnimationPlaybackSpeeds
{
	enum Type
	{
		OneTenth=0,
		Quarter,
		Half,
		Normal,
		Double,
		FiveTimes,
		TenTimes,
		NumPlaybackSpeeds
	};

	extern float Values[NumPlaybackSpeeds];
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewport

class SAnimationEditorViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SAnimationEditorViewport) {}
	SLATE_END_ARGS()

	~SAnimationEditorViewport();

	void Construct(const FArguments& InArgs, TSharedPtr<class FPersona> InPersona, TSharedPtr<class SAnimationEditorViewportTabBody> InTabBody);

protected:
	// SEditorViewport interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	// End of SEditorViewport interface

	/**  Handle undo/redo by refreshing the viewport */
	void OnUndoRedo();

protected:
	// Viewport client
	TSharedPtr<class FAnimationViewportClient> LevelViewportClient;

	// Pointer to the compound widget that owns this viewport widget
	TWeakPtr<class SAnimationEditorViewportTabBody> TabBodyPtr;

	// Pointer back to the Persona tool that owns us
	TWeakPtr<class FPersona> PersonaPtr;
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewportTabBody

class SAnimationEditorViewportTabBody : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimationEditorViewportTabBody )
		: _Persona()
		, _Skeleton()
		, _IsEditable(true)
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>, Persona )
		SLATE_ARGUMENT( USkeleton*, Skeleton )
		SLATE_ATTRIBUTE( bool, IsEditable )
	SLATE_END_ARGS()
public:

	void Construct(const FArguments& InArgs);
	SAnimationEditorViewportTabBody();
	virtual ~SAnimationEditorViewportTabBody();

	void RefreshViewport();

	/**
	 * @return The list of commands on the viewport that are bound to delegates                    
	 */
	const TSharedPtr<FUICommandList>& GetCommandList() const { return UICommandList; }

	/** Preview mode related function **/
	bool IsPreviewModeOn( int32 PreviewMode ) const;

	/** Sets up the viewport with Persona's preview component */
	void SetPreviewComponent( class UDebugSkelMeshComponent* PreviewComponent);

	/** Function to get the number of LOD models associated with the preview skeletal mesh*/
	int32 GetLODModelCount() const;

	/** LOD model selection checking function*/
	bool IsLODModelSelected( int32 LODSelectionType ) const;
	int32 GetLODSelection() const { return LODSelection; };

	/** Function to set the current playback speed*/
	void OnSetPlaybackSpeed(int32 PlaybackSpeedMode);

	/** Function to return whether the supplied playback speed is the current active one */
	bool IsPlaybackSpeedSelected(int32 PlaybackSpeedMode);

	/** Function to get anim viewport widget */
	TSharedPtr<class SEditorViewport> GetViewportWidget() const { return ViewportWidget; }

	/** Function to get viewport's current background color */
	FLinearColor GetViewportBackgroundColor( ) const;

	/** Function to set viewport's new background color */
	void SetViewportBackgroundColor( FLinearColor InColor );

	/** Function to get viewport's background color brightness */
	float GetBackgroundBrightness( ) const;

	/** Function to set viewport's background color brightness */
	void SetBackgroundBrightness( float Value );

	/** Function to check whether grid is displayed or not */
	bool IsShowingGrid() const;

	/** Gets the editor client for this viewport */
	FEditorViewportClient& GetLevelViewportClient()
	{		
		return *LevelViewportClient;
	}

	/** Gets the animation viewport client */
	TSharedRef<class FAnimationViewportClient> GetAnimationViewportClient() const;

	/** Save data from OldViewport, and Restore **/
	void SaveData(class SAnimationEditorViewportTabBody* OldViewport);
	void RestoreData();

	/** Returns Detail description of what's going with viewport **/
	FText GetDisplayString() const;

	/** Can we use gizmos? */
	bool CanUseGizmos() const;

	/** Function to check whether floor is auto aligned or not */
	bool IsAutoAlignFloor() const;
	
	/** Clears our reference to Persona, also cleaning up anything that depends on Persona first */
	void CleanupPersonaReferences();

	/** Gets Persona to access to PreviewComponent for checking whether it shows clothing options or not */
	TWeakPtr<FPersona> GetPersona() const 
	{ 
		return PersonaPtr; 
	}

	void SetWindStrength( float SliderPos );

	/** Function to get slider value which represents wind strength (0 - 1)*/
	float GetWindStrengthSliderValue() const;

	/** Function to get slider value which returns a string*/
	FText GetWindStrengthLabel() const;

	bool IsApplyingClothWind() const;

	/** Show gravity scale */
	void SetGravityScale( float SliderPos );
	float GetGravityScaleSliderValue() const;
	FText GetGravityScaleLabel() const;

	/** Function to set LOD model selection*/
	void OnSetLODModel(int32 LODSelectionType);

protected:


private:
	bool IsVisible() const;

	/**
	 * Binds our UI commands to delegates
	 */ 
	void BindCommands();

	/** Preview mode related function **/
	void SetPreviewMode( int32 PreviewMode );

	/** Show Morphtarget of SkeletalMesh **/
	void OnShowMorphTargets();

	bool IsShowingMorphTargets() const;

	/** Show Raw Animation on top of Compressed Animation **/
	void OnShowRawAnimation();

	bool IsShowingRawAnimation() const;

	/** Show non retargeted animation. */
	void OnShowNonRetargetedAnimation();

	bool IsShowingNonRetargetedPose() const;

	/** Show non retargeted animation. */
	void OnShowSourceRawAnimation();

	bool IsShowingSourceRawAnimation() const;

	/** Show non retargeted animation. */
	void OnShowBakedAnimation();

	bool IsShowingBakedAnimation() const;


	/** Additive Base Pose on top of full animation **/
	void OnShowAdditiveBase();

	bool IsShowingAdditiveBase() const;

	bool IsPreviewingAnimation() const;

	/** Function to show/hide bone names */
	void OnShowBoneNames();

	/** Function to check whether bone names are displayed or not */
	bool IsShowingBoneNames() const;
	
	/** Function to show/hide selected bone weight */
	void OnShowBoneWeight();

	/** Function to check whether bone weights are displayed or not*/
	bool IsShowingBoneWeight() const;

	/** Function to set Local axes mode of the specificed type */
	void OnSetBoneDrawMode(int32 BoneDrawMode);

	/** Local axes mode checking function for the specificed type*/
	bool IsBoneDrawModeSet(int32 BoneDrawMode) const;

	/** Function to set Local axes mode of the specificed type */
	void OnSetLocalAxesMode( int32 LocalAxesMode );

	/** Local axes mode checking function for the specificed type*/
	bool IsLocalAxesModeSet( int32 LocalAxesMode ) const;

	/** Function to show/hide socket hit points */
	void OnShowSockets();

	/** Function to check whether socket hit points are displayed or not*/
	bool IsShowingSockets() const;

	/** Function to show/hide mesh info*/
	void OnShowDisplayInfo(int32 DisplayInfoMode);

	/** Function to check whether mesh info is displayed or not */
	bool IsShowingMeshInfo(int32 DisplayInfoMode) const;

	/** Function to show/hide grid in the viewport */
	void OnShowGrid();
	
	/** Toggles floor alignment in the preview scene */
	void OnToggleAutoAlignFloor();

	/** update reference pose with current preview mesh */
	void UpdateReferencePose();

	/** Called to toggle showing of reference pose on current preview mesh */
	void ShowReferencePose();
	bool CanShowReferencePose() const;
	bool IsShowReferencePoseEnabled() const;

	/** Called to toggle showing of reference pose on current preview mesh */
	void ShowRetargetBasePose();
	bool CanShowRetargetBasePose() const;
	bool IsShowRetargetBasePoseEnabled() const;

	/** Called to toggle showing of the bounds of the current preview mesh */
	void ShowBound();
	bool CanShowBound() const;
	bool IsShowBoundEnabled() const;

	/** Called to toggle showing of the current preview mesh */
	void ToggleShowPreviewMesh();
	bool CanShowPreviewMesh() const;
	bool IsShowPreviewMeshEnabled() const;

	/** Called to toggle using in-game bound on current preview mesh */
	void UseInGameBound();
	bool CanUseInGameBound() const;
	bool IsUsingInGameBound() const;

	/** Called by UV channel combo box on selection change */
	void ComboBoxSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo );

	/** Populates choices for UV Channel combo box for each lod based on current preview asset */
	void PopulateNumUVChannels();

	/** Populates choices for UV Channel combo box */
	void PopulateUVChoices();

	void AnimChanged(UAnimationAsset* AnimAsset);

	/** Called to toggle camera lock for naviagating **/
	void ToggleCameraFollow();
	bool IsCameraFollowEnabled() const;

	/** Called to determine whether the camera mode menu options should be enabled */
	bool CanChangeCameraMode() const;

	/** Tests to see if bone move mode buttons should be visibile */
	EVisibility GetBoneMoveModeButtonVisibility() const;

	/** Changes the currently selected LoD if the current one becomes invalid */
	void OnLODChanged();

	/** Function to mute/unmute viewport audio */
	void OnMuteAudio();

	/** Whether audio from the viewport is muted */
	bool IsAudioMuted();

	/** Function to set whether we are previewing root motion */
	void OnTogglePreviewRootMotion();
	
	/** Whether or not we are previewing root motion */
	bool IsPreviewingRootMotion() const;

private:
	/** Selected Turn Table speed  */
	EAnimationPlaybackSpeeds::Type SelectedTurnTableSpeed;
	/** Selected turn table mode */
	EPersonaTurnTableMode::Type SelectedTurnTableMode;

	void OnSetTurnTableSpeed(int32 SpeedIndex);
	void OnSetTurnTableMode(int32 ModeIndex);
	bool IsTurnTableModeSelected(int32 ModeIndex) const;

public:
	bool IsTurnTableSpeedSelected(int32 SpeedIndex) const;

#if WITH_APEX_CLOTHING
	/** 
	 * clothing show options 
	*/
private:
	/** disable cloth simulation */
	void OnDisableClothSimulation();
	bool IsDisablingClothSimulation() const;

	void OnApplyClothWind();

	/** Show cloth simulation normals */
	void OnShowClothSimulationNormals();
	bool IsShowingClothSimulationNormals() const;

	/** Show cloth graphical tangents */
	void OnShowClothGraphicalTangents();
	bool IsShowingClothGraphicalTangents() const;

	/** Show cloth collision volumes */
	void OnShowClothCollisionVolumes();
	bool IsShowingClothCollisionVolumes() const;

	/** Enable collision with clothes on attached children */
	void OnEnableCollisionWithAttachedClothChildren();
	bool IsEnablingCollisionWithAttachedClothChildren() const;

	/** Show cloth physical mesh wire */
	void OnShowClothPhysicalMeshWire();
	bool IsShowingClothPhysicalMeshWire() const;

	/** Show cloth max distances */
	void OnShowClothMaxDistances();
	bool IsShowingClothMaxDistances() const;

	/** Show cloth back stops */
	void OnShowClothBackstops();
	bool IsShowingClothBackstops() const;

	/** Show only fixed vertices */
	void OnShowClothFixedVertices();
	bool IsShowingClothFixedVertices() const;

	/** Show all sections which means the original state */
	void OnSetSectionsDisplayMode(int32 DisplayMode);
	bool IsSectionsDisplayMode(int32 DisplayMode) const;

#endif // #if WITH_APEX_CLOTHING

private:
	/** Pointer back to the Persona tool that owns us */
	TWeakPtr<FPersona> PersonaPtr;

	/** Skeleton */
	USkeleton* TargetSkeleton;

	/** Is this view editable */
	TAttribute<bool> IsEditable;

	/** Level viewport client */
	TSharedPtr<FEditorViewportClient> LevelViewportClient;

	/** Viewport widget*/
	TSharedPtr<class SAnimationEditorViewport> ViewportWidget;

	/** Toolbar widget */
	TSharedPtr<SHorizontalBox> ToolbarBox;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

public:
	/** UV Channel Selector */
	TSharedPtr< class STextComboBox > UVChannelCombo;
	
private:
	/** Choices for UVChannelCombo */
	TArray< TSharedPtr< FString > > UVChannels;

	/** Num UV Channels at each LOD of Preview Mesh */
	TArray<int32> NumUVChannels;

	/** Box that contains scrub panel */
	TSharedPtr<SVerticalBox> ScrubPanelContainer;

	bool bPreviewLockModeOn;

	/** Current LOD selection*/
	int32 LODSelection;

	/** Selected playback speed mode, used for deciding scale */
	EAnimationPlaybackSpeeds::Type AnimationPlaybackSpeedMode;

	/** Get Min/Max Input of value **/
	float GetViewMinInput() const;
	float GetViewMaxInput() const;

	/** Sets The EngineShowFlags.MeshEdges flag on the viewport based on current state */
	void UpdateShowFlagForMeshEdges();

	/** Update scrub panel to reflect viewed animation asset */
	void UpdateScrubPanel(UAnimationAsset* AnimAsset);
private:
	friend class FPersona;

	EVisibility GetViewportCornerImageVisibility() const;
	const FSlateBrush* GetViewportCornerImage() const;

	EVisibility GetViewportCornerTextVisibility() const;
	FText GetViewportCornerText() const;
	FText GetViewportCornerTooltip() const;
	FReply ClickedOnViewportCornerText();

};
