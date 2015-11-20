// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once	
#include "VectorVMDataObject.generated.h"


/* Vector VM data object; encapsulates buffers, curves and other data in its derivatives
*  for access by VectorVM kernels;
*/
UCLASS(EditInlineNew, MinimalAPI)
class UNiagaraDataObject : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	virtual FVector4 Sample(const FVector4& InCoords) const { return FVector4(0.0f, 0.0f, 0.0f, 0.0f); }
	virtual FVector4 Write(const FVector4& InCoords, const FVector4& InValue) { return FVector4(0.0f, 0.0f, 0.0f, 0.0f); }
};

/* Curve object; encapsulates a curve for the VectorVM
*/
UCLASS(EditInlineNew, MinimalAPI)
class UNiagaraCurveDataObject : public UNiagaraDataObject
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, Category="Curve")
	class UCurveVector *CurveObj;
public:
	UNiagaraCurveDataObject(class UCurveVector *InCurve) : CurveObj(InCurve)
	{
	}

	FVector4 Sample(const FVector4& InCoords) const;
	virtual FVector4 Write(const FVector4& InCoords, const FVector4& InValue) override
	{
		return FVector4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	UCurveVector *GetCurveObject()	{ return CurveObj; }
	void SetCurveObject(UCurveVector *InCurve)	{ CurveObj = InCurve; }
};


/* Volume data object; encapsulates volumetric data for VectorVM
*/
UCLASS(EditInlineNew, MinimalAPI, Transient)
class UNiagaraSparseVolumeDataObject : public UNiagaraDataObject
{
	GENERATED_UCLASS_BODY()
private:
	TArray<FVector4> Data;
	uint64 NumBuckets;
	int32 Size;
public:
	uint32 MakeHash(const FVector4& InCoords) const
	{
		FVector4 Coords = InCoords;
		const int64 P1 = 73856093; // some large primes
		const int64 P2 = 19349663;
		const int64 P3 = 83492791;
		Coords *= 0.1f;
		uint64 N = (P1*static_cast<int64>(Coords.X)) ^ (P2*static_cast<int64>(Coords.Y)) ^ (P3*static_cast<int64>(Coords.Z));
		N %= NumBuckets;
		return FMath::Clamp(N, 0ULL, NumBuckets);
	}

	virtual FVector4 Sample(const FVector4& InCoords) const override
	{
		int32 Index = MakeHash(InCoords);
		Index = FMath::Clamp(Index, 0, Data.Num() - 1);
		return Data[Index];
	}

	virtual FVector4 Write(const FVector4& InCoords, const FVector4& InValue) override
	{
		Data[MakeHash(InCoords)] = InValue;
		return InValue;
	}
};
