// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation sequence that can be played and evaluated to produce a pose.
 *
 */

#include "AnimationAsset.h"
#include "SmartName.h"
#include "Skeleton.h"
#include "Curves/CurveBase.h"
#include "AnimLinkableElement.h"
#include "AnimSequenceBase.generated.h"

#define DEFAULT_SAMPLERATE			30.f
#define MINIMUM_ANIMATION_LENGTH	(1/DEFAULT_SAMPLERATE)

namespace EAnimEventTriggerOffsets
{
	enum Type
	{
		OffsetBefore,
		OffsetAfter,
		NoOffset
	};
}

ENGINE_API float GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::Type OffsetType);

/** Ticking method for AnimNotifies in AnimMontages */
UENUM()
namespace EMontageNotifyTickType
{
	enum Type
	{
		/** Queue notifies, and trigger them at the end of the evaluation phase (faster). Not suitable for changing sections or montage position. */
		Queued,
		/** Trigger notifies as they are encountered (Slower). Suitable for changing sections or montage position. */
		BranchingPoint,
	};
}

/** Filtering method for deciding whether to trigger a notify */
UENUM()
namespace ENotifyFilterType
{
	enum Type
	{
		/** No filtering*/
		NoFiltering,

		/** Filter By Skeletal Mesh LOD */
		LOD,
	};
}

/**
 * Triggers an animation notify.  Each AnimNotifyEvent contains an AnimNotify object
 * which has its Notify method called and passed to the animation.
 */
USTRUCT()
struct FAnimNotifyEvent : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

	/** The user requested time for this notify */
	UPROPERTY()
	float DisplayTime_DEPRECATED;

	/** An offset from the DisplayTime to the actual time we will trigger the notify, as we cannot always trigger it exactly at the time the user wants */
	UPROPERTY()
	float TriggerTimeOffset;

	/** An offset similar to TriggerTimeOffset but used for the end scrub handle of a notify state's duration */
	UPROPERTY()
	float EndTriggerTimeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotifyEvent)
	float TriggerWeightThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AnimNotifyEvent)
	FName NotifyName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotify * Notify;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotifyState * NotifyStateClass;

	UPROPERTY()
	float Duration;

	/** Linkable element to use for the end handle representing a notify state duration */
	UPROPERTY()
	FAnimLinkableElement EndLink;

	/** If TRUE, this notify has been converted from an old BranchingPoint. */
	UPROPERTY()
	bool bConvertedFromBranchingPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotifyEvent)
	TEnumAsByte<EMontageNotifyTickType::Type> MontageTickType;

	/** Defines the chance of of this notify triggering, 0 = No Chance, 1 = Always triggers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NotifyTriggerChance;

	/** Defines a method for filtering notifies (stopping them triggering) e.g. by looking at the meshes current LOD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings)
	TEnumAsByte<ENotifyFilterType::Type> NotifyFilterType;

	/** LOD to start filtering this notify from.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings, meta = (ClampMin = "0"))
	int32 NotifyFilterLOD;

#if WITH_EDITORONLY_DATA
	/** Color of Notify in editor */
	UPROPERTY()
	FColor NotifyColor;

	/** Visual position of notify in the vertical 'tracks' of the notify editor panel */
	UPROPERTY()
	int32 TrackIndex;

#endif // WITH_EDITORONLY_DATA

	FAnimNotifyEvent()
		: DisplayTime_DEPRECATED(0)
		, TriggerTimeOffset(0)
		, EndTriggerTimeOffset(0)
		, TriggerWeightThreshold(ZERO_ANIMWEIGHT_THRESH)
#if WITH_EDITORONLY_DATA
		, Notify(NULL)
#endif // WITH_EDITORONLY_DATA
		, NotifyStateClass(NULL)
		, Duration(0)
		, bConvertedFromBranchingPoint(false)
		, MontageTickType(EMontageNotifyTickType::Queued)
		, NotifyTriggerChance(1.f)
		, NotifyFilterType(ENotifyFilterType::NoFiltering)
		, NotifyFilterLOD(0)
#if WITH_EDITORONLY_DATA
		, TrackIndex(0)
#endif // WITH_EDITORONLY_DATA
	{
	}

	/** Updates trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Updates end trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshEndTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Returns the actual trigger time for this notify. In some cases this may be different to the DisplayTime that has been set */
	ENGINE_API float GetTriggerTime() const;

	/** Returns the actual end time for a state notify. In some cases this may be different from DisplayTime + Duration */
	ENGINE_API float GetEndTriggerTime() const;

	ENGINE_API float GetDuration() const;

	ENGINE_API void SetDuration(float NewDuration);

	/** Returns true is this AnimNotify is a BranchingPoint */
	ENGINE_API bool IsBranchingPoint() const
	{
		return (MontageTickType == EMontageNotifyTickType::BranchingPoint) && GetLinkedMontage();
	}

	/** Returns true if this is blueprint derived notifies **/
	bool IsBlueprintNotify() const
	{
		return Notify != NULL || NotifyStateClass != NULL;
	}

	bool operator ==(const FAnimNotifyEvent& Other) const
	{
		return(
			(Notify && Notify == Other.Notify) || 
			(NotifyStateClass && NotifyStateClass == Other.NotifyStateClass) ||
			(!IsBlueprintNotify() && NotifyName == Other.NotifyName)
			);
	}

	/** This can be used with the Sort() function on a TArray of FAnimNotifyEvents to sort the notifies array by time, earliest first. */
	ENGINE_API bool operator <(const FAnimNotifyEvent& Other) const;

	ENGINE_API virtual void SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override;
};

// Used by UAnimSequenceBase::SortNotifies() to sort its Notifies array
FORCEINLINE bool FAnimNotifyEvent::operator<(const FAnimNotifyEvent& Other) const
{
	float ATime = GetTriggerTime();
	float BTime = Other.GetTriggerTime();

#if WITH_EDITORONLY_DATA
	// this sorting only works if it's saved in editor or loaded with editor data
	// this is required for gameplay team to have reliable order of notifies
	// but it was noted that this change will require to resave notifies. 
	// once you resave, this order will be preserved
	if (FMath::IsNearlyEqual(ATime, BTime))
	{

		// if the 2 anim notify events are the same display time sort based off of track index
		return TrackIndex < Other.TrackIndex;
	}
	else
#endif // WITH_EDITORONLY_DATA
	{
		return ATime < BTime;
	}
}

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct FAnimNotifyTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName TrackName;

	UPROPERTY()
	FLinearColor TrackColor;

	TArray<FAnimNotifyEvent*> Notifies;

	FAnimNotifyTrack()
		: TrackName(TEXT(""))
		, TrackColor(FLinearColor::White)
	{
	}

	FAnimNotifyTrack(FName InTrackName, FLinearColor InTrackColor)
		: TrackName(InTrackName)
		, TrackColor(InTrackColor)
	{
	}
};

/** native enum for curve types **/
enum EAnimCurveFlags
{
	// Used as morph target curve
	ACF_DrivesMorphTarget	= 0x00000001,
	// Used as triggering event
	ACF_TriggerEvent		= 0x00000002,
	// Is editable in Sequence Editor
	ACF_Editable			= 0x00000004,
	// Used as a material curve
	ACF_DrivesMaterial		= 0x00000008,
	// Is a metadata 'curve'
	ACF_Metadata			= 0x00000010,
	// motifies bone track
	ACF_DriveTrack			= 0x00000020,
	// disabled, right now it's used by track
	ACF_Disabled			= 0x00000040,

	// default flag when created
	ACF_DefaultCurve		= ACF_TriggerEvent | ACF_Editable,
	// curves created from Morph Target
	ACF_MorphTargetCurve	= ACF_DrivesMorphTarget
};

/**
 * Float curve data for one track
 */

USTRUCT()
struct FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	// Last observed name of the curve. We store this so we can recover from situations that
	// mean the skeleton doesn't have a mapped name for our UID (such as a user saving the an
	// animation but not the skeleton).
	UPROPERTY()
	FName		LastObservedName;

	// For smart naming - management purpose - i.e. rename/delete
	FSmartNameMapping::UID CurveUid;

private:

	/** Curve Type Flags */
	UPROPERTY()
	int32		CurveTypeFlags;

public:
	FAnimCurveBase(){}

	FAnimCurveBase(FName InName, int32 InCurveTypeFlags)
		: LastObservedName(InName)
		, CurveTypeFlags(InCurveTypeFlags)
	{	
	}

	FAnimCurveBase(USkeleton::AnimCurveUID Uid, int32 InCurveTypeFlags)
		: CurveUid(Uid)
		, CurveTypeFlags(InCurveTypeFlags)
	{}

	// To be able to use typedef'd types we need to serialize manually
	virtual void Serialize(FArchive& Ar)
	{
		if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		{
			Ar << CurveUid;
		}
	}

	/**
	 * Set InFlag to bValue
	 */
	ENGINE_API void SetCurveTypeFlag(EAnimCurveFlags InFlag, bool bValue);

	/**
	 * Toggle the value of the specified flag
	 */
	ENGINE_API void ToggleCurveTypeFlag(EAnimCurveFlags InFlag);

	/**
	 * Return true if InFlag is set, false otherwise 
	 */
	ENGINE_API bool GetCurveTypeFlag(EAnimCurveFlags InFlag) const;

	/**
	 * Set CurveTypeFlags to NewCurveTypeFlags
	 * This just overwrites CurveTypeFlags
	 */
	void SetCurveTypeFlags(int32 NewCurveTypeFlags);
	/** 
	 * returns CurveTypeFlags
	 */
	int32 GetCurveTypeFlags() const;
};

USTRUCT()
struct FFloatCurve : public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve data for float. */
	UPROPERTY()
	FRichCurve	FloatCurve;

	FFloatCurve(){}

	FFloatCurve(FName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	FFloatCurve(USkeleton::AnimCurveUID Uid, int32 InCurveTypeFlags)
		: FAnimCurveBase(Uid, InCurveTypeFlags)
	{}

	virtual void Serialize(FArchive& Ar) override
	{
		FAnimCurveBase::Serialize(Ar);
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(FFloatCurve& SourceCurve);
	float Evaluate(float CurrentTime, float BlendWeight) const;
	void UpdateOrAddKey(float NewKey, float CurrentTime);
	void Resize(float NewLength);
};

USTRUCT()
struct FVectorCurve : public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	enum EIndex
	{
		X = 0, 
		Y, 
		Z, 
		Max
	};

	/** Curve data for float. */
	UPROPERTY()
	FRichCurve	FloatCurves[3];

	FVectorCurve(){}

	FVectorCurve(FName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	FVectorCurve(USkeleton::AnimCurveUID Uid, int32 InCurveTypeFlags)
		: FAnimCurveBase(Uid, InCurveTypeFlags)
	{}

	virtual void Serialize(FArchive& Ar) override
	{
		FAnimCurveBase::Serialize(Ar);
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(FVectorCurve& SourceCurve);
	FVector Evaluate(float CurrentTime, float BlendWeight) const;
	void UpdateOrAddKey(const FVector& NewKey, float CurrentTime);
	bool DoesContainKey() const { return (FloatCurves[0].GetNumKeys() > 0 || FloatCurves[1].GetNumKeys() > 0 || FloatCurves[2].GetNumKeys() > 0);}
	void Resize(float NewLength);
};

USTRUCT()
struct FTransformCurve: public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve data for each transform. */
	UPROPERTY()
	FVectorCurve	TranslationCurve;

	/** Rotation curve - right now we use euler because quat also doesn't provide linear interpolation - curve editor can't handle quat interpolation
	 * If you hit gimbal lock, you should add extra key to fix it. This will cause gimbal lock. 
	 * @TODO: Eventually we'll need FRotationCurve that would contain rotation curve - that will interpolate as slerp or as quaternion 
	 */
	UPROPERTY()
	FVectorCurve	RotationCurve;

	UPROPERTY()
	FVectorCurve	ScaleCurve;

	FTransformCurve(){}

	FTransformCurve(FName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName, InCurveTypeFlags)
	{
	}

	FTransformCurve(USkeleton::AnimCurveUID Uid, int32 InCurveTypeFlags)
		: FAnimCurveBase(Uid, InCurveTypeFlags)
	{}

	virtual void Serialize(FArchive& Ar) override
	{
		FAnimCurveBase::Serialize(Ar);
	}

	// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
	void CopyCurve(FTransformCurve& SourceCurve);
	FTransform Evaluate(float CurrentTime, float BlendWeight) const;
	ENGINE_API void UpdateOrAddKey(const FTransform& NewKey, float CurrentTime);
	void Resize(float NewLength);
};
/**
 * Raw Curve data for serialization
 */
USTRUCT()
struct FRawCurveTracks
{
	GENERATED_USTRUCT_BODY()

	enum ESupportedCurveType
	{
		FloatType,
		VectorType,
		TransformType,
		Max, 
	};

	UPROPERTY()
	TArray<FFloatCurve>		FloatCurves;

#if WITH_EDITORONLY_DATA
	/**
	 * @note : Currently VectorCurves are not evaluated or used for anything else but transient data for modifying bone track
	 *			Note that it doesn't have UPROPERTY tag yet. In the future, we'd like this to be serialized, but not for now
	 **/
	UPROPERTY(transient)
	TArray<FVectorCurve>	VectorCurves;

	/**
	 * @note : Currently VectorCurves are not evaluated or used for anything else but transient data for modifying bone track
	 *			Note that it doesn't have UPROPERTY tag yet. In the future, we'd like this to be serialized, but not for now
	 **/
	UPROPERTY()
	TArray<FTransformCurve>	TransformCurves;
#endif // #if WITH_EDITOR_DATA
	/**
	 * Evaluate curve data at the time CurrentTime, and add to Instance. It only evaluates Float Curve for now
	 */
	void EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const;

#if WITH_EDITOR
	/**
	 *	Evaluate transform curves 
	 */
	ENGINE_API void EvaluateTransformCurveData(USkeleton * Skeleton, TMap<FName, FTransform>&OutCurves, float CurrentTime, float BlendWeight) const;
#endif // WITH_EDITOR
	/**
	 * Find curve data based on the curve UID
	 */
	ENGINE_API FAnimCurveBase * GetCurveData(USkeleton::AnimCurveUID Uid, ESupportedCurveType SupportedCurveType = FloatType);
	/**
	 * Add new curve from the provided UID and return true if success
	 * bVectorInterpCurve == true, then it will create FVectorCuve, otherwise, FFloatCurve
	 */
	ENGINE_API bool AddCurveData(USkeleton::AnimCurveUID Uid, int32 CurveFlags = ACF_DefaultCurve, ESupportedCurveType SupportedCurveType = FloatType);

	/**
	 * Delete curve data 
	 */
	ENGINE_API bool DeleteCurveData(USkeleton::AnimCurveUID Uid, ESupportedCurveType SupportedCurveType = FloatType);
	/**
	 * Duplicate curve data
	 * 
	 */
	ENGINE_API bool DuplicateCurveData(USkeleton::AnimCurveUID ToCopyUid, USkeleton::AnimCurveUID NewUid, ESupportedCurveType SupportedCurveType = FloatType);

	/**
	 * Updates the LastObservedName field of the curves from the provided name container
	 */
	ENGINE_API void UpdateLastObservedNames(FSmartNameMapping* NameMapping, ESupportedCurveType SupportedCurveType = FloatType);

	/** 
	 * Serialize
	 */
	void Serialize(FArchive& Ar);

	/*
	 * resize curve length. If longer, it doesn't do any. If shorter, remove previous keys and add new key to the end of the frame. 
	 */
	void Resize(float TotalLength);
	/** 
	 * Clear all keys
	 */
	void Empty()
	{
		FloatCurves.Empty();
#if WITH_EDITORONLY_DATA
		VectorCurves.Empty();
		TransformCurves.Empty();
#endif
	}
private:
	/** 
	 * Adding vector curve support - this is all transient data for now. This does not save and all these data will be baked into RawAnimationData
	 */

	/**
	 * Find curve data based on the curve UID
	 */
	template <typename DataType>
	DataType * GetCurveDataImpl(TArray<DataType>& Curves, USkeleton::AnimCurveUID Uid);

	/**
	 * Add new curve from the provided UID and return true if success
	 * bVectorInterpCurve == true, then it will create FVectorCuve, otherwise, FFloatCurve
	 */
	template <typename DataType>
	bool AddCurveDataImpl(TArray<DataType>& Curves, USkeleton::AnimCurveUID Uid, int32 CurveFlags);
	/**
	 * Delete curve data 
	 */
	template <typename DataType>
	bool DeleteCurveDataImpl(TArray<DataType>& Curves, USkeleton::AnimCurveUID Uid);
	/**
	 * Duplicate curve data
	 * 
	 */
	template <typename DataType>
	bool DuplicateCurveDataImpl(TArray<DataType>& Curves, USkeleton::AnimCurveUID ToCopyUid, USkeleton::AnimCurveUID NewUid);

	/**
	 * Updates the LastObservedName field of the curves from the provided name container
	 */
	template <typename DataType>
	void UpdateLastObservedNamesImpl(TArray<DataType>& Curves, FSmartNameMapping* NameMapping);
};

UENUM()
enum ETypeAdvanceAnim
{
	ETAA_Default,
	ETAA_Finished,
	ETAA_Looped
};

UCLASS(abstract, MinimalAPI, BlueprintType)
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

	/** Animation notifies, sorted by time (earliest notification first). */
	UPROPERTY()
	TArray<struct FAnimNotifyEvent> Notifies;

	/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0. */
	UPROPERTY(Category=Length, AssetRegistrySearchable, VisibleAnywhere)
	float SequenceLength;

	/** Number for tweaking playback rate of this animation globally. */
	UPROPERTY(EditAnywhere, Category=Animation)
	float RateScale;
	
	/**
	 * Raw uncompressed float curve data 
	 */
	UPROPERTY()
	FRawCurveTracks RawCurveData;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	UPROPERTY()
	TArray<FAnimNotifyTrack> AnimNotifyTracks;
#endif // WITH_EDITORONLY_DATA

	// Begin UObject interface
	virtual void PostLoad() override;
	// End of UObject interface

	/** Sort the Notifies array by time, earliest first. */
	ENGINE_API void SortNotifies();	

	/** 
	 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
	 * Time will be advanced and support looping if bAllowLooping is true.
	 * Supports playing backwards (DeltaTime<0).
	 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
	 */
	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	/** 
	 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
	 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
	 * Supports playing backwards (CurrentPosition<PreviousPosition).
	 * Only supports contiguous range, does NOT support looping and wrapping over.
	 */
	ENGINE_API void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	/** Evaluate curve data to Instance at the time of CurrentTime **/
	ENGINE_API virtual void EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const;

	/**
	 * return true if this is valid additive animation
	 * false otherwise
	 */
	virtual bool IsValidAdditive() const { return false; }

#if WITH_EDITOR
	/** Return Number of Frames **/
	virtual int32 GetNumberOfFrames() const;

	/** Get the frame number for the provided time */
	virtual int32 GetFrameAtTime(const float Time) const;

	/** Get the time at the given frame */
	virtual float GetTimeAtFrame(const int32 Frame) const;

	// update anim notify track cache
	ENGINE_API void UpdateAnimNotifyTrackCache();
	
	// @todo document
	ENGINE_API void InitializeNotifyTrack();

	/** Fix up any notifies that are positioned beyond the end of the sequence */
	ENGINE_API void ClampNotifiesAtEndOfSequence();

	/** Calculates what (if any) offset should be applied to the trigger time of a notify given its display time */ 
	virtual EAnimEventTriggerOffsets::Type CalculateOffsetForNotify(float NotifyDisplayTime) const;

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	
	// Get a pointer to the data for a given Anim Notify
	ENGINE_API uint8* FindNotifyPropertyData(int32 NotifyIndex, UArrayProperty*& ArrayProperty);

#endif	//WITH_EDITORONLY_DATA

	// Begin UAnimationAsset interface
	virtual void TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const override;
	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() override { return SequenceLength; }
	// End of UAnimationAsset interface

	virtual void OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const;

	virtual bool HasRootMotion() const { return false; }

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
private:
	DECLARE_MULTICAST_DELEGATE( FOnNotifyChangedMulticaster );
	FOnNotifyChangedMulticaster OnNotifyChanged;

public:
	typedef FOnNotifyChangedMulticaster::FDelegate FOnNotifyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate);
	ENGINE_API void UnregisterOnNotifyChanged(void* Unregister);
	ENGINE_API virtual bool IsValidToPlay() const { return true; }
#endif

protected:
	template <typename DataType>
	void VerifyCurveNames(USkeleton* Skeleton, const FName& NameContainer, TArray<DataType>& CurveList);
};
