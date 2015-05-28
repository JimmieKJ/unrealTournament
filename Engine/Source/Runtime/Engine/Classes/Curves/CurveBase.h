// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveBase.generated.h"

//////////////////////////////////////////////////////////////////////////
// FIndexedCurve

/** Key handles are used to keep a handle to a key. They are completely transient. */
struct FKeyHandle
{
	ENGINE_API FKeyHandle();

	bool operator == (const FKeyHandle& Other) const
	{
		return Index == Other.Index;
	}
	
	bool operator != (const FKeyHandle& Other) const
	{
		return Index != Other.Index;
	}
	
	friend uint32 GetTypeHash( const FKeyHandle& Handle )
	{
		return GetTypeHash(Handle.Index);
	}

	friend FArchive& operator<<(FArchive& Ar,FKeyHandle& Handle)
	{
		Ar << Handle.Index;
		return Ar;
	}
private:
	uint32 Index;
};

/**
 * Represents a mapping of key handles to key index that may be serialized 
 */
USTRUCT()
struct FKeyHandleMap
{
	GENERATED_USTRUCT_BODY()
public:
	FKeyHandleMap() {}

	// This struct is not copyable.  This must be public or because derived classes are allowed to be copied
	FKeyHandleMap( const FKeyHandleMap& Other ) {}
	void operator=(const FKeyHandleMap& Other) {}

	/** TMap functionality */
	void Add( const FKeyHandle& InHandle, int32 InIndex );
	void Empty();
	void Remove( const FKeyHandle& InHandle );
	const int32* Find( const FKeyHandle& InHandle ) const;
	const FKeyHandle* FindKey( int32 KeyIndex ) const;
	int32 Num() const;
	TMap<FKeyHandle, int32>::TConstIterator CreateConstIterator() const;
	TMap<FKeyHandle, int32>::TIterator CreateIterator();

	/** ICPPStructOps implementation */
	bool Serialize(FArchive& Ar);
	bool operator==(const FKeyHandleMap& Other) const;
	bool operator!=(const FKeyHandleMap& Other) const;

	friend FArchive& operator<<(FArchive& Ar,FKeyHandleMap& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

private:
	TMap<FKeyHandle, int32> KeyHandlesToIndices;
};

template<>
struct TStructOpsTypeTraits< FKeyHandleMap > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};


/** A curve base class which enables key handles to index lookups */
// @todo Some heavy refactoring can be done here. Much more stuff can go in this base class
USTRUCT()
struct ENGINE_API FIndexedCurve
{
	GENERATED_USTRUCT_BODY()
public:
	FIndexedCurve() {}

	/** Get number of keys in curve. */
	virtual int32 GetNumKeys() const PURE_VIRTUAL(FIndexedCurve::GetNumKeys,return 0;);

	/** Const iterator for the handles */
	TMap<FKeyHandle, int32>::TConstIterator GetKeyHandleIterator() const;
	
	/** Gets the index of a handle, checks if the key handle is valid first */
	int32 GetIndexSafe(FKeyHandle KeyHandle) const;

	/** Checks to see if the key handle is valid for this curve */
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const;

protected:
	/** Internal tool to get a handle from an index */
	FKeyHandle GetKeyHandle(int32 KeyIndex) const;
	
	/** Gets the index of a handle */
	int32 GetIndex(FKeyHandle KeyHandle) const;

	/** Makes sure our handles are all valid and correct */
	void EnsureIndexHasAHandle(int32 KeyIndex) const;
	void EnsureAllIndicesHaveHandles() const;

protected:
	/** Map of which key handles go to which indices */
	UPROPERTY(transient)
	mutable FKeyHandleMap KeyHandlesToIndices;
};

//////////////////////////////////////////////////////////////////////////
// Rich curve data

/** Method of interpolation between this key and the next */
UENUM()
enum ERichCurveInterpMode
{
	RCIM_Linear,
	RCIM_Constant,
	RCIM_Cubic
};

/** If using RCIM_Cubic, this enum describes how the tangents should be controlled in editor */
UENUM()
enum ERichCurveTangentMode
{
	RCTM_Auto,
	RCTM_User,
	RCTM_Break
};

/** Enum to indicate whether if a tangent is 'weighted' (ie can be stretched) */
UENUM()
enum ERichCurveTangentWeightMode
{
	RCTWM_WeightedNone,
	RCTWM_WeightedArrive,
	RCTWM_WeightedLeave,
	RCTWM_WeightedBoth
};

/** One key in a rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurveKey
{
	GENERATED_USTRUCT_BODY()

	/** Interpolation mode between this key and the next */
	UPROPERTY()
	TEnumAsByte<ERichCurveInterpMode>			InterpMode;

	/** Mode for tangents at this key */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentMode>			TangentMode;

	/** If either tangent at this key is 'weighted' */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentWeightMode>	TangentWeightMode;

	/** Time at this key */
	UPROPERTY()
	float Time;

	/** Value at this key */
	UPROPERTY()
	float Value;

	/** If RCIM_Cubic, the arriving tangent at this key */
	UPROPERTY()
	float ArriveTangent;

	/** If RCTWM_WeightedArrive or RCTWM_WeightedBoth, the weight of the left tangent */
	UPROPERTY()
	float ArriveTangentWeight;

	/** If RCIM_Cubic, the leaving tangent at this key */
	UPROPERTY()
	float LeaveTangent;

	/** If RCTWM_WeightedLeave or RCTWM_WeightedBoth, the weight of the right tangent */
	UPROPERTY()
	float LeaveTangentWeight;

	FRichCurveKey()
	: InterpMode(RCIM_Linear)
	, TangentMode(RCTM_Auto)
	, TangentWeightMode(RCTWM_WeightedNone)
	, Time(0.f)
	, Value(0.f)
	, ArriveTangent(0.f)
	, ArriveTangentWeight(0.f)
	, LeaveTangent(0.f)
	, LeaveTangentWeight(0.f)
	{}

	FRichCurveKey(float InTime, float InValue)
	: InterpMode(RCIM_Linear)
	, TangentMode(RCTM_Auto)
	, TangentWeightMode(RCTWM_WeightedNone)
	, Time(InTime)
	, Value(InValue)
	, ArriveTangent(0.f)
	, ArriveTangentWeight(0.f)
	, LeaveTangent(0.f)
	, LeaveTangentWeight(0.f)
	{}

	FRichCurveKey(float InTime, float InValue, float InArriveTangent, const float InLeaveTangent, ERichCurveInterpMode InInterpMode)
	: InterpMode(InInterpMode)
	, TangentMode(RCTM_Auto)
	, TangentWeightMode(RCTWM_WeightedNone)
	, Time(InTime)
	, Value(InValue)
	, ArriveTangent(InArriveTangent)
	, ArriveTangentWeight(0.f)
	, LeaveTangent(InLeaveTangent)
	, LeaveTangentWeight(0.f)
	{}

	/** Conversion constructor */
	FRichCurveKey(const FInterpCurvePoint<float>& InPoint);
	FRichCurveKey(const FInterpCurvePoint<FVector>& InPoint, int32 ComponentIndex);

	/** ICPPStructOps interface */
	bool Serialize(FArchive& Ar);
	bool operator==(const FRichCurveKey& Other) const;
	bool operator!=(const FRichCurveKey& Other) const;

	friend FArchive& operator<<(FArchive& Ar, FRichCurveKey& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

};

template<> struct TIsPODType<FRichCurveKey> { enum { Value = true }; };

template<>
struct TStructOpsTypeTraits< FRichCurveKey > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};

/** A rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurve : public FIndexedCurve
{
	GENERATED_USTRUCT_BODY()

public:
	virtual ~FRichCurve()
	{
	}

	/** Gets a copy of the keys, so indices and handles can't be meddled with */
	TArray<FRichCurveKey> GetCopyOfKeys() const;

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FRichCurveKey>::TConstIterator GetKeyIterator() const;
	
	/** Functions for getting keys based on handles */
	FRichCurveKey& GetKey(FKeyHandle KeyHandle);
	FRichCurveKey GetKey(FKeyHandle KeyHandle) const;
	
	/** Quick accessors for the first and last keys */
	FRichCurveKey GetFirstKey() const;
	FRichCurveKey GetLastKey() const;

	/** Get number of key in curve. */
	virtual int32 GetNumKeys() const override;
	
	/** Checks to see if the key handle is valid for this curve */
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const override;

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param	bUnwindRotation		When true, the value will be treated like a rotation value in degrees, and will automatically be unwound to prevent flipping 360 degrees from the previous key 
	  * @param  KeyHandle			Optionally can specify what handle this new key should have, otherwise, it'll make a new one
	  */
	FKeyHandle AddKey(float InTime, float InValue, const bool bUnwindRotation = false, FKeyHandle KeyHandle = FKeyHandle());

	/** Remove the specified key from the curve.*/
	void DeleteKey(FKeyHandle KeyHandle);

	/** Finds the key at InTime, and updates its value. If it can't find the key, it adds one at that time */
	FKeyHandle UpdateOrAddKey(float InTime, float InValue);

	/** Move a key to a new time. This may change the index of the key, so the new key index is returned. */
	FKeyHandle SetKeyTime(FKeyHandle KeyHandle, float NewTime);

	/** Get the time for the Key with the specified index. */
	float GetKeyTime(FKeyHandle KeyHandle) const;

	/** Finds a key a the specified time */
	FKeyHandle FindKey( float KeyTime ) const;

	/** Set the value of the specified key */
	void SetKeyValue(FKeyHandle KeyHandle, float NewValue, bool bAutoSetTangents=true);

	/** Returns the value of the specified key */
	float GetKeyValue(FKeyHandle KeyHandle) const;

	/** Shifts all keys forwards or backwards in time by an even amount, preserving order */
	void ShiftCurve(float DeltaTime);
	
	/** Scales all keys about an origin, preserving order */
	void ScaleCurve(float ScaleOrigin, float ScaleFactor);

	/** Set the interp mode of the specified key */
	void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode);

	/** Set the tangent mode of the specified key */
	void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode NewTangentMode);

	/** Set the tangent weight mode of the specified key */
	void SetKeyTangentWeightMode(FKeyHandle KeyHandle, ERichCurveTangentWeightMode NewTangentWeightMode);

	/** Get the interp mode of the specified key */
	ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const;

	/** Get the tangent mode of the specified key */
	ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const;

	/** Get range of input time values. Outside this region curve continues constantly the start/end values. */
	void GetTimeRange(float& MinTime, float& MaxTime) const;

	/** Get range of output values. */
	void GetValueRange(float& MinValue, float& MaxValue) const;

	/** Clear all keys. */
	void Reset();

	/** Evaluate this rich curve at the specified time */
	float Eval(float InTime, float DefaultValue = 0.0f) const;

	/** Auto set tangents for any 'auto' keys in curve */
	void AutoSetTangents(float Tension = 0.f);

	/** Resize curve length to the [MinTimeRange, MaxTimeRange] */
	void ResizeTimeRange(float NewMinTimeRange, float NewMaxTimeRange);

	/** Determine if two RichCurves are the same */
	bool operator == (const FRichCurve& Curve) const;
private:
	/** Sorted array of keys */
	UPROPERTY()
	TArray<FRichCurveKey> Keys;
};

//////////////////////////////////////////////////////////////////////////
// Curve editor interface

/** Info about a curve to be edited */
template<class T> struct FRichCurveEditInfoTemplate
{
	/** Name of curve, used when displaying in editor. Can include comma's to allow tree expansion in editor */
	FName			CurveName;

	/** Pointer to curves to be edited */
	T				CurveToEdit;

	FRichCurveEditInfoTemplate()
		: CurveName(NAME_None)
		, CurveToEdit(nullptr)
	{}

	FRichCurveEditInfoTemplate(T InCurveToEdit)
	:	CurveName(NAME_None)
	,	CurveToEdit(InCurveToEdit)
	{}

	FRichCurveEditInfoTemplate(T InCurveToEdit, FName InCurveName)
	:	CurveName(InCurveName)
	,	CurveToEdit(InCurveToEdit)
	{}

	inline bool operator==(const FRichCurveEditInfoTemplate<T>& Other) const
	{
		return Other.CurveName.IsEqual(CurveName) && Other.CurveToEdit == CurveToEdit;
	}

	uint32 GetTypeHash() const
	{
		return HashCombine(::GetTypeHash(CurveName), PointerHash(CurveToEdit));
	}
};

template<class T>
inline uint32 GetTypeHash( const FRichCurveEditInfoTemplate<T>& RichCurveEditInfo )
{
	return RichCurveEditInfo.GetTypeHash();
}

typedef FRichCurveEditInfoTemplate<FRichCurve*>			FRichCurveEditInfo;
typedef FRichCurveEditInfoTemplate<const FRichCurve*>	FRichCurveEditInfoConst;


/** Interface you implement if you want the CurveEditor to be able to edit curves on you */
class FCurveOwnerInterface
{
public:
	virtual ~FCurveOwnerInterface() {}

	/** Returns set of curves to edit. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const = 0;

	/** Returns set of curves to query. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfo> GetCurves() = 0;

	/** Called to modify the owner of the curve */
	virtual void ModifyOwner() = 0;

	/** Called to make curve owner transactional */
	virtual void MakeTransactional() = 0;

	/** Called when any of the curves have been changed */
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) = 0;

	/** Whether the curve represents a linear color */
	virtual bool IsLinearColorCurve() const { return false; }

	/** Evaluate this color curve at the specified time */
	virtual FLinearColor GetLinearColorValue(float InTime) const { return FLinearColor::Black; }

	/** @return True if the curve has any alpha keys */
	virtual bool HasAnyAlphaKeys() const { return false; }

	/** Validates that a previously retrieved curve is still valid for editing. */
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) = 0;
};


//////////////////////////////////////////////////////////////////////////

/**
 * Defines a curve of interpolated points to evaluate over a given range
 */
UCLASS(abstract)
class ENGINE_API UCurveBase : public UObject, public FCurveOwnerInterface
{
	GENERATED_UCLASS_BODY()

	/** The filename imported to create this object. Relative to this object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath;

	/** Get the time range across all curves */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	void GetTimeRange(float& MinTime, float& MaxTime) const;

	/** Get the value range across all curves */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	void GetValueRange(float& MinValue, float& MaxValue) const;

public:
	// Begin FCurveOwnerInterface
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override
	{
		TArray<FRichCurveEditInfoConst> Curves;
		return Curves;
	}

	virtual TArray<FRichCurveEditInfo> GetCurves() override
	{
		TArray<FRichCurveEditInfo> Curves;
		return Curves;
	}

	virtual void ModifyOwner() override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override { return false; };
	// End FCurveOwnerInterface

	// Begin UCurveBase interface

	/** 
	 *	Create curve from CSV style comma-separated string. 
	 *	@return	Set of problems encountered while processing input
	 */
	TArray<FString> CreateCurveFromCSVString(const FString& InString);

	/** Reset all curve data */
	void ResetCurve();

	// End UCurveBase interface

	// Begin UObject interface
#if WITH_EDITORONLY_DATA
	/** Override to ensure we write out the asset import data */
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	// End UObject interface
};

//////////////////////////////////////////////////////////////////////////

/** An integral key, which holds the key time and the key value */
USTRUCT()
struct FIntegralKey
{
	GENERATED_USTRUCT_BODY()
public:
	FIntegralKey(float InTime = 0.f, int32 InValue = 0)
		: Time(InTime)
		, Value(InValue) {}
	
	/** The keyed time */
	UPROPERTY()
	float Time;

	/** The keyed integral value */
	UPROPERTY()
	int32 Value;
};

/** An integral curve, which holds the key time and the key value */
USTRUCT()
struct ENGINE_API FIntegralCurve : public FIndexedCurve
{
	GENERATED_USTRUCT_BODY()
public:
	
	/** Get number of keys in curve. */
	virtual int32 GetNumKeys() const override;
	
	/** Checks to see if the key handle is valid for this curve */
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const override;

	/** Evaluates the value of an array of keys at a time */
	int32 Evaluate(float Time) const;

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FIntegralKey>::TConstIterator GetKeyIterator() const;
	
	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param  KeyHandle			Optionally can specify what handle this new key should have, otherwise, it'll make a new one
	  */
	FKeyHandle AddKey( float InTime, int32 InValue, FKeyHandle KeyHandle = FKeyHandle() );
	
	/** Remove the specified key from the curve.*/
	void DeleteKey(FKeyHandle KeyHandle);
	
	/** Finds the key at InTime, and updates its value. If it can't find the key, it adds one at that time */
	FKeyHandle UpdateOrAddKey( float Time, int32 Value );
	
	/** Move a key to a new time. This may change the index of the key, so the new key index is returned. */
	FKeyHandle SetKeyTime(FKeyHandle KeyHandle, float NewTime);

	/** Get the time for the Key with the specified index. */
	float GetKeyTime(FKeyHandle KeyHandle) const;
	
	/** Shifts all keys forwards or backwards in time by an even amount, preserving order */
	void ShiftCurve(float DeltaTime);
	
	/** Scales all keys about an origin, preserving order */
	void ScaleCurve(float ScaleOrigin, float ScaleFactor);

	/** Functions for getting keys based on handles */
	FIntegralKey& GetKey(FKeyHandle KeyHandle);
	FIntegralKey GetKey(FKeyHandle KeyHandle) const;

private:
	/** The keys, ordered by time */
	UPROPERTY()
	TArray<FIntegralKey> Keys;
};
