// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "Animation/AnimInstanceProxy.h"

#define LEADERSCORE_ALWAYSLEADER  	2.f
#define LEADERSCORE_MONTAGE			3.f
//////////////////////////////////////////////////////////////////////////
// FAnimGroupInstance

void FAnimGroupInstance::TestTickRecordForLeadership(EAnimGroupRole::Type MembershipType)
{
	// always set leader score if you have potential to be leader
	// that way if the top leader fails, we'll continue to search next available leader
	int32 TestIndex = ActivePlayers.Num() - 1;
	FAnimTickRecord& Candidate = ActivePlayers[TestIndex];

	switch (MembershipType)
	{
	case EAnimGroupRole::CanBeLeader:
	case EAnimGroupRole::TransitionLeader:
		Candidate.LeaderScore = Candidate.EffectiveBlendWeight;
		break;
	case EAnimGroupRole::AlwaysLeader:
		// Always set the leader index
		Candidate.LeaderScore = LEADERSCORE_ALWAYSLEADER;
		break;
	default:
	case EAnimGroupRole::AlwaysFollower:
		// Never set the leader index; the actual tick code will handle the case of no leader by using the first element in the array
		break;
	}
}

void FAnimGroupInstance::TestMontageTickRecordForLeadership()
{
	int32 TestIndex = ActivePlayers.Num() - 1;
	ensure(TestIndex <= 1);
	FAnimTickRecord& Candidate = ActivePlayers[TestIndex];

	// if the candidate has higher weight
	if (Candidate.EffectiveBlendWeight > MontageLeaderWeight)
	{
		// if this is going to be leader, I'll clean ActivePlayers because we don't sync multi montages
		const int32 LastIndex = TestIndex - 1;
		if (LastIndex >= 0)
		{
			ActivePlayers.RemoveAt(TestIndex - 1, 1);
		}

		// at this time, it should only have one
		ensure(ActivePlayers.Num() == 1);

		// then override
		// @note : leader weight doesn't applied WITHIN montages
		// we still only contain one montage at a time, if this montage fails, next candidate will get the chance, not next weight montage
		MontageLeaderWeight = Candidate.EffectiveBlendWeight;
		Candidate.LeaderScore = LEADERSCORE_MONTAGE;
	}
}

void FAnimGroupInstance::Finalize(const FAnimGroupInstance* PreviousGroup)
{
	if (!PreviousGroup || PreviousGroup->GroupLeaderIndex != GroupLeaderIndex || ValidMarkers != PreviousGroup->ValidMarkers
		|| (PreviousGroup->MontageLeaderWeight > 0.f && MontageLeaderWeight == 0.f/*if montage disappears, we should reset as well*/))
	{
		UE_LOG(LogAnimMarkerSync, Log, TEXT("Resetting Marker Sync Groups"));

		for (int32 RecordIndex = GroupLeaderIndex + 1; RecordIndex < ActivePlayers.Num(); ++RecordIndex)
		{
			ActivePlayers[RecordIndex].MarkerTickRecord->Reset();
		}
	}
}

void FAnimGroupInstance::Prepare(const FAnimGroupInstance* PreviousGroup)
{
	ActivePlayers.Sort();

	TArray<FName>* MarkerNames = ActivePlayers[0].SourceAsset->GetUniqueMarkerNames();
	if (MarkerNames)
	{
		// Group leader has markers, off to a good start
		ValidMarkers = *MarkerNames;
		ActivePlayers[0].bCanUseMarkerSync = true;
		bCanUseMarkerSync = true;

		int32 PlayerIndexToResetMarkers = INDEX_NONE;

		//filter markers based on what exists in the other animations
		for ( int32 ActivePlayerIndex = 0; ActivePlayerIndex < ActivePlayers.Num(); ++ActivePlayerIndex )
		{
			FAnimTickRecord& Candidate = ActivePlayers[ActivePlayerIndex];

			if (PreviousGroup)
			{
				bool bCandidateFound = false;
				for (const FAnimTickRecord& PrevRecord : PreviousGroup->ActivePlayers)
				{
					if (PrevRecord.MarkerTickRecord == Candidate.MarkerTickRecord)
					{
						// Found previous record for "us"
						if (PrevRecord.SourceAsset != Candidate.SourceAsset)
						{
							Candidate.MarkerTickRecord->Reset(); // Changed animation, clear our cached data
						}
						bCandidateFound = true;
						break;
					}
				}
				if (!bCandidateFound)
				{
					Candidate.MarkerTickRecord->Reset(); // we weren't active last frame, reset
				}
			}

			if (ActivePlayerIndex != 0 && ValidMarkers.Num() > 0)
			{
				TArray<FName>* PlayerMarkerNames = Candidate.SourceAsset->GetUniqueMarkerNames();
				if ( PlayerMarkerNames ) // Let anims with no markers set use length scaling sync
				{
					Candidate.bCanUseMarkerSync = true;
					for ( int32 ValidMarkerIndex = ValidMarkers.Num() - 1; ValidMarkerIndex >= 0; --ValidMarkerIndex )
					{
						FName& MarkerName = ValidMarkers[ValidMarkerIndex];
						if ( !PlayerMarkerNames->Contains(MarkerName) )
						{
							ValidMarkers.RemoveAtSwap(ValidMarkerIndex, 1, false);

							PlayerIndexToResetMarkers = ActivePlayerIndex;
						}
					}
				}
			}
		}

		bCanUseMarkerSync = ValidMarkers.Num() > 0;

		ValidMarkers.Sort();

		// if we have list of markers that needs to rest
		if (PlayerIndexToResetMarkers > 0)
		{
			// if you removed valid markers, we also should reset previous candidate marker tick record
			// because they might contain previous marker sets
			for (int32 InternalActivePlayerIndex = 0; InternalActivePlayerIndex < PlayerIndexToResetMarkers; ++InternalActivePlayerIndex)
			{
				ActivePlayers[InternalActivePlayerIndex].MarkerTickRecord->Reset();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UAnimationAsset

UAnimationAsset::UAnimationAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimationAsset::PostLoad()
{
	Super::PostLoad();

	// Load skeleton, to make sure anything accessing from PostLoad
	// skeleton is ready
	if (Skeleton)
	{
		Skeleton ->ConditionalPostLoad();
	}

	ValidateSkeleton();

	check( Skeleton==NULL || SkeletonGuid.IsValid() );
}

void UAnimationAsset::ResetSkeleton(USkeleton* NewSkeleton)
{
// @TODO LH, I'd like this to work outside of editor, but that requires unlocking track names data in game
#if WITH_EDITOR
	Skeleton = NULL;
	ReplaceSkeleton(NewSkeleton);
#endif
}

void UAnimationAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		Ar << SkeletonGuid;
	}
}

void UAnimationAsset::SetSkeleton(USkeleton* NewSkeleton)
{
	if (NewSkeleton && NewSkeleton != Skeleton)
	{
		Skeleton = NewSkeleton;
		SkeletonGuid = NewSkeleton->GetGuid();
	}
}

void UAnimationAsset::TickAssetPlayerInstance(FAnimTickRecord& Instance, class UAnimInstance* AnimInstance, FAnimAssetTickContext& Context) const
{ 
	// @todo: remove after deprecation
	// Forward to non-deprecated function
	TickAssetPlayer(Instance, AnimInstance->NotifyQueue, Context); 
}

#if WITH_EDITOR
bool UAnimationAsset::ReplaceSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces/*=false*/)
{
	// if it's not same 
	if (NewSkeleton != Skeleton)
	{
		// get all sequences that need to change
		TArray<UAnimSequence*> AnimSeqsToReplace;
		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(this))
		{
			AnimSeqsToReplace.AddUnique(AnimSequence);
		}
		if (GetAllAnimationSequencesReferred(AnimSeqsToReplace))
		{
			for (auto Iter = AnimSeqsToReplace.CreateIterator(); Iter; ++Iter)
			{
				UAnimSequence* AnimSeq = *Iter;
				if (AnimSeq && AnimSeq->Skeleton != NewSkeleton)
				{
					AnimSeq->RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);
				}
			}
		}

		SetSkeleton(NewSkeleton);

		PostEditChange();
		MarkPackageDirty();
		return true;
	}

	return false;
}

bool UAnimationAsset::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences)
{
	return false;
}

void UAnimationAsset::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
}

USkeletalMesh* UAnimationAsset::GetPreviewMesh() 
{
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.Get();
	if(!PreviewMesh)
	{
		// if preview mesh isn't loaded, see if we have set
		FStringAssetReference PreviewMeshStringRef = PreviewSkeletalMesh.ToStringReference();
		// load it since now is the time to load
		if(!PreviewMeshStringRef.ToString().IsEmpty())
		{
			PreviewMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), NULL, *PreviewMeshStringRef.ToString(), NULL, LOAD_None, NULL));
			// if somehow skeleton changes, just nullify it. 
			if (PreviewMesh && PreviewMesh->Skeleton != Skeleton)
			{
				PreviewMesh = NULL;
				SetPreviewMesh(NULL);
			}
		}
	}

	return PreviewMesh;
}

void UAnimationAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh)
{
	Modify();
	PreviewSkeletalMesh = PreviewMesh;
}
#endif

void UAnimationAsset::ValidateSkeleton()
{
	if (Skeleton && Skeleton->GetGuid() != SkeletonGuid)
	{
		// reset Skeleton
		ResetSkeleton(Skeleton);
	}
}

//////////////////////////////////////////////////////////////////////////
// FBoneContainer

FBoneContainer::FBoneContainer()
: Asset(NULL)
, AssetSkeletalMesh(NULL)
, AssetSkeleton(NULL)
, RefSkeleton(NULL)
, bDisableRetargeting(false)
, bUseRAWData(false)
, bUseSourceData(false)
{
	BoneIndicesArray.Empty();
	BoneSwitchArray.Empty();
	SkeletonToPoseBoneIndexArray.Empty();
	PoseToSkeletonBoneIndexArray.Empty();
}

FBoneContainer::FBoneContainer(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, UObject& InAsset)
: BoneIndicesArray(InRequiredBoneIndexArray)
, Asset(&InAsset)
, AssetSkeletalMesh(NULL)
, AssetSkeleton(NULL)
, RefSkeleton(NULL)
, bDisableRetargeting(false)
, bUseRAWData(false)
, bUseSourceData(false)
{
	Initialize();
}

void FBoneContainer::InitializeTo(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, UObject& InAsset)
{
	BoneIndicesArray = InRequiredBoneIndexArray;
	Asset = &InAsset;

	Initialize();
}

void FBoneContainer::Initialize()
{
	RefSkeleton = NULL;
	UObject* AssetObj = Asset.Get();
	USkeletalMesh* AssetSkeletalMeshObj = Cast<USkeletalMesh>(AssetObj);
	USkeleton* AssetSkeletonObj = nullptr;

	if (AssetSkeletalMeshObj)
	{
		RefSkeleton = &AssetSkeletalMeshObj->RefSkeleton;
		AssetSkeletonObj = AssetSkeletalMeshObj->Skeleton;
	}
	else
	{
		AssetSkeletonObj = Cast<USkeleton>(AssetObj);
		if (AssetSkeletonObj)
		{
			RefSkeleton = &AssetSkeletonObj->GetReferenceSkeleton();
		}
	}

	// Only supports SkeletalMeshes or Skeletons.
	check( AssetSkeletalMeshObj || AssetSkeletonObj );
	// Skeleton should always be there.
	checkf( AssetSkeletonObj, TEXT("%s missing skeleton"), *GetNameSafe(AssetSkeletalMeshObj));
	check( RefSkeleton );

	AssetSkeleton = AssetSkeletonObj;
	AssetSkeletalMesh = AssetSkeletalMeshObj;

	// Take biggest amount of bones between SkeletalMesh and Skeleton for BoneSwitchArray.
	// SkeletalMesh can have less, but AnimSequences tracks will map to Skeleton which can have more.
	const int32 MaxBones = AssetSkeletonObj ? FMath::Max<int32>(RefSkeleton->GetNum(), AssetSkeletonObj->GetReferenceSkeleton().GetNum()) : RefSkeleton->GetNum();

	// Initialize BoneSwitchArray.
	BoneSwitchArray.Init(false, MaxBones);
	const int32 NumRequiredBones = BoneIndicesArray.Num();
	for(int32 Index=0; Index<NumRequiredBones; Index++)
	{
		const FBoneIndexType BoneIndex = BoneIndicesArray[Index];
		checkSlow( BoneIndex < MaxBones );
		BoneSwitchArray[BoneIndex] = true;
	}

	// Clear remapping table
	SkeletonToPoseBoneIndexArray.Empty();

	// Cache our mapping tables
	// Here we create look up tables between our target asset and its USkeleton's refpose.
	// Most times our Target is a SkeletalMesh
	if (AssetSkeletalMeshObj)
	{
		RemapFromSkelMesh(*AssetSkeletalMeshObj, *AssetSkeletonObj);
	}
	// But we also support a Skeleton's RefPose.
	else
	{
		// Right now we only support a single Skeleton. Skeleton hierarchy coming soon!
		RemapFromSkeleton(*AssetSkeletonObj);
	}

	//Set up compact pose data
	int32 NumReqBones = BoneIndicesArray.Num();
	CompactPoseParentBones.Empty(NumReqBones);
	
	CompactPoseRefPoseBones.Empty(NumReqBones);
	CompactPoseRefPoseBones.AddUninitialized(NumReqBones);
	
	CompactPoseToSkeletonIndex.Empty(NumReqBones);
	CompactPoseToSkeletonIndex.AddUninitialized(NumReqBones);
	
	SkeletonToCompactPose.Empty(SkeletonToPoseBoneIndexArray.Num());

	const TArray<FTransform>& RefPoseArray = RefSkeleton->GetRefBonePose();

	for (int32 CompactBoneIndex = 0; CompactBoneIndex < NumReqBones; ++CompactBoneIndex)
	{
		FBoneIndexType MeshPoseIndex = BoneIndicesArray[CompactBoneIndex];

		//Parent Bone
		const int32 ParentIndex = GetParentBoneIndex(MeshPoseIndex);
		const int32 CompactParentIndex = ParentIndex == INDEX_NONE ? INDEX_NONE : BoneIndicesArray.IndexOfByKey(ParentIndex);

		CompactPoseParentBones.Add(FCompactPoseBoneIndex(CompactParentIndex));

		//Ref Pose
		CompactPoseRefPoseBones[CompactBoneIndex] = RefPoseArray[MeshPoseIndex];

		CompactPoseToSkeletonIndex[CompactBoneIndex] = PoseToSkeletonBoneIndexArray[MeshPoseIndex];
	}

	for (int32 SkeletonBoneIndex = 0; SkeletonBoneIndex < SkeletonToPoseBoneIndexArray.Num(); ++SkeletonBoneIndex)
	{
		int32 PoseBoneIndex = SkeletonToPoseBoneIndexArray[SkeletonBoneIndex];
		int32 CompactIndex  = (PoseBoneIndex != INDEX_NONE) ? (BoneIndicesArray.IndexOfByKey(PoseBoneIndex)) : INDEX_NONE;
		SkeletonToCompactPose.Add(FCompactPoseBoneIndex(CompactIndex));
	}
}

int32 FBoneContainer::GetPoseBoneIndexForBoneName(const FName& BoneName) const
{
	checkSlow( IsValid() );
	return RefSkeleton->FindBoneIndex(BoneName);
}

int32 FBoneContainer::GetParentBoneIndex(const int32& BoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow(BoneIndex != INDEX_NONE);
	return RefSkeleton->GetParentIndex(BoneIndex);
}

FCompactPoseBoneIndex FBoneContainer::GetParentBoneIndex(const FCompactPoseBoneIndex& BoneIndex) const
{
	checkSlow(IsValid());
	checkSlow(BoneIndex != INDEX_NONE);
	return CompactPoseParentBones[BoneIndex.GetInt()];
}

int32 FBoneContainer::GetDepthBetweenBones(const int32& BoneIndex, const int32& ParentBoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow( BoneIndex != INDEX_NONE );
	return RefSkeleton->GetDepthBetweenBones(BoneIndex, ParentBoneIndex);
}

bool FBoneContainer::BoneIsChildOf(const int32& BoneIndex, const int32& ParentBoneIndex) const
{
	checkSlow( IsValid() );
	checkSlow( (BoneIndex != INDEX_NONE) && (ParentBoneIndex != INDEX_NONE) );
	return RefSkeleton->BoneIsChildOf(BoneIndex, ParentBoneIndex);
}

void FBoneContainer::RemapFromSkelMesh(USkeletalMesh const & SourceSkeletalMesh, USkeleton& TargetSkeleton)
{
	int32 const SkelMeshLinkupIndex = TargetSkeleton.GetMeshLinkupIndex(&SourceSkeletalMesh);
	check(SkelMeshLinkupIndex != INDEX_NONE);

	FSkeletonToMeshLinkup const & LinkupTable = TargetSkeleton.LinkupCache[SkelMeshLinkupIndex];

	// Copy LinkupTable arrays for now.
	// @laurent - Long term goal is to trim that down based on LOD, so we can get rid of the BoneIndicesArray and branch cost of testing if PoseBoneIndex is in that required bone index array.
	SkeletonToPoseBoneIndexArray = LinkupTable.SkeletonToMeshTable;
	PoseToSkeletonBoneIndexArray = LinkupTable.MeshToSkeletonTable;
}

void FBoneContainer::RemapFromSkeleton(USkeleton const & SourceSkeleton)
{
	// Map SkeletonBoneIndex to the SkeletalMesh Bone Index, taking into account the required bone index array.
	SkeletonToPoseBoneIndexArray.Init(INDEX_NONE, SourceSkeleton.GetRefLocalPoses().Num());
	for(int32 Index=0; Index<BoneIndicesArray.Num(); Index++)
	{
		int32 const & PoseBoneIndex = BoneIndicesArray[Index];
		SkeletonToPoseBoneIndexArray[PoseBoneIndex] = PoseBoneIndex;
	}

	// Skeleton to Skeleton mapping...
	PoseToSkeletonBoneIndexArray = SkeletonToPoseBoneIndexArray;
}

void FBlendSampleData::NormalizeDataWeight(TArray<FBlendSampleData>& SampleDataList)
{
	float TotalSum = 0.f;

	check(SampleDataList.Num() > 0);
	int32 NumBones = SampleDataList[0].PerBoneBlendData.Num();

	TArray<float> PerBoneTotalSums;
	PerBoneTotalSums.AddZeroed(NumBones);

	for(int32 I = 0; I < SampleDataList.Num(); ++I)
	{
		checkf(SampleDataList[I].PerBoneBlendData.Num() == NumBones, TEXT("Attempted to normalise a blend sample list, but the samples have differing numbers of bones."));

		TotalSum += SampleDataList[I].GetWeight();

		if(SampleDataList[I].PerBoneBlendData.Num() > 0)
		{
			// now interpolate the per bone weights
			for(int32 Iter = 0; Iter<SampleDataList[I].PerBoneBlendData.Num(); ++Iter)
			{
				PerBoneTotalSums[Iter] += SampleDataList[I].PerBoneBlendData[Iter];
			}
		}
	}

	if(ensure(TotalSum > ZERO_ANIMWEIGHT_THRESH))
	{
		for(int32 I = 0; I < SampleDataList.Num(); ++I)
		{
			if(FMath::Abs<float>(TotalSum - 1.f) > ZERO_ANIMWEIGHT_THRESH)
			{
				SampleDataList[I].TotalWeight /= TotalSum;
			}

			// now interpolate the per bone weights
			for(int32 Iter = 0; Iter < SampleDataList[I].PerBoneBlendData.Num(); ++Iter)
			{
				if(FMath::Abs<float>(PerBoneTotalSums[Iter] - 1.f) > ZERO_ANIMWEIGHT_THRESH)
				{
					SampleDataList[I].PerBoneBlendData[Iter] /= PerBoneTotalSums[Iter];
				}
			}
		}
	}
}
