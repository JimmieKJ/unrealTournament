// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveBase.h"
#include "AnimTypes.h"
#include "AnimCurveTypes.generated.h"

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

	/* For smart naming - management purpose - i.e. rename/delete */
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
	ENGINE_API float Evaluate(float CurrentTime) const;
	void UpdateOrAddKey(float NewKey, float CurrentTime);
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
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
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
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
	void Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
};

/**
* This is array of curves that run when collecting curves natively 
*/
struct FCurveElement
{
	/** Curve Value */
	float					Value;
	/** Curve Flags - this does OR ops when it gets blended*/
	int32					Flags;

	FCurveElement(float InValue, int32	InFlags)
		:  Value(InValue), Flags(InFlags)
	{}
};

/**
 * This struct is used to create curve snap shot of current time when extracted
 */
struct ENGINE_API FBlendedCurve
{
	/**
	* List of curve elements for this pose
	*/
	TArray<FCurveElement> Elements;
	TArray<FSmartNameMapping::UID> UIDList;

	/**
	 * constructor
	 */
	FBlendedCurve()
		: bInitialized(false)
	{
	}

	/**
	 * constructor
	 */
	FBlendedCurve(const class UAnimInstance* AnimInstance);

	/**
	 * constructor
	 */
	FBlendedCurve(const class USkeleton* Skeleton)
	{
		check (Skeleton);
		InitFrom(Skeleton);
	}

	/** Initialize Curve Data from following data */
	void InitFrom(const class USkeleton* Skeleton);
	void InitFrom(const FBlendedCurve& InCurveToInitFrom);

	/** Set value of InUID to InValue */
	void Set(USkeleton::AnimCurveUID InUid, float InValue, int32 InFlags);

	/**
	 * Blend (A, B) using Alpha, same as Lerp
	 */
	void Blend(const FBlendedCurve& A, const FBlendedCurve& B, float Alpha);
	/**
	 * Blend with Other using Alpha, same as Lerp 
	 */
	void BlendWith(const FBlendedCurve& Other, float Alpha);
	/**
	 * Convert current curves to Additive (this - BaseCurve) if same found
	 */
	void ConvertToAdditive(const FBlendedCurve& BaseCurve);
	/**
	 * Accumulate the input curve with input Weight
	 */
	void Accumulate(const FBlendedCurve& AdditiveCurve, float Weight);

	/**
	 * This doesn't blend but combine MAX(current weight, curvetocombine weight)
	 */
	void Combine(const FBlendedCurve& CurveToCombine);

	/**
	 * Override with inupt curve * weight
	 */
	void Override(const FBlendedCurve& CurveToOverrideFrom, float Weight);
	/**
	 * Override with inupt curve 
	 */
	void Override(const FBlendedCurve& CurveToOverrideFrom);

	/** Return number of elements */
	int32 Num() const { return Elements.Num(); }

	/** CopyFrom as expected. */
	void CopyFrom(const FBlendedCurve& CurveToCopyFrom);
	/** CopyFrom/MoveFrom as expected. Once moved, it is invalid */
	void MoveFrom(FBlendedCurve& CurveToMoveFrom);

	/** Empty */
	void Empty()
	{
		UIDList.Reset();
		Elements.Reset();
	}
private:
	/**  Whether initialized or not */
	bool bInitialized;
	/** Empty and allocate Count number */
	void Reset(int32 Count);
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
	 * @note : TransformCurves are used to edit additive animation in editor. 
	 **/
	UPROPERTY()
	TArray<FTransformCurve>	TransformCurves;
#endif // #if WITH_EDITOR_DATA

	/**
	 * Evaluate curve data at the time CurrentTime, and add to Instance. It only evaluates Float Curve for now
	 *
	 * return true if curve exists, false otherwise
	 */
	void EvaluateCurveData( FBlendedCurve& Curves, float CurrentTime ) const;

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
	 * Delete all curve data 
	 */
	ENGINE_API void DeleteAllCurveData(ESupportedCurveType SupportedCurveType = FloatType);

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
	void Resize(float TotalLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);
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

	void SortFloatCurvesByUID()
	{
		struct FCurveSortByUid
		{
			FORCEINLINE bool operator()(const FFloatCurve& A, const FFloatCurve& B) const
			{
				return (A.CurveUid < B.CurveUid);
			}
		};

		FloatCurves.Sort(FCurveSortByUid());
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