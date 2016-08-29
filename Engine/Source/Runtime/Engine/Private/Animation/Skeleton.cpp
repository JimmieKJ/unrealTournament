// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Skeleton.cpp: Skeleton features
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineUtils.h"
#include "AnimationRuntime.h"
#include "AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/Rig.h"
#include "Animation/BlendProfile.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "Skeleton"
#define ROOT_BONE_PARENT	INDEX_NONE

#if WITH_EDITOR
const FName USkeleton::AnimNotifyTag = FName(TEXT("AnimNotifyList"));
const FString USkeleton::AnimNotifyTagDelimiter = TEXT(";");

const FName USkeleton::CurveTag = FName(TEXT("CurveUIDList"));
const FString USkeleton::CurveTagDelimiter = TEXT(";");

const FName USkeleton::RigTag = FName(TEXT("Rig"));
#endif 

// Names of smartname containers for skeleton properties
const FName USkeleton::AnimCurveMappingName = FName(TEXT("AnimationCurves"));
const FName USkeleton::AnimTrackCurveMappingName = FName(TEXT("AnimationTrackCurves"));

const FName FAnimSlotGroup::DefaultGroupName = FName(TEXT("DefaultGroup"));
const FName FAnimSlotGroup::DefaultSlotName = FName(TEXT("DefaultSlot"));

FArchive& operator<<(FArchive& Ar, FReferencePose & P)
{
	Ar << P.PoseName;
	Ar << P.ReferencePose;
#if WITH_EDITORONLY_DATA
	//TODO: we should use strip flags but we need to rev the serialization version
	if (!Ar.IsCooking())
	{
		Ar << P.ReferenceMesh;
	}
#endif
	return Ar;
}

USkeleton::USkeleton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make sure we have somewhere for curve names.
	SmartNames.AddContainer(AnimCurveMappingName);
}

void USkeleton::PostInitProperties()
{
	Super::PostInitProperties();

	// this gets called after constructor, and this data can get
	// serialized back if this already has Guid
	if (!IsTemplate())
	{
		RegenerateGuid();
	}
}

void USkeleton::PostLoad()
{
	Super::PostLoad();

	if( GetLinker() && (GetLinker()->UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR) )
	{
		// Convert RefLocalPoses & BoneTree to FReferenceSkeleton
		ConvertToFReferenceSkeleton();
	}

	// catch any case if guid isn't valid
	check(Guid.IsValid());

	// Cache smart name uids for animation curve names
	CacheAnimCurveMappingNameUids();
}

void USkeleton::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		// regenerate Guid
		RegenerateGuid();
	}
}

void USkeleton::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USkeleton::Serialize"), STAT_Skeleton_Serialize, STATGROUP_LoadTime );

	Super::Serialize(Ar);

	if( Ar.UE4Ver() >= VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		Ar << ReferenceSkeleton;
	}

	if (Ar.UE4Ver() >= VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION)
	{
		// Load Animation RetargetSources
		if (Ar.IsLoading())
		{
			int32 NumOfRetargetSources;
			Ar << NumOfRetargetSources;

			FName RetargetSourceName;
			FReferencePose RetargetSource;
			for (int32 Index=0; Index<NumOfRetargetSources; ++Index)
			{
				Ar << RetargetSourceName;
				Ar << RetargetSource;

				AnimRetargetSources.Add(RetargetSourceName, RetargetSource);
			}
		}
		else 
		{
			int32 NumOfRetargetSources = AnimRetargetSources.Num();
			Ar << NumOfRetargetSources;

			for (auto Iter = AnimRetargetSources.CreateIterator(); Iter; ++Iter)
			{
				Ar << Iter.Key();
				Ar << Iter.Value();
			}
		}
	}
	else
	{
		// this is broken, but we have to keep it to not corrupt content. 
		for (auto Iter = AnimRetargetSources.CreateIterator(); Iter; ++Iter)
		{
			Ar << Iter.Key();
			Ar << Iter.Value();
		}
	}

	if (Ar.UE4Ver() < VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		RegenerateGuid();
	}
	else
	{
		Ar << Guid;
	}

	// If we should be using smartnames, serialize the mappings
	if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		SmartNames.Serialize(Ar);
	}

	// Build look up table between Slot nodes and their Group.
	if(Ar.UE4Ver() < VER_UE4_FIX_SLOT_NAME_DUPLICATION)
	{
		// In older assets we may have duplicates, remove these while building the map.
		BuildSlotToGroupMap(true);
	}
	else
	{
		BuildSlotToGroupMap();
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() < VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE)
	{
		PreviewAttachedAssetContainer.SaveAttachedObjectsFromDeprecatedProperties();
	}
#endif
}

void USkeleton::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USkeleton* This = CastChecked<USkeleton>(InThis);

#if WITH_EDITORONLY_DATA
	for (auto Iter = This->AnimRetargetSources.CreateIterator(); Iter; ++Iter)
	{		
		Collector.AddReferencedObject(Iter.Value().ReferenceMesh, This);
	}
#endif

	Super::AddReferencedObjects( This, Collector );
}

/** Remove this function when VER_UE4_REFERENCE_SKELETON_REFACTOR is removed. */
void USkeleton::ConvertToFReferenceSkeleton()
{
	check( BoneTree.Num() == RefLocalPoses_DEPRECATED.Num() );

	const int32 NumRefBones = RefLocalPoses_DEPRECATED.Num();
	ReferenceSkeleton.Empty();

	for(int32 BoneIndex=0; BoneIndex<NumRefBones; BoneIndex++)
	{
		const FBoneNode& BoneNode = BoneTree[BoneIndex];
		FMeshBoneInfo BoneInfo(BoneNode.Name_DEPRECATED, BoneNode.Name_DEPRECATED.ToString(), BoneNode.ParentIndex_DEPRECATED);
		const FTransform& BoneTransform = RefLocalPoses_DEPRECATED[BoneIndex];

		// All should be good. Parents before children, no duplicate bones?
		ReferenceSkeleton.Add(BoneInfo, BoneTransform);
	}

	// technically here we should call 	RefershAllRetargetSources(); but this is added after 
	// VER_UE4_REFERENCE_SKELETON_REFACTOR, this shouldn't be needed. It shouldn't have any 
	// AnimatedRetargetSources
	ensure (AnimRetargetSources.Num() == 0);
}

bool USkeleton::DoesParentChainMatch(int32 StartBoneIndex, const USkeletalMesh* InSkelMesh) const
{
	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;

	// if start is root bone
	if ( StartBoneIndex == 0 )
	{
		// verify name of root bone matches
		return (SkeletonRefSkel.GetBoneName(0) == MeshRefSkel.GetBoneName(0));
	}

	int32 SkeletonBoneIndex = StartBoneIndex;
	// If skeleton bone is not found in mesh, fail.
	int32 MeshBoneIndex = MeshRefSkel.FindBoneIndex( SkeletonRefSkel.GetBoneName(SkeletonBoneIndex) );
	if( MeshBoneIndex == INDEX_NONE )
	{
		return false;
	}
	do
	{
		// verify if parent name matches
		int32 ParentSkeletonBoneIndex = SkeletonRefSkel.GetParentIndex(SkeletonBoneIndex);
		int32 ParentMeshBoneIndex = MeshRefSkel.GetParentIndex(MeshBoneIndex);

		// if one of the parents doesn't exist, make sure both end. Otherwise fail.
		if( (ParentSkeletonBoneIndex == INDEX_NONE) || (ParentMeshBoneIndex == INDEX_NONE) )
		{
			return (ParentSkeletonBoneIndex == ParentMeshBoneIndex);
		}

		// If parents are not named the same, fail.
		if( SkeletonRefSkel.GetBoneName(ParentSkeletonBoneIndex) != MeshRefSkel.GetBoneName(ParentMeshBoneIndex) )
		{
			return false;
		}

		// move up
		SkeletonBoneIndex = ParentSkeletonBoneIndex;
		MeshBoneIndex = ParentMeshBoneIndex;
	} while ( true );

	return true;
}

bool USkeleton::IsCompatibleMesh(const USkeletalMesh* InSkelMesh) const
{
	// at least % of bone should match 
	int32 NumOfBoneMatches = 0;

	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;
	const int32 NumBones = MeshRefSkel.GetNum();

	// first ensure the parent exists for each bone
	for (int32 MeshBoneIndex=0; MeshBoneIndex<NumBones; MeshBoneIndex++)
	{
		FName MeshBoneName = MeshRefSkel.GetBoneName(MeshBoneIndex);
		// See if Mesh bone exists in Skeleton.
		int32 SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex( MeshBoneName );

		// if found, increase num of bone matches count
		if( SkeletonBoneIndex != INDEX_NONE )
		{
			++NumOfBoneMatches;

			// follow the parent chain to verify the chain is same
			if(!DoesParentChainMatch(SkeletonBoneIndex, InSkelMesh))
			{
				UE_LOG(LogAnimation, Warning, TEXT("%s : Hierarchy does not match."), *MeshBoneName.ToString());
				return false;
			}
		}
		else
		{
			int32 CurrentBoneId = MeshBoneIndex;
			// if not look for parents that matches
			while (SkeletonBoneIndex == INDEX_NONE && CurrentBoneId != INDEX_NONE)
			{
				// find Parent one see exists
				const int32 ParentMeshBoneIndex = MeshRefSkel.GetParentIndex(CurrentBoneId);
				if ( ParentMeshBoneIndex != INDEX_NONE )
				{
					// @TODO: make sure RefSkeleton's root ParentIndex < 0 if not, I'll need to fix this by checking TreeBoneIdx
					FName ParentBoneName = MeshRefSkel.GetBoneName(ParentMeshBoneIndex);
					SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(ParentBoneName);
				}

				// root is reached
				if( ParentMeshBoneIndex == 0 )
				{
					break;
				}
				else
				{
					CurrentBoneId = ParentMeshBoneIndex;
				}
			}

			// still no match, return false, no parent to look for
			if( SkeletonBoneIndex == INDEX_NONE )
			{
				UE_LOG(LogAnimation, Warning, TEXT("%s : Missing joint on skeleton.  Make sure to assign to the skeleeton."), *MeshBoneName.ToString());
				return false;
			}

			// second follow the parent chain to verify the chain is same
			if( !DoesParentChainMatch(SkeletonBoneIndex, InSkelMesh) )
			{
				UE_LOG(LogAnimation, Warning, TEXT("%s : Hierarchy does not match."), *MeshBoneName.ToString());
				return false;
			}
		}
	}

	// originally we made sure at least matches more than 50% 
	// but then slave components can't play since they're only partial
	// if the hierarchy matches, and if it's more then 1 bone, we allow
	return (NumOfBoneMatches > 0);
}

void USkeleton::ClearCacheData()
{
	LinkupCache.Empty();
	SkelMesh2LinkupCache.Empty();
}

int32 USkeleton::GetMeshLinkupIndex(const USkeletalMesh* InSkelMesh)
{
	const int32* IndexPtr = SkelMesh2LinkupCache.Find(InSkelMesh);
	int32 LinkupIndex = INDEX_NONE;

	if ( IndexPtr == NULL )
	{
		LinkupIndex = BuildLinkup(InSkelMesh);
	}
	else
	{
		LinkupIndex = *IndexPtr;
	}

	// make sure it's not out of range
	check (LinkupIndex < LinkupCache.Num());

	return LinkupIndex;
}


int32 USkeleton::BuildLinkup(const USkeletalMesh* InSkelMesh)
{
	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;

	// @todoanim : need to refresh NULL SkeletalMeshes from Cache
	// since now they're autoweak pointer, they will go away if not used
	// so whenever map transition happens, this links will need to clear up
	FSkeletonToMeshLinkup NewMeshLinkup;

	// First, make sure the Skeleton has all the bones the SkeletalMesh possesses.
	// This can get out of sync if a mesh was imported on that Skeleton, but the Skeleton was not saved.

	const int32 NumMeshBones = MeshRefSkel.GetNum();
	NewMeshLinkup.MeshToSkeletonTable.Empty(NumMeshBones);
	NewMeshLinkup.MeshToSkeletonTable.AddUninitialized(NumMeshBones);

#if WITH_EDITOR
	// The message below can potentially fire many times if the skeleton has been put into a state where it is no longer
	// fully compatible with the mesh we're trying to merge into the bone tree. We use this flag to only show it once per mesh
	bool bDismissedMessage = false;
#endif

	for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; MeshBoneIndex++)
	{
		const FName MeshBoneName = MeshRefSkel.GetBoneName(MeshBoneIndex);
		int32 SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(MeshBoneName);

#if WITH_EDITOR
		// If we're in editor, and skeleton is missing a bone, fix it.
		// not currently supported in-game.
		if (SkeletonBoneIndex == INDEX_NONE)
		{
			if(!bDismissedMessage)
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("SkeletonBuildLinkupMissingBones", "The Skeleton {0}, is missing bones that SkeletalMesh {1} needs. They will be added now. Please save the Skeleton!"), FText::FromString(GetNameSafe(this)), FText::FromString(GetNameSafe(InSkelMesh))));
				bDismissedMessage = true;
			}

			static FName NAME_LoadErrors("LoadErrors");
			FMessageLog LoadErrors(NAME_LoadErrors);

			TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones1", "The Skeleton ")));
			Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString( GetNameSafe(this) ) ));
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones2", " is missing bones that SkeletalMesh ")));
			Message->AddToken(FAssetNameToken::Create(InSkelMesh->GetPathName(), FText::FromString( GetNameSafe(InSkelMesh) )));
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones3", "  needs. They will be added now. Please save the Skeleton!")));
			LoadErrors.Open();

			// Re-add all SkelMesh bones to the Skeleton.
			MergeAllBonesToBoneTree(InSkelMesh);

			// Fix missing bone.
			SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(MeshBoneName);
		}
#endif

		NewMeshLinkup.MeshToSkeletonTable[MeshBoneIndex] = SkeletonBoneIndex;
	}

	const int32 NumSkeletonBones = SkeletonRefSkel.GetNum();
	NewMeshLinkup.SkeletonToMeshTable.Empty(NumSkeletonBones);
	NewMeshLinkup.SkeletonToMeshTable.AddUninitialized(NumSkeletonBones);
	
	for (int32 SkeletonBoneIndex=0; SkeletonBoneIndex<NumSkeletonBones; SkeletonBoneIndex++)
	{
		const int32 MeshBoneIndex = MeshRefSkel.FindBoneIndex( SkeletonRefSkel.GetBoneName(SkeletonBoneIndex) );
		NewMeshLinkup.SkeletonToMeshTable[SkeletonBoneIndex] = MeshBoneIndex;
	}

	int32 NewIndex = LinkupCache.Add(NewMeshLinkup);
	check (NewIndex != INDEX_NONE);
	SkelMesh2LinkupCache.Add(InSkelMesh, NewIndex);

	return NewIndex;
}


void USkeleton::RebuildLinkup(const USkeletalMesh* InSkelMesh)
{
	// remove the key
	SkelMesh2LinkupCache.Remove(InSkelMesh);
	// build new one
	BuildLinkup(InSkelMesh);
}

void USkeleton::UpdateReferencePoseFromMesh(const USkeletalMesh* InSkelMesh)
{
	check (InSkelMesh);

	for (int32 BoneIndex=0; BoneIndex<ReferenceSkeleton.GetNum(); BoneIndex++)
	{
		// find index from ref pose array
		const int32 MeshBoneIndex = InSkelMesh->RefSkeleton.FindBoneIndex(ReferenceSkeleton.GetBoneName(BoneIndex));
		if( MeshBoneIndex != INDEX_NONE )
		{
			ReferenceSkeleton.UpdateRefPoseTransform(BoneIndex, InSkelMesh->RefSkeleton.GetRefBonePose()[MeshBoneIndex]);
		}
	}

	MarkPackageDirty();
}

bool USkeleton::RecreateBoneTree(USkeletalMesh* InSkelMesh)
{
	if( InSkelMesh )
	{
		// regenerate Guid
		RegenerateGuid();	
		BoneTree.Empty();
		ReferenceSkeleton.Empty();

		return MergeAllBonesToBoneTree(InSkelMesh);
	}

	return false;
}

bool USkeleton::MergeAllBonesToBoneTree(const USkeletalMesh* InSkelMesh)
{
	if( InSkelMesh )
	{
		TArray<int32> RequiredBoneIndices;

		// for now add all in this case. 
		RequiredBoneIndices.AddUninitialized(InSkelMesh->RefSkeleton.GetNum());
		// gather bone list
		for (int32 I=0; I<InSkelMesh->RefSkeleton.GetNum(); ++I)
		{
			RequiredBoneIndices[I] = I;
		}

		if( RequiredBoneIndices.Num() > 0 )
		{
			// merge bones to the selected skeleton
			return MergeBonesToBoneTree( InSkelMesh, RequiredBoneIndices );
		}
	}

	return false;
}

bool USkeleton::CreateReferenceSkeletonFromMesh(const USkeletalMesh* InSkeletalMesh, const TArray<int32> & RequiredRefBones)
{
	// Filter list, we only want bones that have their parents present in this array.
	TArray<int32> FilteredRequiredBones; 
	FAnimationRuntime::ExcludeBonesWithNoParents(RequiredRefBones, InSkeletalMesh->RefSkeleton, FilteredRequiredBones);

	if( FilteredRequiredBones.Num() > 0 )
	{
		const int32 NumBones = FilteredRequiredBones.Num();
		ReferenceSkeleton.Allocate(NumBones);
		BoneTree.Empty(NumBones);
		BoneTree.AddZeroed(NumBones);

		for (int32 Index=0; Index<FilteredRequiredBones.Num(); Index++)
		{
			const int32& BoneIndex = FilteredRequiredBones[Index];

			FMeshBoneInfo NewMeshBoneInfo = InSkeletalMesh->RefSkeleton.GetRefBoneInfo()[BoneIndex];
			// Fix up ParentIndex for our new Skeleton.
			if( BoneIndex == 0 )
			{
				NewMeshBoneInfo.ParentIndex = INDEX_NONE; // root
			}
			else
			{
				NewMeshBoneInfo.ParentIndex = ReferenceSkeleton.FindBoneIndex(InSkeletalMesh->RefSkeleton.GetBoneName(InSkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex)));
			}
			ReferenceSkeleton.Add(NewMeshBoneInfo, InSkeletalMesh->RefSkeleton.GetRefBonePose()[BoneIndex]);
		}

		return true;
	}

	return false;
}


bool USkeleton::MergeBonesToBoneTree(const USkeletalMesh* InSkeletalMesh, const TArray<int32> & RequiredRefBones)
{
	// see if it needs all animation data to remap - only happens when bone structure CHANGED - added
	bool bSuccess = false;
	bool bShouldHandleHierarchyChange = false;
	// clear cache data since it won't work anymore once this is done
	ClearCacheData();

	// if it's first time
	if( BoneTree.Num() == 0 )
	{
		bSuccess = CreateReferenceSkeletonFromMesh(InSkeletalMesh, RequiredRefBones);
		bShouldHandleHierarchyChange = true;
	}
	else
	{
		// can we play? - hierarchy matches
		if( IsCompatibleMesh(InSkeletalMesh) )
		{
			// Exclude bones who do not have a parent.
			TArray<int32> FilteredRequiredBones;
			FAnimationRuntime::ExcludeBonesWithNoParents(RequiredRefBones, InSkeletalMesh->RefSkeleton, FilteredRequiredBones);

			for (int32 Index=0; Index<FilteredRequiredBones.Num(); Index++)
			{
				const int32& MeshBoneIndex = FilteredRequiredBones[Index];
				const int32& SkeletonBoneIndex = ReferenceSkeleton.FindBoneIndex(InSkeletalMesh->RefSkeleton.GetBoneName(MeshBoneIndex));
				
				// Bone doesn't already exist. Add it.
				if( SkeletonBoneIndex == INDEX_NONE )
				{
					FMeshBoneInfo NewMeshBoneInfo = InSkeletalMesh->RefSkeleton.GetRefBoneInfo()[MeshBoneIndex];
					// Fix up ParentIndex for our new Skeleton.
					if( ReferenceSkeleton.GetNum() == 0 )
					{
						NewMeshBoneInfo.ParentIndex = INDEX_NONE; // root
					}
					else
					{
						NewMeshBoneInfo.ParentIndex = ReferenceSkeleton.FindBoneIndex(InSkeletalMesh->RefSkeleton.GetBoneName(InSkeletalMesh->RefSkeleton.GetParentIndex(MeshBoneIndex)));
					}

					ReferenceSkeleton.Add(NewMeshBoneInfo, InSkeletalMesh->RefSkeleton.GetRefBonePose()[MeshBoneIndex]);
					BoneTree.AddZeroed(1);
					bShouldHandleHierarchyChange = true;
				}
			}

			bSuccess = true;
		}
	}

	// if succeed
	if (bShouldHandleHierarchyChange)
	{
#if WITH_EDITOR
		HandleSkeletonHierarchyChange();
#endif
	}

	return bSuccess;
}

void USkeleton::SetBoneTranslationRetargetingMode(const int32& BoneIndex, EBoneTranslationRetargetingMode::Type NewRetargetingMode, bool bChildrenToo)
{
	BoneTree[BoneIndex].TranslationRetargetingMode = NewRetargetingMode;

	if( bChildrenToo )
	{
		// Bones are guaranteed to be sorted in increasing order. So children will be after this bone.
		const int32 NumBones = ReferenceSkeleton.GetNum();
		for(int32 ChildIndex=BoneIndex+1; ChildIndex<NumBones; ChildIndex++)
		{
			if( ReferenceSkeleton.BoneIsChildOf(ChildIndex, BoneIndex) )
			{
				BoneTree[ChildIndex].TranslationRetargetingMode = NewRetargetingMode;
			}
		}
	}
}

int32 USkeleton::GetAnimationTrackIndex(const int32& InSkeletonBoneIndex, const UAnimSequence* InAnimSeq, const bool bUseRawData)
{
	const TArray<FTrackToSkeletonMap>& TrackToSkelMap = bUseRawData ? InAnimSeq->TrackToSkeletonMapTable : InAnimSeq->CompressedTrackToSkeletonMapTable;
	if( InSkeletonBoneIndex != INDEX_NONE )
	{
		for (int32 TrackIndex = 0; TrackIndex<TrackToSkelMap.Num(); ++TrackIndex)
		{
			const FTrackToSkeletonMap& TrackToSkeleton = TrackToSkelMap[TrackIndex];
			if( TrackToSkeleton.BoneTreeIndex == InSkeletonBoneIndex )
			{
				return TrackIndex;
			}
		}
	}

	return INDEX_NONE;
}


int32 USkeleton::GetSkeletonBoneIndexFromMeshBoneIndex(const USkeletalMesh* InSkelMesh, const int32& MeshBoneIndex)
{
	check(MeshBoneIndex != INDEX_NONE);
	const int32 LinkupCacheIdx = GetMeshLinkupIndex(InSkelMesh);
	const FSkeletonToMeshLinkup& LinkupTable = LinkupCache[LinkupCacheIdx];

	return LinkupTable.MeshToSkeletonTable[MeshBoneIndex];
}


int32 USkeleton::GetMeshBoneIndexFromSkeletonBoneIndex(const USkeletalMesh* InSkelMesh, const int32& SkeletonBoneIndex)
{
	check(SkeletonBoneIndex != INDEX_NONE);
	const int32 LinkupCacheIdx = GetMeshLinkupIndex(InSkelMesh);
	const FSkeletonToMeshLinkup& LinkupTable = LinkupCache[LinkupCacheIdx];

	return LinkupTable.SkeletonToMeshTable[SkeletonBoneIndex];
}

#if WITH_EDITORONLY_DATA
void USkeleton::UpdateRetargetSource( const FName Name )
{
	FReferencePose * PoseFound = AnimRetargetSources.Find(Name);

	if (PoseFound)
	{
		USkeletalMesh * ReferenceMesh = PoseFound->ReferenceMesh;
		
		// reference mesh can be deleted after base pose is created, don't update it if it's not there. 
		if(ReferenceMesh)
		{
			const TArray<FTransform>& MeshRefPose = ReferenceMesh->RefSkeleton.GetRefBonePose();
			const TArray<FTransform>& SkeletonRefPose = GetReferenceSkeleton().GetRefBonePose();
			const TArray<FMeshBoneInfo> & SkeletonBoneInfo = GetReferenceSkeleton().GetRefBoneInfo();

			PoseFound->ReferencePose.Empty(SkeletonRefPose.Num());
			PoseFound->ReferencePose.AddUninitialized(SkeletonRefPose.Num());

			for (int32 SkeletonBoneIndex=0; SkeletonBoneIndex<SkeletonRefPose.Num(); ++SkeletonBoneIndex)
			{
				FName SkeletonBoneName = SkeletonBoneInfo[SkeletonBoneIndex].Name;
				int32 MeshBoneIndex = ReferenceMesh->RefSkeleton.FindBoneIndex(SkeletonBoneName);
				if (MeshBoneIndex != INDEX_NONE)
				{
					PoseFound->ReferencePose[SkeletonBoneIndex] = MeshRefPose[MeshBoneIndex];
				}
				else
				{
					PoseFound->ReferencePose[SkeletonBoneIndex] = FTransform::Identity;
				}
			}
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Reference Mesh for Retarget Source %s has been removed."), *Name.ToString());
		}
	}
}

void USkeleton::RefreshAllRetargetSources()
{
	for (auto Iter = AnimRetargetSources.CreateConstIterator(); Iter; ++Iter)
	{
		UpdateRetargetSource(Iter.Key());
	}
}

int32 USkeleton::GetChildBones(int32 ParentBoneIndex, TArray<int32> & Children) const
{
	Children.Empty();

	const int32 NumBones = ReferenceSkeleton.GetNum();
	for(int32 ChildIndex=ParentBoneIndex+1; ChildIndex<NumBones; ChildIndex++)
	{
		if ( ParentBoneIndex == ReferenceSkeleton.GetParentIndex(ChildIndex) )
		{
			Children.Add(ChildIndex);
		}
	}

	return Children.Num();
}

void USkeleton::CollectAnimationNotifies()
{
	// need to verify if these pose is used by anybody else
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// @Todo : remove it when we know the asset registry is updated
	// meanwhile if you remove this, this will miss the links
	//AnimationNotifies.Empty();
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AssetList, true);
#if WITH_EDITOR
	// do not clear AnimationNotifies. We can't remove old ones yet. 
	FString CurrentSkeletonName = FAssetData(this).GetExportTextName();
	for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
	{
		const FAssetData& Asset = *Iter;
		const FString SkeletonValue = Asset.GetTagValueRef<FString>(TEXT("Skeleton"));
		if (SkeletonValue == CurrentSkeletonName)
		{
			FString Value;
			if (Asset.GetTagValue(USkeleton::AnimNotifyTag, Value))
			{
				TArray<FString> NotifyList;
				Value.ParseIntoArray(NotifyList, *USkeleton::AnimNotifyTagDelimiter, true);
				for (auto NotifyIter = NotifyList.CreateConstIterator(); NotifyIter; ++NotifyIter)
				{
					FString NotifyName = *NotifyIter;
					AddNewAnimationNotify(FName(*NotifyName));
				}
			}
		}
	}
#endif
}

void USkeleton::AddNewAnimationNotify(FName NewAnimNotifyName)
{
	if (NewAnimNotifyName!=NAME_None)
	{
		AnimationNotifies.AddUnique( NewAnimNotifyName);
	}
}

USkeletalMesh* USkeleton::GetPreviewMesh(bool bFindIfNotSet)
{
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.Get();
	if(!PreviewMesh)
	{
		// if preview mesh isn't loaded, see if we have set
		FStringAssetReference PreviewMeshStringRef = PreviewSkeletalMesh.ToStringReference();
		// load it since now is the time to load
		if (!PreviewMeshStringRef.ToString().IsEmpty())
		{
			PreviewMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), NULL, *PreviewMeshStringRef.ToString(), NULL, LOAD_None, NULL));
		}
	}

	if(PreviewMesh && PreviewMesh->Skeleton != this) // fix mismatched skeleton
	{
		PreviewSkeletalMesh.Reset();
		PreviewMesh = nullptr;
	}

	// if not existing, and if bFindIfNotExisting is true, then try find one
	if(!PreviewMesh && bFindIfNotSet)
	{
		FARFilter Filter;
		Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());

		FString SkeletonString = FAssetData(this).GetExportTextName();
		Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(USkeletalMesh, Skeleton), SkeletonString);

		TArray<FAssetData> AssetList;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetAssets(Filter, AssetList);

		if(AssetList.Num() > 0)
		{
			SetPreviewMesh( Cast<USkeletalMesh>(AssetList[0].GetAsset()), false );
			// update PreviewMesh
			PreviewMesh = PreviewSkeletalMesh.Get();
		}
	}

	return PreviewMesh;
}

USkeletalMesh* USkeleton::GetPreviewMesh() const
{
	return PreviewSkeletalMesh.Get();
}

USkeletalMesh* USkeleton::GetAssetPreviewMesh(UAnimationAsset* AnimAsset) 
{
	USkeletalMesh * PreviewMesh = NULL;
	// return asset preview asset
	// if nothing is assigned, return skeleton asset
	if (AnimAsset)
	{
		PreviewMesh = AnimAsset->GetPreviewMesh();
	}

	if (!PreviewMesh)
	{
		PreviewMesh = GetPreviewMesh(false);
	}

	return PreviewMesh;
}

void USkeleton::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
	if (bMarkAsDirty)
	{
		Modify();
	}

	PreviewSkeletalMesh = PreviewMesh;
}

int32 USkeleton::ValidatePreviewAttachedObjects()
{
	int32 NumBrokenAssets = PreviewAttachedAssetContainer.ValidatePreviewAttachedObjects();

	if(NumBrokenAssets > 0)
	{
		MarkPackageDirty();
	}
	return NumBrokenAssets;
}

#if WITH_EDITOR

void USkeleton::RemoveBonesFromSkeleton( const TArray<FName>& BonesToRemove, bool bRemoveChildBones )
{
	TArray<int32> BonesRemoved = ReferenceSkeleton.RemoveBonesByName(BonesToRemove);
	if(BonesRemoved.Num() > 0)
	{
		BonesRemoved.Sort();
		for(int32 Index = BonesRemoved.Num()-1; Index >=0; --Index)
		{
			BoneTree.RemoveAt(Index);
		}
		HandleSkeletonHierarchyChange();
	}
}

void USkeleton::HandleSkeletonHierarchyChange()
{
	MarkPackageDirty();

	RegenerateGuid();

	// Clear exiting MeshLinkUp tables.
	ClearCacheData();

	// Fix up loaded animations (any animations that aren't loaded will be fixed on load)
	for(TObjectIterator<UAnimationAsset> It; It; ++It)
	{
		UAnimationAsset* CurrentAnimation = *It;
		if (CurrentAnimation->GetSkeleton() == this)
		{
			CurrentAnimation->ValidateSkeleton();
		}
	}

#if WITH_EDITORONLY_DATA
	RefreshAllRetargetSources();
#endif

	OnSkeletonHierarchyChanged.Broadcast();
}

void USkeleton::RegisterOnSkeletonHierarchyChanged(const FOnSkeletonHierarchyChanged& Delegate)
{
	OnSkeletonHierarchyChanged.Add(Delegate);
}

void USkeleton::UnregisterOnSkeletonHierarchyChanged(void* Unregister)
{
	OnSkeletonHierarchyChanged.RemoveAll(Unregister);
}

#endif

#endif // WITH_EDITORONLY_DATA

const TArray<FAnimSlotGroup>& USkeleton::GetSlotGroups() const
{
	return SlotGroups;
}

void USkeleton::BuildSlotToGroupMap(bool bInRemoveDuplicates)
{
	SlotToGroupNameMap.Empty();

	for (FAnimSlotGroup& SlotGroup : SlotGroups)
	{
		for (const FName& SlotName : SlotGroup.SlotNames)
		{
			SlotToGroupNameMap.Add(SlotName, SlotGroup.GroupName);
		}
	}

	// Use the map we've just build to rebuild the slot groups
	if(bInRemoveDuplicates)
	{
		for(FAnimSlotGroup& SlotGroup : SlotGroups)
		{
			SlotGroup.SlotNames.Empty(SlotGroup.SlotNames.Num());

			for(TPair<FName, FName>& SlotToGroupPair : SlotToGroupNameMap)
			{
				if(SlotToGroupPair.Value == SlotGroup.GroupName)
				{
					SlotGroup.SlotNames.Add(SlotToGroupPair.Key);
				}
			}
		}

	}
}

FAnimSlotGroup* USkeleton::FindAnimSlotGroup(const FName& InGroupName)
{
	return SlotGroups.FindByPredicate([&InGroupName](const FAnimSlotGroup& Item)
	{
		return Item.GroupName == InGroupName;
	});
}

bool USkeleton::ContainsSlotName(const FName& InSlotName) 
{
	return SlotToGroupNameMap.Contains(InSlotName);
}

void USkeleton::RegisterSlotNode(const FName& InSlotName)
{
	// verify the slot name exists, if not create it in the default group.
	if (!ContainsSlotName(InSlotName))
	{
		SetSlotGroupName(InSlotName, FAnimSlotGroup::DefaultGroupName);
	}
}

void USkeleton::SetSlotGroupName(const FName& InSlotName, const FName& InGroupName)
{
// See if Slot already exists and belongs to a group.
	const FName* FoundGroupNamePtr = SlotToGroupNameMap.Find(InSlotName);

	// If slot exists, but is not in the right group, remove it from there
	if (FoundGroupNamePtr && ((*FoundGroupNamePtr) != InGroupName))
	{
		FAnimSlotGroup* OldSlotGroupPtr = FindAnimSlotGroup(*FoundGroupNamePtr);
		if (OldSlotGroupPtr)
		{
			OldSlotGroupPtr->SlotNames.RemoveSingleSwap(InSlotName);
		}
	}

	// Add the slot to the right group if it's not
	if ((FoundGroupNamePtr == NULL) || (*FoundGroupNamePtr != InGroupName))
	{
		// If the SlotGroup does not exist, create it.
		FAnimSlotGroup* SlotGroupPtr = FindAnimSlotGroup(InGroupName);
		if (SlotGroupPtr == NULL)
		{
			SlotGroups.AddZeroed(1);
			SlotGroupPtr = &SlotGroups.Last();
			SlotGroupPtr->GroupName = InGroupName;
		}
		// Add Slot to group.
		SlotGroupPtr->SlotNames.Add(InSlotName);
		// Keep our TMap up to date.
		SlotToGroupNameMap.Add(InSlotName, InGroupName);
	}
}

bool USkeleton::AddSlotGroupName(const FName& InNewGroupName)
{
	FAnimSlotGroup* ExistingSlotGroupPtr = FindAnimSlotGroup(InNewGroupName);
	if (ExistingSlotGroupPtr == NULL)
	{
		// if not found, create a new one.
		SlotGroups.AddZeroed(1);
		ExistingSlotGroupPtr = &SlotGroups.Last();
		ExistingSlotGroupPtr->GroupName = InNewGroupName;
		return true;
	}

	return false;
}

FName USkeleton::GetSlotGroupName(const FName& InSlotName) const
{
	const FName* FoundGroupNamePtr = SlotToGroupNameMap.Find(InSlotName);
	if (FoundGroupNamePtr)
	{
		return *FoundGroupNamePtr;
	}

	// If Group name cannot be found, use DefaultSlotGroupName.
	return FAnimSlotGroup::DefaultGroupName;
}

void USkeleton::RemoveSlotName(const FName& InSlotName)
{
	FName GroupName = GetSlotGroupName(InSlotName);
	
	if(SlotToGroupNameMap.Remove(InSlotName) > 0)
	{
		FAnimSlotGroup* SlotGroup = FindAnimSlotGroup(GroupName);
		SlotGroup->SlotNames.Remove(InSlotName);
	}
}

void USkeleton::RemoveSlotGroup(const FName& InSlotGroupName)
{
	FAnimSlotGroup* SlotGroup = FindAnimSlotGroup(InSlotGroupName);
	// Remove slot mappings
	for(const FName& SlotName : SlotGroup->SlotNames)
	{
		SlotToGroupNameMap.Remove(SlotName);
	}

	// Remove group
	SlotGroups.RemoveAll([&InSlotGroupName](const FAnimSlotGroup& Item)
	{
		return Item.GroupName == InSlotGroupName;
	});
}

void USkeleton::RenameSlotName(const FName& OldName, const FName& NewName)
{
	// Can't rename a name that doesn't exist
	check(ContainsSlotName(OldName))

	FName GroupName = GetSlotGroupName(OldName);
	RemoveSlotName(OldName);
	SetSlotGroupName(NewName, GroupName);
}

#if WITH_EDITOR

bool USkeleton::AddSmartNameAndModify(FName ContainerName, FName NewDisplayName, FSmartName& NewName)
{
	bool Successful = false;
	FSmartNameMapping* RequestedMapping = GetOrAddSmartNameContainer(ContainerName);
	if (RequestedMapping)
	{
		if (RequestedMapping->FindOrAddSmartName(NewDisplayName, NewName))
		{
			Modify(true);
			Successful = true;
			CacheAnimCurveMappingNameUids();
		}
	}
	return Successful;
}
bool USkeleton::RenameSmartnameAndModify(FName ContainerName, FSmartNameMapping::UID Uid, FName NewName)
{
	bool Successful = false;
	FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		FName CurrentName;
		RequestedMapping->GetName(Uid, CurrentName);
		if (CurrentName != NewName)
		{
			Modify();
			Successful = RequestedMapping->Rename(Uid, NewName);
			CacheAnimCurveMappingNameUids();
		}
	}
	return Successful;
}

void USkeleton::RemoveSmartnameAndModify(FName ContainerName, FSmartNameMapping::UID Uid)
{
	FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		Modify();
		if (RequestedMapping->Remove(Uid))
		{
			CacheAnimCurveMappingNameUids();
		}
	}
}

void USkeleton::RemoveSmartnamesAndModify(FName ContainerName, const TArray<FSmartNameMapping::UID>& Uids)
{
	FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		bool bModified = false;
		for (const FSmartNameMapping::UID& UID : Uids)
		{
			if (RequestedMapping->Exists(UID))
			{
				RequestedMapping->Remove(UID);
				bModified = true;
			}
		}

		if (bModified)
		{
			Modify();
			CacheAnimCurveMappingNameUids();
		}
	}
}
#endif // WITH_EDITOR

bool USkeleton::GetSmartNameByUID(const FName& ContainerName, FSmartNameMapping::UID UID, FSmartName& OutSmartName)
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		return (RequestedMapping->FindSmartNameByUID(UID, OutSmartName));
	}

	return false;
}

bool USkeleton::GetSmartNameByName(const FName& ContainerName, const FName& InName, FSmartName& OutSmartName)
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		return (RequestedMapping->FindSmartName(InName, OutSmartName));
	}

	return false;
}

FSmartNameMapping::UID USkeleton::GetUIDByName(const FName& ContainerName, const FName& Name)
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		if (const FSmartNameMapping::UID* UID = RequestedMapping->FindUID(Name))
		{
			if (UID)
			{
				return *UID;
			}
		}
	}

	return FSmartNameMapping::MaxUID;
}

// this does very simple thing.
// @todo: this for now prioritize FName because that is main issue right now
// @todo: @fixme: this has to be fixed when we have GUID
void USkeleton::VerifySmartName(const FName& ContainerName, FSmartName& InOutSmartName)
{
	VerifySmartNameInternal(ContainerName, InOutSmartName);
	if (ContainerName == USkeleton::AnimCurveMappingName)
	{
		CacheAnimCurveMappingNameUids();
	}
}


bool USkeleton::FillSmartNameByDisplayName(FSmartNameMapping* Mapping, const FName& DisplayName, FSmartName& OutSmartName)
{
	FSmartName SkeletonName;
	if (Mapping->FindSmartName(DisplayName, SkeletonName))
	{
		OutSmartName.DisplayName = DisplayName;

#if WITH_EDITORONLY_DATA
		// if same guid, this is same
		if (SkeletonName.Guid == OutSmartName.Guid)
		{
			OutSmartName.UID = SkeletonName.UID;
			return true;
		}
		// else, take Skeleton Guid, we don't allow same name with different Guid
		else
		{
			OutSmartName.Guid = SkeletonName.Guid;
			OutSmartName.UID = SkeletonName.UID;
			return true;
		}
#else
		// if not editor, we assume name is always correct
		OutSmartName.UID = SkeletonName.UID;
		return true;
#endif // WITH_EDITORONLY_DATA
	}

	return false;
}
//@todo: this does prioritize name over UID since UID doesn't work well
// until we have GUID, we'll prioritize name over UID
bool USkeleton::VerifySmartNameInternal(const FName&  ContainerName, FSmartName& InOutSmartName)
{
	FSmartNameMapping* Mapping = GetOrAddSmartNameContainer(ContainerName);
	if (Mapping != nullptr)
	{
		// make a copy just in case we change by accident
		FName DisplayName = InOutSmartName.DisplayName;

#if WITH_EDITOR
		// if I find the name, fill up the data
		if (FillSmartNameByDisplayName(Mapping, DisplayName, InOutSmartName) == false)
		{
			// look for same guid, the name might have been changed
			if (Mapping->GetNameByGuid(InOutSmartName.Guid, DisplayName))
			{
				ensureAlways(FillSmartNameByDisplayName(Mapping, DisplayName, InOutSmartName));
			}
			else if (InOutSmartName.IsValid())
			{
				// this is only case where we add new one
				ensureAlways(Mapping->AddSmartName(InOutSmartName));
				Modify();
				return true;
			}
			else
			{
				// this is only case where we add new one
				Mapping->FindOrAddSmartName(InOutSmartName.DisplayName, InOutSmartName);
				return true;
			}
		}
#else
		// it always should have name
		ensureAlways(FillSmartNameByDisplayName(Mapping, DisplayName, InOutSmartName));
#endif // WITH_EDITOR
	}

	return false;
}

void USkeleton::VerifySmartNames(const FName&  ContainerName, TArray<FSmartName>& InOutSmartNames)
{
	bool bRefreshCache = false;

	for(auto& SmartName: InOutSmartNames)
	{
		VerifySmartNameInternal(ContainerName, SmartName);
	}

	if (ContainerName == USkeleton::AnimCurveMappingName)
	{
		CacheAnimCurveMappingNameUids();
	}
}

FSmartNameMapping* USkeleton::GetOrAddSmartNameContainer(const FName& ContainerName)
{
	FSmartNameMapping* Mapping = SmartNames.GetContainerInternal(ContainerName);
	if (Mapping == nullptr)
	{
		Modify();
		CacheAnimCurveMappingNameUids();
		SmartNames.AddContainer(ContainerName);
		Mapping = SmartNames.GetContainerInternal(ContainerName);		
	}

	return Mapping;
}

const FSmartNameMapping* USkeleton::GetSmartNameContainer(const FName& ContainerName) const
{
	return SmartNames.GetContainer(ContainerName);
}

void USkeleton::RegenerateGuid()
{
	Guid = FGuid::NewGuid();
	check(Guid.IsValid());
}

bool USkeleton::RenameSmartName(FName ContainerName, const FSmartNameMapping::UID& Uid, FName NewName)
{
	FSmartNameMapping* Mapping = SmartNames.GetContainerInternal(ContainerName);
	if (Mapping->Exists(Uid))
	{
		Modify();
		CacheAnimCurveMappingNameUids();
		Mapping->Rename(Uid, NewName);
		return true;
	}

	return false;
}

void USkeleton::CacheAnimCurveMappingNameUids()
{
	const FSmartNameMapping* Mapping = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
	if (Mapping != nullptr)
	{
		Mapping->FillUidArray(CachedAnimCurveMappingNameUids);
	}
}

const TArray<FSmartNameMapping::UID>& USkeleton::GetCachedAnimCurveMappingNameUids()
{
	return CachedAnimCurveMappingNameUids;
}

#if WITH_EDITOR
void USkeleton::SetRigConfig(URig * Rig)
{
	if (RigConfig.Rig != Rig)
	{
		RigConfig.Rig = Rig;
		RigConfig.BoneMappingTable.Empty();

		if (Rig)
		{
			const FReferenceSkeleton& RefSkeleton = GetReferenceSkeleton();
			const TArray<FNode> & Nodes = Rig->GetNodes();
			// now add bone mapping table
			for (auto Node: Nodes)
			{
				// if find same bone, use that bone for mapping
				if (RefSkeleton.FindBoneIndex(Node.Name) != INDEX_NONE)
				{
					RigConfig.BoneMappingTable.Add(FNameMapping(Node.Name, Node.Name));
				}
				else
				{
					RigConfig.BoneMappingTable.Add(FNameMapping(Node.Name));
				}
			}
		}
	}
}

int32 USkeleton::FindRigBoneMapping(const FName& NodeName) const
{
	int32 Index=0;
	for(const auto & NameMap : RigConfig.BoneMappingTable)
	{
		if(NameMap.NodeName == NodeName)
		{
			return Index;
		}

		++Index;
	}

	return INDEX_NONE;
}

FName USkeleton::GetRigBoneMapping(const FName& NodeName) const
{
	int32 Index = FindRigBoneMapping(NodeName);

	if (Index != INDEX_NONE)
	{
		return RigConfig.BoneMappingTable[Index].BoneName;
	}

	return NAME_None;
}

FName USkeleton::GetRigNodeNameFromBoneName(const FName& BoneName) const
{
	for(const auto & NameMap : RigConfig.BoneMappingTable)
	{
		if(NameMap.BoneName == BoneName)
		{
			return NameMap.NodeName;
		}
	}

	return NAME_None;
}

int32 USkeleton::GetMappedValidNodes(TArray<FName> &OutValidNodeNames)
{
	OutValidNodeNames.Empty();

	for (auto Entry : RigConfig.BoneMappingTable)
	{
		if (Entry.BoneName != NAME_None)
		{
			OutValidNodeNames.Add(Entry.NodeName);
		}
	}

	return OutValidNodeNames.Num();
}

bool USkeleton::SetRigBoneMapping(const FName& NodeName, FName BoneName)
{
	// make sure it's valid
	int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);

	// @todo we need to have validation phase where you can't set same bone for different nodes
	// but it might be annoying to do that right now since the tool is ugly
	// so for now it lets you set everything, but in the future
	// we'll have to add verification
	if ( BoneIndex == INDEX_NONE )
	{
		BoneName = NAME_None;
	}

	int32 Index = FindRigBoneMapping(NodeName);

	if(Index != INDEX_NONE)
	{
		RigConfig.BoneMappingTable[Index].BoneName = BoneName;
		return true;
	}

	return false;
}

void USkeleton::RefreshRigConfig()
{
	if (RigConfig.Rig != NULL)
	{
		if (RigConfig.BoneMappingTable.Num() > 0)
		{
			// verify if any missing bones or anything
			// remove if removed
			for ( int32 TableId=0; TableId<RigConfig.BoneMappingTable.Num(); ++TableId )
			{
				auto & BoneMapping = RigConfig.BoneMappingTable[TableId];

				if ( RigConfig.Rig->FindNode(BoneMapping.NodeName) == INDEX_NONE)
				{
					// if not contains, remove it
					RigConfig.BoneMappingTable.RemoveAt(TableId);
					--TableId;
				}
			}

			// if the count doesn't match, there is missing nodes. 
			if (RigConfig.Rig->GetNodeNum() != RigConfig.BoneMappingTable.Num())
			{
				int32 NodeNum = RigConfig.Rig->GetNodeNum();
				for(int32 NodeId=0; NodeId<NodeNum; ++NodeId)
				{
					const auto* Node = RigConfig.Rig->GetNode(NodeId);

					if (FindRigBoneMapping(Node->Name) == INDEX_NONE)
					{
						RigConfig.BoneMappingTable.Add(FNameMapping(Node->Name));
					}
				}
			}
		}
	}
}

URig * USkeleton::GetRig() const
{
	return RigConfig.Rig;
}

void USkeleton::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	FString RigFullName = (RigConfig.Rig)? RigConfig.Rig->GetFullName() : TEXT("");

	OutTags.Add(FAssetRegistryTag(USkeleton::RigTag, RigFullName, FAssetRegistryTag::TT_Hidden));
}

UBlendProfile* USkeleton::CreateNewBlendProfile(const FName& InProfileName)
{
	Modify();
	UBlendProfile* NewProfile = NewObject<UBlendProfile>(this, InProfileName, RF_Public);
	BlendProfiles.Add(NewProfile);

	return NewProfile;
}

UBlendProfile* USkeleton::GetBlendProfile(const FName& InProfileName)
{
	UBlendProfile** FoundProfile = BlendProfiles.FindByPredicate([InProfileName](const UBlendProfile* Profile)
	{
		return Profile->GetName() == InProfileName.ToString();
	});

	if(FoundProfile)
	{
		return *FoundProfile;
	}
	return nullptr;
}

#endif //WITH_EDITOR
#undef LOCTEXT_NAMESPACE 