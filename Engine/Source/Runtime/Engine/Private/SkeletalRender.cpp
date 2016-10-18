// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRender.cpp: Skeletal mesh skinning/rendering code.
=============================================================================*/

#include "EnginePrivate.h"
#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"
#include "PhysicsEngine/PhysicsAsset.h"

/*-----------------------------------------------------------------------------
Globals
-----------------------------------------------------------------------------*/

// smallest blend weight for vertex anims
const float MinMorphTargetBlendWeight = SMALL_NUMBER;
// largest blend weight for vertex anims
const float MaxMorphTargetBlendWeight = 5.0f;

/*-----------------------------------------------------------------------------
FSkeletalMeshObject
-----------------------------------------------------------------------------*/

FSkeletalMeshObject::FSkeletalMeshObject(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel)
:	MinDesiredLODLevel(0)
,	MaxDistanceFactor(0.f)
,	WorkingMinDesiredLODLevel(0)
,	WorkingMaxDistanceFactor(0.f)
,   bHasBeenUpdatedAtLeastOnce(false)
#if WITH_EDITORONLY_DATA
,   SectionIndexPreview(InMeshComponent->SectionIndexPreview)
#endif	
,	SkeletalMeshResource(InSkeletalMeshResource)
,	SkeletalMeshLODInfo(InMeshComponent->SkeletalMesh->LODInfo)
,	LastFrameNumber(0)
#if WITH_EDITORONLY_DATA
,	ProgressiveDrawingFraction(InMeshComponent->ProgressiveDrawingFraction)
#endif
,	CustomSortAlternateIndexMode((ECustomSortAlternateIndexMode)InMeshComponent->CustomSortAlternateIndexMode)
,	bUsePerBoneMotionBlur(InMeshComponent->bPerBoneMotionBlur)
,	StatId(InMeshComponent->SkeletalMesh->GetStatID(true))
,	FeatureLevel(InFeatureLevel)
{
	check(SkeletalMeshResource);

#if WITH_EDITORONLY_DATA
	if ( !GIsEditor )
	{
		SectionIndexPreview = -1;
	}
#endif // #if WITH_EDITORONLY_DATA

	// Assume no GPU skin cache
	FMemory::Memset(GPUSkinCacheKeys, -1, sizeof(GPUSkinCacheKeys));

	// We want to restore the most recent value of the MaxDistanceFactor the SkeletalMeshComponent
	// cached, which will be 0.0 when first created, and a valid, updated value when recreating
	// this mesh object (e.g. during a component reregister), avoiding issues with a transient
	// assignment of 0.0 and then back to an updated value the next frame.
	MaxDistanceFactor = InMeshComponent->MaxDistanceFactor;
	WorkingMaxDistanceFactor = MaxDistanceFactor;

	InitLODInfos(InMeshComponent);
}

void FSkeletalMeshObject::UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, int32 FrameNumber)
{
	static const auto* SkeletalMeshLODRadiusScale = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SkeletalMeshLODRadiusScale"));
	float LODScale = FMath::Clamp(SkeletalMeshLODRadiusScale->GetValueOnRenderThread(), 0.25f, 1.0f);

	const FVector4 ScreenPosition( View->WorldToScreen(Bounds.Origin) );
	const float ScreenRadius = FMath::Max(View->ViewRect.Width() / 2.0f * View->ViewMatrices.ProjMatrix.M[0][0],
		View->ViewRect.Height() / 2.0f * View->ViewMatrices.ProjMatrix.M[1][1]) * Bounds.SphereRadius / FMath::Max(ScreenPosition.W,1.0f);

	const float LODFactor = LODScale * ScreenRadius / 320.0f;

	check( SkeletalMeshLODInfo.Num() == SkeletalMeshResource->LODModels.Num() );

	// Need the current LOD
	const int32 CurrentLODLevel = GetLOD();
	const float HysteresisOffset = 0.f;

	int32 NewLODLevel = 0;

	// Look for a lower LOD if the EngineShowFlags is enabled - Thumbnail rendering disables LODs
	if( 1==View->Family->EngineShowFlags.LOD )
	{
		// Iterate from worst to best LOD
		for(int32 LODLevel = SkeletalMeshResource->LODModels.Num()-1; LODLevel > 0; LODLevel--) 
		{
			// Get DistanceFactor for this LOD
			float LODDistanceFactor = SkeletalMeshLODInfo[LODLevel].ScreenSize;

			// If we are considering shifting to a better (lower) LOD, bias with hysteresis.
			if(LODLevel  <= CurrentLODLevel)
			{
				LODDistanceFactor += SkeletalMeshLODInfo[LODLevel].LODHysteresis;
			}

			// If have passed this boundary, use this LOD
			if(LODDistanceFactor > LODFactor)
			{
				NewLODLevel = LODLevel;
				break;
			}
		}
	}

	// Different path for first-time vs subsequent-times in this function (ie splitscreen)
	if(FrameNumber != LastFrameNumber)
	{
		// Copy last frames value to the version that will be read by game thread
		MaxDistanceFactor = WorkingMaxDistanceFactor;
		MinDesiredLODLevel = WorkingMinDesiredLODLevel;
		LastFrameNumber = FrameNumber;

		WorkingMaxDistanceFactor = LODFactor;
		WorkingMinDesiredLODLevel = NewLODLevel;
	}
	else
	{
		WorkingMaxDistanceFactor = FMath::Max(WorkingMaxDistanceFactor, LODFactor);
		WorkingMinDesiredLODLevel = FMath::Min(WorkingMinDesiredLODLevel, NewLODLevel);
	}
}

/**
 * List of chunks to be rendered based on instance weight usage. Full swap of weights will render with its own chunks.
 * @return Chunks to iterate over for rendering
 */
const TArray<FSkelMeshSection>& FSkeletalMeshObject::GetRenderSections(int32 InLODIndex) const
{
	const FStaticLODModel& LOD = SkeletalMeshResource->LODModels[InLODIndex];
	return LOD.Sections;
}

/**
 * Update the hidden material section flags for an LOD entry
 *
 * @param InLODIndex - LOD entry to update hidden material flags for
 * @param HiddenMaterials - array of hidden material sections
 */
void FSkeletalMeshObject::SetHiddenMaterials(int32 InLODIndex,const TArray<bool>& HiddenMaterials)
{
	check(LODInfo.IsValidIndex(InLODIndex));
	LODInfo[InLODIndex].HiddenMaterials = HiddenMaterials;		
}

/**
 * Determine if the material section entry for an LOD is hidden or not
 *
 * @param InLODIndex - LOD entry to get hidden material flags for
 * @param MaterialIdx - index of the material section to check
 */
bool FSkeletalMeshObject::IsMaterialHidden(int32 InLODIndex,int32 MaterialIdx) const
{
	check(LODInfo.IsValidIndex(InLODIndex));
	return LODInfo[InLODIndex].HiddenMaterials.IsValidIndex(MaterialIdx) && LODInfo[InLODIndex].HiddenMaterials[MaterialIdx];
}
/**
 * Initialize the array of LODInfo based on the settings of the current skel mesh component
 */
void FSkeletalMeshObject::InitLODInfos(const USkinnedMeshComponent* SkelComponent)
{
	LODInfo.Empty(SkeletalMeshLODInfo.Num());
	for (int32 Idx=0; Idx < SkeletalMeshLODInfo.Num(); Idx++)
	{
		FSkelMeshObjectLODInfo& MeshLODInfo = *new(LODInfo) FSkelMeshObjectLODInfo();
		if (SkelComponent->LODInfo.IsValidIndex(Idx))
		{
			const FSkelMeshComponentLODInfo &Info = SkelComponent->LODInfo[Idx];

			MeshLODInfo.HiddenMaterials = Info.HiddenMaterials;
		}		
	}
}

/*-----------------------------------------------------------------------------
Global functions
-----------------------------------------------------------------------------*/

/**
 * Utility function that fills in the array of ref-pose to local-space matrices using 
 * the mesh component's updated space bases
 * @param	ReferenceToLocal - matrices to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - each LOD has its own mapping of bones to update
 * @param	ExtraRequiredBoneIndices - any extra bones apart from those active in the LOD that we'd like to update
 */
void UpdateRefToLocalMatrices( TArray<FMatrix>& ReferenceToLocal, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshResource* InSkeletalMeshResource, int32 LODIndex, const TArray<FBoneIndexType>* ExtraRequiredBoneIndices )
{
	const USkeletalMesh* const ThisMesh = InMeshComponent->SkeletalMesh;
	const USkinnedMeshComponent* const MasterComp = InMeshComponent->MasterPoseComponent.Get();
	const USkeletalMesh* const MasterCompMesh = MasterComp? MasterComp->SkeletalMesh : nullptr;
	const FStaticLODModel& LOD = InSkeletalMeshResource->LODModels[LODIndex];

	const TArray<int32>& MasterBoneMap = InMeshComponent->GetMasterBoneMap();

	check( ThisMesh->RefBasesInvMatrix.Num() != 0 );
	if(ReferenceToLocal.Num() != ThisMesh->RefBasesInvMatrix.Num())
	{
		ReferenceToLocal.Reset();
		ReferenceToLocal.AddUninitialized(ThisMesh->RefBasesInvMatrix.Num());
	}

	const bool bIsMasterCompValid = MasterComp && MasterBoneMap.Num() == ThisMesh->RefSkeleton.GetNum();

	const TArray<FBoneIndexType>* RequiredBoneSets[3] = { &LOD.ActiveBoneIndices, ExtraRequiredBoneIndices, NULL };

	const bool bBoneVisibilityStatesValid = InMeshComponent->BoneVisibilityStates.Num() == InMeshComponent->GetNumComponentSpaceTransforms();

	// Handle case of using ParentAnimComponent for SpaceBases.
	for( int32 RequiredBoneSetIndex = 0; RequiredBoneSets[RequiredBoneSetIndex]!=NULL; RequiredBoneSetIndex++ )
	{
		const TArray<FBoneIndexType>& RequiredBoneIndices = *RequiredBoneSets[RequiredBoneSetIndex];

		// Get the index of the bone in this skeleton, and loop up in table to find index in parent component mesh.
		for(int32 BoneIndex = 0;BoneIndex < RequiredBoneIndices.Num();BoneIndex++)
		{
			const int32 ThisBoneIndex = RequiredBoneIndices[BoneIndex];

			if ( ThisMesh->RefBasesInvMatrix.IsValidIndex(ThisBoneIndex) )
			{
				// On the off chance the parent matrix isn't valid, revert to identity.
				ReferenceToLocal[ThisBoneIndex] = FMatrix::Identity;

				if( bIsMasterCompValid )
				{
					// If valid, use matrix from parent component.
					const int32 MasterBoneIndex = MasterBoneMap[ThisBoneIndex];
					if (MasterComp->GetComponentSpaceTransforms().IsValidIndex(MasterBoneIndex))
					{
						const int32 ParentIndex = MasterCompMesh->RefSkeleton.GetParentIndex(MasterBoneIndex);
						bool bNeedToHideBone = MasterComp->BoneVisibilityStates[MasterBoneIndex] != BVS_Visible;
						if (bNeedToHideBone && ParentIndex != INDEX_NONE)
						{
							ReferenceToLocal[ThisBoneIndex] = ReferenceToLocal[ParentIndex].ApplyScale(0.f);
						}
						else
						{
							checkSlow(MasterComp->GetComponentSpaceTransforms()[MasterBoneIndex].IsRotationNormalized());
							ReferenceToLocal[ThisBoneIndex] = MasterComp->GetComponentSpaceTransforms()[MasterBoneIndex].ToMatrixWithScale();
						}
					}
				}
				else
				{
					if (InMeshComponent->GetComponentSpaceTransforms().IsValidIndex(ThisBoneIndex))
					{
						// If we can't find this bone in the parent, we just use the reference pose.
						if (bBoneVisibilityStatesValid)
						{
							const int32 ParentIndex = ThisMesh->RefSkeleton.GetParentIndex(ThisBoneIndex);
							bool bNeedToHideBone = InMeshComponent->BoneVisibilityStates[ThisBoneIndex] != BVS_Visible;
							if (bNeedToHideBone && ParentIndex != INDEX_NONE)
							{
								ReferenceToLocal[ThisBoneIndex] = ReferenceToLocal[ParentIndex].ApplyScale(0.f);
							}
							else
							{
								checkSlow(InMeshComponent->GetComponentSpaceTransforms()[ThisBoneIndex].IsRotationNormalized());
								ReferenceToLocal[ThisBoneIndex] = InMeshComponent->GetComponentSpaceTransforms()[ThisBoneIndex].ToMatrixWithScale();
							}
						}
						else
						{
							checkSlow(InMeshComponent->GetComponentSpaceTransforms()[ThisBoneIndex].IsRotationNormalized());
							ReferenceToLocal[ThisBoneIndex] = InMeshComponent->GetComponentSpaceTransforms()[ThisBoneIndex].ToMatrixWithScale();
						}
					}
				}
			}
			// removed else statement to set ReferenceToLocal[ThisBoneIndex] = FTransform::Identity;
			// since it failed in ( ThisMesh->RefBasesInvMatrix.IsValidIndex(ThisBoneIndex) ), ReferenceToLocal is not valid either
			// because of the initialization code line above to match both array count
			// if(ReferenceToLocal.Num() != ThisMesh->RefBasesInvMatrix.Num())
		}
	}

	for (int32 ThisBoneIndex = 0; ThisBoneIndex < ReferenceToLocal.Num(); ++ThisBoneIndex)
	{
		ReferenceToLocal[ThisBoneIndex] = ThisMesh->RefBasesInvMatrix[ThisBoneIndex] * ReferenceToLocal[ThisBoneIndex];
	}
}

/**
 * Utility function that calculates the local-space origin and bone direction vectors for the
 * current pose for any TRISORT_CustomLeftRight sections.
 * @param	OutVectors - origin and direction vectors to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - current LOD
 */
void UpdateCustomLeftRightVectors( TArray<FTwoVectors>& OutVectors, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshResource* InSkeletalMeshResource, int32 LODIndex )
{
	const USkeletalMesh* const ThisMesh = InMeshComponent->SkeletalMesh;
	const USkinnedMeshComponent* const MasterComp = InMeshComponent->MasterPoseComponent.Get();
	const FStaticLODModel& LOD = InSkeletalMeshResource->LODModels[LODIndex];
	const FSkeletalMeshLODInfo& LODInfo = ThisMesh->LODInfo[LODIndex];
	const TArray<int32>& MasterBoneMap = InMeshComponent->GetMasterBoneMap();

	if(OutVectors.Num() != LODInfo.TriangleSortSettings.Num())
	{
		OutVectors.Reset();
		OutVectors.AddUninitialized(LODInfo.TriangleSortSettings.Num());
	}

	const FVector AxisDirections[] = { FVector(1.f,0.f,0.f), FVector(0.f,1.f,0.f), FVector(0.f,0.f,1.f) };

	for ( int32 SectionIndex = 0 ; SectionIndex < LOD.Sections.Num() ; ++SectionIndex )
	{
		if( LOD.Sections[SectionIndex].TriangleSorting == TRISORT_CustomLeftRight )
		{
			FName CustomLeftRightBoneName = LODInfo.TriangleSortSettings[SectionIndex].CustomLeftRightBoneName;
			if( CustomLeftRightBoneName == NAME_None )
			{
				OutVectors[SectionIndex].v1 = FVector::ZeroVector;
				OutVectors[SectionIndex].v2 = AxisDirections[LODInfo.TriangleSortSettings[SectionIndex].CustomLeftRightAxis];
			}
			else
			{
				int32 SpaceBasesBoneIndex = ThisMesh->RefSkeleton.FindBoneIndex(CustomLeftRightBoneName);
				const TArray<FTransform>* SpaceBases = &InMeshComponent->GetComponentSpaceTransforms();
				
				// Handle case of using MasterPoseComponent for SpaceBases.
				if( MasterComp && MasterBoneMap.Num() == ThisMesh->RefSkeleton.GetNum() && SpaceBasesBoneIndex != INDEX_NONE )
				{
					// If valid, use matrix from parent component.
					SpaceBasesBoneIndex = MasterBoneMap[SpaceBasesBoneIndex];
					SpaceBases = &MasterComp->GetComponentSpaceTransforms();
				}

				if (SpaceBases->IsValidIndex(SpaceBasesBoneIndex))
				{
					const FMatrix BoneMatrix = (*SpaceBases)[SpaceBasesBoneIndex].ToMatrixWithScale();
					OutVectors[SectionIndex].v1 = BoneMatrix.GetOrigin();

					EAxis::Type MatrixAxis = EAxis::X;

					// TEnumAsByte<ETriangleSortAxis> won't cast to EMatrixAxis, so lets do this the hard way...
					switch ( LODInfo.TriangleSortSettings[SectionIndex].CustomLeftRightAxis )
					{
					case TSA_X_Axis:
						MatrixAxis = EAxis::X;
						break;

					case TSA_Y_Axis:
						MatrixAxis = EAxis::Y;
						break;

					case TSA_Z_Axis:
						MatrixAxis = EAxis::Z;
						break;

					default:
						check(0);
					}

					OutVectors[SectionIndex].v2 = BoneMatrix.GetScaledAxis( MatrixAxis );
				}
				else
				{
					OutVectors[SectionIndex].v1 = FVector::ZeroVector;
					OutVectors[SectionIndex].v2 = AxisDirections[LODInfo.TriangleSortSettings[SectionIndex].CustomLeftRightAxis];
				}
			}
		}
	}
}
