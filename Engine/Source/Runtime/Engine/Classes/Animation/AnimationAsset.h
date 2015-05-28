// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Abstract base class of animation assets that can be played back and evaluated to produce a pose.
 *
 */

#pragma once

#include "SkeletalMeshTypes.h"
#include "AnimInterpFilter.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "AnimationAsset.generated.h"

/** Transform definition */
USTRUCT(BlueprintType)
struct FBlendSampleData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 SampleDataIndex;

	UPROPERTY()
	float TotalWeight;

	UPROPERTY()
	float Time;

	// transient perbone interpolation data
	TArray<float> PerBoneBlendData;

	FBlendSampleData()
		:	SampleDataIndex(0)
		,	TotalWeight(0.f)
		,	Time(0.f)
	{}
	FBlendSampleData(int32 Index)
		:	SampleDataIndex(Index)
		,	TotalWeight(0.f)
		,	Time(0.f)
	{}
	bool operator==( const FBlendSampleData& Other ) const 
	{
		// if same position, it's same point
		return (Other.SampleDataIndex== SampleDataIndex);
	}
	void AddWeight(float Weight)
	{
		TotalWeight += Weight;
	}
	float GetWeight() const
	{
		return FMath::Clamp<float>(TotalWeight, 0.f, 1.f);
	}
};

USTRUCT()
struct FBlendFilter
{
	GENERATED_USTRUCT_BODY()

	FFIRFilterTimeBased FilterPerAxis[3];

	FBlendFilter()
	{
	}

	FVector GetFilterLastOutput()
	{
		return FVector (FilterPerAxis[0].LastOutput, FilterPerAxis[1].LastOutput, FilterPerAxis[2].LastOutput);
	}
};

// Root Bone Lock options when extracting Root Motion
UENUM()
namespace ERootMotionRootLock
{
	enum Type
	{
		// Use reference pose root bone position
		RefPose,

		// Use root bone position on first frame of animation.
		AnimFirstFrame,

		// FTransform::Identity
		Zero
	};
}

UENUM()
namespace ERootMotionMode
{
	enum Type
	{
		// Leave root motion in animation.
		NoRootMotionExtraction,

		// Extract root motion but do not apply it.
		IgnoreRootMotion,

		// Root motion is taken from all animations contributing to the final pose, not suitable for network multiplayer setups
		RootMotionFromEverything,

		// Root motion is only taken from montages, suitable for network multiplayer setups
		RootMotionFromMontagesOnly,
	};
}

/** Animation Extraction Context */
USTRUCT()
struct FAnimExtractContext
{
	GENERATED_USTRUCT_BODY()

	/** Is Root Motion being extracted? */
	UPROPERTY()
	bool bExtractRootMotion;

	/** Position in animation to extract pose from */
	UPROPERTY()
	float CurrentTime;

	FAnimExtractContext()
		: bExtractRootMotion(false)
		, CurrentTime(0.f)
	{
	}

	FAnimExtractContext(float InCurrentTime)
		: bExtractRootMotion(false)
		, CurrentTime(InCurrentTime)
	{
	}

	FAnimExtractContext(float InCurrentTime, bool InbExtractRootMotion)
		: bExtractRootMotion(InbExtractRootMotion)
		, CurrentTime(InCurrentTime)
	{
	}
};


/**
 * Information about an animation asset that needs to be ticked
 */
USTRUCT()
struct FAnimTickRecord
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class UAnimationAsset* SourceAsset;

	float* TimeAccumulator;
	FVector BlendSpacePosition;	
	FBlendFilter* BlendFilter;
	TArray<FBlendSampleData>* BlendSampleDataCache;
	float PlayRateMultiplier;
	float EffectiveBlendWeight;
	bool bLooping;

public:
	FAnimTickRecord()
	{
	}
};

UENUM()
namespace EAnimGroupRole
{
	enum Type
	{
		// This node can be the leader, as long as it has a higher blend weight than the previous best leader
		CanBeLeader,
		
		// This node will always be a follower (unless there are only followers, in which case the first one ticked wins)
		AlwaysFollower,

		// This node will always be a leader (if more than one node is AlwaysLeader, the last one ticked wins)
		AlwaysLeader
	};
}

USTRUCT()
struct FAnimGroupInstance
{
	GENERATED_USTRUCT_BODY()

public:
	// The list of animation players in this group which are going to be evaluated this frame
	TArray<FAnimTickRecord> ActivePlayers;

	// The current group leader
	int32 GroupLeaderIndex;
public:
	FAnimGroupInstance()
		: GroupLeaderIndex(INDEX_NONE)
	{
	}

	void Reset()
	{
		GroupLeaderIndex = INDEX_NONE;
		ActivePlayers.Empty(ActivePlayers.Num());
	}

	// Checks the last tick record in the ActivePlayers array to see if it's a better leader than the current candidate.
	// This should be called once for each record added to ActivePlayers, after the record is setup.
	void TestTickRecordForLeadership(EAnimGroupRole::Type MembershipType)
	{
		int32 TestIndex = ActivePlayers.Num() - 1;
		const FAnimTickRecord& Candidate = ActivePlayers[TestIndex];
		
		switch (MembershipType)
		{
		case EAnimGroupRole::CanBeLeader:
			// Set it if we're better than the current leader (or if there is no leader yet)
			if ((GroupLeaderIndex == INDEX_NONE) || (ActivePlayers[GroupLeaderIndex].EffectiveBlendWeight < Candidate.EffectiveBlendWeight))
			{
				// This is a better leader
				GroupLeaderIndex = TestIndex;
			}
			break;
		case EAnimGroupRole::AlwaysLeader:
			// Always set the leader index
			GroupLeaderIndex = TestIndex;
			break;
		default:
		case EAnimGroupRole::AlwaysFollower:
			// Never set the leader index; the actual tick code will handle the case of no leader by using the first element in the array
			break;
		}
	}
};

/** Utility struct to accumulate root motion. */
USTRUCT()
struct FRootMotionMovementParams
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool bHasRootMotion;

	UPROPERTY()
	float BlendWeight;

	UPROPERTY()
	FTransform RootMotionTransform;

	FRootMotionMovementParams()
		: bHasRootMotion(false)
		, BlendWeight(0.f)
		, RootMotionTransform(FTransform::Identity)
	{
	}

	void Set(const FTransform& InTransform)
	{
		bHasRootMotion = true;
		RootMotionTransform = InTransform;
		BlendWeight = 1.f;
	}

	void Accumulate(const FTransform& InTransform)
	{
		if (!bHasRootMotion)
		{
			Set(InTransform);
		}
		else
		{
			RootMotionTransform = InTransform * RootMotionTransform;
		}
	}

	void Accumulate(const FRootMotionMovementParams& MovementParams)
	{
		if (MovementParams.bHasRootMotion)
		{
			Accumulate(MovementParams.RootMotionTransform);
		}
	}

	void AccumulateWithBlend(const FTransform& InTransform, float InBlendWeight)
	{
		const ScalarRegister VBlendWeight(InBlendWeight);
		if (bHasRootMotion)
		{
			RootMotionTransform.AccumulateWithShortestRotation(InTransform, VBlendWeight);
			BlendWeight += InBlendWeight;
		}
		else
		{
			Set(InTransform * VBlendWeight);
			BlendWeight = InBlendWeight;
		}
	}

	void AccumulateWithBlend(const FRootMotionMovementParams & MovementParams, float InBlendWeight)
	{
		if (MovementParams.bHasRootMotion)
		{
			AccumulateWithBlend(MovementParams.RootMotionTransform, InBlendWeight);
		}
	}

	void Clear()
	{
		bHasRootMotion = false;
		BlendWeight = 0.f;
	}

	void MakeUpToFullWeight()
	{
		float WeightLeft = FMath::Max(1.f - BlendWeight, 0.f);
		if (WeightLeft > KINDA_SMALL_NUMBER)
		{
			AccumulateWithBlend(FTransform(), WeightLeft);
		}
		RootMotionTransform.NormalizeRotation();
	}

	FRootMotionMovementParams ConsumeRootMotion(float Alpha)
	{
		const ScalarRegister VAlpha(Alpha);
		FTransform PartialRootMotion = (RootMotionTransform*VAlpha);
		PartialRootMotion.SetScale3D(FVector(1.f));
		PartialRootMotion.NormalizeRotation();
		RootMotionTransform = RootMotionTransform.GetRelativeTransform(PartialRootMotion);
		RootMotionTransform.NormalizeRotation(); //Make sure we are normalized, this needs to be investigated further

		FRootMotionMovementParams ReturnParams;
		ReturnParams.Set(PartialRootMotion);

		check(PartialRootMotion.IsRotationNormalized());
		check(RootMotionTransform.IsRotationNormalized());
		return ReturnParams;
	}
};

// This structure is used to either advance or synchronize animation players
struct FAnimAssetTickContext
{
public:
	FAnimAssetTickContext(float InDeltaTime, ERootMotionMode::Type InRootMotionMode)
		: RootMotionMode(InRootMotionMode)
		, DeltaTime(InDeltaTime)
		, SyncPoint(0.0f)
		, bIsLeader(true)
	{
	}
	
	// Are we the leader of our sync group (or ungrouped)?
	bool IsLeader() const
	{
		return bIsLeader;
	}

	bool IsFollower() const
	{
		return !bIsLeader;
	}

	// Return the delta time of the tick
	float GetDeltaTime() const
	{
		return DeltaTime;
	}

	void SetSyncPoint(float NormalizedTime)
	{
		SyncPoint = NormalizedTime;
	}

	// Returns the synchronization point (normalized time; only legal to call if ticking a follower)
	float GetSyncPoint() const
	{
		checkSlow(!bIsLeader);
		return SyncPoint;
	}

	void ConvertToFollower()
	{
		bIsLeader = false;
	}

	bool ShouldGenerateNotifies() const
	{
		return IsLeader();
	}

	//Root Motion accumulated from this tick context
	FRootMotionMovementParams RootMotionMovementParams;

	// The root motion mode of the owning AnimInstance
	ERootMotionMode::Type RootMotionMode;

private:
	float DeltaTime;

	// The structure used to pass synchronization state between members of a sync group
	float SyncPoint;

	bool bIsLeader;
};

USTRUCT()
struct FAnimationGroupReference
{
	GENERATED_USTRUCT_BODY()
	
	// The name of the group
	UPROPERTY(EditAnywhere, Category=Settings)
	FName GroupName;

	// The type of membership in the group (potential leader, always follower, etc...)
	UPROPERTY(EditAnywhere, Category=Settings)
	TEnumAsByte<EAnimGroupRole::Type> GroupRole;

	FAnimationGroupReference()
		: GroupRole(EAnimGroupRole::CanBeLeader)
	{
	}
};

UCLASS(abstract, MinimalAPI)
class UAnimationAsset : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	/** Pointer to the Skeleton this asset can be played on .	*/
	UPROPERTY(AssetRegistrySearchable, Category=Animation, VisibleAnywhere)
	class USkeleton* Skeleton;

	/** Skeleton guid. If changes, you need to remap info*/
	FGuid SkeletonGuid;

public:
	/** Advances the asset player instance 
	 * 
	 * @param Instance		AnimationTickRecord Instance - saves data to evaluate
	 * @param InstanceOwner	AnimInstance playing this asset
	 * @param Context		The tick context (leader/follower, delta time, sync point, etc...)
	 */
	virtual void TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const {}
	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() { return 0.f; }

	ENGINE_API void SetSkeleton(USkeleton* NewSkeleton);
	void ResetSkeleton(USkeleton* NewSkeleton);
	virtual void PostLoad() override;

	/** Validate our stored data against our skeleton and update accordingly */
	void ValidateSkeleton();

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	/** Replace Skeleton 
	 * 
	 * @param NewSkeleton	NewSkeleton to change to 
	 */
	ENGINE_API bool ReplaceSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces=false);

	/** Retrieve all animations that are used by this asset 
	 * 
	 * @param (out)		AnimationSequences 
	 **/
	ENGINE_API virtual bool GetAllAnimationSequencesReferred(TArray<class UAnimSequence*>& AnimationSequences);

	/** Replace this assets references to other animations based on ReplacementMap 
	 * 
	 * @param ReplacementMap	Mapping of original asset to new asset
	 **/
	ENGINE_API virtual void ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap);	

	ENGINE_API void SetPreviewMesh(USkeletalMesh* PreviewMesh);
	ENGINE_API USkeletalMesh* GetPreviewMesh();
#endif //WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

private:
	/** The default skeletal mesh to use when previewing this asset - this only applies when you open Persona using this asset*/
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TAssetPtr<class USkeletalMesh> PreviewSkeletalMesh;
#endif //WITH_EDITORONLY_DATA

public:
	class USkeleton* GetSkeleton() const { return Skeleton; }
};

