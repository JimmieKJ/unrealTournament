// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "VertexAnimBase.generated.h"


/** Morph mesh vertex data used for rendering */
struct FVertexAnimDelta
{
	/** change in position */
	FVector			PositionDelta;
	/** Tangent basis normal */
	FVector			TangentZDelta;

	/** index of source vertex to apply deltas to */
	uint32			SourceIdx;

	/** pipe operator */
	friend FArchive& operator<<( FArchive& Ar, FVertexAnimDelta& V )
	{
		if ( Ar.UE4Ver() < VER_UE4_MORPHTARGET_CPU_TANGENTZDELTA_FORMATCHANGE )
		{
			/** old format of change in tangent basis normal */
			FPackedNormal	TangentZDelta_DEPRECATED;
			if (Ar.IsSaving())
			{
				TangentZDelta_DEPRECATED = FPackedNormal(V.TangentZDelta);
			}

			Ar << V.PositionDelta << TangentZDelta_DEPRECATED << V.SourceIdx;

			if (Ar.IsLoading())
			{
				V.TangentZDelta = FVector(TangentZDelta_DEPRECATED);
			}
		}
		else
		{
			Ar << V.PositionDelta << V.TangentZDelta << V.SourceIdx;
		}
		return Ar;
	}
};

/** Subclassed by different vertex anim types if they need intermediate state for decompressing */
struct FVertexAnimEvalStateBase
{

};





UCLASS(hidecategories=Object, MinimalAPI, abstract)
class UVertexAnimBase : public UObject
{
	GENERATED_UCLASS_BODY()

	/** USkeletalMesh that this vertex animation works on. */
	UPROPERTY(AssetRegistrySearchable)
	class USkeletalMesh* BaseSkelMesh;

	/** Must be called before calling GetDeltasAtTime to allocate any temporary state */
	virtual FVertexAnimEvalStateBase* InitEval()
	{
		return NULL;
	}

	/** Must be called when you are finished evaluating animation, will release any State */
	virtual void TermEval(FVertexAnimEvalStateBase* State)
	{
		check(State == NULL);
	}

	/** Get vertex deltas for this animation at a particular time. Must call InitEval before calling this, and pass in State allocated. */
	virtual FVertexAnimDelta* GetDeltasAtTime(float Time, int32 LODIndex, FVertexAnimEvalStateBase* State, int32& OutNumDeltas)
	{
		check(State != NULL);
		OutNumDeltas = 0;
		return NULL;
	}

	virtual bool HasDataForLOD(int32 LODIndex)
	{
		return false;
	}
};
