// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhATPrivatePCH.h"
#include "PhATModule.h"
#include "PhysicsPublic.h"
#include "EditorSupportDelegates.h"
#include "ScopedTransaction.h"
#include "SPhATNewAssetDlg.h"
#include "PhATEdSkeletalMeshComponent.h"
#include "PhATSharedData.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "PhATShared"

FPhATSharedData::FPhATSharedData()
	: PreviewScene( FPreviewScene::ConstructionValues().ShouldSimulatePhysics(true) )
	, COMRenderColor(255,255,100)
	, CopiedBodySetup(NULL)
	, CopiedConstraintTemplate(NULL)
	, bInsideSelChange(false)
{
	// Editor variables
	BodyEdit_MeshViewMode = PRM_Solid;
	BodyEdit_CollisionViewMode = PRM_Wireframe;
	BodyEdit_ConstraintViewMode = PCV_AllPositions;

	ConstraintEdit_MeshViewMode = PRM_None;
	ConstraintEdit_CollisionViewMode = PRM_Wireframe;
	ConstraintEdit_ConstraintViewMode = PCV_AllPositions;

	Sim_MeshViewMode = PRM_Solid;
	Sim_CollisionViewMode = PRM_Wireframe;
	Sim_ConstraintViewMode = PCV_None;

	EditingMode = PEM_BodyEdit;

	bShowCOM = false;
	bShowHierarchy = false;
	bShowInfluences = false;
	bDrawGround = true;
	bShowFixedStatus = true;
	bShowAnimSkel = false;

	bSelectionLock = false;
	bRunningSimulation = false;
	bNoGravitySimulation = false;
	bShowInstanceProps = false;

	bManipulating = false;
	
	// Construct mouse handle
	MouseHandle = NewObject<UPhysicsHandleComponent>();

	// Construct sim options.
	EditorSimOptions = NewObject<UPhATSimOptions>(GetTransientPackage(), TEXT("EditorSimOptions"));
	check(EditorSimOptions);

	EditorSimOptions->HandleLinearDamping = MouseHandle->LinearDamping;
	EditorSimOptions->HandleLinearStiffness = MouseHandle->LinearStiffness;
	EditorSimOptions->HandleAngularDamping = MouseHandle->AngularDamping;
	EditorSimOptions->HandleAngularStiffness = MouseHandle->AngularStiffness;
	EditorSimOptions->InterpolationSpeed = MouseHandle->InterpolationSpeed;
}

FPhATSharedData::~FPhATSharedData()
{

}

void FPhATSharedData::Initialize()
{
	EditorSkelComp = NULL;
	PhysicalAnimationComponent = nullptr;

	USkeletalMesh * PreviewMesh =  NULL; 
	FStringAssetReference PreviewMeshStringRef = PhysicsAsset->PreviewSkeletalMesh.ToStringReference();
	// load it since now is the time to load
	if (!PreviewMeshStringRef.ToString().IsEmpty())
	{
		PreviewMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), NULL, *PreviewMeshStringRef.ToString(), NULL, LOAD_None, NULL));
	}

	if ( PreviewMesh == NULL)
	{
		// Fall back to the default skeletal mesh in the EngineMeshes package.
		// This is statically loaded as the package is likely not fully loaded
		// (otherwise, it would have been found in the above iteration).
		PreviewMesh = (USkeletalMesh*)StaticLoadObject(
			USkeletalMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), NULL, LOAD_None, NULL);
		check(PreviewMesh);

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				NSLOCTEXT("UnrealEd", "Error_PhysicsAssetHasNoSkelMesh",
				"Warning: Physics Asset has no skeletal mesh assigned!  For now, a simple default skeletal mesh ({0}) will be used.  You can fix this by opening PhAT, selecting the appropriate skeletal mesh in the content browser, and using (Asset -> Change Mesh) before saving this asset."),
				FText::FromString(PreviewMesh->GetFullName())));
	}

	EditorSkelMesh = PreviewMesh;

	// Create SkeletalMeshComponent for rendering skeletal mesh
	EditorSkelComp = NewObject<UPhATEdSkeletalMeshComponent>();
	EditorSkelComp->SharedData = this;

	PhysicalAnimationComponent = NewObject<UPhysicalAnimationComponent>();
	PhysicalAnimationComponent->SetSkeletalMeshComponent(EditorSkelComp);
	
	// first disable collision first to avoid creating physics body
	EditorSkelComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	EditorSkelComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);

	// Create floor component
	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PhAT_FloorBox.PhAT_FloorBox"), NULL, LOAD_None, NULL);
	check(FloorMesh);

	EditorFloorComp = NewObject<UStaticMeshComponent>();
	EditorFloorComp->StaticMesh = FloorMesh;
	EditorFloorComp->SetRelativeScale3D(FVector(4.f));

	PreviewScene.AddComponent(EditorSkelComp, FTransform::Identity);
	PreviewScene.AddComponent(EditorFloorComp, FTransform::Identity);
	PreviewScene.AddComponent(PhysicalAnimationComponent, FTransform::Identity);

	// Look for body setups with no shapes (how does this happen?).
	// If we find one- just bang on a default box.
	bool bFoundEmptyShape = false;
	for (int32 i = 0; i <PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[i];
		if (BodySetup->AggGeom.GetElementCount() == 0)
		{
			FKBoxElem BoxElem;
			BoxElem.SetTransform(FTransform::Identity);
			BoxElem.X = 15.f;
			BoxElem.Y = 15.f;
			BoxElem.Z = 15.f;
			BodySetup->AggGeom.BoxElems.Add(BoxElem);
			check(BodySetup->AggGeom.BoxElems.Num() == 1);

			bFoundEmptyShape = true;
		}
	}

	// Pop up a warning about what we did.
	if (bFoundEmptyShape)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "EmptyBodyFound", "Bodies was found with no primitives!\nThey have been reset to have a box."));
	}

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	// Used for viewing bone influences, resetting bone geometry etc.
	MeshUtilities.CalcBoneVertInfos(EditorSkelMesh, DominantWeightBoneInfos, true);
	MeshUtilities.CalcBoneVertInfos(EditorSkelMesh, AnyWeightBoneInfos, false);

	EditorSkelComp->SetSkeletalMesh(EditorSkelMesh);
	EditorSkelComp->SetPhysicsAsset(PhysicsAsset);

	// Ensure PhysicsAsset mass properties are up to date.
	PhysicsAsset->UpdateBoundsBodiesArray();

	// Check if there are any bodies in the Asset which do not have bones in the skeletal mesh.
	// If so, put up a warning.
	TArray<int32> MissingBodyIndices;
	FString BoneNames;
	for (int32 i = 0; i <PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		FName BoneName = PhysicsAsset->SkeletalBodySetups[i]->BoneName;
		int32 BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			MissingBodyIndices.Add( i );
			BoneNames += FString::Printf(TEXT("\t%s\n"), *BoneName.ToString());
		}
	}

	const FText MissingBodyMsg = FText::Format( LOCTEXT( "MissingBones", "The following Bodies are in the PhysicsAsset, but have no corresponding bones in the SkeletalMesh.\nClick OK to delete them, or Cancel to ignore.\n\n{0}" ), FText::FromString( BoneNames ) );

	if ( MissingBodyIndices.Num() )
	{
		if ( FMessageDialog::Open( EAppMsgType::OkCancel, MissingBodyMsg ) == EAppReturnType::Ok )
		{
			// Delete the bodies with no associated bones

			const FScopedTransaction Transaction( LOCTEXT( "DeleteUnusedPhysicsBodies", "Delete Physics Bodies With No Bones" ) );
			PhysicsAsset->SetFlags(RF_Transactional);
			PhysicsAsset->Modify();

			// Iterate backwards, as PhysicsAsset->SkeletalBodySetups is a TArray and UE4 containers don't support remove_if()
			for ( int32 i = MissingBodyIndices.Num() - 1; i >= 0; --i )
			{
				DeleteBody( MissingBodyIndices[i] );
			}
		}
	}

	// Register handle component
	MouseHandle->RegisterComponentWithWorld(PreviewScene.GetWorld());

	// Support undo/redo
	PhysicsAsset->SetFlags(RF_Transactional);

	EditorSkelComp->Stop();

	SetSelectedBody(NULL);
	SetSelectedConstraint(INDEX_NONE);

	ResetTM = EditorSkelComp->GetComponentToWorld(); 

	EnableSimulation(false);
}

void FPhATSharedData::CopyConstraintProperties(UPhysicsConstraintTemplate * FromConstraintSetup, UPhysicsConstraintTemplate * ToConstraintSetup)
{
	ToConstraintSetup->Modify();
	FConstraintInstance OldInstance = ToConstraintSetup->DefaultInstance;
	ToConstraintSetup->DefaultInstance.CopyConstraintParamsFrom(&FromConstraintSetup->DefaultInstance);

	// recover certain data that we'd like to keep - i.e. bone indices those still should stay.  
	// frame position offsets taken from old, but frame orientations are taken from new source
	ToConstraintSetup->DefaultInstance.ConstraintIndex = OldInstance.ConstraintIndex;
	ToConstraintSetup->DefaultInstance.ConstraintData = OldInstance.ConstraintData;
	ToConstraintSetup->DefaultInstance.JointName = OldInstance.JointName;
	ToConstraintSetup->DefaultInstance.ConstraintBone1 = OldInstance.ConstraintBone1;
	ToConstraintSetup->DefaultInstance.ConstraintBone2 = OldInstance.ConstraintBone2;
	ToConstraintSetup->DefaultInstance.Pos1 = OldInstance.Pos1;
	ToConstraintSetup->DefaultInstance.Pos2 = OldInstance.Pos2;
}

struct FMirrorInfo
{
	FName BoneName;
	int32 BoneIndex;
	int32 BodyIndex;
	int32 ConstraintIndex;
	FMirrorInfo()
	{
		BoneIndex = INDEX_NONE;
		BodyIndex = INDEX_NONE;
		ConstraintIndex = INDEX_NONE;
		BoneName = NAME_None;
	}
};

void FPhATSharedData::Mirror()
{
	TArray<FMirrorInfo> MirrorInfos;
	if (EditingMode == PEM_BodyEdit)	//grab all selected bodies
	{
		for (const FSelection& Selection : SelectedBodies)
		{
			MirrorInfos.AddUninitialized();
			FMirrorInfo & MirrorInfo = MirrorInfos[MirrorInfos.Num() - 1];
			MirrorInfo.BoneName = PhysicsAsset->SkeletalBodySetups[Selection.Index]->BoneName;
			MirrorInfo.BodyIndex = Selection.Index;
			MirrorInfo.ConstraintIndex = PhysicsAsset->FindConstraintIndex(MirrorInfo.BoneName);
		}
	}
	else if (EditingMode == PEM_ConstraintEdit)	//grab all selected constraints
	{
		for (const FSelection& Selection : SelectedConstraints)
		{
			MirrorInfos.AddUninitialized();
			FMirrorInfo & MirrorInfo = MirrorInfos[MirrorInfos.Num() - 1];
			MirrorInfo.BoneName = PhysicsAsset->ConstraintSetup[Selection.Index]->DefaultInstance.ConstraintBone1;
			MirrorInfo.BodyIndex = PhysicsAsset->FindBodyIndex(MirrorInfo.BoneName);
			MirrorInfo.ConstraintIndex = Selection.Index;
		}
	}

	for (FMirrorInfo & MirrorInfo : MirrorInfos)	//mirror all selected bodies/constraints
	{
		int32 BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(MirrorInfo.BoneName);

		int32 MirrorBoneIndex = PhysicsAsset->FindMirroredBone(EditorSkelMesh, BoneIndex);
		if (MirrorBoneIndex != INDEX_NONE)
		{
			UBodySetup * SrcBody = PhysicsAsset->SkeletalBodySetups[MirrorInfo.BodyIndex];
			const FScopedTransaction Transaction(NSLOCTEXT("PhAT", "MirrorBody", "MirrorBody"));
			MakeNewBody(MirrorBoneIndex, false);

			int32 MirrorBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, MirrorBoneIndex);

			UBodySetup * DestBody = PhysicsAsset->SkeletalBodySetups[MirrorBodyIndex];
			DestBody->Modify();
			DestBody->CopyBodyPropertiesFrom(SrcBody);
			FQuat ArtistMirrorConvention(0,0,1,0);   // how Epic Maya artists rig the right and left orientation differently.  todo: perhaps move to cvar 
			for (FKSphylElem& Sphyl : DestBody->AggGeom.SphylElems)
			{
				Sphyl.Orientation = ArtistMirrorConvention*Sphyl.Orientation;
				Sphyl.Center = ArtistMirrorConvention.RotateVector(Sphyl.Center);
			}
			for (FKBoxElem& Box : DestBody->AggGeom.BoxElems)
			{
				Box.Orientation = ArtistMirrorConvention*Box.Orientation;
				Box.Center      = ArtistMirrorConvention.RotateVector(Box.Center);
			}
			for (FKSphereElem& Sphere : DestBody->AggGeom.SphereElems)
			{
				Sphere.Center = ArtistMirrorConvention.RotateVector(Sphere.Center);
			}
			int32 MirrorConstraintIndex = PhysicsAsset->FindConstraintIndex(DestBody->BoneName);
			UPhysicsConstraintTemplate * FromConstraint = PhysicsAsset->ConstraintSetup[MirrorInfo.ConstraintIndex];
			UPhysicsConstraintTemplate * ToConstraint = PhysicsAsset->ConstraintSetup[MirrorConstraintIndex];
			CopyConstraintProperties(FromConstraint, ToConstraint);
		}
	}
}

FPhATSharedData::EPhATRenderMode FPhATSharedData::GetCurrentMeshViewMode()
{
	if (bRunningSimulation)
	{
		return Sim_MeshViewMode;
	}
	else if (EditingMode == PEM_BodyEdit)
	{
		return BodyEdit_MeshViewMode;
	}
	else
	{
		return ConstraintEdit_MeshViewMode;
	}
}

FPhATSharedData::EPhATRenderMode FPhATSharedData::GetCurrentCollisionViewMode()
{
	if (bRunningSimulation)
	{
		return Sim_CollisionViewMode;
	}
	else if (EditingMode == PEM_BodyEdit)
	{
		return BodyEdit_CollisionViewMode;
	}
	else
	{
		return ConstraintEdit_CollisionViewMode;
	}
}

FPhATSharedData::EPhATConstraintViewMode FPhATSharedData::GetCurrentConstraintViewMode()
{
	if (bRunningSimulation)
	{
		return Sim_ConstraintViewMode;
	}
	else if (EditingMode == PEM_BodyEdit)
	{
		return BodyEdit_ConstraintViewMode;
	}
	else
	{
		return ConstraintEdit_ConstraintViewMode;
	}
}

void FPhATSharedData::HitBone(int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, bool bGroupSelect /* = false*/, bool bGroupSelectRemove /* = true */)
{
	if (EditingMode == FPhATSharedData::PEM_BodyEdit && !bSelectionLock && !bRunningSimulation)
	{
		FPhATSharedData::FSelection Selection(BodyIndex, PrimType, PrimIndex);
		SetSelectedBody( &Selection, bGroupSelect, bGroupSelectRemove);
	}
}

void FPhATSharedData::HitConstraint(int32 ConstraintIndex, bool bGroupSelect)
{
	if (EditingMode == FPhATSharedData::PEM_ConstraintEdit && !bSelectionLock && !bRunningSimulation)
	{
		SetSelectedConstraint(ConstraintIndex, bGroupSelect);
	}
}

void FPhATSharedData::RefreshPhysicsAssetChange(const UPhysicsAsset* InPhysAsset)
{
	if (InPhysAsset)
	{
		for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
			if  (SkeletalMeshComponent->GetPhysicsAsset() == InPhysAsset)
			{
				// it needs to recreate IF it already has been created
				if (SkeletalMeshComponent->IsPhysicsStateCreated())
				{
					SkeletalMeshComponent->RecreatePhysicsState();
				}
			}
		}

		for (FObjectIterator Iter(UWheeledVehicleMovementComponent::StaticClass()); Iter; ++Iter)
		{
			UWheeledVehicleMovementComponent * WheeledVehicleMovementComponent = Cast<UWheeledVehicleMovementComponent>(*Iter);
			if (USkeletalMeshComponent * SkeltalMeshComponent = Cast<USkeletalMeshComponent>(WheeledVehicleMovementComponent->UpdatedComponent))
			{
				if (SkeltalMeshComponent->GetPhysicsAsset() == InPhysAsset)
				{
					//Need to recreate car data
					WheeledVehicleMovementComponent->RecreatePhysicsState();
				}

			}
		}

		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		// since we recreate physicsstate, a lot of transient state data will be gone
		// so have to turn simulation off again. 
		// ideally maybe in the future, we'll fix it by controlling tick?
		EnableSimulation(false);
	}
}

void FPhATSharedData::SetSelectedBodyAnyPrim(int32 BodyIndex, bool bGroupSelect /* = false */)
{
	if (BodyIndex == INDEX_NONE)
	{
		SetSelectedBody(NULL);
		return;
	}
	
	UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
	check(BodySetup);

	if (BodySetup->AggGeom.SphereElems.Num() > 0)
	{
		FSelection Selection(BodyIndex, KPT_Sphere, 0);
		SetSelectedBody(&Selection, bGroupSelect);
	}
	else if (BodySetup->AggGeom.BoxElems.Num() > 0)
	{
		FSelection Selection(BodyIndex, KPT_Box, 0);
		SetSelectedBody(&Selection, bGroupSelect);
	}
	else if (BodySetup->AggGeom.SphylElems.Num() > 0)
	{
		FSelection Selection(BodyIndex, KPT_Sphyl, 0);
		SetSelectedBody(&Selection, bGroupSelect);
	}
	else if (BodySetup->AggGeom.ConvexElems.Num() > 0)
	{
		FSelection Selection(BodyIndex, KPT_Convex, 0);
		SetSelectedBody(&Selection, bGroupSelect);
	}
	else
	{
		UE_LOG(LogPhAT, Fatal, TEXT("Body Setup with No Primitives!")); 
	}
}

void FPhATSharedData::SetSelectedBody(const FSelection* Body, bool bGroupSelect /*= false*/, bool bGroupSelectRemove /* = true */)
{
	if(bInsideSelChange)
	{
		return;
	}

	if(bGroupSelect == false)
	{
		SelectedBodies.Empty();
	}
	
	if(Body)
	{
		bool bAlreadySelected = false;
		//unselect if already selected
		for(int32 i=0; i<SelectedBodies.Num(); ++i)
		{
			if(SelectedBodies[i] == *Body)
			{
				if(bGroupSelectRemove)
				{
					SelectedBodies.RemoveAt(i);
				}
				bAlreadySelected = true;
				break;
			}
		}

		if(bAlreadySelected == false)
		{
			SelectedBodies.AddUnique(*Body);
		}
	}

	if (SelectedBodies.Num() == 0)
	{
		// No bone selected
		SelectionChangedEvent.Broadcast(EditorSimOptions, NULL);
	}
	else
	{

		check(GetSelectedBody() && GetSelectedBody()->Index >= 0 && GetSelectedBody()->Index < PhysicsAsset->SkeletalBodySetups.Num());

		// Set properties dialog to display selected bone (or bone instance) info.
		TArray<UObject*> Objs;
		for(int i=0; i<SelectedBodies.Num(); ++i)
		{
			Objs.Add(PhysicsAsset->SkeletalBodySetups[SelectedBodies[i].Index]);
		}
		
		GroupSelectionChangedEvent.Broadcast(Objs);
	}

	//bInsideSelChange = true;
	//HierarchySelectionChangedEvent.Broadcast();	//TODO: disable for now
	bInsideSelChange = false;

	ControlledBones.Empty();
	if(!GetSelectedBody())
	{
		return;
	}

	for (int32 i = 0; i <EditorSkelMesh->RefSkeleton.GetNum(); ++i)
	{
		int32 ControllerBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, i);
		if (ControllerBodyIndex == GetSelectedBody()->Index)
		{
			ControlledBones.Add(i);
		}
	}
	

	UpdateNoCollisionBodies();
	PreviewChangedEvent.Broadcast();
}

void FPhATSharedData::SetSelectedBodiesFromConstraints()
{
	if(SelectedConstraints.Num() == 0)
	{
		return;
	}

	SetSelectedBody(nullptr, false);
	for (const FSelection& Selection : SelectedConstraints)
	{
		UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[Selection.Index];
		FConstraintInstance & DefaultInstance = ConstraintTemplate->DefaultInstance;

		for (int32 BodyIdx = 0; BodyIdx < PhysicsAsset->SkeletalBodySetups.Num(); ++BodyIdx)
		{
			UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIdx];
			if (DefaultInstance.JointName == BodySetup->BoneName && BodySetup->AggGeom.GetElementCount() > 0)
			{
				FSelection NewSelection(BodyIdx, KPT_Unknown, 0);
				int32 PrimIndex = 0;
				for(int32 GeomType = 0; GeomType < KPT_Unknown; ++GeomType)
				{
					if(BodySetup->AggGeom.GetElementCount(GeomType) > 0)
					{
						NewSelection.PrimitiveType = (EKCollisionPrimitiveType)GeomType;
						break;
					}
				}
				SetSelectedBody(&NewSelection, true);
			}
		}
	}
}

void FPhATSharedData::SetSelectedConstraintsFromBodies()
{
	if (SelectedBodies.Num() == 0)
	{
		return;
	}

	TSet<int32> TmpSelectedConstraints;	//We could have multiple shapes selected which would cause us to add and remove the same constraint.
	SetSelectedConstraint(INDEX_NONE, false);
	for (const FSelection& Selection : SelectedBodies)
	{
		UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[Selection.Index];
		for(int32 ConstraintIdx = 0; ConstraintIdx < PhysicsAsset->ConstraintSetup.Num(); ++ConstraintIdx)
		{
			const UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIdx]; 
			if(ConstraintTemplate->DefaultInstance.JointName == BodySetup->BoneName && !TmpSelectedConstraints.Contains(ConstraintIdx))
			{
				TmpSelectedConstraints.Add(ConstraintIdx);
				SetSelectedConstraint(ConstraintIdx, true);
			}
		}
	}
}


void FPhATSharedData::UpdateNoCollisionBodies()
{
	NoCollisionBodies.Empty();

	// Query disable table with selected body and every other body.
	for (int32 i = 0; i <PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		// Add any bodies with bNoCollision
		if (PhysicsAsset->SkeletalBodySetups[i]->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			NoCollisionBodies.Add(i);
		}
		else if (GetSelectedBody() && i != GetSelectedBody()->Index)
		{
			// Add this body if it has disabled collision with selected.
			FRigidBodyIndexPair Key(i, GetSelectedBody()->Index);

			if (PhysicsAsset->SkeletalBodySetups[GetSelectedBody()->Index]->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
				PhysicsAsset->CollisionDisableTable.Find(Key))
			{
				NoCollisionBodies.Add(i);
			}
		}
	}
}

void FPhATSharedData::SetSelectedConstraint(int32 ConstraintIndex, bool bGroupSelect /*= false*/)
{
	if(bGroupSelect == false)
	{
		SelectedConstraints.Empty();
	}
	
	if(ConstraintIndex != INDEX_NONE)
	{
		bool bAlreadySelected = false;
		for(int32 i=0; i<SelectedConstraints.Num(); ++i)
		{
			if(SelectedConstraints[i].Index == ConstraintIndex)
			{
				bAlreadySelected = true;
				SelectedConstraints.RemoveAt(i);
				break;
			}
		}

		if(bAlreadySelected == false)
		{
			FSelection Constraint(ConstraintIndex, KPT_Unknown, INDEX_NONE);
			SelectedConstraints.AddUnique(Constraint);
		}
	}

	if (!GetSelectedConstraint())
	{
		SelectionChangedEvent.Broadcast(EditorSimOptions, NULL);
	}
	else
	{
		check(GetSelectedConstraint()->Index >= 0 && GetSelectedConstraint()->Index < PhysicsAsset->ConstraintSetup.Num());

		TArray<UObject*> Objs;
		for(int i=0; i<SelectedConstraints.Num(); ++i)
		{
			Objs.Add(PhysicsAsset->ConstraintSetup[SelectedConstraints[i].Index]);
		}

		GroupSelectionChangedEvent.Broadcast(Objs);
	}	

	PreviewChangedEvent.Broadcast();
}

void FPhATSharedData::SetCollisionBetweenSelected(bool bEnableCollision)
{
	if (bRunningSimulation || SelectedBodies.Num() == 0)
	{
		return;
	}

	PhysicsAsset->Modify();

	for(int32 i=0; i<SelectedBodies.Num(); ++i)
	{
		for(int32 j=i+1; j<SelectedBodies.Num(); ++j)
		{
			if(bEnableCollision)
			{
				PhysicsAsset->EnableCollision(SelectedBodies[i].Index, SelectedBodies[j].Index);
			}else
			{
				PhysicsAsset->DisableCollision(SelectedBodies[i].Index, SelectedBodies[j].Index);
			}

		}
	}


	UpdateNoCollisionBodies();

	PreviewChangedEvent.Broadcast();
}

void FPhATSharedData::SetCollisionBetween(int32 Body1Index, int32 Body2Index, bool bEnableCollision)
{
	if (bRunningSimulation)
	{
		return;
	}

	PhysicsAsset->Modify();

	if (Body1Index != INDEX_NONE && Body2Index != INDEX_NONE && Body1Index != Body2Index)
	{
		if (bEnableCollision)
		{
			PhysicsAsset->EnableCollision(Body1Index, Body2Index);
		}
		else
		{
			PhysicsAsset->DisableCollision(Body1Index, Body2Index);
		}

		UpdateNoCollisionBodies();
	}

	PreviewChangedEvent.Broadcast();
}

void FPhATSharedData::CopyBody()
{
	check(SelectedBodies.Num() == 1);

	CopiedBodySetup = PhysicsAsset->SkeletalBodySetups[GetSelectedBody()->Index];
}

void FPhATSharedData::PasteBodyProperties()
{
	// Can't do this while simulating!
	if (bRunningSimulation)
	{
		return;
	}

	// Must have two valid bodies (which are different)
	if(CopiedBodySetup == NULL)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "PasteBodyProperties", "Paste Body Properties") );

	for(int32 i=0; i<SelectedBodies.Num(); ++i)
	{
		// Copy setup/instance properties - based on what we are viewing.
		if (!bShowInstanceProps)
		{
			UBodySetup* ToBodySetup = PhysicsAsset->SkeletalBodySetups[SelectedBodies[i].Index];
			UBodySetup* FromBodySetup = CopiedBodySetup;
			ToBodySetup->Modify();
			ToBodySetup->CopyBodyPropertiesFrom(FromBodySetup);
		}
		else
		{
			FBodyInstance* ToBodyInstance = &PhysicsAsset->SkeletalBodySetups[SelectedBodies[i].Index]->DefaultInstance;
			FBodyInstance* FromBodyInstance = &CopiedBodySetup->DefaultInstance;
			ToBodyInstance->CopyBodyInstancePropertiesFrom(FromBodyInstance);
		}

	}
	
	SetSelectedBody(NULL);	//paste can change the primitives on our selected bodies. There's probably a way to properly update this, but for now just deselect
	PreviewChangedEvent.Broadcast();
}

bool FPhATSharedData::WeldSelectedBodies(bool bWeld /* = true */)
{
	bool bCanWeld = false;
	if (bRunningSimulation)
	{
		return false;
	}

	if(SelectedBodies.Num() <= 1)
	{
		return false;
	}

	//we only support two body weld
	int BodyIndex0 = 0;
	int BodyIndex1 = INDEX_NONE;

	for(int32 i=1; i<SelectedBodies.Num(); ++i)
	{
		if(SelectedBodies[BodyIndex0].Index == SelectedBodies[i].Index)
		{
			continue;
		}

		if(BodyIndex1== INDEX_NONE)
		{
			BodyIndex1 = i;
		}else
		{
			if(SelectedBodies[BodyIndex1].Index != SelectedBodies[i].Index)
			{
				return false;
			}
		}
	}

	//need to weld bodies not primitives
	if(BodyIndex1 == INDEX_NONE)
	{
		return false;
	}

	const FSelection& Body0 = SelectedBodies[BodyIndex0];
	const FSelection& Body1 = SelectedBodies[BodyIndex1];

	FName Bone0Name = PhysicsAsset->SkeletalBodySetups[Body0.Index]->BoneName;
	int32 Bone0Index = EditorSkelMesh->RefSkeleton.FindBoneIndex(Bone0Name);
	check(Bone0Index != INDEX_NONE);

	FName Bone1Name = PhysicsAsset->SkeletalBodySetups[Body1.Index]->BoneName;
	int32 Bone1Index = EditorSkelMesh->RefSkeleton.FindBoneIndex(Bone1Name);
	check(Bone1Index != INDEX_NONE);

	int32 Bone0ParentIndex = EditorSkelMesh->RefSkeleton.GetParentIndex(Bone0Index);
	int32 Bone1ParentIndex = EditorSkelMesh->RefSkeleton.GetParentIndex(Bone1Index);

	int ParentBodyIndex = INDEX_NONE;
	int ChildBodyIndex = INDEX_NONE;
	FName ParentBoneName;
	EKCollisionPrimitiveType ParentPrimitiveType = KPT_Unknown;
	EKCollisionPrimitiveType ChildPrimitiveType = KPT_Unknown;
	int32 ParentPrimitiveIndex = INDEX_NONE;
	int32 ChildPrimitiveIndex = INDEX_NONE;

	if (PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, Bone1ParentIndex) == Body0.Index)
	{
		ParentBodyIndex = Body0.Index;
		ParentBoneName = Bone0Name;
		ChildBodyIndex = Body1.Index;
		ParentPrimitiveType = Body0.PrimitiveType;
		ChildPrimitiveType = Body1.PrimitiveType;
		ParentPrimitiveIndex = Body0.PrimitiveIndex;
		//Child geoms get appended so just add it. This is kind of a hack but this whole indexing scheme needs to be rewritten anyway
		ChildPrimitiveIndex = Body1.PrimitiveIndex + PhysicsAsset->SkeletalBodySetups[Body0.Index]->AggGeom.GetElementCount(ChildPrimitiveType);

		bCanWeld = true;
	}else if(PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, Bone0ParentIndex) == Body1.Index)
	{
		ParentBodyIndex = Body1.Index;
		ParentBoneName = Bone1Name;
		ChildBodyIndex = Body0.Index;
		ParentPrimitiveType = Body1.PrimitiveType;
		ChildPrimitiveType = Body0.PrimitiveType;
		ParentPrimitiveIndex = Body1.PrimitiveIndex;
		//Child geoms get appended so just add it. This is kind of a hack but this whole indexing scheme needs to be rewritten anyway
		ChildPrimitiveIndex = Body0.PrimitiveIndex + PhysicsAsset->SkeletalBodySetups[Body1.Index]->AggGeom.GetElementCount(ChildPrimitiveType);

		bCanWeld = true;
	}

	//function is used for the action and the check
	if(bWeld == false)
	{
		return bCanWeld;
	}

	check(ParentBodyIndex != INDEX_NONE);
	check(ChildBodyIndex != INDEX_NONE);

	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "WeldBodies", "Weld Bodies") );

		// .. the asset itself..
		PhysicsAsset->Modify();

		// .. the parent and child bodies..
		PhysicsAsset->SkeletalBodySetups[ParentBodyIndex]->Modify();
		PhysicsAsset->SkeletalBodySetups[ChildBodyIndex]->Modify();

		// .. and any constraints of the 'child' body..
		TArray<int32>	Constraints;
		PhysicsAsset->BodyFindConstraints(ChildBodyIndex, Constraints);

		for (int32 i = 0; i <Constraints.Num(); ++i)
		{
			int32 ConstraintIndex = Constraints[i];
			PhysicsAsset->ConstraintSetup[ConstraintIndex]->Modify();
		}

		// Do the actual welding
		FPhysicsAssetUtils::WeldBodies(PhysicsAsset, ParentBodyIndex, ChildBodyIndex, EditorSkelComp);
	}

	// update the tree
	HierarchyChangedEvent.Broadcast();

	// Body index may have changed, so we re-find it.
	int32 BodyIndex = PhysicsAsset->FindBodyIndex(ParentBoneName);
	FSelection SelectionParent(BodyIndex, ParentPrimitiveType, ParentPrimitiveIndex);
	SetSelectedBody(&SelectionParent); // This redraws the viewport as well...

	FSelection SelectionChild(BodyIndex, ChildPrimitiveType, ChildPrimitiveIndex);
	SetSelectedBody(&SelectionChild, true); // This redraws the viewport as well...

	// Just to be safe - deselect any selected constraints
	SetSelectedConstraint(INDEX_NONE);
	RefreshPhysicsAssetChange(PhysicsAsset);
	return true;
}


void FPhATSharedData::InitConstraintSetup(UPhysicsConstraintTemplate* ConstraintSetup, int32 ChildBodyIndex, int32 ParentBodyIndex)
{
	check(ConstraintSetup);

	ConstraintSetup->Modify(false);

	UBodySetup* ChildBodySetup = PhysicsAsset->SkeletalBodySetups[ ChildBodyIndex ];
	UBodySetup* ParentBodySetup = PhysicsAsset->SkeletalBodySetups[ ParentBodyIndex ];
	check(ChildBodySetup && ParentBodySetup);

	const int32 ChildBoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ChildBodySetup->BoneName);
	const int32 ParentBoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ParentBodySetup->BoneName);
	check(ChildBoneIndex != INDEX_NONE && ParentBoneIndex != INDEX_NONE);

	// Transform of child from parent is just child ref-pose entry.
	FMatrix ChildBoneTM = EditorSkelComp->GetBoneMatrix(ChildBoneIndex);
	ChildBoneTM.RemoveScaling();

	FMatrix ParentBoneTM = EditorSkelComp->GetBoneMatrix(ParentBoneIndex);
	ParentBoneTM.RemoveScaling();

	FMatrix RelTM = ChildBoneTM * ParentBoneTM.Inverse();

	// Place joint at origin of child
	ConstraintSetup->DefaultInstance.ConstraintBone1 = ChildBodySetup->BoneName;
	ConstraintSetup->DefaultInstance.Pos1 = FVector::ZeroVector;
	ConstraintSetup->DefaultInstance.PriAxis1 = FVector(1.f, 0.f, 0.f);
	ConstraintSetup->DefaultInstance.SecAxis1 = FVector(0.f, 1.f, 0.f);

	ConstraintSetup->DefaultInstance.ConstraintBone2 = ParentBodySetup->BoneName;
	ConstraintSetup->DefaultInstance.Pos2 = RelTM.GetOrigin();
	ConstraintSetup->DefaultInstance.PriAxis2 = RelTM.GetUnitAxis( EAxis::X );
	ConstraintSetup->DefaultInstance.SecAxis2 = RelTM.GetUnitAxis( EAxis::Y );

	// Disable collision between constrained bodies by default.
	SetCollisionBetween(ChildBodyIndex, ParentBodyIndex, false);
}

void FPhATSharedData::MakeNewBody(int32 NewBoneIndex, bool bAutoSelect)
{
	FName NewBoneName = EditorSkelMesh->RefSkeleton.GetBoneName(NewBoneIndex);

	// If this body is already physical - do nothing.
	int32 NewBodyIndex = PhysicsAsset->FindBodyIndex(NewBoneName);
	if (NewBodyIndex != INDEX_NONE)
	{
		return;
	}

	// Find body that currently controls this bone.
	int32 ParentBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, NewBoneIndex);

	PhysicsAsset->Modify();

	// Create the physics body.
	NewBodyIndex = FPhysicsAssetUtils::CreateNewBody(PhysicsAsset, NewBoneName);
	UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[ NewBodyIndex ];
	check(BodySetup->BoneName == NewBoneName);
	
	BodySetup->Modify();

	bool bCreatedBody = false;
	// Create a new physics body for this bone.
	if (NewBodyData.VertWeight == EVW_DominantWeight)
	{
		bCreatedBody = FPhysicsAssetUtils::CreateCollisionFromBone(BodySetup, EditorSkelMesh, NewBoneIndex, NewBodyData, DominantWeightBoneInfos);
	}
	else
	{
		bCreatedBody = FPhysicsAssetUtils::CreateCollisionFromBone(BodySetup, EditorSkelMesh, NewBoneIndex, NewBodyData, AnyWeightBoneInfos);
	}

	if (bCreatedBody == false)
	{
		FPhysicsAssetUtils::DestroyBody(PhysicsAsset, NewBodyIndex);
		return;
	}

	// Check if the bone of the new body has any physical children bones
	for (int32 i = 0; i < EditorSkelMesh->RefSkeleton.GetNum(); ++i)
	{
		if (EditorSkelMesh->RefSkeleton.BoneIsChildOf(i, NewBoneIndex))
		{
			const int32 ChildBodyIndex = PhysicsAsset->FindBodyIndex(EditorSkelMesh->RefSkeleton.GetBoneName(i));
			
			// If the child bone is physical, it may require fixing up in regards to constraints
			if (ChildBodyIndex != INDEX_NONE)
			{
				UBodySetup* ChildBody = PhysicsAsset->SkeletalBodySetups[ ChildBodyIndex ];
				check(ChildBody);

				int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBody->BoneName);
				
				// If the child body is not constrained already, create a new constraint between
				// the child body and the new body
				if (ConstraintIndex == INDEX_NONE)
				{
					ConstraintIndex = FPhysicsAssetUtils::CreateNewConstraint(PhysicsAsset, ChildBody->BoneName);
					check(ConstraintIndex != INDEX_NONE);
				}
				// If there's a pre-existing constraint, see if it needs to be fixed up
				else
				{
					UPhysicsConstraintTemplate* ExistingConstraintSetup = PhysicsAsset->ConstraintSetup[ ConstraintIndex ];
					check(ExistingConstraintSetup);
					
					const int32 ExistingConstraintBoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ExistingConstraintSetup->DefaultInstance.ConstraintBone2);
					check(ExistingConstraintBoneIndex != INDEX_NONE);

					// If the constraint exists between two child bones, then no fix up is required
					if (EditorSkelMesh->RefSkeleton.BoneIsChildOf(ExistingConstraintBoneIndex, NewBoneIndex))
					{
						continue;
					}
					
					// If the constraint isn't between two child bones, then it is between a physical bone higher in the bone
					// hierarchy than the new bone, so it needs to be fixed up by setting the constraint to point to the new bone
					// instead. Additionally, collision needs to be re-enabled between the child bone and the identified "grandparent"
					// bone.
					const int32 ExistingConstraintBodyIndex = PhysicsAsset->FindBodyIndex(ExistingConstraintSetup->DefaultInstance.ConstraintBone2);
					check(ExistingConstraintBodyIndex != INDEX_NONE);
					check(ExistingConstraintBodyIndex == ParentBodyIndex);

					SetCollisionBetween(ChildBodyIndex, ExistingConstraintBodyIndex, true);
				}

				UPhysicsConstraintTemplate* ChildConstraintSetup = PhysicsAsset->ConstraintSetup[ ConstraintIndex ];
				check(ChildConstraintSetup);

				InitConstraintSetup(ChildConstraintSetup, ChildBodyIndex, NewBodyIndex);
			}
		}
	}

	// If we have a physics parent, create a joint to it.
	if (ParentBodyIndex != INDEX_NONE)
	{
		const int32 NewConstraintIndex = FPhysicsAssetUtils::CreateNewConstraint(PhysicsAsset, NewBoneName);
		UPhysicsConstraintTemplate* ConstraintSetup = PhysicsAsset->ConstraintSetup[ NewConstraintIndex ];
		check(ConstraintSetup);

		InitConstraintSetup(ConstraintSetup, NewBodyIndex, ParentBodyIndex);
	}

	// update the tree
	HierarchyChangedEvent.Broadcast();

	if (bAutoSelect)
	{
		SetSelectedBodyAnyPrim(NewBodyIndex);
	}
	

	RefreshPhysicsAssetChange(PhysicsAsset);
}

void FPhATSharedData::SetSelectedConstraintRelTM(const FTransform& RelTM)
{
	FTransform WParentFrame = GetConstraintWorldTM(GetSelectedConstraint(), EConstraintFrame::Frame2);
	FTransform WNewChildFrame = RelTM * WParentFrame;

	UPhysicsConstraintTemplate* ConstraintSetup = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
	ConstraintSetup->Modify();

	// Get child bone transform
	int32 BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone1);
	if (BoneIndex != INDEX_NONE)
	{
		FTransform BoneTM = EditorSkelComp->GetBoneTransform(BoneIndex);
		BoneTM.RemoveScaling();

		ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame1, WNewChildFrame.GetRelativeTransform(BoneTM));
	}
}

void FPhATSharedData::CopyConstraint()
{
	check(SelectedConstraints.Num() == 1);

	CopiedConstraintTemplate = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
}

void FPhATSharedData::PasteConstraintProperties()
{
	if (CopiedConstraintTemplate == NULL)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "PasteConstraintProperties", "Paste Constraint Properties") );

	UPhysicsConstraintTemplate* FromConstraintSetup = CopiedConstraintTemplate;

	for(int32 i=0; i<SelectedConstraints.Num(); ++i)
	{
		// If we are showing instance properties - copy instance properties. If showing setup, just copy setup properties.
		UPhysicsConstraintTemplate* ToConstraintSetup = PhysicsAsset->ConstraintSetup[SelectedConstraints[i].Index];
		CopyConstraintProperties(FromConstraintSetup, ToConstraintSetup);
	}
	
}

void CycleMatrixRows(FMatrix* TM)
{
	float Tmp[3];

	Tmp[0]		= TM->M[0][0];	Tmp[1]		= TM->M[0][1];	Tmp[2]		= TM->M[0][2];
	TM->M[0][0] = TM->M[1][0];	TM->M[0][1] = TM->M[1][1];	TM->M[0][2] = TM->M[1][2];
	TM->M[1][0] = TM->M[2][0];	TM->M[1][1] = TM->M[2][1];	TM->M[1][2] = TM->M[2][2];
	TM->M[2][0] = Tmp[0];		TM->M[2][1] = Tmp[1];		TM->M[2][2] = Tmp[2];
}

void FPhATSharedData::CycleCurrentConstraintOrientation()
{
	UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
	FMatrix ConstraintTransform = ConstraintTemplate->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2).ToMatrixWithScale();
	FTransform WParentFrame = GetConstraintWorldTM(GetSelectedConstraint(), EConstraintFrame::Frame2);
	FTransform WChildFrame = GetConstraintWorldTM(GetSelectedConstraint(), EConstraintFrame::Frame1);
	FTransform RelativeTransform = WChildFrame * WParentFrame.Inverse();

	CycleMatrixRows(&ConstraintTransform);

	ConstraintTemplate->DefaultInstance.SetRefFrame(EConstraintFrame::Frame2, FTransform(ConstraintTransform));
	SetSelectedConstraintRelTM(RelativeTransform);
}

void FPhATSharedData::CycleCurrentConstraintActive()
{
	for(int32 i=0; i<SelectedConstraints.Num(); ++i)
	{
		UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
		FConstraintInstance & DefaultInstance = ConstraintTemplate->DefaultInstance;

		if(DefaultInstance.GetAngularSwing1Motion() != ACM_Limited && DefaultInstance.GetAngularSwing2Motion() != ACM_Limited)
		{
			DefaultInstance.SetAngularSwing1Motion(ACM_Limited);
			DefaultInstance.SetAngularSwing2Motion(ACM_Locked);
			DefaultInstance.SetAngularTwistMotion(ACM_Locked);
		}else if(DefaultInstance.GetAngularSwing2Motion() != ACM_Limited && DefaultInstance.GetAngularTwistMotion() != ACM_Limited)
		{
			DefaultInstance.SetAngularSwing1Motion(ACM_Locked);
			DefaultInstance.SetAngularSwing2Motion(ACM_Limited);
			DefaultInstance.SetAngularTwistMotion(ACM_Locked);
		}else
		{
			DefaultInstance.SetAngularSwing1Motion(ACM_Locked);
			DefaultInstance.SetAngularSwing2Motion(ACM_Locked);
			DefaultInstance.SetAngularTwistMotion(ACM_Limited);
		}
		
	}
}

void FPhATSharedData::ToggleConstraint(EPhATConstraintType Constraint)
{
	for(int32 i=0; i<SelectedConstraints.Num(); ++i)
	{
		UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
		FConstraintInstance & DefaultInstance = ConstraintTemplate->DefaultInstance;

		if(Constraint == PCT_Swing1)
		{
			DefaultInstance.SetAngularSwing1Motion(DefaultInstance.GetAngularSwing1Motion() == ACM_Limited ? ACM_Locked : ACM_Limited);
		}else if(Constraint == PCT_Swing2)
		{
			DefaultInstance.SetAngularSwing2Motion(DefaultInstance.GetAngularSwing2Motion() == ACM_Limited ? ACM_Locked : ACM_Limited);
		}else
		{
			DefaultInstance.SetAngularTwistMotion(DefaultInstance.GetAngularTwistMotion() == ACM_Limited ? ACM_Locked : ACM_Limited);
		}
		
	}
}

void FPhATSharedData::DeleteBody(int32 DelBodyIndex, bool bRefreshComponent)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DeleteBody", "Delete Body") );

	// The physics asset and default instance..
	PhysicsAsset->Modify();

	// .. the body..
	UBodySetup * BodySetup = PhysicsAsset->SkeletalBodySetups[DelBodyIndex];
	BodySetup->Modify();	

	// .. and any constraints to the body.
	TArray<int32>	Constraints;
	PhysicsAsset->BodyFindConstraints(DelBodyIndex, Constraints);

	//we want to fixup constraints so that nearest child bodies get constraint with parent body
	TArray<int32> NearestBodiesBelow;
	PhysicsAsset->GetNearestBodyIndicesBelow(NearestBodiesBelow, BodySetup->BoneName, EditorSkelMesh);
	
	int32 BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(BodySetup->BoneName);

	if (BoneIndex != INDEX_NONE)	//it's possible to delete bodies that have no bones. In this case just ignore all of this fixup code
	{
		int32 ParentBodyIndex = PhysicsAsset->FindParentBodyIndex(EditorSkelMesh, BoneIndex);

		UBodySetup * ParentBody = ParentBodyIndex != INDEX_NONE ? PhysicsAsset->SkeletalBodySetups[ParentBodyIndex] : NULL;

		for (const int32 ConstraintIndex : Constraints)
		{
			UPhysicsConstraintTemplate * Constraint = PhysicsAsset->ConstraintSetup[ConstraintIndex];
			Constraint->Modify();

			if (ParentBody)
			{
				//for all constraints that contain a nearest child of this body, create a copy of the constraint between the child and parent
				for (const int32 BodyBelowIndex : NearestBodiesBelow)
				{
					UBodySetup * BodyBelow = PhysicsAsset->SkeletalBodySetups[BodyBelowIndex];

					if (Constraint->DefaultInstance.ConstraintBone1 == BodyBelow->BoneName)
					{
						int32 NewConstraintIndex = FPhysicsAssetUtils::CreateNewConstraint(PhysicsAsset, BodyBelow->BoneName, Constraint);
						UPhysicsConstraintTemplate * NewConstraint = PhysicsAsset->ConstraintSetup[NewConstraintIndex];
						InitConstraintSetup(NewConstraint, BodyBelowIndex, ParentBodyIndex);
					}
				}
			}
		}
	}

	// Now actually destroy body. This will destroy any constraints associated with the body as well.
	FPhysicsAssetUtils::DestroyBody(PhysicsAsset, DelBodyIndex);

	// Select nothing.
	SetSelectedBody(NULL);
	SetSelectedConstraint(INDEX_NONE);
	HierarchyChangedEvent.Broadcast();

	if (bRefreshComponent)
	{
		RefreshPhysicsAssetChange(PhysicsAsset);
	}
}

void FPhATSharedData::DeleteCurrentPrim()
{
	if (bRunningSimulation)
	{
		return;
	}

	if (!GetSelectedBody())
	{
		return;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	//We will first get all the bodysetups we're interested in. The number of duplicates each bodysetup has tells us how many geoms are being deleted
	//We need to do this first because deleting will modify our selection
	TMap<UBodySetup *, TArray<FSelection>> BodySelectionMap;
	TArray<UBodySetup*> BodySetups;
	for(int32 i=0; i<SelectedBodies.Num(); ++i)
	{
		UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[SelectedBodies[i].Index];
		BodySelectionMap.FindOrAdd(BodySetup).Add(SelectedBodies[i]);
	}

	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DeletePrimitive", "Delete Primitive") );

	for (TMap<UBodySetup*, TArray<FSelection> >::TConstIterator It(BodySelectionMap); It; ++It)
	{
		UBodySetup * BodySetup = It.Key();
		const TArray<FSelection> & SelectedPrimitives = It.Value();

		int32 SphereDeletedCount = 0;
		int32 BoxDeletedCount = 0;
		int32 SphylDeletedCount = 0;
		int32 ConvexDeletedCount = 0;

		for (int32 i = 0; i < SelectedPrimitives.Num(); ++i)
		{
			const FSelection& SelectedBody = SelectedPrimitives[i];
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(BodySetup->BoneName);

			BodySetup->Modify();

			if (SelectedBody.PrimitiveType == KPT_Sphere)
			{
				BodySetup->AggGeom.SphereElems.RemoveAt(SelectedBody.PrimitiveIndex - (SphereDeletedCount++));
			}
			else if (SelectedBody.PrimitiveType == KPT_Box)
			{
				BodySetup->AggGeom.BoxElems.RemoveAt(SelectedBody.PrimitiveIndex - (BoxDeletedCount++));
			}
			else if (SelectedBody.PrimitiveType == KPT_Sphyl)
			{
				BodySetup->AggGeom.SphylElems.RemoveAt(SelectedBody.PrimitiveIndex - (SphylDeletedCount++));
			}
			else if (SelectedBody.PrimitiveType == KPT_Convex)
			{
				BodySetup->AggGeom.ConvexElems.RemoveAt(SelectedBody.PrimitiveIndex - (ConvexDeletedCount++));
				// Need to invalidate GUID in this case as cooked data must be updated
				BodySetup->InvalidatePhysicsData();
			}

			// If this bone has no more geometry - remove it totally.
			if (BodySetup->AggGeom.GetElementCount() == 0)
			{
				check(i == SelectedPrimitives.Num() - 1);	//we should really only delete on last prim - only reason this is even in for loop is because of API needing body index
				if (BodyIndex != INDEX_NONE)
				{
					DeleteBody(BodyIndex, false);
				}

				if (CopiedBodySetup == BodySetup)
				{
					CopiedBodySetup = NULL;
				}
			}
		}
	}
	
	HierarchyChangedEvent.Broadcast();
	SetSelectedBodyAnyPrim(INDEX_NONE); // Will call UpdateViewport
	RefreshPhysicsAssetChange(PhysicsAsset);
}
FTransform FPhATSharedData::GetConstraintBodyTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame) const
{
	if (ConstraintSetup == NULL)
	{
		return FTransform::Identity;
	}

	int32 BoneIndex;
	if (Frame == EConstraintFrame::Frame1)
	{
		BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone1);
	}
	else
	{
		BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone2);
	}

	// If we couldn't find the bone - fall back to identity.
	if (BoneIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}
	else
	{
		FTransform BoneTM = EditorSkelComp->GetBoneTransform(BoneIndex);
		BoneTM.RemoveScaling();

		return BoneTM;
	}
}

FTransform FPhATSharedData::GetConstraintWorldTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame, float Scale) const
{
	if (ConstraintSetup == NULL)
	{
		return FTransform::Identity;
	}

	FVector Scale3D(Scale);

	int32 BoneIndex;
	FTransform LFrame = ConstraintSetup->DefaultInstance.GetRefFrame(Frame);
	if (Frame == EConstraintFrame::Frame1)
	{
		BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone1);
	}
	else
	{
		BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone2);
	}

	// If we couldn't find the bone - fall back to identity.
	if (BoneIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}
	else
	{
		FTransform BoneTM = EditorSkelComp->GetBoneTransform(BoneIndex);
		BoneTM.RemoveScaling();

		LFrame.ScaleTranslation(Scale3D);

		return LFrame * BoneTM;
	}
}

FTransform FPhATSharedData::GetConstraintMatrix(int32 ConstraintIndex, EConstraintFrame::Type Frame, float Scale) const
{
	UPhysicsConstraintTemplate* ConstraintSetup = PhysicsAsset->ConstraintSetup[ConstraintIndex];
	return GetConstraintWorldTM(ConstraintSetup, Frame, Scale);
}


FTransform FPhATSharedData::GetConstraintWorldTM(const FSelection* Constraint, EConstraintFrame::Type Frame) const
{
	int32 ConstraintIndex = Constraint ? Constraint->Index : INDEX_NONE;
	if (ConstraintIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	UPhysicsConstraintTemplate* ConstraintSetup = PhysicsAsset->ConstraintSetup[ConstraintIndex];
	return GetConstraintWorldTM(ConstraintSetup, Frame, 1.f);
}


void FPhATSharedData::DeleteCurrentConstraint()
{
	if (EditingMode != PEM_ConstraintEdit || !GetSelectedConstraint())
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "DeleteConstraint", "Delete Constraint") );

	//Save indices before delete because delete modifies our Selected array
	TArray<int32> Indices;
	for(int32 i=0; i<SelectedConstraints.Num(); ++i)
	{
		Indices.Add(SelectedConstraints[i].Index);
	}

	Indices.Sort();

	//These are indices into an array, we must remove it from greatest to smallest so that the indices don't shift
	for(int32 i=Indices.Num() - 1; i>= 0; --i)
	{
		
		if(PhysicsAsset->ConstraintSetup[Indices[i]] == CopiedConstraintTemplate)
		{
			CopiedConstraintTemplate = NULL;
		}

		PhysicsAsset->Modify();
		FPhysicsAssetUtils::DestroyConstraint(PhysicsAsset, Indices[i]);
		
	}
	
	SetSelectedConstraint(INDEX_NONE);

	HierarchyChangedEvent.Broadcast();
	PreviewChangedEvent.Broadcast();
}

void FPhATSharedData::ToggleInstanceProperties()
{
	bShowInstanceProps = !bShowInstanceProps;

	PreviewChangedEvent.Broadcast();

	if (EditingMode == PEM_ConstraintEdit)
	{
		if (GetSelectedConstraint())
		{
			UPhysicsConstraintTemplate* ConSetup = PhysicsAsset->ConstraintSetup[GetSelectedConstraint()->Index];
			FSelection Selection(GetSelectedConstraint()->Index, KPT_Unknown, INDEX_NONE);
			SelectionChangedEvent.Broadcast(ConSetup, &Selection);
		}
	}
	else if (EditingMode == PEM_BodyEdit)
	{
		if (GetSelectedBody())
		{
			UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[GetSelectedBody()->Index];

			// Set properties dialog to display selected bone (or bone instance) info.
			SelectionChangedEvent.Broadcast(BodySetup, GetSelectedBody());
		}
	}
}

void FPhATSharedData::ToggleSimulation()
{
	// don't start simulation if there are no bodies or if we are manipulating a body
	if (PhysicsAsset->SkeletalBodySetups.Num() == 0 || bManipulating)
	{
		return;  
	}

	EnableSimulation(!bRunningSimulation);

	bRunningSimulation = !bRunningSimulation;
}

void FPhATSharedData::EnableSimulation(bool bEnableSimulation)
{
	if (bEnableSimulation)
	{
		// Flush geometry cache inside the asset (don't want to use cached version of old geometry!)
		PhysicsAsset->InvalidateAllPhysicsMeshes();

		// We should not already have an instance (destroyed when stopping sim).
		EditorSkelComp->SetSimulatePhysics(true);
		EditorSkelComp->SetPhysicsBlendWeight(EditorSimOptions->PhysicsBlend);
		EditorSkelComp->InitArticulated(PreviewScene.GetWorld()->GetPhysicsScene());
		PhysicalAnimationComponent->SetSkeletalMeshComponent(EditorSkelComp);

		// Make it start simulating
		EditorSkelComp->WakeAllRigidBodies();

		// Set the properties window to point at the simulation options object.
		SelectionChangedEvent.Broadcast(EditorSimOptions, NULL);
	}
	else
	{
		// Stop any animation and clear node when stopping simulation.
		EditorSkelComp->SetAnimation(NULL);
		PhysicalAnimationComponent->SetSkeletalMeshComponent(nullptr);

		// Turn off/remove the physics instance for this thing, and move back to start location.
		EditorSkelComp->TermArticulated();
		EditorSkelComp->SetSimulatePhysics(false);
		EditorSkelComp->SetPhysicsBlendWeight(0.f);

		// Since simulation, actor location changes. Reset to identity 
		EditorSkelComp->SetWorldTransform(ResetTM);
		// Force an update of the skeletal mesh to get it back to ref pose
		EditorSkelComp->RefreshBoneTransforms();
		
		PreviewChangedEvent.Broadcast();

		// Put properties window back to selected.
		if (EditingMode == FPhATSharedData::PEM_BodyEdit)
		{
			SetSelectedBody(NULL, true);
		}
		else
		{
			SetSelectedConstraint(INDEX_NONE, true);
		}
	}
}

void FPhATSharedData::OpenNewBodyDlg()
{
	OpenNewBodyDlg(&NewBodyData, &NewBodyResponse);
}

void FPhATSharedData::OpenNewBodyDlg(FPhysAssetCreateParams* NewBodyData, EAppReturnType::Type* NewBodyResponse)
{
	auto ModalWindow = SNew(SWindow)
		.Title( NSLOCTEXT("PhAT", "NewAssetTitle", "New Asset") )
		.SizingRule( ESizingRule::Autosized )
		.SupportsMinimize(false) .SupportsMaximize(false);

	auto MessageBox = SNew(SPhATNewAssetDlg)
		.ParentWindow(ModalWindow)
		.NewBodyData(NewBodyData)
		.NewBodyResponse(NewBodyResponse);

	ModalWindow->SetContent(MessageBox);

	GEditor->EditorAddModalWindow(ModalWindow);
}

void FPhATSharedData::PostUndo()
{
	if (bRunningSimulation)
	{
		return;
	}

	bool bInvalidSelection = false;

	for (int32 BodyIndex = 0; BodyIndex < SelectedBodies.Num() && bInvalidSelection == false; ++BodyIndex)
	{
		const FSelection& Selection = SelectedBodies[BodyIndex];
		if (PhysicsAsset->SkeletalBodySetups.Num() <= Selection.Index)
		{
			bInvalidSelection = true;
		}
		else
		{
			
			if (UBodySetup * BodySetup = PhysicsAsset->SkeletalBodySetups[Selection.Index])
			{
				switch (Selection.PrimitiveType)
				{
				case KPT_Box: bInvalidSelection = BodySetup->AggGeom.BoxElems.Num() <= Selection.PrimitiveIndex ? true : bInvalidSelection; break;
				case KPT_Convex: bInvalidSelection = BodySetup->AggGeom.ConvexElems.Num() <= Selection.PrimitiveIndex ? true : bInvalidSelection; break;
				case KPT_Sphere: bInvalidSelection = BodySetup->AggGeom.SphereElems.Num() <= Selection.PrimitiveIndex ? true : bInvalidSelection; break;
				case KPT_Sphyl: bInvalidSelection = BodySetup->AggGeom.SphylElems.Num() <= Selection.PrimitiveIndex ? true : bInvalidSelection; break;
				default: bInvalidSelection = true;
				}
			}
			else
			{
				bInvalidSelection = true;
			}
		}
	}

	for (int32 ConstraintIndex = 0; ConstraintIndex < SelectedConstraints.Num() && bInvalidSelection == false; ++ConstraintIndex)
	{
		const FSelection& Selection = SelectedConstraints[ConstraintIndex];
		if (PhysicsAsset->ConstraintSetup.Num() <= Selection.Index)
		{
			bInvalidSelection = true;
		}
	}

	if (bInvalidSelection)
	{
		// Clear selection before we undo. We don't transact the editor itself - don't want to have something selected that is then removed.
		SetSelectedBody(NULL);
		SetSelectedConstraint(INDEX_NONE);
	}

	PreviewChangedEvent.Broadcast();
	HierarchyChangedEvent.Broadcast();
}

void FPhATSharedData::Redo()
{
	if (bRunningSimulation)
	{
		return;
	}

	SetSelectedBody(NULL);
	SetSelectedConstraint(INDEX_NONE);

	GEditor->RedoTransaction();
	PhysicsAsset->UpdateBodySetupIndexMap();

	PreviewChangedEvent.Broadcast();
	HierarchyChangedEvent.Broadcast();
}

#undef LOCTEXT_NAMESPACE