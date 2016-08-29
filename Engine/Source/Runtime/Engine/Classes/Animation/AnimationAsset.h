// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Abstract base class of animation assets that can be played back and evaluated to produce a pose.
 *
 */

#pragma once

#include "SkeletalMeshTypes.h"
#include "AnimInterpFilter.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "AnimationAsset.generated.h"

namespace MarkerIndexSpecialValues
{
	enum Type
	{
		Unitialized = -2,
		AnimationBoundary = -1,
	};
};

struct FMarkerPair
{
	int32 MarkerIndex;
	float TimeToMarker;

	FMarkerPair() : MarkerIndex(MarkerIndexSpecialValues::Unitialized) {}

	void Reset() { MarkerIndex = MarkerIndexSpecialValues::Unitialized; }
};

struct FMarkerTickRecord
{
	//Current Position in marker space, equivalent to TimeAccumulator
	FMarkerPair PreviousMarker;
	FMarkerPair NextMarker;

	bool IsValid() const { return PreviousMarker.MarkerIndex != MarkerIndexSpecialValues::Unitialized && NextMarker.MarkerIndex != MarkerIndexSpecialValues::Unitialized; }

	void Reset() { PreviousMarker.Reset(); NextMarker.Reset(); }
};

/** Transform definition */
USTRUCT(BlueprintType)
struct FBlendSampleData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 SampleDataIndex;

	UPROPERTY()
	class UAnimSequence* Animation;

	UPROPERTY()
	float TotalWeight;

	UPROPERTY()
	float Time;

	UPROPERTY()
	float PreviousTime;

	FMarkerTickRecord MarkerTickRecord;

	// transient perbone interpolation data
	TArray<float> PerBoneBlendData;

	FBlendSampleData()
		:	SampleDataIndex(0)
		,	TotalWeight(0.f)
		,	Time(0.f)
		,	PreviousTime(0.f)
	{}
	FBlendSampleData(int32 Index)
		:	SampleDataIndex(Index)
		,	Animation(nullptr)
		,	TotalWeight(0.f)
		,	Time(0.f)
		,	PreviousTime(0.f)
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

	static void ENGINE_API NormalizeDataWeight(TArray<FBlendSampleData>& SampleDataList);
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

UENUM()
/** Root Bone Lock options when extracting Root Motion. */
namespace ERootMotionRootLock
{
	enum Type
	{
		/** Use reference pose root bone position. */
		RefPose,

		/** Use root bone position on first frame of animation. */
		AnimFirstFrame,

		/** FTransform::Identity. */
		Zero
	};
}

UENUM()
namespace ERootMotionMode
{
	enum Type
	{
		/** Leave root motion in animation. */
		NoRootMotionExtraction,

		/** Extract root motion but do not apply it. */
		IgnoreRootMotion,

		/** Root motion is taken from all animations contributing to the final pose, not suitable for network multiplayer setups. */
		RootMotionFromEverything,

		/** Root motion is only taken from montages, suitable for network multiplayer setups. */
		RootMotionFromMontagesOnly,
	};
}

/** Animation Extraction Context */
struct FAnimExtractContext
{
	/** Is Root Motion being extracted? */
	bool bExtractRootMotion;

	/** Position in animation to extract pose from */
	float CurrentTime;

	/** 
	 * Pose Curve Values to extract pose from pose assets. 
	 * This is used by pose asset extraction 
	 * This always has to match with pose # in the asset it's extracting from
	 */
	TArray<float> PoseCurves;

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

	FAnimExtractContext(TArray<float>& InPoseCurves)
		: bExtractRootMotion(false)
		, CurrentTime(0.f)
		, PoseCurves(InPoseCurves)
	{
		// @todo: no support on root motion
	}
};

//Represent a current play position in an animation
//based on sync markers
USTRUCT(BlueprintType)
struct FMarkerSyncAnimPosition
{
	GENERATED_USTRUCT_BODY()

	/** The marker we have passed*/
	UPROPERTY()
	FName PreviousMarkerName;

	/** The marker we are heading towards */
	UPROPERTY()
	FName NextMarkerName;

	/** Value between 0 and 1 representing where we are:
	0   we are at PreviousMarker
	1   we are at NextMarker
	0.5 we are half way between the two */
	UPROPERTY()
	float PositionBetweenMarkers;

	/** Is this a valid Marker Sync Position */
	bool IsValid() const { return (PreviousMarkerName != NAME_None && NextMarkerName != NAME_None); }

	FMarkerSyncAnimPosition()
	{}

	FMarkerSyncAnimPosition(const FName& InPrevMarkerName, const FName& InNextMarkerName, const float& InAlpha)
		: PreviousMarkerName(InPrevMarkerName)
		, NextMarkerName(InNextMarkerName)
		, PositionBetweenMarkers(InAlpha)
	{}

	/** Debug output function*/
	FString ToString() const
	{
		return FString::Printf(TEXT("[PreviousMarker %s, NextMarker %s] : %0.2f "), *PreviousMarkerName.ToString(), *NextMarkerName.ToString(), PositionBetweenMarkers);
	}
};

struct FPassedMarker
{
	FName PassedMarkerName;

	float DeltaTimeWhenPassed;
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

	float*  TimeAccumulator;
	float PlayRateMultiplier;
	float EffectiveBlendWeight;
	float RootMotionWeightModifier;

	bool bLooping;

	union
	{
		struct
		{
			float  BlendSpacePositionX;
			float  BlendSpacePositionY;
			FBlendFilter* BlendFilter;
			TArray<FBlendSampleData>* BlendSampleDataCache;
		} BlendSpace;

		struct
		{
			float CurrentPosition;  // montage doesn't use accumulator, but this
			float PreviousPosition;
			float MoveDelta; // MoveDelta isn't always abs(CurrentPosition-PreviousPosition) because Montage can jump or repeat or loop
			TArray<FPassedMarker>* MarkersPassedThisTick;
		} Montage;
	};

	// marker sync related data
	FMarkerTickRecord* MarkerTickRecord;
	bool bCanUseMarkerSync;
	float LeaderScore;

	// Return the root motion weight for this tick record
	float GetRootMotionWeight() const { return EffectiveBlendWeight * RootMotionWeightModifier; }

public:
	FAnimTickRecord()
		: TimeAccumulator(nullptr)
		, PlayRateMultiplier(1.f)
		, EffectiveBlendWeight(0.f)
		, RootMotionWeightModifier(1.f)
		, bLooping(false)
		, MarkerTickRecord(nullptr)
		, bCanUseMarkerSync(false)
		, LeaderScore(0.f)
	{
	}

	/** This can be used with the Sort() function on a TArray of FAnimTickRecord to sort from higher leader score */
	ENGINE_API bool operator <(const FAnimTickRecord& Other) const { return LeaderScore > Other.LeaderScore; }
};

class FMarkerTickContext
{
public:
	FMarkerTickContext(const TArray<FName>& ValidMarkerNames) : ValidMarkers(ValidMarkerNames) {}
	FMarkerTickContext() {}

	void SetMarkerSyncStartPosition(const FMarkerSyncAnimPosition& SyncPosition)
	{
		MarkerSyncStartPostion = SyncPosition;
	}

	void SetMarkerSyncEndPosition(const FMarkerSyncAnimPosition& SyncPosition)
	{
		MarkerSyncEndPostion = SyncPosition;
	}

	const FMarkerSyncAnimPosition& GetMarkerSyncStartPosition() const
	{
		return MarkerSyncStartPostion;
	}

	const FMarkerSyncAnimPosition& GetMarkerSyncEndPosition() const
	{
		return MarkerSyncEndPostion;
	}

	const TArray<FName>& GetValidMarkerNames() const
	{
		return ValidMarkers;
	}

	bool IsMarkerSyncStartValid() const
	{
		return MarkerSyncStartPostion.IsValid();
	}

	bool IsMarkerSyncEndValid() const
	{
		// does it have proper end position
		return MarkerSyncEndPostion.IsValid();
		
	}
	TArray<FPassedMarker> MarkersPassedThisTick;

	/** Debug output function */
	FString  ToString() const
	{
		FString MarkerString;

		for (const auto& ValidMarker : ValidMarkers)
		{
			MarkerString.Append(FString::Printf(TEXT("%s,"), *ValidMarker.ToString()));
		}

		return FString::Printf(TEXT(" - Sync Start Position : %s\n - Sync End Position : %s\n - Markers : %s"),
			*MarkerSyncStartPostion.ToString(), *MarkerSyncEndPostion.ToString(), *MarkerString);
	}
private:
	// Structure representing our sync position based on markers before tick
	// This is used to allow new animations to play from the right marker position
	FMarkerSyncAnimPosition MarkerSyncStartPostion;

	// Structure representing our sync position based on markers after tick
	FMarkerSyncAnimPosition MarkerSyncEndPostion;


	// Valid marker names for this sync group
	TArray<FName> ValidMarkers;
};


UENUM()
namespace EAnimGroupRole
{
	enum Type
	{
		/** This node can be the leader, as long as it has a higher blend weight than the previous best leader. */
		CanBeLeader,
		
		/** This node will always be a follower (unless there are only followers, in which case the first one ticked wins). */
		AlwaysFollower,

		/** This node will always be a leader (if more than one node is AlwaysLeader, the last one ticked wins). */
		AlwaysLeader,

		/** This node will be excluded from the sync group while blending in. Once blended in it will be the sync group leader until blended out*/
		TransitionLeader,
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
	// @note : before ticking, this is invalid
	// after ticking, this should contain the real leader
	// during ticket, this list gets sorted by LeaderScore of AnimTickRecord,
	// and it starts from 0 index, but if that fails due to invalid position, it will go to the next available leader
	int32 GroupLeaderIndex;

	// Valid marker names for this sync group
	TArray<FName> ValidMarkers;

	// Can we use sync markers for ticking this sync group
	bool bCanUseMarkerSync;

	// This has latest Montage Leader Weight
	float MontageLeaderWeight;

	FMarkerTickContext MarkerTickContext;

public:
	FAnimGroupInstance()
		: GroupLeaderIndex(INDEX_NONE)
		, bCanUseMarkerSync(false)
		, MontageLeaderWeight(0.f)
	{
	}

	void Reset()
	{
		GroupLeaderIndex = INDEX_NONE;
		ActivePlayers.Reset();
		bCanUseMarkerSync = false;
		MontageLeaderWeight = 0.f;
		MarkerTickContext = FMarkerTickContext();
	}

	// Checks the last tick record in the ActivePlayers array to see if it's a better leader than the current candidate.
	// This should be called once for each record added to ActivePlayers, after the record is setup.
	ENGINE_API void TestTickRecordForLeadership(EAnimGroupRole::Type MembershipType);
	// Checks the last tick record in the ActivePlayers array to see if we have a better leader for montage
	// This is simple rule for higher weight becomes leader
	// Only one from same sync group with highest weight will be leader
	ENGINE_API void TestMontageTickRecordForLeadership();

	// Called after leader has been ticked and decided
	ENGINE_API void Finalize(const FAnimGroupInstance* PreviousGroup);

	// Called after all tick records have been added but before assets are actually ticked
	ENGINE_API void Prepare(const FAnimGroupInstance* PreviousGroup);
};

/** Utility struct to accumulate root motion. */
USTRUCT()
struct FRootMotionMovementParams
{
	GENERATED_USTRUCT_BODY()

private:
	static FVector RootMotionScale;

	// TODO: Remove when we make RootMotionTransform private
	FORCEINLINE FTransform& GetRootMotionTransformInternal()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RootMotionTransform;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	FORCEINLINE const FTransform& GetRootMotionTransformInternal() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RootMotionTransform;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

public:
	
	UPROPERTY()
	bool bHasRootMotion;

	UPROPERTY()
	float BlendWeight;

	// To be made private
	DEPRECATED(4.13, "RootMotionTransform should not be accessed directly, please use GetRootMotionTransform() to read it or one of the set/accumulate functions to modify it")
	UPROPERTY()
	FTransform RootMotionTransform;

	FRootMotionMovementParams()
		: bHasRootMotion(false)
		, BlendWeight(0.f)
	{
	}

	// Copy/Move constructors and assignment operator added for deprecation support
	// Could be removed once RootMotionTransform is made private
	FRootMotionMovementParams(const FRootMotionMovementParams& Other)
		: bHasRootMotion(Other.bHasRootMotion)
		, BlendWeight(Other.BlendWeight)
	{
		GetRootMotionTransformInternal() = Other.GetRootMotionTransformInternal();
	}

	FRootMotionMovementParams(const FRootMotionMovementParams&& Other)
		: bHasRootMotion(Other.bHasRootMotion)
		, BlendWeight(Other.BlendWeight)
	{
		GetRootMotionTransformInternal() = Other.GetRootMotionTransformInternal();
	}

	FRootMotionMovementParams& operator=(const FRootMotionMovementParams& Other)
	{
		bHasRootMotion = Other.bHasRootMotion;
		BlendWeight = Other.BlendWeight;
		GetRootMotionTransformInternal() = Other.GetRootMotionTransformInternal();
		return *this;
	}

	void Set(const FTransform& InTransform)
	{
		bHasRootMotion = true;
		GetRootMotionTransformInternal() = InTransform;
		GetRootMotionTransformInternal().SetScale3D(RootMotionScale);
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
			GetRootMotionTransformInternal() = InTransform * GetRootMotionTransformInternal();
			GetRootMotionTransformInternal().SetScale3D(RootMotionScale);
		}
	}

	void Accumulate(const FRootMotionMovementParams& MovementParams)
	{
		if (MovementParams.bHasRootMotion)
		{
			Accumulate(MovementParams.GetRootMotionTransformInternal());
		}
	}

	void AccumulateWithBlend(const FTransform& InTransform, float InBlendWeight)
	{
		const ScalarRegister VBlendWeight(InBlendWeight);
		if (bHasRootMotion)
		{
			GetRootMotionTransformInternal().AccumulateWithShortestRotation(InTransform, VBlendWeight);
			GetRootMotionTransformInternal().SetScale3D(RootMotionScale);
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
			AccumulateWithBlend(MovementParams.GetRootMotionTransformInternal(), InBlendWeight);
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
		GetRootMotionTransformInternal().NormalizeRotation();
	}

	FRootMotionMovementParams ConsumeRootMotion(float Alpha)
	{
		const ScalarRegister VAlpha(Alpha);
		FTransform PartialRootMotion = (GetRootMotionTransformInternal()*VAlpha);
		PartialRootMotion.SetScale3D(RootMotionScale); // Reset scale after multiplication above
		PartialRootMotion.NormalizeRotation();
		GetRootMotionTransformInternal() = GetRootMotionTransformInternal().GetRelativeTransform(PartialRootMotion);
		GetRootMotionTransformInternal().NormalizeRotation(); //Make sure we are normalized, this needs to be investigated further

		FRootMotionMovementParams ReturnParams;
		ReturnParams.Set(PartialRootMotion);

		check(PartialRootMotion.IsRotationNormalized());
		check(GetRootMotionTransformInternal().IsRotationNormalized());
		return ReturnParams;
	}

	const FTransform& GetRootMotionTransform() const { return GetRootMotionTransformInternal(); }
	void ScaleRootMotionTranslation(float TranslationScale) { GetRootMotionTransformInternal().ScaleTranslation(TranslationScale); }
};

// This structure is used to either advance or synchronize animation players
struct FAnimAssetTickContext
{
public:
	FAnimAssetTickContext(float InDeltaTime, ERootMotionMode::Type InRootMotionMode, bool bInOnlyOneAnimationInGroup, const TArray<FName>& ValidMarkerNames)
		: RootMotionMode(InRootMotionMode)
		, MarkerTickContext(ValidMarkerNames)
		, DeltaTime(InDeltaTime)
		, LeaderDelta(0.f)
		, AnimLengthRatio(0.0f)
		, bIsMarkerPositionValid(ValidMarkerNames.Num() > 0)
		, bIsLeader(true)
		, bOnlyOneAnimationInGroup(bInOnlyOneAnimationInGroup)
	{
	}

	FAnimAssetTickContext(float InDeltaTime, ERootMotionMode::Type InRootMotionMode, bool bInOnlyOneAnimationInGroup)
		: RootMotionMode(InRootMotionMode)
		, DeltaTime(InDeltaTime)
		, LeaderDelta(0.f)
		, AnimLengthRatio(0.0f)
		, bIsMarkerPositionValid(false)
		, bIsLeader(true)
		, bOnlyOneAnimationInGroup(bInOnlyOneAnimationInGroup)
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

	void SetLeaderDelta(float InLeaderDelta)
	{
		LeaderDelta = InLeaderDelta;
	}

	float GetLeaderDelta() const
	{
		return LeaderDelta;
	}

	void SetAnimationPositionRatio(float NormalizedTime)
	{
		AnimLengthRatio = NormalizedTime;
	}

	// Returns the synchronization point (normalized time; only legal to call if ticking a follower)
	float GetAnimationPositionRatio() const
	{
		checkSlow(!bIsLeader);
		return AnimLengthRatio;
	}

	bool CanUseMarkerPosition() const
	{
		return bIsMarkerPositionValid;
	}

	void ConvertToFollower()
	{
		bIsLeader = false;
	}

	bool ShouldGenerateNotifies() const
	{
		return IsLeader();
	}

	bool IsSingleAnimationContext() const
	{
		return bOnlyOneAnimationInGroup;
	}

	//Root Motion accumulated from this tick context
	FRootMotionMovementParams RootMotionMovementParams;

	// The root motion mode of the owning AnimInstance
	ERootMotionMode::Type RootMotionMode;

	FMarkerTickContext MarkerTickContext;

private:
	float DeltaTime;

	float LeaderDelta;

	// Float in 0 - 1 range representing how far through an animation we are
	float AnimLengthRatio;

	bool bIsMarkerPositionValid;

	bool bIsLeader;

	bool bOnlyOneAnimationInGroup;
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
class UAnimationAsset : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

private:
	/** Pointer to the Skeleton this asset can be played on .	*/
	UPROPERTY(AssetRegistrySearchable, Category=Animation, VisibleAnywhere)
	class USkeleton* Skeleton;

	/** Skeleton guid. If changes, you need to remap info*/
	FGuid SkeletonGuid;

	/** Meta data that can be saved with the asset 
	 * 
	 * You can query by GetMetaData function
	 */
	UPROPERTY(Category=MetaData, instanced, EditAnywhere)
	TArray<class UAnimMetaData*> MetaData;

protected:
	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Animation)
	TArray<UAssetUserData*> AssetUserData;

public:
	/** Advances the asset player instance 
	 * 
	 * @param Instance		AnimationTickRecord Instance - saves data to evaluate
	 * @param InstanceOwner	AnimInstance playing this asset
	 * @param Context		The tick context (leader/follower, delta time, sync point, etc...)
	 */
	DEPRECATED(4.11, "This function is deprecated, use TickAssetPlayer")
	ENGINE_API virtual void TickAssetPlayerInstance(FAnimTickRecord& Instance, class UAnimInstance* AnimInstance, FAnimAssetTickContext& Context) const;

	/** Advances the asset player instance 
	 * 
	 * @param Instance		AnimationTickRecord Instance - saves data to evaluate
	 * @param NotifyQueue	Queue for any notifies we create
	 * @param Context		The tick context (leader/follower, delta time, sync point, etc...)
	 */
	virtual void TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const {}

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

	/** Get available Metadata within the animation asset
	 */
	ENGINE_API const TArray<class UAnimMetaData*>& GetMetaData() const { return MetaData; }

#if WITH_EDITOR
	/** Replace Skeleton 
	 * 
	 * @param NewSkeleton	NewSkeleton to change to 
	 */
	ENGINE_API bool ReplaceSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces=false);

	// Helper function for GetAllAnimationSequencesReferred, it adds itself first and call GetAllAnimationSEquencesReferred
	ENGINE_API void HandleAnimReferenceCollection(TArray<UAnimationAsset*>& AnimationAssets);

protected:
	/** Retrieve all animations that are used by this asset 
	 * 
	 * @param (out)		AnimationSequences 
	 **/
	ENGINE_API virtual bool GetAllAnimationSequencesReferred(TArray<class UAnimationAsset*>& AnimationSequences);

public:
	/** Replace this assets references to other animations based on ReplacementMap 
	 * 
	 * @param ReplacementMap	Mapping of original asset to new asset
	 **/
	ENGINE_API virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap);

	ENGINE_API void SetPreviewMesh(USkeletalMesh* PreviewMesh);
	ENGINE_API USkeletalMesh* GetPreviewMesh();

	ENGINE_API virtual int32 GetMarkerUpdateCounter() const { return 0; }
#endif //WITH_EDITOR

	/** Return a list of unique marker names for blending compatibility */
	ENGINE_API virtual TArray<FName>* GetUniqueMarkerNames() { return NULL; }

	//~ Begin IInterface_AssetUserData Interface
	ENGINE_API virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	ENGINE_API virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

	/**
	* return true if this is valid additive animation
	* false otherwise
	*/
	virtual bool IsValidAdditive() const { return false; }

#if WITH_EDITORONLY_DATA
	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	/** The default skeletal mesh to use when previewing this asset - this only applies when you open Persona using this asset*/
	// @todo: note that this doesn't retarget right now
	UPROPERTY(duplicatetransient, EditAnywhere, Category = Animation)
	class UPoseAsset* PreviewPoseAsset;

private:
	/** The default skeletal mesh to use when previewing this asset - this only applies when you open Persona using this asset*/
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TAssetPtr<class USkeletalMesh> PreviewSkeletalMesh;
#endif //WITH_EDITORONLY_DATA

protected:
#if WITH_EDITOR
	virtual void RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces);
#endif // WITH_EDITOR

public:
	class USkeleton* GetSkeleton() const { return Skeleton; }
};

