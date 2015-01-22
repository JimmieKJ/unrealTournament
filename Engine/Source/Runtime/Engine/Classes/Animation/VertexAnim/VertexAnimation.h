// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Animation/VertexAnim/VertexAnimBase.h"
#include "VertexAnimation.generated.h"


/** Data for one frame of a vertex animation */
struct FVertexAnimFrame
{
	/**  */
	float						Time;

	/** */
	TArray<FVector>				Deltas;

	/** */
	friend FArchive& operator<<( FArchive& Ar, FVertexAnimFrame& F )
	{
		Ar << F.Time << F.Deltas;
		return Ar;
	}
};


UCLASS(hidecategories=Object, MinimalAPI)
class UVertexAnimation : public UVertexAnimBase
{
	GENERATED_UCLASS_BODY()

	/** Number of verts animated by this animation, should be size of Deltas array in each frame */
	UPROPERTY()
	int32	NumAnimatedVerts;

	/** Raw vertex anim data */
	TArray<FVertexAnimFrame>	VertexAnimData;

	// Begin UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	// Begin UObject interface.

	// Begin UVertexAnimBase interface
	virtual FVertexAnimEvalStateBase* InitEval() override;
	virtual void TermEval(FVertexAnimEvalStateBase* State) override;
	virtual FVertexAnimDelta* GetDeltasAtTime(float Time, int32 LODIndex, FVertexAnimEvalStateBase* State, int32& OutNumDeltas) override;
	virtual bool HasDataForLOD(int32 LODIndex) override;
	// End UVertexAnimBase interface

	/** Get the number of frames in the animation */
	ENGINE_API int32 GetNumFrames();

	/** Get the end time of the animation */
	ENGINE_API float GetAnimLength();
};
