// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "PhATSharedData.h"
#include "EditorUndoClient.h"
#include "PhysicsEngine/BodySetupEnums.h"

class SPhATPreviewViewport;
class FPhATTreeInfo;

typedef TSharedPtr<FPhATTreeInfo> FTreeElemPtr;

enum EPhATConstraintType
{
	EPCT_BSJoint,
	EPCT_Hinge,
	EPCT_SkelJoint,
	EPCT_Prismatic
};

enum EPhATSimulationMode
{
	EPSM_Normal,
	EPSM_Gravity,
};

/*-----------------------------------------------------------------------------
   FPhAT
-----------------------------------------------------------------------------*/

class FPhAT : public IPhAT, public FGCObject, public FEditorUndoClient, public FTickableEditorObject
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	TSharedRef<SDockTab> SpawnTab( const FSpawnTabArgs& TabSpawnArgs, FName TabIdentifier );

	/** Destructor */
	virtual ~FPhAT();

	/** Edits the specified PhysicsAsset object */
	void InitPhAT(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UPhysicsAsset* ObjectToEdit);

	/** Shared data accessor */
	TSharedPtr<FPhATSharedData> GetSharedData() const;

	/** Handles a selection change... assigns the proper object to the properties widget and the hierarchy tree view */
	void SetPropertiesSelection(UObject* Obj, FPhATSharedData::FSelection * Selection = 0);

	/** Handles a group selection change... assigns the proper object to the properties widget and the hierarchy tree view */
	void SetPropertiesGroupSelection(const TArray<UObject*> & Objs);

	/** Repopulates the hierarchy tree view */
	void RefreshHierachyTree();

	/** Updates the selections in the hierarchy tree */
	void RefreshHierachyTreeSelection();

	/** Refreshes the preview viewport */
	void RefreshPreviewViewport();
	TSharedPtr< SPhATPreviewViewport > GetPreviewViewportWidget() const { return PreviewViewport; }

	/** Methods for building the various context menus */
	TSharedPtr<SWidget> BuildMenuWidgetBody(bool bHierarchy = false);
	TSharedPtr<SWidget> BuildMenuWidgetConstraint(bool bHierarchy = false);
	TSharedPtr<SWidget> BuildMenuWidgetBone();
	TSharedRef<SWidget> BuildStaticMeshAssetPicker();
	TSharedRef<SWidget> BuildHierarchyFilterMenu();
	FText GetHierarchyFilter() const;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Physics/PhAT"));
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Returns whether a PIE session is running. */
	static bool IsPIERunning();

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

	void OnFocusSelection();

	bool IsRecording() const;

private:

	enum EPhatHierarchyFilterMode
	{
		PHFM_All,
		PHFM_Bodies
	};

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();
	
	/** Builds the menu for the PhysicsAsset editor */
	void ExtendMenu();

	/** Builds the toolbar widget for the PhysicsAsset editor */
	void ExtendToolbar();
	
	/**	Binds our UI commands to delegates */
	void BindCommands();

	/** Methods related to the bone hiearchy tree view */
	TSharedRef<ITableRow> OnGenerateRowForTree(FTreeElemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildrenForTree(FTreeElemPtr Parent, TArray<FTreeElemPtr>& OutChildren);
	void OnTreeSelectionChanged(FTreeElemPtr TreeElem, ESelectInfo::Type SelectInfo);
	void OnTreeDoubleClick(FTreeElemPtr TreeElem);
	void OnTreeHighlightChanged();
	TSharedPtr<SWidget> OnTreeRightClick();
	void OnAssetSelectedFromStaticMeshAssetPicker(const FAssetData& AssetData);

	/** Call back for when bone/body properties are changed */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** SContentReference uses this to set the current animation */
	void AnimationSelectionChanged(UObject* Object);

	/** Returns the currently selected animation (if any) */
	UObject* GetSelectedAnimation() const;

	/** Helper function for SContentReference which tells it whether a particular asset is valid for the current skeleton */
	bool ShouldFilterAssetBasedOnSkeleton(const FAssetData& AssetData);

	/** Constraint editing helper methods */
	void SnapConstraintToBone(int32 ConstraintIndex, const FTransform& WParentFrame);
	void CreateOrConvertConstraint(EPhATConstraintType ConstraintType);
	
	/** Collision editing helper methods */
	void AddNewPrimitive(EKCollisionPrimitiveType PrimitiveType, bool bCopySelected = false);
	void SetBodiesBelowSelectedPhysicsType( EPhysicsType InPhysicsType, bool bMarkAsDirty);
	void SetBodiesBelowPhysicsType( EPhysicsType InPhysicsType, const TArray<int32> & Indices, bool bMarkAsDirty);

	/** Toolbar/menu command methods */
	bool IsNotSimulation() const;
	bool IsEditBodyMode() const;
	bool IsSelectedEditBodyMode() const;
	bool IsEditConstraintMode() const;
	bool IsSelectedEditConstraintMode() const;
	bool CanStartSimulation() const;
	void OnChangeDefaultMesh();
	void OnResetEntireAsset();
	void OnResetBoneCollision();
	void OnApplyPhysicalMaterial();
	void OnEditingMode(int32 Mode);
	bool IsEditingMode(int32 Mode) const;
	void OnCopyProperties();
	bool IsCopyProperties() const;
	bool CanCopyProperties() const;
	void OnPasteProperties();
	bool CanPasteProperties() const;
	bool IsSelectedEditMode() const;
	void OnInstanceProperties();
	bool IsInstanceProperties() const;
	void OnSetSimulationMode(EPhATSimulationMode Mode);
	bool IsSimulationMode(EPhATSimulationMode Mode) const;
	void OnToggleSimulation();
	void OnToggleSimulationNoGravity();
	void OnToggleSelectedSimulation();
	void OnSelectedSimulation();
	bool IsSelectedSimulation();
	bool IsToggleSimulation() const;
	void OnMeshRenderingMode(FPhATSharedData::EPhATRenderMode Mode);
	bool IsMeshRenderingMode(FPhATSharedData::EPhATRenderMode Mode) const;
	void OnCollisionRenderingMode(FPhATSharedData::EPhATRenderMode Mode);
	bool IsCollisionRenderingMode(FPhATSharedData::EPhATRenderMode Mode) const;
	void OnConstraintRenderingMode(FPhATSharedData::EPhATConstraintViewMode Mode);
	bool IsConstraintRenderingMode(FPhATSharedData::EPhATConstraintViewMode Mode) const;
	void OnShowFixedBodies();
	bool IsShowFixedBodies() const;
	void OnDrawGroundBox();
	bool IsDrawGroundBox() const;
	void OnToggleGraphicsHierarchy();
	bool IsToggleGraphicsHierarchy() const;
	void OnToggleBoneInfluences();
	bool IsToggleBoneInfluences() const;
	void OnToggleMassProperties();
	bool IsToggleMassProperties() const;
	void OnSetCollision(bool bEnable);
	bool CanSetCollision() const;
	void OnWeldToBody();
	bool CanWeldToBody();
	void OnAddNewBody();
	void OnAddSphere();
	void OnAddSphyl();
	void OnAddBox();
	bool CanAddPrimitive() const;
	void OnDeletePrimitive();
	void OnDuplicatePrimitive();
	bool CanDuplicatePrimitive() const;
	void OnResetConstraint();
	void OnSnapConstraint();
	void OnConvertToBallAndSocket();
	void OnConvertToHinge();
	void OnConvertToPrismatic();
	void OnConvertToSkeletal();
	void OnDeleteConstraint();
	void OnPlayAnimation();
	bool IsPlayAnimation() const;
	void OnShowSkeleton();
	void OnViewType(ELevelViewportType ViewType);
	bool IsShowSkeleton() const;
	void OnSetBodyPhysicsType( EPhysicsType InPhysicsType );
	bool IsBodyPhysicsType( EPhysicsType InPhysicsType );
	void OnDeleteBody();
	void OnDeleteAllBodiesBelow();
	void OnLockSelection();
	void OnDeleteSelection();
	void OnCycleConstraintOrientation();
	void OnCycleConstraintActive();
	void OnToggleSwing1();
	void OnToggleSwing2();
	void OnToggleTwist();
	
	void Mirror();

	void EditPhysicalAnimations();

	//menu commands
	void OnSelectAll();
	void SetHierarchyFilter(EPhatHierarchyFilterMode Mode);
	bool FilterTreeElement(FTreeElemPtr TreeElem) const;

	FText GetRepeatLastSimulationToolTip() const;
	FSlateIcon GetRepeatLastSimulationIcon() const;

	FText GetEditModeLabel() const;
	FText GetEditModeToolTip() const;
	FSlateIcon GetEditModeIcon() const;

private:
	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<SDockableTab> > SpawnedToolPanels;

	/** Preview Viewport */
	TSharedPtr<SPhATPreviewViewport> PreviewViewport;

	/** Ticks until forcing viewport refresh */
	int32 TickCountUntilViewportRefresh;

	/** Properties Tab */
	TSharedPtr<class IDetailsView> Properties;

	/** Physics asset properties tab */
	TSharedPtr<class IDetailsView> PhysAssetProperties;

	/** Bone Hierarchy Tree View */
	TSharedPtr< STreeView<FTreeElemPtr> > Hierarchy;
	TSharedPtr<SBorder> HierarchyControl;
	TSharedPtr<SComboButton> HierarchyFilter;
	TSharedPtr<SComboButton> PickerComboButton;

	/** Hierarchy tree items */
	TArray<FTreeElemPtr> TreeElements;
	TArray<FTreeElemPtr> RootBone;

	/** Data and methods shared across multiple classes */
	TSharedPtr<FPhATSharedData> SharedData;

	TSharedPtr<STextComboBox> PhysicalAnimationComboBox;

	/** Toolbar extender - used repeatedly as the body/constraints mode will remove/add this when changed */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Menu extender - used for commands like Select All */
	TSharedPtr<FExtender> MenuExtender;

	/** Currently selected animation */
	UAnimationAsset* SelectedAnimation;

	/** True if in OnTreeSelectionChanged()... protects against infinite recursion */
	bool InsideSelChanged;

	/** True if we want to only simulate from selected body/constraint down*/
	bool SelectedSimulation;

	/** Holds the array of strings the UI needs to show physical animation profiles for the editing physics asset*/
	TArray<TSharedPtr<FString>> PhysicalAnimationProfiles;

	/** Determines which simulation mode wer're currently in */
	EPhATSimulationMode SimulationMode;

	/** Determine which filter mode to use for the hierarchy */
	EPhatHierarchyFilterMode HierarchyFilterMode;

	/** Used to keep track of the physics type before using Selected Simulation */
	TArray<EPhysicsType> PhysicsTypeState;
	void FixPhysicsState();
	void ImpToggleSimulation();

	/** Records PhAT related data - simulating or mode change */
	void OnAddPhatRecord(const FString& Action, bool bRecordSimulate, bool bRecordMode);

private:
	void RecordAnimation();
	bool IsRecordAvailable() const;
	FSlateIcon GetRecordStatusImage() const;
	FText GetRecordStatusTooltip() const;
	FText GetRecordStatusLabel() const;
	FText GetRecordMenuLabel() const;
	FWidget::EWidgetMode BeforeSimulationWidgetMode;
};
