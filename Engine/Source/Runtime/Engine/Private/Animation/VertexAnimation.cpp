// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Animation/VertexAnim/VertexAnimation.h"

UVertexAnimation::UVertexAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVertexAnimation::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	//Ar << VertexAnimData;
}


SIZE_T UVertexAnimation::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

int32 UVertexAnimation::GetNumFrames()
{
	return VertexAnimData.Num();
}

float UVertexAnimation::GetAnimLength()
{
	if(VertexAnimData.Num() > 0)
	{
		return VertexAnimData.Last().Time;
	}
	else
	{
		return 0.f;
	}
}


struct FVertexAnimEvalState : public FVertexAnimEvalStateBase
{
	FVertexAnimEvalState(int32 NumDeltas)
	{
		Deltas = (FVertexAnimDelta*)FMemory::Malloc(NumDeltas * sizeof(FVertexAnimDelta));
	}

	~FVertexAnimEvalState()
	{
		FMemory::Free(Deltas);
	}


	FVertexAnimDelta* Deltas;
};


/** Must be called before calling GetDeltasAtTime to allocate any temporary state */
FVertexAnimEvalStateBase* UVertexAnimation::InitEval()
{
	FSkeletalMeshResource* SkelMeshResource = BaseSkelMesh ? BaseSkelMesh->GetImportedResource() : NULL;
	if(	VertexAnimData.Num() > 0 && 
		SkelMeshResource != NULL &&
		SkelMeshResource->LODModels.Num() > 0 &&
		SkelMeshResource->LODModels[0].MeshToImportVertexMap.Num() > 0)
	{
		FVertexAnimEvalState* State = new FVertexAnimEvalState(SkelMeshResource->LODModels[0].MeshToImportVertexMap.Num());
		return State;
	}
	else
	{
		return NULL;
	}
}

/** Must be called when you are finished evaluating animation, will release any State */
void UVertexAnimation::TermEval(FVertexAnimEvalStateBase* State)
{
	if(State != NULL)
	{
		delete State;
	}
}

/** Get vertex deltas for this animation at a particular time. Must call InitEval before calling this, and pass in State allocated. */
FVertexAnimDelta* UVertexAnimation::GetDeltasAtTime(float Time, int32 LODIndex, FVertexAnimEvalStateBase* State, int32& OutNumDeltas)
{
	FVertexAnimEvalState* VertAnimState = (FVertexAnimEvalState*)State;
	if(VertAnimState != NULL)
	{
		check(VertexAnimData.Num() > 0);

		TArray<FVector> ImportVertDeltas;
		ImportVertDeltas.AddUninitialized(NumAnimatedVerts);

		// If time is before first frame (or only 1 frame)
		if(Time <= VertexAnimData[0].Time || VertexAnimData.Num() == 1)
		{
			FMemory::Memcpy(ImportVertDeltas.GetData(), VertexAnimData[0].Deltas.GetData(), NumAnimatedVerts);
		}
		// If time is after last frame
		else if(Time >= VertexAnimData.Last().Time)
		{
			FMemory::Memcpy(ImportVertDeltas.GetData(), VertexAnimData.Last().Deltas.GetData(), NumAnimatedVerts);
		}
		// Find frames to interpolate
		else
		{
			int32 AfterFrameIdx = INDEX_NONE;
			for(int32 FrameIdx=1; FrameIdx<VertexAnimData.Num(); FrameIdx++)
			{
				if(Time < VertexAnimData[FrameIdx].Time)
				{
					AfterFrameIdx = FrameIdx;
					break;
				}
			}
			check(AfterFrameIdx != INDEX_NONE); // We ensure above that Time < VertexAnimData.Last().Time

			// Interpolate between FrameIdx and FrameIdx-1
			const FVertexAnimFrame& A = VertexAnimData[AfterFrameIdx-1];
			const FVertexAnimFrame& B = VertexAnimData[AfterFrameIdx];
			const float Alpha = (Time - A.Time)/(B.Time - A.Time);

			check(A.Deltas.Num() == NumAnimatedVerts);
			check(B.Deltas.Num() == NumAnimatedVerts);

			for(int32 DeltaIdx=0; DeltaIdx<NumAnimatedVerts; DeltaIdx++)
			{
				ImportVertDeltas[DeltaIdx] = FMath::Lerp<FVector>(A.Deltas[DeltaIdx], B.Deltas[DeltaIdx], Alpha);
			}

		}

		// So now we have all the position deltas for import verts, need to convert this into FVertexAnimDelta, one for each final vert.
		OutNumDeltas = BaseSkelMesh->GetImportedResource()->LODModels[0].MeshToImportVertexMap.Num();

		for(int32 VertIdx=0; VertIdx<OutNumDeltas; VertIdx++)
		{
			int32 ImportIdx = BaseSkelMesh->GetImportedResource()->LODModels[0].MeshToImportVertexMap[VertIdx];

			FVertexAnimDelta* Delta = VertAnimState->Deltas + VertIdx;
			Delta->PositionDelta = ImportVertDeltas[ImportIdx];
			Delta->TangentZDelta = FVector::ZeroVector;
			Delta->SourceIdx = VertIdx;
		}

		return VertAnimState->Deltas;
	}
	else
	{
		return NULL;
	}
}

bool UVertexAnimation::HasDataForLOD(int32 LODIndex)
{
	return (LODIndex == 0);
}
