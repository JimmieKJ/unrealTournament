// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"
#include "PhysicsPublic.h"

class UPhATEdSkeletalMeshComponent;
class UPhysicsHandleComponent;
#define DEBUG_CLICK_VIEWPORT 0

/*-----------------------------------------------------------------------------
   FPhATSharedData
-----------------------------------------------------------------------------*/

class FPhATSharedData
{
public:
	/** Constructor/Destructor */
	FPhATSharedData();
	virtual ~FPhATSharedData();

	enum EPhATEditingMode
	{
		PEM_BodyEdit,
		PEM_ConstraintEdit
	};

	enum EPhATRenderMode
	{
		PRM_Solid,
		PRM_Wireframe,
		PRM_None
	};

	enum EPhATConstraintViewMode
	{
		PCV_None,
		PCV_AllPositions,
		PCV_AllLimits
	};

	enum EPhATConstraintType
	{
		PCT_Swing1,
		PCT_Swing2,
		PCT_Twist,
	};

	struct FSelection
	{
		int32 Index;
		EKCollisionPrimitiveType PrimitiveType;
		int32 PrimitiveIndex;
		FTransform WidgetTM;
		FTransform ManipulateTM;

		FSelection(int32 GivenBodyIndex, EKCollisionPrimitiveType GivenPrimitiveType, int32 GivenPrimitiveIndex) :
			Index(GivenBodyIndex), PrimitiveType(GivenPrimitiveType), PrimitiveIndex(GivenPrimitiveIndex),
			WidgetTM(FTransform::Identity), ManipulateTM(FTransform::Identity)
		{
		}

		bool operator==(const FSelection& rhs) const
		{
			return Index == rhs.Index && PrimitiveType == rhs.PrimitiveType && PrimitiveIndex == rhs.PrimitiveIndex;
		}
	};


	/** Initializes members */
	void Initialize();

	/** Accessors */
	EPhATRenderMode GetCurrentMeshViewMode();
	EPhATRenderMode GetCurrentCollisionViewMode();
	EPhATConstraintViewMode GetCurrentConstraintViewMode();
	
	/** Constraint editing */
	void SetSelectedConstraint(int32 ConstraintIndex, bool bGroupSelect = false);
	FTransform GetConstraintWorldTM(const FSelection* Constraint, EConstraintFrame::Type Frame) const;
	FTransform GetConstraintWorldTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame, float Scale = 1.f) const;
	FTransform GetConstraintMatrix(int32 ConstraintIndex, EConstraintFrame::Type Frame, float Scale) const;
	
	FTransform GetConstraintBodyTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame) const;

	void SetSelectedConstraintRelTM(const FTransform& RelTM);	
	
	void DeleteCurrentConstraint();
	void PasteConstraintProperties();
	
	/** Cycles the rows of the transform matrix for the selected constraint. Assumes the selected constraint
	  * is valid and that we are in constraint editing mode*/
	void CycleCurrentConstraintOrientation();

	/** Cycles the active constraint*/
	void CycleCurrentConstraintActive();

	/** Cycles the active constraint*/
	void ToggleConstraint(EPhATConstraintType Constraint);

	/** Collision geometry editing */
	void SetSelectedBody(const FSelection* Body, bool bGroupSelect = false, bool bGroupSelectRemove = true);// int32 BodyIndex, EKCollisionPrimitiveType PrimitiveType, int32 PrimitiveIndex);
	void SetSelectedBodyAnyPrim(int32 BodyIndex, bool bGroupSelect = false);
	void DeleteCurrentPrim();
	void DeleteBody(int32 DelBodyIndex, bool bRefreshComponent=true);
	void RefreshPhysicsAssetChange(const UPhysicsAsset* InPhysAsset);
	void MakeNewBody(int32 NewBoneIndex, bool bAutoSelect = true);
	void CopyBody();
	void CopyConstraint();
	void PasteBodyProperties();
	bool WeldSelectedBodies(bool bWeld = true);
	void SetSelectedConstraintsFromBodies();
	void SetSelectedBodiesFromConstraints();

	void Mirror();

	/** Misc */
	void ToggleInstanceProperties();
	void ToggleSimulation();
	void OpenNewBodyDlg();
	static void OpenNewBodyDlg(FPhysAssetCreateParams* NewBodyData, EAppReturnType::Type* NewBodyResponse);
	void HitBone(int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, bool bGroupSelect = false, bool bGroupSelectRemove = true);
	void HitConstraint(int32 ConstraintIndex, bool bGroupSelect = false);

	/** Undo/Redo */
	void PostUndo();
	void Redo();
	void SetCollisionBetweenSelected(bool bEnableCollision);


private:
	/** Initializes a constraint setup */
	void InitConstraintSetup(UPhysicsConstraintTemplate* ConstraintSetup, int32 ChildBodyIndex, int32 ParentBodyIndex);

	/** Collision editing helper methods */
	void SetCollisionBetween(int32 Body1Index, int32 Body2Index, bool bEnableCollision);
	void UpdateNoCollisionBodies();

	void EnableSimulation(bool bEnableSimulation);

	void CopyConstraintProperties(UPhysicsConstraintTemplate * FromConstraintSetup, UPhysicsConstraintTemplate * ToConstraintSetup);

public:
	/** Callback for handling selection changes */
	DECLARE_EVENT_TwoParams(FPhATSharedData, FSelectionChanged, UObject*, FSelection *);
	FSelectionChanged SelectionChangedEvent;

	DECLARE_EVENT_OneParam(FPhATSharedData, FGroupSelectionChanged, const TArray<UObject*> &);
	FGroupSelectionChanged GroupSelectionChangedEvent;

	/** Callback for handling changes to the bone/body/constraint hierarchy */
	DECLARE_EVENT(FPhATSharedData, FHierarchyChanged);
	FHierarchyChanged HierarchyChangedEvent;

	/** Callback for handling changes to the current selection in the tree */
	DECLARE_EVENT(FPhATSharedData, FHierarchySelectionChangedEvent);
	FHierarchySelectionChangedEvent HierarchySelectionChangedEvent;
	

	/** Callback for triggering a refresh of the preview viewport */
	DECLARE_EVENT(FPhATSharedData, FPreviewChanged);
	FPreviewChanged PreviewChangedEvent;

	/** The PhysicsAsset asset being inspected */
	UPhysicsAsset* PhysicsAsset;

	/** PhAT specific skeletal mesh component */
	UPhATEdSkeletalMeshComponent* EditorSkelComp;

	/** PhAT specific physical animation component */
	class UPhysicalAnimationComponent* PhysicalAnimationComponent;

	/** The skeletal mesh being used to preview the physics asset */
	USkeletalMesh* EditorSkelMesh;

	/** Floor static mesh component */
	UStaticMeshComponent* EditorFloorComp;

	/** Preview scene */
	FPreviewScene PreviewScene;

	/** Simulation options */
	UPhATSimOptions* EditorSimOptions;

	/** Results from the new body dialog */
	FPhysAssetCreateParams NewBodyData;
	EAppReturnType::Type NewBodyResponse;

	/** Helps define how the asset behaves given user interaction in simulation mode*/
	UPhysicsHandleComponent* MouseHandle;

	/** Draw color for center of mass debug strings */
	const FColor COMRenderColor;

	/** List of bodies that don't collide with the currently selected collision body */
	TArray<int32> NoCollisionBodies;

	/** Bone info */
	TArray<FBoneVertInfo> DominantWeightBoneInfos;
	TArray<FBoneVertInfo> AnyWeightBoneInfos;
	TArray<int32> ControlledBones;

	/** Collision editing */
	/*int32 SelectedBody->BodyIndex;
	EKCollisionPrimitiveType SelectedBody->PrimitiveType;
	int32 SelectedBody->PrimitiveIndex;*/

	TArray<FSelection> SelectedBodies;
	FSelection * GetSelectedBody()
	{
		int32 Count = SelectedBodies.Num();
		return Count ? &SelectedBodies[Count - 1] : NULL;
	}

	UBodySetup * CopiedBodySetup;
	UPhysicsConstraintTemplate * CopiedConstraintTemplate;

	/** Constraint editing */
	TArray<FSelection> SelectedConstraints;
	FSelection * GetSelectedConstraint()
	{
		int32 Count = SelectedConstraints.Num();
		return Count ? &SelectedConstraints[Count - 1] : NULL;
	}


	/** Show flags */
	bool bShowHierarchy;
	bool bShowCOM;
	bool bShowInfluences;
	bool bDrawGround;
	bool bShowFixedStatus;
	bool bShowAnimSkel;

	/** Misc toggles */
	bool bSelectionLock;
	bool bRunningSimulation;
	bool bNoGravitySimulation;
	bool bShowInstanceProps;

	/** Various mode flags */
	EPhATEditingMode EditingMode;
	
	/** We have a different set of view setting per editing mode */
	EPhATRenderMode BodyEdit_MeshViewMode;
	EPhATRenderMode BodyEdit_CollisionViewMode;
	EPhATConstraintViewMode BodyEdit_ConstraintViewMode;
	EPhATRenderMode ConstraintEdit_MeshViewMode;
	EPhATRenderMode ConstraintEdit_CollisionViewMode;
	EPhATConstraintViewMode ConstraintEdit_ConstraintViewMode;
	EPhATRenderMode Sim_MeshViewMode;
	EPhATRenderMode Sim_CollisionViewMode;
	EPhATConstraintViewMode Sim_ConstraintViewMode;

	/** Manipulation (rotate, translate, scale) */
	bool bManipulating;

	/** Used to prevent recursion with tree hierarchy ... needs to be rewritten! */
	bool bInsideSelChange;

	FTransform ResetTM;

#if DEBUG_CLICK_VIEWPORT
	FVector LastClickOrigin;
	FVector LastClickDirection;
#endif
	FIntPoint LastClickPos;
};
