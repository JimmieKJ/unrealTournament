// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/SplineMeshComponent.h"
#include "SplineMeshSceneProxy.h"
#include "ShaderParameterUtils.h"
#include "NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"


//////////////////////////////////////////////////////////////////////////
// FSplineMeshVertexFactoryShaderParameters

void FSplineMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	SplineStartPosParam.Bind(ParameterMap, TEXT("SplineStartPos"), SPF_Mandatory);
	SplineStartTangentParam.Bind(ParameterMap, TEXT("SplineStartTangent"), SPF_Mandatory);
	SplineStartRollParam.Bind(ParameterMap, TEXT("SplineStartRoll"), SPF_Mandatory);
	SplineStartScaleParam.Bind(ParameterMap, TEXT("SplineStartScale"), SPF_Mandatory);
	SplineStartOffsetParam.Bind(ParameterMap, TEXT("SplineStartOffset"), SPF_Mandatory);

	SplineEndPosParam.Bind(ParameterMap, TEXT("SplineEndPos"), SPF_Mandatory);
	SplineEndTangentParam.Bind(ParameterMap, TEXT("SplineEndTangent"), SPF_Mandatory);
	SplineEndRollParam.Bind(ParameterMap, TEXT("SplineEndRoll"), SPF_Mandatory);
	SplineEndScaleParam.Bind(ParameterMap, TEXT("SplineEndScale"), SPF_Mandatory);
	SplineEndOffsetParam.Bind(ParameterMap, TEXT("SplineEndOffset"), SPF_Mandatory);

	SplineUpDirParam.Bind(ParameterMap, TEXT("SplineUpDir"), SPF_Mandatory);
	SmoothInterpRollScaleParam.Bind(ParameterMap, TEXT("SmoothInterpRollScale"), SPF_Mandatory);

	SplineMeshMinZParam.Bind(ParameterMap, TEXT("SplineMeshMinZ"), SPF_Mandatory);
	SplineMeshScaleZParam.Bind(ParameterMap, TEXT("SplineMeshScaleZ"), SPF_Mandatory);

	SplineMeshDirParam.Bind(ParameterMap, TEXT("SplineMeshDir"), SPF_Mandatory);
	SplineMeshXParam.Bind(ParameterMap, TEXT("SplineMeshX"), SPF_Mandatory);
	SplineMeshYParam.Bind(ParameterMap, TEXT("SplineMeshY"), SPF_Mandatory);
}

void FSplineMeshVertexFactoryShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	if (Shader->GetVertexShader())
	{
		FSplineMeshVertexFactory* SplineVertexFactory = (FSplineMeshVertexFactory*)VertexFactory;
		FSplineMeshSceneProxy* SplineProxy = SplineVertexFactory->SplineSceneProxy;
		FSplineMeshParams& SplineParams = SplineProxy->SplineParams;

		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineStartPosParam, SplineParams.StartPos);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineStartTangentParam, SplineParams.StartTangent);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineStartRollParam, SplineParams.StartRoll);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineStartScaleParam, SplineParams.StartScale);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineStartOffsetParam, SplineParams.StartOffset);

		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineEndPosParam, SplineParams.EndPos);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineEndTangentParam, SplineParams.EndTangent);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineEndRollParam, SplineParams.EndRoll);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineEndScaleParam, SplineParams.EndScale);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineEndOffsetParam, SplineParams.EndOffset);

		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineUpDirParam, SplineProxy->SplineUpDir);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SmoothInterpRollScaleParam, SplineProxy->bSmoothInterpRollScale);

		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineMeshMinZParam, SplineProxy->SplineMeshMinZ);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineMeshScaleZParam, SplineProxy->SplineMeshScaleZ);

		FVector DirMask(0, 0, 0);
		DirMask[SplineProxy->ForwardAxis] = 1;
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineMeshDirParam, DirMask);
		DirMask = FVector::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 1) % 3] = 1;
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineMeshXParam, DirMask);
		DirMask = FVector::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 2) % 3] = 1;
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), SplineMeshYParam, DirMask);
	}
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshVertexFactory

IMPLEMENT_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory, "LocalVertexFactory", true, true, true, true, true);


FVertexFactoryShaderParameters* FSplineMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FSplineMeshVertexFactoryShaderParameters() : NULL;
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy



void FSplineMeshSceneProxy::InitResources(USplineMeshComponent* InComponent, int32 InLODIndex)
{
	// Initialize the static mesh's vertex factory.
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		InitSplineMeshVertexFactory,
		FSplineMeshVertexFactory*, VertexFactory, LODResources[InLODIndex].VertexFactory,
		const FStaticMeshLODResources*, RenderData, &InComponent->StaticMesh->RenderData->LODResources[InLODIndex],
		UStaticMesh*, Parent, InComponent->StaticMesh,
		{
		FLocalVertexFactory::DataType Data;

		Data.PositionComponent = FVertexStreamComponent(
			&RenderData->PositionVertexBuffer,
			STRUCT_OFFSET(FPositionVertex, Position),
			RenderData->PositionVertexBuffer.GetStride(),
			VET_Float3
			);
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex, TangentX),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex, TangentZ),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);

		if (RenderData->ColorVertexBuffer.GetNumVertices() > 0)
		{
			Data.ColorComponent = FVertexStreamComponent(
				&RenderData->ColorVertexBuffer,
				0,	// Struct offset to color
				RenderData->ColorVertexBuffer.GetStride(),
				VET_Color
				);
		}

		Data.TextureCoordinates.Empty();

		if (!RenderData->VertexBuffer.GetUseFullPrecisionUVs())
		{
			int32 UVIndex;
			for (UVIndex = 0; UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords() - 1; UVIndex += 2)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2DHalf) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half4
					));
			}
			// possible last UV channel if we have an odd number
			if (UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2DHalf) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					));
			}

			if (Parent->LightMapCoordinateIndex >= 0 && (uint32)Parent->LightMapCoordinateIndex < RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.LightMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2DHalf) * Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					);
			}
		}
		else
		{
			int32 UVIndex;
			for (UVIndex = 0; UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords() - 1; UVIndex += 2)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2D) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float4
					));
			}
			// possible last UV channel if we have an odd number
			if (UVIndex < (int32)RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2D) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					));
			}

			if (Parent->LightMapCoordinateIndex >= 0 && (uint32)Parent->LightMapCoordinateIndex < RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.LightMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2D) * Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					);
			}
		}

		VertexFactory->SetData(Data);

		VertexFactory->InitResource();
	});
}


void FSplineMeshSceneProxy::ReleaseResources()
{
	for (FSplineMeshSceneProxy::FLODResources& LODResource : LODResources)
	{
		LODResource.VertexFactory->ReleaseResource();
	}
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshComponent

USplineMeshComponent::USplineMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Static;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bAllowSplineEditingPerInstance = false;
	bSmoothInterpRollScale = false;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	SplineUpDir.Z = 1.0f;

	// Default to useful length and scale
	SplineParams.StartTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.StartScale = FVector2D(1.f, 1.f);

	SplineParams.EndPos = FVector(100.f, 0.f, 0.f);
	SplineParams.EndTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.EndScale = FVector2D(1.f, 1.f);

	SplineBoundaryMin = 0;
	SplineBoundaryMax = 0;
}

FVector USplineMeshComponent::GetStartPosition() const
{
	return SplineParams.StartPos;
}

void USplineMeshComponent::SetStartPosition(FVector StartPos)
{
	SplineParams.StartPos = StartPos;
	MarkSplineParamsDirty();
}

FVector USplineMeshComponent::GetStartTangent() const
{
	return SplineParams.StartTangent;
}

void USplineMeshComponent::SetStartTangent(FVector StartTangent)
{
	SplineParams.StartTangent = StartTangent;
	MarkSplineParamsDirty();
}

FVector USplineMeshComponent::GetEndPosition() const
{
	return SplineParams.EndPos;
}

void USplineMeshComponent::SetEndPosition(FVector EndPos)
{
	SplineParams.EndPos = EndPos;
	MarkSplineParamsDirty();
}

FVector USplineMeshComponent::GetEndTangent() const
{
	return SplineParams.EndTangent;
}

void USplineMeshComponent::SetEndTangent(FVector EndTangent)
{
	SplineParams.EndTangent = EndTangent;
	MarkSplineParamsDirty();
}

void USplineMeshComponent::SetStartAndEnd(FVector StartPos, FVector StartTangent, FVector EndPos, FVector EndTangent)
{
	SplineParams.StartPos = StartPos;
	SplineParams.StartTangent = StartTangent;
	SplineParams.EndPos = EndPos;
	SplineParams.EndTangent = EndTangent;
	MarkSplineParamsDirty();
}

FVector2D USplineMeshComponent::GetStartScale() const
{
	return SplineParams.StartScale;
}

void USplineMeshComponent::SetStartScale(FVector2D StartScale)
{
	SplineParams.StartScale = StartScale;
	MarkSplineParamsDirty();
}

float USplineMeshComponent::GetStartRoll() const
{
	return SplineParams.StartRoll;
}

void USplineMeshComponent::SetStartRoll(float StartRoll)
{
	SplineParams.StartRoll = StartRoll;
	MarkSplineParamsDirty();
}

FVector2D USplineMeshComponent::GetStartOffset() const
{
	return SplineParams.StartOffset;
}

void USplineMeshComponent::SetStartOffset(FVector2D StartOffset)
{
	SplineParams.StartOffset = StartOffset;
	MarkSplineParamsDirty();
}

FVector2D USplineMeshComponent::GetEndScale() const
{
	return SplineParams.EndScale;
}

void USplineMeshComponent::SetEndScale(FVector2D EndScale)
{
	SplineParams.EndScale = EndScale;
	MarkSplineParamsDirty();
}

float USplineMeshComponent::GetEndRoll() const
{
	return SplineParams.EndRoll;
}

void USplineMeshComponent::SetEndRoll(float EndRoll)
{
	SplineParams.EndRoll = EndRoll;
	MarkSplineParamsDirty();
}

FVector2D USplineMeshComponent::GetEndOffset() const
{
	return SplineParams.EndOffset;
}

void USplineMeshComponent::SetEndOffset(FVector2D EndOffset)
{
	SplineParams.EndOffset = EndOffset;
	MarkSplineParamsDirty();
}

ESplineMeshAxis::Type USplineMeshComponent::GetForwardAxis() const
{
	return ForwardAxis;
}

void USplineMeshComponent::SetForwardAxis(ESplineMeshAxis::Type InForwardAxis)
{
	ForwardAxis = InForwardAxis;
	MarkSplineParamsDirty();
}

FVector USplineMeshComponent::GetSplineUpDir() const
{
	return SplineUpDir;
}

void USplineMeshComponent::SetSplineUpDir(const FVector& InSplineUpDir)
{
	SplineUpDir = InSplineUpDir.GetSafeNormal();
	MarkSplineParamsDirty();
}

float USplineMeshComponent::GetBoundaryMin() const
{
	return SplineBoundaryMin;
}

void USplineMeshComponent::SetBoundaryMin(float InBoundaryMin)
{
	SplineBoundaryMin = InBoundaryMin;
	MarkSplineParamsDirty();
}

float USplineMeshComponent::GetBoundaryMax() const
{
	return SplineBoundaryMax;
}

void USplineMeshComponent::SetBoundaryMax(float InBoundaryMax)
{
	SplineBoundaryMax = InBoundaryMax;
	MarkSplineParamsDirty();
}


void USplineMeshComponent::MarkSplineParamsDirty()
{
	MarkRenderStateDirty();

#if WITH_EDITOR
	if (!GetWorld()->AreActorsInitialized())
	{
		RecreateCollision();
	}
#endif // WITH_EDITOR
}

void USplineMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_SPLINE_MESH_ORIENTATION)
	{
		ForwardAxis = ESplineMeshAxis::Z;
		SplineParams.StartRoll -= HALF_PI;
		SplineParams.EndRoll -= HALF_PI;

		float Temp = SplineParams.StartOffset.X;
		SplineParams.StartOffset.X = -SplineParams.StartOffset.Y;
		SplineParams.StartOffset.Y = Temp;
		Temp = SplineParams.EndOffset.X;
		SplineParams.EndOffset.X = -SplineParams.EndOffset.Y;
		SplineParams.EndOffset.Y = Temp;
	}

#if WITH_EDITOR
	if (BodySetup != NULL)
	{
		BodySetup->SetFlags(RF_Transactional);
	}
#endif
}

bool USplineMeshComponent::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	if (BodySetup != NULL)
	{
		BodySetup->Modify(bAlwaysMarkDirty);
	}

	return bSavedToTransactionBuffer;
}

FPrimitiveSceneProxy* USplineMeshComponent::CreateSceneProxy()
{
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData();

	if (bMeshIsValid)
	{
		return ::new FSplineMeshSceneProxy(this);
	}
	else
	{
		return NULL;
	}
}


/**
* Functions used for transforming a static mesh component based on a spline.
* This needs to be updated if the spline functionality changes!
*/
static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

static FVector SplineEvalPos(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const float A2 = A  * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector SplineEvalDir(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const FVector C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector E = StartTangent;

	const float A2 = A  * A;

	return ((C * A2) + (D * A) + E).GetSafeNormal();
}


FBoxSphereBounds USplineMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!StaticMesh)
	{
		return FBoxSphereBounds(FBox(0));
	}

	float MinT = 0.0f;
	float MaxT = 1.0f;

	const FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();

	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		// If there's a custom boundary, alter the min/max of the spline we need to evaluate
		const float MeshMin = GetAxisValue(MeshBounds.Origin - MeshBounds.BoxExtent, ForwardAxis);
		const float MeshMax = GetAxisValue(MeshBounds.Origin + MeshBounds.BoxExtent, ForwardAxis);

		const float MeshMinT = (MeshMin - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
		const float MeshMaxT = (MeshMax - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);

		// Disallow extrapolation beyond a certain value; enormous bounding boxes cause the render thread to crash
		const float MaxSplineExtrapolation = 4.0f;
		if (FMath::Abs(MeshMinT) < MaxSplineExtrapolation && FMath::Abs(MeshMaxT) < MaxSplineExtrapolation)
		{
			MinT = MeshMinT;
			MaxT = MeshMaxT;
		}
	}

	const FVector AxisMask = GetAxisMask(ForwardAxis);
	const FVector FlattenedMeshOrigin = MeshBounds.Origin * AxisMask;
	const FVector FlattenedMeshExtent = MeshBounds.BoxExtent * AxisMask;
	const FBox MeshBoundingBox = FBox(FlattenedMeshOrigin - FlattenedMeshExtent, FlattenedMeshOrigin + FlattenedMeshExtent);

	FBox BoundingBox(0);
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MinT));
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MaxT));

	// Work out coefficients of the cubic spline derivative equation dx/dt
	const FVector A = 6.0f * SplineParams.StartPos + 3.0f * SplineParams.StartTangent + 3.0f * SplineParams.EndTangent - 6.0f * SplineParams.EndPos;
	const FVector B = -6.0f * SplineParams.StartPos - 4.0f * SplineParams.StartTangent - 2.0f * SplineParams.EndTangent + 6.0f * SplineParams.EndPos;
	const FVector C = SplineParams.StartTangent;

	// Minima/maxima happen where dx/dt == 0, calculate t values
	const FVector Discriminant = B * B - 4.0f * A * C;

	// Work out minima/maxima component-by-component.
	// Negative discriminant means no solution; A == 0 implies coincident start/end points
	if (Discriminant.X > 0.0f && !FMath::IsNearlyZero(A.X))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.X);
		const float Denominator = 0.5f / A.X;
		const float T0 = (-B.X + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.X - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Y > 0.0f && !FMath::IsNearlyZero(A.Y))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Y);
		const float Denominator = 0.5f / A.Y;
		const float T0 = (-B.Y + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Y - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Z > 0.0f && !FMath::IsNearlyZero(A.Z))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Z);
		const float Denominator = 0.5f / A.Z;
		const float T0 = (-B.Z + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Z - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
}


FTransform USplineMeshComponent::CalcSliceTransform(const float DistanceAlong) const
{
	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);

	// Find how far 'along' mesh we are
	float Alpha;
	if (bHasCustomBoundary)
	{
		Alpha = (DistanceAlong - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
	}
	else
	{
		const FBoxSphereBounds StaticMeshBounds = StaticMesh->GetBounds();
		const float MeshMinZ = GetAxisValue(StaticMeshBounds.Origin, ForwardAxis) - GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis);
		const float MeshRangeZ = 2.0f * GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis);
		Alpha = (DistanceAlong - MeshMinZ) / MeshRangeZ;
	}

	return CalcSliceTransformAtSplineOffset(Alpha);
}

FTransform USplineMeshComponent::CalcSliceTransformAtSplineOffset(const float Alpha) const
{
	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector SplinePos = SplineEvalPos( SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha );
	const FVector SplineDir = SplineEvalDir( SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha );

	// Find base frenet frame
	const FVector BaseXVec = (SplineUpDir ^ SplineDir).GetSafeNormal();
	const FVector BaseYVec = (SplineDir ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2D SliceOffset = FMath::Lerp<FVector2D>(SplineParams.StartOffset, SplineParams.EndOffset, HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Find scale at this point along spline
	const FVector2D UseScale = FMath::Lerp(SplineParams.StartScale, SplineParams.EndScale, HermiteAlpha);

	// Build overall transform
	FTransform SliceTransform;
	switch (ForwardAxis)
	{
	case ESplineMeshAxis::X:
		SliceTransform = FTransform(SplineDir, XVec, YVec, SplinePos);
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
		break;
	case ESplineMeshAxis::Y:
		SliceTransform = FTransform(YVec, SplineDir, XVec, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
		break;
	case ESplineMeshAxis::Z:
		SliceTransform = FTransform(XVec, YVec, SplineDir, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
		break;
	default:
		check(0);
		break;
	}

	return SliceTransform;
}


bool USplineMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	if (StaticMesh)
	{
		StaticMesh->GetPhysicsTriMeshData(CollisionData, InUseAllTriData);

		FVector Mask = FVector(1,1,1);
		GetAxisValue(Mask, ForwardAxis) = 0;

		for (FVector& CollisionVert : CollisionData->Vertices)
		{
			CollisionVert = CalcSliceTransform(GetAxisValue(CollisionVert, ForwardAxis)).TransformPosition(CollisionVert * Mask);
		}
		return true;
	}

	return false;
}

bool USplineMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	if (StaticMesh)
	{
		return StaticMesh->ContainsPhysicsTriMeshData(InUseAllTriData);
	}

	return false;
}

void USplineMeshComponent::GetMeshId(FString& OutMeshId)
{
	// new method: Same guid as the base mesh but with a unique DDC-id based on the spline params.
	// This fixes the bug where running a blueprint construction script regenerates the guid and uses
	// a new DDC slot even if the mesh hasn't changed
	// If BodySetup is null that means we're *currently* duplicating one, and haven't transformed its data
	// to fit the spline yet, so just use the data from the base mesh by using a blank MeshId
	// It would be better if we could stop it building data in that case at all...
	if (BodySetup != nullptr && BodySetup->BodySetupGuid == CachedMeshBodySetupGuid)
	{
		OutMeshId += FString::Printf(TEXT("(%s,%s,%s,%f,%s)_(%s,%s,%s,%f,%s)_%s_%d_%c"),
			*SplineParams.StartPos.ToString(),
			*SplineParams.StartTangent.ToString(),
			*SplineParams.StartScale.ToString(),
			SplineParams.StartRoll,
			SplineParams.StartOffset != FVector2D::ZeroVector ? *SplineParams.StartOffset.ToString() : TEXT(""),
			*SplineParams.EndPos.ToString(),
			*SplineParams.EndTangent.ToString(),
			*SplineParams.EndScale.ToString(),
			SplineParams.EndRoll,
			SplineParams.EndOffset != FVector2D::ZeroVector ? *SplineParams.EndOffset.ToString() : TEXT(""),
			SplineUpDir != FVector::UpVector ? *SplineUpDir.ToString() : TEXT(""),
			(int32)bSmoothInterpRollScale,
			TEXT("XYZ")[(int32)ForwardAxis.GetValue()]);
	}
}

void USplineMeshComponent::CreatePhysicsState()
{
#if WITH_EDITOR
	// With editor code we can recreate the collision if the mesh changes
	const FGuid MeshBodySetupGuid = (StaticMesh != NULL ? StaticMesh->BodySetup->BodySetupGuid : FGuid());
	if (CachedMeshBodySetupGuid != MeshBodySetupGuid)
	{
		RecreateCollision();
	}
#else
	// Without editor code we can only destroy the collision if the mesh is missing
	if (StaticMesh == NULL && BodySetup != NULL)
	{
		DestroyBodySetup();
	}
#endif

	return Super::CreatePhysicsState();
}

UBodySetup* USplineMeshComponent::GetBodySetup()
{
	// Don't return a body setup that has no collision, it means we are interactively moving the spline and don't want to build collision.
	// Instead we explicitly build collision with USplineMeshComponent::RecreateCollision()
	if (BodySetup != NULL && (BodySetup->TriMesh != NULL || BodySetup->AggGeom.GetElementCount() > 0))
	{
		return BodySetup;
	}
	return NULL;
}

bool USplineMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	// the NavCollision is supposed to be faster than exporting the regular collision,
	// but I'm not sure that's true here, as the regular collision is pre-distorted to the spline

	if (StaticMesh != NULL && StaticMesh->NavCollision != NULL)
	{
		UNavCollision* NavCollision = StaticMesh->NavCollision;
		
		if (ensure(!NavCollision->bIsDynamicObstacle))
		{
			if (NavCollision->bHasConvexGeometry)
			{
				TArray<FVector> VertexBuffer;
				VertexBuffer.Reserve(FMath::Max(NavCollision->ConvexCollision.VertexBuffer.Num(), NavCollision->TriMeshCollision.VertexBuffer.Num()));

				for (int32 i = 0; i < NavCollision->ConvexCollision.VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->ConvexCollision.VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValue(Vertex, ForwardAxis)).TransformPosition(Vertex);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(),
					ComponentToWorld);

				VertexBuffer.Reset();
				for (int32 i = 0; i < NavCollision->TriMeshCollision.VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->TriMeshCollision.VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValue(Vertex, ForwardAxis)).TransformPosition(Vertex);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(),
					ComponentToWorld);

				return false;
			}
		}
	}

	return true;
}

void USplineMeshComponent::DestroyBodySetup()
{
	if (BodySetup != NULL)
	{
		BodySetup->MarkPendingKill();
		BodySetup = NULL;
#if WITH_EDITORONLY_DATA
		CachedMeshBodySetupGuid.Invalidate();
#endif
	}
}

#if WITH_EDITOR
void USplineMeshComponent::RecreateCollision()
{
	if (StaticMesh && IsCollisionEnabled())
	{
		if (BodySetup == NULL)
		{
			BodySetup = DuplicateObject<UBodySetup>(StaticMesh->BodySetup, this);
			BodySetup->SetFlags(RF_Transactional);
			BodySetup->InvalidatePhysicsData();
		}
		else
		{
			BodySetup->Modify();
			BodySetup->InvalidatePhysicsData();
			BodySetup->CopyBodyPropertiesFrom(StaticMesh->BodySetup);
			BodySetup->CollisionTraceFlag = StaticMesh->BodySetup->CollisionTraceFlag;
		}
		BodySetup->BodySetupGuid = StaticMesh->BodySetup->BodySetupGuid;
		CachedMeshBodySetupGuid = StaticMesh->BodySetup->BodySetupGuid;

		if (BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
		{
			BodySetup->AggGeom.EmptyElements();
		}
		else
		{
			FVector Mask = FVector(1, 1, 1);
			GetAxisValue(Mask, ForwardAxis) = 0;

			// distortion of a sphere can't be done nicely, so we just transform the origin and size
			for (FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
			{
				const float Z = GetAxisValue(SphereElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphereElem.Center *= Mask;

				SphereElem.Radius *= SliceTransform.GetMaximumAxisScale();

				SliceTransform.RemoveScaling();
				SphereElem.Center = SliceTransform.TransformPosition(SphereElem.Center);
			}

			// distortion of a sphyl can't be done nicely, so we just transform the origin and size
			for (FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
			{
				const float Z = GetAxisValue(SphylElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphylElem.Center *= Mask;

				FTransform TM = SphylElem.GetTransform();
				SphylElem.Length = (TM * SliceTransform).TransformVector(FVector(0, 0, SphylElem.Length)).Size();
				SphylElem.Radius *= SliceTransform.GetMaximumAxisScale();

				SliceTransform.RemoveScaling();
				SphylElem.SetTransform(TM * SliceTransform);
			}

			// Convert boxes to convex hulls to better respect distortion
			for (FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
			{
				FKConvexElem& ConvexElem = *new(BodySetup->AggGeom.ConvexElems) FKConvexElem();

				const FVector Radii = FVector(BoxElem.X / 2, BoxElem.Y / 2, BoxElem.Z / 2);
				const FTransform ElementTM = BoxElem.GetTransform();
				ConvexElem.VertexData.Empty(8);
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1,-1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1,-1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1,-1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1,-1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1, 1,-1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector( 1, 1, 1)));

				ConvexElem.UpdateElemBox();
			}
			BodySetup->AggGeom.BoxElems.Empty();

			// transform the points of the convex hulls into spline space
			for (FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
			{
				for (FVector& Point : ConvexElem.VertexData)
				{
					Point = CalcSliceTransform(GetAxisValue(Point, ForwardAxis)).TransformPosition(Point * Mask);
				}

				ConvexElem.UpdateElemBox();
			}
		}

		BodySetup->CreatePhysicsMeshes();
	}
	else
	{
		DestroyBodySetup();
	}
}
#endif

/** Used to store spline mesh data during RerunConstructionScripts */
class FSplineMeshInstanceData : public FSceneComponentInstanceData
{
public:
	explicit FSplineMeshInstanceData(const USplineMeshComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<USplineMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FVector StartPos;
	FVector EndPos;
	FVector StartTangent;
	FVector EndTangent;
};

FName USplineMeshComponent::GetComponentInstanceDataType() const
{
	static const FName SplineMeshInstanceDataTypeName(TEXT("SplineMeshInstanceData"));
	return SplineMeshInstanceDataTypeName;
}

FActorComponentInstanceData* USplineMeshComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = nullptr;
	if (bAllowSplineEditingPerInstance)
	{
		FSplineMeshInstanceData *SplineMeshInstanceData = new FSplineMeshInstanceData(this);
		SplineMeshInstanceData->StartPos = SplineParams.StartPos;
		SplineMeshInstanceData->EndPos = SplineParams.EndPos;
		SplineMeshInstanceData->StartTangent = SplineParams.StartTangent;
		SplineMeshInstanceData->EndTangent = SplineParams.EndTangent;
		InstanceData = SplineMeshInstanceData;
	}
	else
	{
		InstanceData = Super::GetComponentInstanceData();
	}
	return InstanceData;
}

void USplineMeshComponent::ApplyComponentInstanceData(FSplineMeshInstanceData* SplineMeshInstanceData)
{
	if (SplineMeshInstanceData)
	{
		if (bAllowSplineEditingPerInstance)
		{
			SplineParams.StartPos = SplineMeshInstanceData->StartPos;
			SplineParams.EndPos = SplineMeshInstanceData->EndPos;
			SplineParams.StartTangent = SplineMeshInstanceData->StartTangent;
			SplineParams.EndTangent = SplineMeshInstanceData->EndTangent;
			MarkSplineParamsDirty();
		}
	}
}


#include "StaticMeshLight.h"
/** */
class FSplineStaticLightingMesh : public FStaticMeshStaticLightingMesh
{
public:

	FSplineStaticLightingMesh(const USplineMeshComponent* InPrimitive,int32 InLODIndex,const TArray<ULightComponent*>& InRelevantLights):
		FStaticMeshStaticLightingMesh(InPrimitive, InLODIndex, InRelevantLights),
		SplineComponent(InPrimitive)
	{
	}

#if WITH_EDITOR
	virtual const struct FSplineMeshParams* GetSplineParameters() const
	{
		return &SplineComponent->SplineParams;
	}
#endif	//WITH_EDITOR

private:
	const USplineMeshComponent* SplineComponent;
};

FStaticMeshStaticLightingMesh* USplineMeshComponent::AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FSplineStaticLightingMesh(this, LODIndex, InRelevantLights);
}
