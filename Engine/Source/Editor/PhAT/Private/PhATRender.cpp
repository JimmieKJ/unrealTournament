// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "PhATSharedData.h"
#include "PhATHitProxies.h"
#include "PhATEdSkeletalMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

UPhATEdSkeletalMeshComponent::UPhATEdSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BoneUnselectedColor(170,155,225)
	, BoneSelectedColor(185,70,0)
	, ElemSelectedColor(255,166,0)
	, ElemSelectedBodyColor(255, 255, 100)
	, NoCollisionColor(200, 200, 200)
	, FixedColor(125,125,0)
	, ConstraintBone1Color(255,166,0)
	, ConstraintBone2Color(0,150,150)
	, HierarchyDrawColor(220, 255, 220)
	, AnimSkelDrawColor(255, 64, 64)
	, COMRenderSize(5.0f)
	, InfluenceLineLength(2.0f)
	, InfluenceLineColor(0,255,0)
{

	// Body materials
	ElemSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_ElemSelectedMaterial.PhAT_ElemSelectedMaterial"), NULL, LOAD_None, NULL);
	check(ElemSelectedMaterial);

	BoneSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_BoneSelectedMaterial.PhAT_BoneSelectedMaterial"), NULL, LOAD_None, NULL);
	check(BoneSelectedMaterial);

	BoneMaterialHit = UMaterial::GetDefaultMaterial(MD_Surface);
	check(BoneMaterialHit);

	BoneUnselectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_UnselectedMaterial.PhAT_UnselectedMaterial"), NULL, LOAD_None, NULL);
	check(BoneUnselectedMaterial);

	BoneNoCollisionMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EditorMaterials/PhAT_NoCollisionMaterial.PhAT_NoCollisionMaterial"), NULL, LOAD_None, NULL);
	check(BoneNoCollisionMaterial);

	// this is because in phat editor, you'd like to see fixed bones to be fixed without animation force update
	KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
	bUpdateJointsFromAnimation = false;
	ForcedLodModel = 1;

	static FName CollisionProfileName(TEXT("PhysicsActor"));
	SetCollisionProfileName(CollisionProfileName);
}

UPhysicsAsset* UPhATEdSkeletalMeshComponent::GetPhysicsAsset() const
{
	return SharedData->PhysicsAsset;
}

void UPhATEdSkeletalMeshComponent::RenderAssetTools(const FSceneView* View, class FPrimitiveDrawInterface* PDI, bool bHitTest)
{
	check(SharedData);

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	check(PhysicsAsset);

	bool bHitTestAndBodyMode = bHitTest && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit;
	bool bHitTestAndConstraintMode = bHitTest && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit;

	FPhATSharedData::EPhATRenderMode CollisionViewMode = SharedData->GetCurrentCollisionViewMode();

#if DEBUG_CLICK_VIEWPORT
	PDI->DrawLine(SharedData->LastClickOrigin, SharedData->LastClickOrigin + SharedData->LastClickDirection * 5000.0f, FLinearColor(1, 1, 0, 1), SDPG_Foreground);
	PDI->DrawPoint(SharedData->LastClickOrigin, FLinearColor(1, 0, 0), 5, SDPG_Foreground);
#endif
	// Draw bodies
	for (int32 i = 0; i <PhysicsAsset->BodySetup.Num(); ++i)
	{
		int32 BoneIndex = GetBoneIndex(PhysicsAsset->BodySetup[i]->BoneName);

		// If we found a bone for it, draw the collision.
		// The logic is as follows; always render in the ViewMode requested when not in hit mode - but if we are in hit mode and the right editing mode, render as solid
		if (BoneIndex != INDEX_NONE)
		{
			FTransform BoneTM = GetBoneTransform(BoneIndex);
			float Scale = BoneTM.GetScale3D().GetAbsMax();
			FVector VectorScale(Scale);
			BoneTM.RemoveScaling();

			FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup[i]->AggGeom;

			for (int32 j = 0; j <AggGeom->SphereElems.Num(); ++j)
			{
				if (bHitTest)
				{
					PDI->SetHitProxy(new HPhATEdBoneProxy(i, KPT_Sphere, j));
				}

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, KPT_Sphere, j, Scale);

				//solids are drawn if it's the ViewMode and we're not doing a hit, or if it's hitAndBodyMode
				if( (CollisionViewMode == FPhATSharedData::PRM_Solid && !bHitTest) || bHitTestAndBodyMode)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, KPT_Sphere, j, bHitTestAndBodyMode);
					AggGeom->SphereElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				//wires are never used during hit
				if(!bHitTest)
				{
					if (CollisionViewMode == FPhATSharedData::PRM_Solid || CollisionViewMode == FPhATSharedData::PRM_Wireframe)
					{
						AggGeom->SphereElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, KPT_Sphere, j));
					}
				}

				if (bHitTest)
				{
					PDI->SetHitProxy(NULL);
				}
				
			}

			for (int32 j = 0; j <AggGeom->BoxElems.Num(); ++j)
			{
				if (bHitTest)
				{
					PDI->SetHitProxy(new HPhATEdBoneProxy(i, KPT_Box, j));
				}

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, KPT_Box, j, Scale);

				if ( (CollisionViewMode == FPhATSharedData::PRM_Solid && !bHitTest) || bHitTestAndBodyMode)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, KPT_Box, j, bHitTestAndBodyMode);
					AggGeom->BoxElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				if(!bHitTest)
				{
					if (CollisionViewMode == FPhATSharedData::PRM_Solid || CollisionViewMode == FPhATSharedData::PRM_Wireframe)
					{
						AggGeom->BoxElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, KPT_Box, j));
					}
				}
				

				if (bHitTest) 
				{
					PDI->SetHitProxy(NULL);
				}
			}

			for (int32 j = 0; j <AggGeom->SphylElems.Num(); ++j)
			{
				if (bHitTest) 
				{
					PDI->SetHitProxy(new HPhATEdBoneProxy(i, KPT_Sphyl, j));
				}

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, KPT_Sphyl, j, Scale);

				if ( (CollisionViewMode == FPhATSharedData::PRM_Solid && !bHitTest) || bHitTestAndBodyMode)
				{
					UMaterialInterface*	PrimMaterial = GetPrimitiveMaterial(i, KPT_Sphyl, j, bHitTestAndBodyMode);
					AggGeom->SphylElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				}

				if(!bHitTest)
				{
					if (CollisionViewMode == FPhATSharedData::PRM_Solid || CollisionViewMode == FPhATSharedData::PRM_Wireframe)
					{
						AggGeom->SphylElems[j].DrawElemWire(PDI, ElemTM, VectorScale, GetPrimitiveColor(i, KPT_Sphyl, j));
					}
				}

				if (bHitTest) 
				{
					PDI->SetHitProxy(NULL);
				}
			}

			for (int32 j = 0; j <AggGeom->ConvexElems.Num(); ++j)
			{
				if (bHitTest) 
				{
					PDI->SetHitProxy(new HPhATEdBoneProxy(i, KPT_Convex, j));
				}

				FTransform ElemTM = GetPrimitiveTransform(BoneTM, i, KPT_Convex, j, Scale);

				//convex doesn't have solid draw so render lines if we're in hitTestAndBodyMode
				if(!bHitTest || bHitTestAndBodyMode)
				{
					if (CollisionViewMode == FPhATSharedData::PRM_Solid || CollisionViewMode == FPhATSharedData::PRM_Wireframe)
					{
						AggGeom->ConvexElems[j].DrawElemWire(PDI, ElemTM, Scale, GetPrimitiveColor(i, KPT_Convex, j));
					}
				}
				

				if (bHitTest) 
				{
					PDI->SetHitProxy(NULL);
				}
			}

			if (!bHitTest && SharedData->bShowCOM && Bodies.IsValidIndex(i))
			{
				Bodies[i]->DrawCOMPosition(PDI, COMRenderSize, SharedData->COMRenderColor);
			}
		}
	}

	// Draw Constraints
	FPhATSharedData::EPhATConstraintViewMode ConstraintViewMode = SharedData->GetCurrentConstraintViewMode();
	if (ConstraintViewMode != FPhATSharedData::PCV_None)
	{
		for (int32 i = 0; i <PhysicsAsset->ConstraintSetup.Num(); ++i)
		{
			int32 BoneIndex1 = GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone1);
			int32 BoneIndex2 = GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone2);
			// if bone doesn't exist, do not draw it. It crashes in random points when we try to manipulate. 
			if (BoneIndex1 != INDEX_NONE && BoneIndex2 != INDEX_NONE)
			{
				if (bHitTest) 
				{
					PDI->SetHitProxy(new HPhATEdConstraintProxy(i));
				}

				if(bHitTestAndConstraintMode || !bHitTest)
				{
					DrawConstraint(i, View, PDI, SharedData->EditorSimOptions->bShowConstraintsAsPoints);
				}
					
				if (bHitTest) 
				{
					PDI->SetHitProxy(NULL);
				}
			}
		}
	}

	if (!bHitTest && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit && SharedData->bShowInfluences)
	{
		DrawCurrentInfluences(PDI);
	}

	// If desired, draw bone hierarchy.
	if (!bHitTest && SharedData->bShowHierarchy)
	{
		DrawHierarchy(PDI, false);
	}

	// If desired, draw animation skeleton.
	if (!bHitTest && SharedData->bShowAnimSkel)
	{
		DrawHierarchy(PDI, SharedData->bRunningSimulation);
	}
}

void UPhATEdSkeletalMeshComponent::Render(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	RenderAssetTools(View, PDI, 0);
}

void UPhATEdSkeletalMeshComponent::RenderHitTest(const FSceneView* View, class FPrimitiveDrawInterface* PDI)
{
	RenderAssetTools(View, PDI, 1);
}

FPrimitiveSceneProxy* UPhATEdSkeletalMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	FPhATSharedData::EPhATRenderMode MeshViewMode = SharedData->GetCurrentMeshViewMode();
	if (MeshViewMode != FPhATSharedData::PRM_None)
	{
		Proxy = USkeletalMeshComponent::CreateSceneProxy();
	}

	return Proxy;
}

void UPhATEdSkeletalMeshComponent::DrawHierarchy(FPrimitiveDrawInterface* PDI, bool bAnimSkel)
{
	for (int32 i=1; i<GetNumSpaceBases(); ++i)
	{
		int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(i);

		FVector ParentPos, ChildPos;
		if (bAnimSkel)
		{
			ParentPos = ComponentToWorld.TransformPosition(AnimationSpaceBases[ParentIndex].GetLocation());
			ChildPos = ComponentToWorld.TransformPosition(AnimationSpaceBases[i].GetLocation());
		}
		else
		{
			ParentPos = ComponentToWorld.TransformPosition(GetSpaceBases()[ParentIndex].GetLocation());
			ChildPos = ComponentToWorld.TransformPosition(GetSpaceBases()[i].GetLocation());
		}

		FColor DrawColor = bAnimSkel ? AnimSkelDrawColor : HierarchyDrawColor;
		PDI->DrawLine(ParentPos, ChildPos, DrawColor, SDPG_Foreground);
	}
}

bool ConstraintInSelected(int32 Index, const TArray<FPhATSharedData::FSelection> & Constraints)
{
	for(int32 i=0; i<Constraints.Num(); ++i)
	{

		if(Constraints[i].Index == Index)
		{
			return true;
		}
	}

	return false;
}

void UPhATEdSkeletalMeshComponent::DrawConstraint(int32 ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bDrawAsPoint)
{
	FPhATSharedData::EPhATConstraintViewMode ConstraintViewMode = SharedData->GetCurrentConstraintViewMode();
	bool bDrawLimits = false;
	bool bConstraintSelected = ConstraintInSelected(ConstraintIndex, SharedData->SelectedConstraints);
	if (ConstraintViewMode == FPhATSharedData::PCV_AllLimits)
	{
		bDrawLimits = true;
	}
	else if (SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && bConstraintSelected)
	{
		bDrawLimits = true;
	}

	bool bDrawSelected = false;
	if (!SharedData->bRunningSimulation && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && bConstraintSelected)
	{
		bDrawSelected = true;
	}

	UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[ConstraintIndex];

	FTransform Con1Frame = SharedData->GetConstraintMatrix(ConstraintIndex, EConstraintFrame::Frame1, 1.f);
	FTransform Con2Frame = SharedData->GetConstraintMatrix(ConstraintIndex, EConstraintFrame::Frame2, 1.f);

	float ZoomFactor = FMath::Min<float>(View->ViewMatrices.ProjMatrix.M[0][0], View->ViewMatrices.ProjMatrix.M[1][1]);
	float DrawScale = View->Project(Con1Frame.GetTranslation()).W * (SharedData->EditorSimOptions->ConstraintDrawSize / ZoomFactor);

	ConstraintSetup->DefaultInstance.DrawConstraint(PDI, 1.f, DrawScale, bDrawLimits, bDrawSelected, Con1Frame, Con2Frame, bDrawAsPoint);
}

void UPhATEdSkeletalMeshComponent::DrawCurrentInfluences(FPrimitiveDrawInterface* PDI)
{
	
	// For each influenced bone, draw a little line for each vertex
	for (int32 i = 0; i <SharedData->ControlledBones.Num(); ++i)
	{
		int32 BoneIndex = SharedData->ControlledBones[i];
		FMatrix BoneTM = SharedData->EditorSkelComp->GetBoneMatrix(BoneIndex);

		FBoneVertInfo* BoneInfo = &SharedData->DominantWeightBoneInfos[ BoneIndex ];
		check(BoneInfo->Positions.Num() == BoneInfo->Normals.Num());

		for (int32 j = 0; j <BoneInfo->Positions.Num(); ++j)
		{
			FVector Position = BoneTM.TransformPosition(BoneInfo->Positions[j]);
			FVector Normal = BoneTM.TransformVector(BoneInfo->Normals[j]);

			PDI->DrawLine(Position, Position + Normal * InfluenceLineLength, InfluenceLineColor, SDPG_World);
		}
	}
}


FTransform UPhATEdSkeletalMeshComponent::GetPrimitiveTransform(FTransform& BoneTM, int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, float Scale)
{
	UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[BodyIndex];
	FVector Scale3D(Scale);

	FTransform ManTM = FTransform::Identity;

	if(SharedData->bManipulating && !SharedData->bRunningSimulation && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		FPhATSharedData::FSelection Body(BodyIndex, PrimType, PrimIndex);
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			if (Body == SharedData->SelectedBodies[i])
			{
				ManTM = SharedData->SelectedBodies[i].ManipulateTM;
				break;
			}

		}
	}


	if (PrimType == KPT_Sphere)
	{
		FTransform PrimTM = ManTM * BodySetup->AggGeom.SphereElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == KPT_Box)
	{
		FTransform PrimTM = ManTM * BodySetup->AggGeom.BoxElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == KPT_Sphyl)
	{
		FTransform PrimTM = ManTM * BodySetup->AggGeom.SphylElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if (PrimType == KPT_Convex)
	{
		FTransform PrimTM = ManTM * BodySetup->AggGeom.ConvexElems[PrimIndex].GetTransform();
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}

	// Should never reach here
	check(0);
	return FTransform::Identity;
}

FColor UPhATEdSkeletalMeshComponent::GetPrimitiveColor(int32 BodyIndex, EKCollisionPrimitiveType PrimitiveType, int32 PrimitiveIndex)
{
	UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[ BodyIndex ];

	if (!SharedData->bRunningSimulation && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		UPhysicsConstraintTemplate* cs = SharedData->PhysicsAsset->ConstraintSetup[ SharedData->GetSelectedConstraint()->Index ];

		if (cs->DefaultInstance.ConstraintBone1 == BodySetup->BoneName)
		{
			return ConstraintBone1Color;
		}
		else if (cs->DefaultInstance.ConstraintBone2 == BodySetup->BoneName)
		{
			return ConstraintBone2Color;
		}
	}

	if (!SharedData->bRunningSimulation && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
	{
		return BoneUnselectedColor;
	}

	FPhATSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	bool bInBody = false;
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		if(BodyIndex == SharedData->SelectedBodies[i].Index)
		{
			bInBody = true;
		}

		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return ElemSelectedColor;
		}
	}

	if(bInBody && !SharedData->bRunningSimulation)	//this primitive is in a body that's currently selected, but this primitive itself isn't selected
	{
		return ElemSelectedBodyColor;
	}
	
	if (SharedData->bShowFixedStatus && SharedData->bRunningSimulation)
	{
		const bool bIsSimulatedAtAll = BodySetup->PhysicsType == PhysType_Simulated || (BodySetup->PhysicsType == PhysType_Default && SharedData->EditorSimOptions->PhysicsBlend > 0.f);
		if (!bIsSimulatedAtAll)
		{
			return FixedColor;
		}
	}
	else
	{
		if (!SharedData->bRunningSimulation && SharedData->SelectedBodies.Num())
		{
			// If there is no collision with this body, use 'no collision material'.
			if (SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE)
			{
				return NoCollisionColor;
			}
		}
	}

	return BoneUnselectedColor;
}

UMaterialInterface* UPhATEdSkeletalMeshComponent::GetPrimitiveMaterial(int32 BodyIndex, EKCollisionPrimitiveType PrimitiveType, int32 PrimitiveIndex, bool bHitTest)
{
	if (SharedData->bRunningSimulation || SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
	{
		return bHitTest ? BoneMaterialHit : BoneUnselectedMaterial;
	}

	FPhATSharedData::FSelection Body(BodyIndex, PrimitiveType, PrimitiveIndex);

	for(int32 i=0; i< SharedData->SelectedBodies.Num(); ++i)
	{
		if (Body == SharedData->SelectedBodies[i] && !SharedData->bRunningSimulation)
		{
			return bHitTest ? BoneMaterialHit : ElemSelectedMaterial;
		}
	}

	// If there is no collision with this body, use 'no collision material'.
	if (SharedData->NoCollisionBodies.Find(BodyIndex) != INDEX_NONE && !SharedData->bRunningSimulation)
	{
		return bHitTest ? BoneMaterialHit : BoneNoCollisionMaterial;
	}
	else
	{
		return bHitTest ? BoneMaterialHit : BoneUnselectedMaterial;
	}

}
