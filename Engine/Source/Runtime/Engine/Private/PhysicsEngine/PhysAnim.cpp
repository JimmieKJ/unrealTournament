// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysAnim.cpp: Code for supporting animation/physics blending
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimTree.h"
#include "SkeletalRenderPublic.h"
#include "Components/LineBatchComponent.h"
#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX
#include "PhysicsPublic.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"


/** Used for drawing pre-phys skeleton if bShowPrePhysBones is true */
static const FColor AnimSkelDrawColor(255, 64, 64);

// Temporary workspace for caching world-space matrices.
struct FAssetWorldBoneTM
{
	FTransform	TM;			// Should never contain scaling.
	bool bUpToDate;			// If this equals PhysAssetUpdateNum, then the matrix is up to date.
};


// Use current pose to calculate world-space position of this bone without physics now.
void UpdateWorldBoneTM(TArray<FAssetWorldBoneTM> & WorldBoneTMs, int32 BoneIndex, USkeletalMeshComponent* SkelComp, const FTransform &LocalToWorldTM, const FVector& Scale3D)
{
	// If its already up to date - do nothing
	if(	WorldBoneTMs[BoneIndex].bUpToDate )
	{
		return;
	}

	FTransform ParentTM, RelTM;
	if(BoneIndex == 0)
	{
		// If this is the root bone, we use the mesh component LocalToWorld as the parent transform.
		ParentTM = LocalToWorldTM;
	}
	else
	{
		// If not root, use our cached world-space bone transforms.
		int32 ParentIndex = SkelComp->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
		UpdateWorldBoneTM(WorldBoneTMs, ParentIndex, SkelComp, LocalToWorldTM, Scale3D);
		ParentTM = WorldBoneTMs[ParentIndex].TM;
	}

	RelTM = SkelComp->LocalAtoms[BoneIndex];
	RelTM.ScaleTranslation( Scale3D );

	WorldBoneTMs[BoneIndex].TM = RelTM * ParentTM;
	WorldBoneTMs[BoneIndex].bUpToDate = true;
}


void USkeletalMeshComponent::BlendPhysicsBones( TArray<FBoneIndexType>& InRequiredBones)
{
	// Get drawscale from Owner (if there is one)
	FVector TotalScale3D = ComponentToWorld.GetScale3D();
	FVector RecipScale3D = TotalScale3D.Reciprocal();

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	check( PhysicsAsset );

	if (GetNumSpaceBases() == 0)
	{
		return;
	}

	// Make sure scratch space is big enough.
	TArray<FAssetWorldBoneTM> WorldBoneTMs;
	WorldBoneTMs.Reset();
	WorldBoneTMs.AddZeroed(GetNumSpaceBases());
	
	FTransform LocalToWorldTM = ComponentToWorld;
	LocalToWorldTM.RemoveScaling();

	TArray<FTransform>& EditableSpaceBases = GetEditableSpaceBases();

	// For each bone - see if we need to provide some data for it.
	for(int32 i=0; i<InRequiredBones.Num(); i++)
	{
		int32 BoneIndex = InRequiredBones[i];

		// See if this is a physics bone..
		int32 BodyIndex = PhysicsAsset ? PhysicsAsset->FindBodyIndex(SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex)) : INDEX_NONE;
		// need to update back to physX so that physX knows where it was after blending
		bool bUpdatePhysics = false;
		FBodyInstance* BodyInstance = NULL;

		// If so - get its world space matrix and its parents world space matrix and calc relative atom.
		if(BodyIndex != INDEX_NONE )
		{	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// tracking down TTP 280421. Remove this if this doesn't happen. 
			if ( !ensure(Bodies.IsValidIndex(BodyIndex)) )
			{
				UE_LOG(LogPhysics, Warning, TEXT("%s(Mesh %s, PhysicsAsset %s)"), 
					*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset));
				if ( PhysicsAsset )
				{
					UE_LOG(LogPhysics, Warning, TEXT(" - # of BodySetup (%d), # of Bodies (%d), Invalid BodyIndex(%d)"), 
						PhysicsAsset->BodySetup.Num(), Bodies.Num(), BodyIndex);
				}
				continue;
			}
#endif
			BodyInstance = Bodies[BodyIndex];

			//if simulated body copy back and blend with animation
			if(BodyInstance->IsInstanceSimulatingPhysics())
			{
				FTransform PhysTM = BodyInstance->GetUnrealWorldTransform();

				// Store this world-space transform in cache.
				WorldBoneTMs[BoneIndex].TM = PhysTM;
				WorldBoneTMs[BoneIndex].bUpToDate = true;

				float UsePhysWeight = (bBlendPhysics)? 1.f : BodyInstance->PhysicsBlendWeight;

				// Find this bones parent matrix.
				FTransform ParentWorldTM;

				// if we wan't 'full weight' we just find 
				if(UsePhysWeight > 0.f)
				{
					if(BoneIndex == 0)
					{
						ParentWorldTM = LocalToWorldTM;
					}
					else
					{
						// If not root, get parent TM from cache (making sure its up-to-date).
						int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
						UpdateWorldBoneTM(WorldBoneTMs, ParentIndex, this, LocalToWorldTM, TotalScale3D);
						ParentWorldTM = WorldBoneTMs[ParentIndex].TM;
					}


					// Then calc rel TM and convert to atom.
					FTransform RelTM = PhysTM.GetRelativeTransform(ParentWorldTM);
					RelTM.RemoveScaling();
					FQuat RelRot(RelTM.GetRotation());
					FVector RelPos =  RecipScale3D * RelTM.GetLocation();
					FTransform PhysAtom = FTransform(RelRot, RelPos, LocalAtoms[BoneIndex].GetScale3D());

					// Now blend in this atom. See if we are forcing this bone to always be blended in
					LocalAtoms[BoneIndex].Blend( LocalAtoms[BoneIndex], PhysAtom, UsePhysWeight );

					if(BoneIndex == 0)
					{
						//We must update RecipScale3D based on the atom scale of the root
						TotalScale3D *= LocalAtoms[0].GetScale3D();
						RecipScale3D = TotalScale3D.Reciprocal();
					}

					if (UsePhysWeight < 1.f)
					{
						bUpdatePhysics = true;
					}
				}
			}
		}

		// Update SpaceBases entry for this bone now
		if( BoneIndex == 0 )
		{
			EditableSpaceBases[0] = LocalAtoms[0];
		}
		else
		{
			const int32 ParentIndex	= SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
			EditableSpaceBases[BoneIndex] = LocalAtoms[BoneIndex] * EditableSpaceBases[ParentIndex];

			/**
			* Normalize rotations.
			* We want to remove any loss of precision due to accumulation of error.
			* i.e. A componentSpace transform is the accumulation of all of its local space parents. The further down the chain, the greater the error.
			* SpaceBases are used by external systems, we feed this to PhysX, send this to gameplay through bone and socket queries, etc.
			* So this is a good place to make sure all transforms are normalized.
			*/
			EditableSpaceBases[BoneIndex].NormalizeRotation();
		}

		if (bUpdatePhysics && BodyInstance)
		{
			BodyInstance->SetBodyTransform(EditableSpaceBases[BoneIndex] * ComponentToWorld, true);
		}
	}

	// Transforms updated, cached local bounds are now out of date.
	InvalidateCachedBounds();
}



bool USkeletalMeshComponent::ShouldBlendPhysicsBones() const
{
	return Bodies.Num() > 0 && (DoAnyPhysicsBodiesHaveWeight() || bBlendPhysics);
}

bool USkeletalMeshComponent::DoAnyPhysicsBodiesHaveWeight() const
{
	for (const FBodyInstance* Body : Bodies)
	{
		if (Body->PhysicsBlendWeight > 0.f)
		{
			return true;
		}
	}

	return false;
}

void USkeletalMeshComponent::BlendInPhysics()
{
	SCOPE_CYCLE_COUNTER(STAT_BlendInPhysics);

	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	// We now have all the animations blended together and final relative transforms for each bone.
	// If we don't have or want any physics, we do nothing.
	if( Bodies.Num() > 0 )
	{
		BlendPhysicsBones( RequiredBones );
		
		// Update Child Transform - The above function changes bone transform, so will need to update child transform
		UpdateChildTransforms();

		// animation often change overlap. 
		UpdateOverlaps();

		// New bone positions need to be sent to render thread
		MarkRenderDynamicDataDirty();
	}
}



void USkeletalMeshComponent::UpdateKinematicBonesToAnim(const TArray<FTransform>& InSpaceBases, bool bTeleport, bool bNeedsSkinning, bool bForceUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBBones);

	// This below code produces some interesting result here
	// - below codes update physics data, so if you don't update pose, the physics won't have the right result
	// - but if we just update physics bone without update current pose, it will have stale data
	// If desired, pass the animation data to the physics joints so they can be used by motors.
	// See if we are going to need to update kinematics
	const bool bUpdateKinematics = (KinematicBonesUpdateType != EKinematicBonesUpdateToPhysics::SkipAllBones);

	// If desired, update physics bodies associated with skeletal mesh component to match.
	if( !bUpdateKinematics && !(bForceUpdate && IsAnySimulatingPhysics()))
	{
		// nothing to do 
		return;
	}

	// Get the scene, and do nothing if we can't get one.
	FPhysScene* PhysScene = nullptr;
	if (GetWorld() != nullptr)
	{
		PhysScene = GetWorld()->GetPhysicsScene();
	}

	if(PhysScene == nullptr)
	{
		return;
	}

	const FTransform& CurrentLocalToWorld = ComponentToWorld;

	// Gracefully handle NaN
	if(CurrentLocalToWorld.ContainsNaN())
	{
		return;
	}

	// If desired, draw the skeleton at the point where we pass it to the physics.
	if (bShowPrePhysBones && SkeletalMesh && InSpaceBases.Num() == SkeletalMesh->RefSkeleton.GetNum())
	{
		for (int32 i = 1; i<InSpaceBases.Num(); i++)
		{
			FVector ThisPos = CurrentLocalToWorld.TransformPosition(InSpaceBases[i].GetLocation());

			int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(i);
			FVector ParentPos = CurrentLocalToWorld.TransformPosition(InSpaceBases[ParentIndex].GetLocation());

			GetWorld()->LineBatcher->DrawLine(ThisPos, ParentPos, AnimSkelDrawColor, SDPG_Foreground);
		}
	}

	// warn if it has non-uniform scale
	const FVector& MeshScale3D = CurrentLocalToWorld.GetScale3D();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !MeshScale3D.IsUniform() )
	{
		UE_LOG(LogPhysics, Log, TEXT("USkeletalMeshComponent::UpdateKinematicBonesToAnim : Non-uniform scale factor (%s) can cause physics to mismatch for %s  SkelMesh: %s"), *MeshScale3D.ToString(), *GetFullName(), SkeletalMesh ? *SkeletalMesh->GetFullName() : TEXT("NULL"));
	}
#endif


	if (bEnablePerPolyCollision == false)
	{
		const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
		if (PhysicsAsset && SkeletalMesh && Bodies.Num() > 0)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!ensure(PhysicsAsset->BodySetup.Num() == Bodies.Num()))
			{
				// related to TTP 280315
				UE_LOG(LogPhysics, Warning, TEXT("Mesh (%s) has PhysicsAsset(%s), and BodySetup(%d) and Bodies(%d) don't match"),
					*SkeletalMesh->GetName(), *PhysicsAsset->GetName(), PhysicsAsset->BodySetup.Num(), Bodies.Num());
				return;
			}
#endif

#if WITH_PHYSX
			// Lock the scenes we need (flags set in InitArticulated)
			if(bHasBodiesInSyncScene)
			{
				SCENE_LOCK_WRITE(PhysScene->GetPhysXScene(PST_Sync))
			}

			if (bHasBodiesInAsyncScene)
			{
				SCENE_LOCK_WRITE(PhysScene->GetPhysXScene(PST_Async))
			}
#endif

			// Iterate over each body
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				// If we have a physics body, and its kinematic...
				FBodyInstance* BodyInst = Bodies[i];
				check(BodyInst);

				if (bForceUpdate || (BodyInst->IsValidBodyInstance() && !BodyInst->IsInstanceSimulatingPhysics()))
				{
					const int32 BoneIndex = BodyInst->InstanceBoneIndex;

					// If we could not find it - warn.
					if (BoneIndex == INDEX_NONE || BoneIndex >= GetNumSpaceBases())
					{
						const FName BodyName = PhysicsAsset->BodySetup[i]->BoneName;
						UE_LOG(LogPhysics, Log, TEXT("UpdateRBBones: WARNING: Failed to find bone '%s' need by PhysicsAsset '%s' in SkeletalMesh '%s'."), *BodyName.ToString(), *PhysicsAsset->GetName(), *SkeletalMesh->GetName());
					}
					else
					{
#if WITH_PHYSX
						// update bone transform to world
						const FTransform BoneTransform = InSpaceBases[BoneIndex] * CurrentLocalToWorld;
						ensure(!BoneTransform.ContainsNaN());

						// If kinematic and not teleporting, set kinematic target
						PxRigidDynamic* PRigidDynamic = BodyInst->GetPxRigidDynamic_AssumesLocked();
						if (!IsRigidBodyNonKinematic_AssumesLocked(PRigidDynamic) && !bTeleport)
						{
							PhysScene->SetKinematicTarget_AssumesLocked(BodyInst, BoneTransform, true);
						}
						// Otherwise, set global pose
						else
						{
							const PxTransform PNewPose = U2PTransform(BoneTransform);
							ensure(PNewPose.isValid());
							PRigidDynamic->setGlobalPose(PNewPose);
						}
#endif


						// now update scale
						// if uniform, we'll use BoneTranform
						if (MeshScale3D.IsUniform())
						{
							// @todo UE4 should we update scale when it's simulated?
							BodyInst->UpdateBodyScale(BoneTransform.GetScale3D());
						}
						else
						{
							// @note When you have non-uniform scale on mesh base,
							// hierarchical bone transform can update scale too often causing performance issue
							// So we just use mesh scale for all bodies when non-uniform
							// This means physics representation won't be accurate, but
							// it is performance friendly by preventing too frequent physics update
							BodyInst->UpdateBodyScale(MeshScale3D);
						}
					}
				}
				else
				{
					//make sure you have physics weight or blendphysics on, otherwise, you'll have inconsistent representation of bodies
					// @todo make this to be kismet log? But can be too intrusive
					if (!bBlendPhysics && BodyInst->PhysicsBlendWeight <= 0.f && BodyInst->BodySetup.IsValid())
					{
						UE_LOG(LogPhysics, Warning, TEXT("%s(Mesh %s, PhysicsAsset %s, Bone %s) is simulating, but no blending. "),
							*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset), *BodyInst->BodySetup.Get()->BoneName.ToString());
					}
				}
			}

#if WITH_PHYSX
			// Unlock the scenes 
			if (bHasBodiesInSyncScene)
			{
				SCENE_UNLOCK_WRITE(PhysScene->GetPhysXScene(PST_Sync))
			}

			if (bHasBodiesInAsyncScene)
			{
				SCENE_UNLOCK_WRITE(PhysScene->GetPhysXScene(PST_Async))
			}
#endif
		}
	}
	else
	{
		//per poly update requires us to update all vertex positions
		if (MeshObject)
		{
			if (bNeedsSkinning)
			{
				const FStaticLODModel& Model = MeshObject->GetSkeletalMeshResource().LODModels[0];
				TArray<FVector> NewPositions;
				if (true)
				{
					SCOPE_CYCLE_COUNTER(STAT_SkinPerPolyVertices);
					ComputeSkinnedPositions(NewPositions);
				}
				else	//keep old way around for now - useful for comparing performance
				{
					NewPositions.AddUninitialized(Model.NumVertices);
					{
						SCOPE_CYCLE_COUNTER(STAT_SkinPerPolyVertices);
						for (uint32 VertIndex = 0; VertIndex < Model.NumVertices; ++VertIndex)
						{
							NewPositions[VertIndex] = GetSkinnedVertexPosition(VertIndex);
						}
					}
				}
				BodyInstance.UpdateTriMeshVertices(NewPositions);
			}
			
			BodyInstance.SetBodyTransform(CurrentLocalToWorld, bTeleport);
		}
	}


}



void USkeletalMeshComponent::UpdateRBJointMotors()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBJoints);

	// moved this flag to here, so that
	// you can call it but still respect the flag
	if( !bUpdateJointsFromAnimation )
	{
		return;
	}

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && Constraints.Num() > 0 && SkeletalMesh)
	{
		check( PhysicsAsset->ConstraintSetup.Num() == Constraints.Num() );


		// Iterate over the constraints.
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[i];
			FConstraintInstance* CI = Constraints[i];

			FName JointName = CS->DefaultInstance.JointName;
			int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(JointName);

			// If we found this bone, and a visible bone that is not the root, and its joint is motorised in some way..
			if( (BoneIndex != INDEX_NONE) && (BoneIndex != 0) &&
				(BoneVisibilityStates[BoneIndex] == BVS_Visible) &&
				(CI->bAngularOrientationDrive) )
			{
				check(BoneIndex < LocalAtoms.Num());

				// If we find the joint - get the local-space animation between this bone and its parent.
				FQuat LocalQuat = LocalAtoms[BoneIndex].GetRotation();
				FQuatRotationTranslationMatrix LocalRot(LocalQuat, FVector::ZeroVector);

				// We loop from the graphics parent bone up to the bone that has the body which the joint is attached to, to calculate the relative transform.
				// We need this to compensate for welding, where graphics and physics parents may not be the same.
				FMatrix ControlBodyToParentBoneTM = FMatrix::Identity;

				int32 TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex); // This give the 'graphics' parent of this bone
				bool bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2); // ConstraintBone2 is the 'physics' parent of this joint.

				while(!bFoundControlBody)
				{
					// Abort if we find a bone scaled to zero.
					const FVector Scale3D = LocalAtoms[TestBoneIndex].GetScale3D();
					const float ScaleSum = Scale3D.X + Scale3D.Y + Scale3D.Z;
					if(ScaleSum < KINDA_SMALL_NUMBER)
					{
						break;
					}

					// Add the current animated local transform into the overall controlling body->parent bone TM
					FMatrix RelTM = LocalAtoms[TestBoneIndex].ToMatrixNoScale();
					RelTM.SetOrigin(FVector::ZeroVector);
					ControlBodyToParentBoneTM = ControlBodyToParentBoneTM * RelTM;

					// Move on to parent
					TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(TestBoneIndex);

					// If we are at the root - bail out.
					if(TestBoneIndex == 0)
					{
						break;
					}

					// See if this is the controlling body
					bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2);
				}

				// If after that we didn't find a parent body, we can' do this, so skip.
				if(bFoundControlBody)
				{
					// The animation rotation is between the two bodies. We need to supply the joint with the relative orientation between
					// the constraint ref frames. So we work out each body->joint transform

					FMatrix Body1TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1).ToMatrixNoScale();
					Body1TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body1TMInv = Body1TM.InverseFast();

					FMatrix Body2TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2).ToMatrixNoScale();
					Body2TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body2TMInv = Body2TM.InverseFast();

					FMatrix JointRot = Body1TM * (LocalRot * ControlBodyToParentBoneTM) * Body2TMInv;
					FQuat JointQuat(JointRot);

					// Then pass new quaternion to the joint!
					CI->SetAngularOrientationTarget(JointQuat);
				}
			}
		}
	}
}

