// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsAsset.cpp
=============================================================================*/ 

#include "EnginePrivate.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"


#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX
#include "PhysicsEngine/PhysicsAsset.h"


///////////////////////////////////////	
//////////// UPhysicsAsset ////////////
///////////////////////////////////////

UPhysicsAsset::UPhysicsAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsAsset::UpdateBoundsBodiesArray()
{
	BoundsBodies.Empty();

	for(int32 i=0; i<BodySetup.Num(); i++)
	{
		check(BodySetup[i]);
		if(BodySetup[i]->bConsiderForBounds)
		{
			BoundsBodies.Add(i);
		}
	}
}

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
	// update BodySetupIndexMap
	BodySetupIndexMap.Empty();

	for(int32 i=0; i<BodySetup.Num(); i++)
	{
		check(BodySetup[i]);
		BodySetupIndexMap.Add(BodySetup[i]->BoneName, i);
	}
}

void UPhysicsAsset::PostLoad()
{
	Super::PostLoad();

	// Ensure array of bounds bodies is up to date.
	if(BoundsBodies.Num() == 0)
	{
		UpdateBoundsBodiesArray();
	}

	if (BodySetup.Num() > 0 && BodySetupIndexMap.Num() == 0)
	{
		UpdateBodySetupIndexMap();
	}
}

void UPhysicsAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CollisionDisableTable;

#if WITH_EDITORONLY_DATA
	if (DefaultSkelMesh_DEPRECATED != NULL)
	{
		PreviewSkeletalMesh = TAssetPtr<USkeletalMesh>(DefaultSkelMesh_DEPRECATED);
		DefaultSkelMesh_DEPRECATED = NULL;
	}
#endif
}


void UPhysicsAsset::EnableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
	if(BodyIndexA == BodyIndexB)
	{
		return;
	}

	FRigidBodyIndexPair Key(BodyIndexA, BodyIndexB);

	// If its not in table - do nothing
	if( !CollisionDisableTable.Find(Key) )
	{
		return;
	}

	CollisionDisableTable.Remove(Key);
}

void UPhysicsAsset::DisableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
	if(BodyIndexA == BodyIndexB)
	{
		return;
	}

	FRigidBodyIndexPair Key(BodyIndexA, BodyIndexB);

	// If its already in the disable table - do nothing
	if( CollisionDisableTable.Find(Key) )
	{
		return;
	}

	CollisionDisableTable.Add(Key, 0);
}

FBox UPhysicsAsset::CalcAABB(const USkinnedMeshComponent* MeshComp, const FTransform& LocalToWorld) const
{
	FBox Box(0);

	if (!MeshComp)
	{
		return Box;
	}

	FVector Scale3D = LocalToWorld.GetScale3D();
	if( Scale3D.IsUniform() )
	{
		const TArray<int32>* BodyIndexRefs = NULL;
		TArray<int32> AllBodies;
		// If we want to consider all bodies, make array with all body indices in
		if(MeshComp->bConsiderAllBodiesForBounds)
		{
			AllBodies.AddUninitialized(BodySetup.Num());
			for(int32 i=0; i<BodySetup.Num();i ++)
			{
				AllBodies[i] = i;
			}
			BodyIndexRefs = &AllBodies;
		}
		// Otherwise, use the cached shortlist of bodies to consider
		else
		{
			BodyIndexRefs = &BoundsBodies;
		}

		// Then iterate over bodies we want to consider, calculating bounding box for each
		const int32 BodySetupNum = (*BodyIndexRefs).Num();

		for(int32 i=0; i<BodySetupNum; i++)
		{
			const int32 BodyIndex = (*BodyIndexRefs)[i];
			UBodySetup* bs = BodySetup[BodyIndex];

			if (bs->bConsiderForBounds)
			{
				if (i+1<BodySetupNum)
				{
					int32 NextIndex = (*BodyIndexRefs)[i+1];
					FPlatformMisc::Prefetch(BodySetup[NextIndex]);
					FPlatformMisc::Prefetch(BodySetup[NextIndex], CACHE_LINE_SIZE);
				}

				int32 BoneIndex = MeshComp->GetBoneIndex(bs->BoneName);
				if(BoneIndex != INDEX_NONE)
				{
					FTransform WorldBoneTransform = MeshComp->GetBoneTransform(BoneIndex, LocalToWorld);
					if(FMath::Abs(WorldBoneTransform.GetDeterminant()) > (float)KINDA_SMALL_NUMBER)
					{
						Box += bs->AggGeom.CalcAABB( WorldBoneTransform );
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPhysics, Log,  TEXT("UPhysicsAsset::CalcAABB : Non-uniform scale factor. You will not be able to collide with it.  Turn off collision and wrap it with a blocking volume.  MeshComp: %s  SkelMesh: %s"), *MeshComp->GetFullName(), MeshComp->SkeletalMesh ? *MeshComp->SkeletalMesh->GetFullName() : TEXT("NULL") );
	}

	if(!Box.IsValid)
	{
		Box = FBox( LocalToWorld.GetLocation(), LocalToWorld.GetLocation() );
	}

	return Box;
}

int32	UPhysicsAsset::FindControllingBodyIndex(class USkeletalMesh* skelMesh, int32 StartBoneIndex)
{
	int32 BoneIndex = StartBoneIndex;
	while(BoneIndex!=INDEX_NONE)
	{
		FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
		int32 BodyIndex = FindBodyIndex(BoneName);

		if(BodyIndex != INDEX_NONE)
			return BodyIndex;

		int32 ParentBoneIndex = skelMesh->RefSkeleton.GetParentIndex(BoneIndex);

		if(ParentBoneIndex == BoneIndex)
			return INDEX_NONE;

		BoneIndex = ParentBoneIndex;
	}

	return INDEX_NONE; // Shouldn't reach here.
}

int32 UPhysicsAsset::FindParentBodyIndex(class USkeletalMesh * skelMesh, int32 StartBoneIndex) const
{
	int32 BoneIndex = StartBoneIndex;
	while ((BoneIndex = skelMesh->RefSkeleton.GetParentIndex(BoneIndex)) != INDEX_NONE)
	{
		FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
		int32 BodyIndex = FindBodyIndex(BoneName);

		if (StartBoneIndex == BoneIndex)
			return INDEX_NONE;

		if (BodyIndex != INDEX_NONE)
			return BodyIndex;
	}

	return INDEX_NONE;
}


int32 UPhysicsAsset::FindBodyIndex(FName bodyName) const
{
	const int32* IdxData = BodySetupIndexMap.Find(bodyName);
	if (IdxData)
	{
		return *IdxData;
	}

	return INDEX_NONE;
}

int32 UPhysicsAsset::FindConstraintIndex(FName ConstraintName)
{
	for(int32 i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup[i]->DefaultInstance.JointName == ConstraintName )
		{
			return i;
		}
	}

	return INDEX_NONE;
}

FName UPhysicsAsset::FindConstraintBoneName(int32 ConstraintIndex)
{
	if ( (ConstraintIndex < 0) || (ConstraintIndex >= ConstraintSetup.Num()) )
	{
		return NAME_None;
	}

	return ConstraintSetup[ConstraintIndex]->DefaultInstance.JointName;
}

int32 UPhysicsAsset::FindMirroredBone(class USkeletalMesh* skelMesh, int32 BoneIndex)
{
	if (BoneIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	//we try to find the mirroed bone using several approaches. The first is to look for the same name but with _R instead of _L or vise versa
	FName BoneName = skelMesh->RefSkeleton.GetBoneName(BoneIndex);
	FString BoneNameString = BoneName.ToString();

	bool bIsLeft = BoneNameString.Find("_L", ESearchCase::IgnoreCase, ESearchDir::FromEnd) == (BoneNameString.Len() - 2);	//has _L at the end
	bool bIsRight = BoneNameString.Find("_R", ESearchCase::IgnoreCase, ESearchDir::FromEnd) == (BoneNameString.Len() - 2);	//has _R at the end
	bool bMirrorConvention = bIsLeft || bIsRight;

	//if the bone follows our left right naming convention then let's try and find its mirrored bone
	int32 BoneIndexMirrored = INDEX_NONE;
	if (bMirrorConvention)
	{
		FString BoneNameMirrored = BoneNameString.LeftChop(2);
		BoneNameMirrored.Append(bIsLeft ? "_R" : "_L");

		BoneIndexMirrored = skelMesh->RefSkeleton.FindBoneIndex(FName(*BoneNameMirrored));
	}

	return BoneIndexMirrored;
}

void UPhysicsAsset::GetBodyIndicesBelow(TArray<int32>& OutBodyIndices, FName InBoneName, USkeletalMesh* SkelMesh, bool bIncludeParent /*= true*/)
{
	int32 BaseIndex = SkelMesh->RefSkeleton.FindBoneIndex(InBoneName);

	// Iterate over all other bodies, looking for 'children' of this one
	for(int32 i=0; i<BodySetup.Num(); i++)
	{
		UBodySetup* BS = BodySetup[i];
		FName TestName = BS->BoneName;
		int32 TestIndex = SkelMesh->RefSkeleton.FindBoneIndex(TestName);

		if( (bIncludeParent && TestIndex == BaseIndex) || SkelMesh->RefSkeleton.BoneIsChildOf(TestIndex, BaseIndex))
		{
			OutBodyIndices.Add(i);
		}
	}
}

void UPhysicsAsset::GetNearestBodyIndicesBelow(TArray<int32> & OutBodyIndices, FName InBoneName, USkeletalMesh * InSkelMesh)
{
	TArray<int32> AllBodiesBelow;
	GetBodyIndicesBelow(AllBodiesBelow, InBoneName, InSkelMesh, false);

	//we need to filter all bodies below to first in the chain
	TArray<bool> Nearest;
	Nearest.AddUninitialized(BodySetup.Num());
	for (int32 i = 0; i < Nearest.Num(); ++i)
	{
		Nearest[i] = true;
	}

	for (int32 i = 0; i < AllBodiesBelow.Num(); i++)
	{
		int32 BodyIndex = AllBodiesBelow[i];
		if (Nearest[BodyIndex] == false) continue;

		UBodySetup * Body = BodySetup[BodyIndex];
		TArray<int32> BodiesBelowMe;
		GetBodyIndicesBelow(BodiesBelowMe, Body->BoneName, InSkelMesh, false);
		
		for (int j = 0; j < BodiesBelowMe.Num(); ++j)
		{
			Nearest[BodiesBelowMe[j]] = false;	
		}
	}

	for (int32 i = 0; i < AllBodiesBelow.Num(); i++)
	{
		int32 BodyIndex = AllBodiesBelow[i];
		if (Nearest[BodyIndex])
		{
			OutBodyIndices.Add(BodyIndex);
		}
	}
}

void UPhysicsAsset::ClearAllPhysicsMeshes()
{
	for(int32 i=0; i<BodySetup.Num(); i++)
	{
		BodySetup[i]->ClearPhysicsMeshes();
	}
}

#if WITH_EDITOR

void UPhysicsAsset::InvalidateAllPhysicsMeshes()
{
	for(int32 i=0; i<BodySetup.Num(); i++)
	{
		BodySetup[i]->InvalidatePhysicsData();
	}
}


void UPhysicsAsset::PostEditUndo()
{
	Super::PostEditUndo();
	
	UpdateBodySetupIndexMap();
	UpdateBoundsBodiesArray();
}

#endif

//// THUMBNAIL SUPPORT //////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UPhysicsAsset::GetDesc()
{
	return FString::Printf( TEXT("%d Bodies, %d Constraints"), BodySetup.Num(), ConstraintSetup.Num() );
}

void UPhysicsAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add( FAssetRegistryTag("Bodies", FString::FromInt(BodySetup.Num()), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("Constraints", FString::FromInt(ConstraintSetup.Num()), FAssetRegistryTag::TT_Numerical) );

	Super::GetAssetRegistryTags(OutTags);
}


void UPhysicsAsset::BodyFindConstraints(int32 BodyIndex, TArray<int32>& Constraints)
{
	Constraints.Empty();
	FName BodyName = BodySetup[BodyIndex]->BoneName;

	for(int32 ConIdx=0; ConIdx<ConstraintSetup.Num(); ConIdx++)
	{
		if( ConstraintSetup[ConIdx]->DefaultInstance.ConstraintBone1 == BodyName || ConstraintSetup[ConIdx]->DefaultInstance.ConstraintBone2 == BodyName )
		{
			Constraints.Add(ConIdx);
		}
	}
}

SIZE_T UPhysicsAsset::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;

	for (const auto& SingleBody : BodySetup)
	{
		ResourceSize += SingleBody->GetResourceSize(Mode);
	}

	ResourceSize += BodySetupIndexMap.GetAllocatedSize();
	ResourceSize += CollisionDisableTable.GetAllocatedSize();

	// @todo implement inclusive mode
	return ResourceSize;
}