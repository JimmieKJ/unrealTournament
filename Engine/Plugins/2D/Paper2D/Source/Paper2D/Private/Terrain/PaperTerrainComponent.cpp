// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "Components/SplineComponent.h"

#include "PaperRenderSceneProxy.h"
#include "PaperGeomTools.h"

#define PAPER_USE_MATERIAL_SLOPES 1

//////////////////////////////////////////////////////////////////////////

FBox2D GetSpriteRenderDataBounds2D(const TArray<FVector4>& Data)
{
	FBox2D Bounds(ForceInit);
	for (const FVector4& XYUV : Data)
	{
		Bounds += FVector2D(XYUV.X, XYUV.Y);
	}

	return Bounds;
}

//////////////////////////////////////////////////////////////////////////
// FPaperTerrainSceneProxy

class FPaperTerrainSceneProxy : public FPaperRenderSceneProxy
{
public:
	FPaperTerrainSceneProxy(const UPaperTerrainComponent* InComponent, const TArray<FPaperTerrainMaterialPair>& InDrawingData);

protected:
	TArray<FPaperTerrainMaterialPair> DrawingData;
protected:
	// FPaperRenderSceneProxy interface
	virtual void GetDynamicMeshElementsForView(const FSceneView* View, int32 ViewIndex, bool bUseOverrideColor, const FLinearColor& OverrideColor, FMeshElementCollector& Collector) const override;
	// End of FPaperRenderSceneProxy interface
};

FPaperTerrainSceneProxy::FPaperTerrainSceneProxy(const UPaperTerrainComponent* InComponent, const TArray<FPaperTerrainMaterialPair>& InDrawingData)
	: FPaperRenderSceneProxy(InComponent)
{
	DrawingData = InDrawingData;

	// Combine the material relevance for all materials
	for (const FPaperTerrainMaterialPair& Batch : DrawingData)
	{
		const UMaterialInterface* MaterialInterface = (Batch.Material != nullptr) ? Batch.Material : UMaterial::GetDefaultMaterial(MD_Surface);
		MaterialRelevance |= MaterialInterface->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	}
}

void FPaperTerrainSceneProxy::GetDynamicMeshElementsForView(const FSceneView* View, int32 ViewIndex, bool bUseOverrideColor, const FLinearColor& OverrideColor, FMeshElementCollector& Collector) const
{
	for (const FPaperTerrainMaterialPair& Batch : DrawingData)
	{
		if (Batch.Material != nullptr)
		{
			GetBatchMesh(View, bUseOverrideColor, OverrideColor, Batch.Material, Batch.Records, ViewIndex, Collector);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UPaperTerrainComponent

UPaperTerrainComponent::UPaperTerrainComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TerrainColor(FLinearColor::White)
	, ReparamStepsPerSegment(8)
	, SpriteCollisionDomain(ESpriteCollisionMode::Use3DPhysics)
	, CollisionThickness(200.0f)
{
	bCanEverAffectNavigation = true;

	static ConstructorHelpers::FObjectFinder<UPaperTerrainMaterial> DefaultMaterialRef(TEXT("/Paper2D/DefaultPaperTerrainMaterial"));
	TerrainMaterial = DefaultMaterialRef.Object;
}

const UObject* UPaperTerrainComponent::AdditionalStatObject() const
{
	return TerrainMaterial;
}

#if WITH_EDITOR
void UPaperTerrainComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(USplineComponent, ReparamStepsPerSegment))
		{
			if (AssociatedSpline != nullptr)
			{
				AssociatedSpline->ReparamStepsPerSegment = ReparamStepsPerSegment;
				AssociatedSpline->UpdateSpline();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UPaperTerrainComponent::OnRegister()
{
	Super::OnRegister();

	if (AssociatedSpline != nullptr)
	{
		AssociatedSpline->OnSplineEdited = FSimpleDelegate::CreateUObject(this, &UPaperTerrainComponent::OnSplineEdited);
	}

	OnSplineEdited();
}

void UPaperTerrainComponent::OnUnregister()
{
	Super::OnUnregister();

	if (AssociatedSpline != nullptr)
	{
		AssociatedSpline->OnSplineEdited.Unbind();
	}
}

FPrimitiveSceneProxy* UPaperTerrainComponent::CreateSceneProxy()
{
	FPaperTerrainSceneProxy* NewProxy = new FPaperTerrainSceneProxy(this, GeneratedSpriteGeometry);
	return NewProxy;
}

FBoxSphereBounds UPaperTerrainComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Determine the rendering bounds
	FBoxSphereBounds LocalRenderBounds;
	{
		FBox BoundingBox(ForceInit);

		for (const FPaperTerrainMaterialPair& DrawCall : GeneratedSpriteGeometry)
		{
			for (const FSpriteDrawCallRecord& Record : DrawCall.Records)
			{
				for (const FVector4& VertXYUV : Record.RenderVerts)
				{
					const FVector Vert((PaperAxisX * VertXYUV.X) + (PaperAxisY * VertXYUV.Y));
					BoundingBox += Vert;
				}
			}
		}

		// Make the whole thing a single unit 'deep'
		const FVector HalfThicknessVector = 0.5f * PaperAxisZ;
		BoundingBox.Min -= HalfThicknessVector;
		BoundingBox.Max += HalfThicknessVector;

		LocalRenderBounds = FBoxSphereBounds(BoundingBox);
	}

	// Graphics bounds.
	FBoxSphereBounds NewBounds = LocalRenderBounds.TransformBy(LocalToWorld);

	// Add bounds of collision geometry (if present).
	if (CachedBodySetup != nullptr)
	{
		const FBox AggGeomBox = CachedBodySetup->AggGeom.CalcAABB(LocalToWorld);
		if (AggGeomBox.IsValid)
		{
			NewBounds = Union(NewBounds, FBoxSphereBounds(AggGeomBox));
		}
	}

	// Apply bounds scale
	NewBounds.BoxExtent *= BoundsScale;
	NewBounds.SphereRadius *= BoundsScale;

	return NewBounds;
}

UBodySetup* UPaperTerrainComponent::GetBodySetup()
{
	return CachedBodySetup;
}

FTransform UPaperTerrainComponent::GetTransformAtDistance(float InDistance) const
{
	const float SplineLength = AssociatedSpline->GetSplineLength();
	InDistance = FMath::Clamp<float>(InDistance, 0.0f, SplineLength);

	const float Param = AssociatedSpline->SplineReparamTable.Eval(InDistance, 0.0f);
	const FVector Position3D = AssociatedSpline->SplineInfo.Eval(Param, FVector::ZeroVector);

	const FVector Tangent = AssociatedSpline->SplineInfo.EvalDerivative(Param, FVector(1.0f, 0.0f, 0.0f)).GetSafeNormal();
	const FVector NormalEst = AssociatedSpline->SplineInfo.EvalSecondDerivative(Param, FVector(0.0f, 1.0f, 0.0f)).GetSafeNormal();
	const FVector Bitangent = FVector::CrossProduct(Tangent, NormalEst);
	const FVector Normal = FVector::CrossProduct(Bitangent, Tangent);
	const FVector Floop = FVector::CrossProduct(PaperAxisZ, Tangent);

	FTransform LocalTransform(Tangent, PaperAxisZ, Floop, Position3D);

	LocalTransform = FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector::ZeroVector) * LocalTransform;

#if PAPER_TERRAIN_DRAW_DEBUG
	FTransform WorldTransform = LocalTransform * ComponentToWorld;

	const float Time = 2.5f;

	DrawDebugCoordinateSystem(GetWorld(), WorldTransform.GetLocation(), FRotator(WorldTransform.GetRotation()), 15.0f, true, Time, SDPG_Foreground);
// 	FVector WorldPos = ComponentToWorld.TransformPosition(Position3D);
// 	WorldPos.Y -= 0.01;
// 
// 	//DrawDebugLine(GetWorld(), WorldPos, WorldPos + ComponentToWorld.TransformVector(Tangent) * 10.0f, FColor::Red, true, Time);
// 	// 		DrawDebugLine(GetWorld(), WorldPos, WorldPos + ComponentToWorld.TransformVector(NormalEst) * 10.0f, FColor::Green, true, Time);
// 	// 		DrawDebugLine(GetWorld(), WorldPos, WorldPos + ComponentToWorld.TransformVector(Bitangent) * 10.0f, FColor::Blue, true, Time);
// 	//DrawDebugLine(GetWorld(), WorldPos, WorldPos + ComponentToWorld.TransformVector(Floop) * 10.0f, FColor::Yellow, true, Time);
// 	// 		DrawDebugPoint(GetWorld(), WorldPos, 4.0f, FColor::White, true, 1.0f);
#endif

	return LocalTransform;
}

void UPaperTerrainComponent::OnSplineEdited()
{
	// Ensure we have the data structure for the desired collision method
	if (SpriteCollisionDomain == ESpriteCollisionMode::Use3DPhysics)
	{
		CachedBodySetup = NewObject<UBodySetup>(this);
	}
	else
	{
		CachedBodySetup = nullptr;
	}

	const float SlopeAnalysisTimeRate = 10.0f;
	const float FillRasterizationTimeRate = 100.0f;

	GeneratedSpriteGeometry.Empty();

	if ((AssociatedSpline != nullptr) && (TerrainMaterial != nullptr))
	{
		if (AssociatedSpline->ReparamStepsPerSegment != ReparamStepsPerSegment)
		{
			AssociatedSpline->ReparamStepsPerSegment = ReparamStepsPerSegment;
			AssociatedSpline->UpdateSpline();
		}

		FRandomStream RandomStream(RandomSeed);

		const FInterpCurveVector& SplineInfo = AssociatedSpline->SplineInfo;

		float SplineLength = AssociatedSpline->GetSplineLength();


		struct FSpriteStamp
		{
			const UPaperSprite* Sprite;
			float NominalWidth;
			float Time;
			float Scale;
			bool bCanStretch;

			FSpriteStamp(const UPaperSprite* InSprite, float InTime, bool bIsEndCap)
				: Sprite(InSprite)
				, Time(InTime)
				, Scale(1.0f)
				, bCanStretch(!bIsEndCap)
			{
				const FBox2D Bounds2D = GetSpriteRenderDataBounds2D(InSprite->BakedRenderData);
				NominalWidth = FMath::Max<float>(Bounds2D.GetSize().X, 1.0f);
			}
		};

		struct FTerrainSegment
		{
			float StartTime;
			float EndTime;
			const FPaperTerrainMaterialRule* Rule;
			TArray<FSpriteStamp> Stamps;

			FTerrainSegment()
				: Rule(nullptr)
			{
			}

			void RepositionStampsToFillSpace()
			{
			}

		};

		struct FTerrainRuleHelper
		{
		public:
			FTerrainRuleHelper(const FPaperTerrainMaterialRule* Rule)
				: StartWidth(0.0f)
				, EndWidth(0.0f)
			{
				for (const UPaperSprite* Sprite : Rule->Body)
				{
					if (Sprite != nullptr)
					{
						const float Width = GetSpriteRenderDataBounds2D(Sprite->BakedRenderData).GetSize().X;
						if (Width > 0.0f)
						{
							ValidBodies.Add(Sprite);
							ValidBodyWidths.Add(Width);
						}
					}
				}

				if (Rule->StartCap != nullptr)
				{
					const float Width = GetSpriteRenderDataBounds2D(Rule->StartCap->BakedRenderData).GetSize().X;
					if (Width > 0.0f)
					{
						StartWidth = Width;
					}
				}

				if (Rule->EndCap != nullptr)
				{
					const float Width = GetSpriteRenderDataBounds2D(Rule->EndCap->BakedRenderData).GetSize().X;
					if (Width > 0.0f)
					{
						EndWidth = Width;
					}
				}
			}

			float StartWidth;
			float EndWidth;

			TArray<const UPaperSprite*> ValidBodies;
			TArray<float> ValidBodyWidths;

			int32 GenerateBodyIndex(FRandomStream& RandomStream) const
			{
				check(ValidBodies.Num() > 0);
				return RandomStream.GetUnsignedInt() % ValidBodies.Num();
			}
		};

		// Split the spline into segments based on the slope rules in the material
		TArray<FTerrainSegment> Segments;

		FTerrainSegment* ActiveSegment = new (Segments) FTerrainSegment();
		ActiveSegment->StartTime = 0.0f;
		ActiveSegment->EndTime = SplineLength;

		{
			float CurrentTime = 0.0f;
			while (CurrentTime < SplineLength)
			{
				const FTransform Frame(GetTransformAtDistance(CurrentTime));
				const FVector UnitTangent = Frame.GetUnitAxis(EAxis::X);
				const float RawSlopeAngleRadians = FMath::Atan2(FVector::DotProduct(UnitTangent, PaperAxisY), FVector::DotProduct(UnitTangent, PaperAxisX));
				const float RawSlopeAngle = FMath::RadiansToDegrees(RawSlopeAngleRadians);
				const float SlopeAngle = FMath::Fmod(FMath::UnwindDegrees(RawSlopeAngle) + 360.0f, 360.0f);

				const FPaperTerrainMaterialRule* DesiredRule = (TerrainMaterial->Rules.Num() > 0) ? &(TerrainMaterial->Rules[0]) : nullptr;
				for (const FPaperTerrainMaterialRule& TestRule : TerrainMaterial->Rules)
				{
					if ((SlopeAngle >= TestRule.MinimumAngle) && (SlopeAngle < TestRule.MaximumAngle))
					{
						DesiredRule = &TestRule;
					}
				}

				if (ActiveSegment->Rule != DesiredRule)
				{
					if (ActiveSegment->Rule == nullptr)
					{
						ActiveSegment->Rule = DesiredRule;
					}
					else
					{
						ActiveSegment->EndTime = CurrentTime;

						ActiveSegment = new (Segments)FTerrainSegment();
						ActiveSegment->StartTime = CurrentTime;
						ActiveSegment->EndTime = SplineLength;
						ActiveSegment->Rule = DesiredRule;
					}
				}

				CurrentTime += SlopeAnalysisTimeRate;
			}
		}

		// Convert those segments to actual geometry
		for (FTerrainSegment& Segment : Segments)
		{
			check(Segment.Rule);
			FTerrainRuleHelper RuleHelper(Segment.Rule);

			float RemainingSegStart = Segment.StartTime + RuleHelper.StartWidth;
			float RemainingSegEnd = Segment.EndTime - RuleHelper.EndWidth;
			const float BodyDistance = RemainingSegEnd - RemainingSegStart;
			float DistanceBudget = BodyDistance;

			bool bUseBodySegments = (DistanceBudget > 0.0f) && (RuleHelper.ValidBodies.Num() > 0);

			// Add the start cap
			if (RuleHelper.StartWidth > 0.0f)
			{
				new (Segment.Stamps) FSpriteStamp(Segment.Rule->StartCap, Segment.StartTime + RuleHelper.StartWidth * 0.5f, /*bIsEndCap=*/ bUseBodySegments);
			}

			// Add the end cap
			if (RuleHelper.EndWidth > 0.0f)
			{
				new (Segment.Stamps) FSpriteStamp(Segment.Rule->EndCap, Segment.EndTime - RuleHelper.EndWidth * 0.5f, /*bIsEndCap=*/ bUseBodySegments);
			}
			

			// Add body segments
			if (bUseBodySegments)
			{
				int32 NumSegments = 0;
				float Position = RemainingSegStart;
				
				while (DistanceBudget > 0.0f)
				{
					const int32 BodyIndex = RuleHelper.GenerateBodyIndex(RandomStream);
					const UPaperSprite* Sprite = RuleHelper.ValidBodies[BodyIndex];
					const float Width = RuleHelper.ValidBodyWidths[BodyIndex];

 					if ((NumSegments > 0) && ((Width * 0.5f) > DistanceBudget))
 					{
 						break;
 					}
					new (Segment.Stamps) FSpriteStamp(Sprite, Position + (Width * 0.5f), /*bIsEndCap=*/ false);

					DistanceBudget -= Width;
					Position += Width;
					++NumSegments;
				}

				const float UsedSpace = (BodyDistance - DistanceBudget);
				const float OverallScaleFactor = BodyDistance / UsedSpace;
				
 				// Stretch body segments
				float PositionCorrectionSum = 0.0f;
 				for (int32 Index = 0; Index < NumSegments; ++Index)
 				{
					FSpriteStamp& Stamp = Segment.Stamps[Index + (Segment.Stamps.Num() - NumSegments)];
					
					const float WidthChange = (OverallScaleFactor - 1.0f) * Stamp.NominalWidth;
					const float FirstGapIsSmallerFactor = (Index == 0) ? 0.5f : 1.0f;
					PositionCorrectionSum += WidthChange * FirstGapIsSmallerFactor;

					Stamp.Scale = OverallScaleFactor;
					Stamp.Time += PositionCorrectionSum;
 				}
			}
			else
			{
				// Stretch endcaps
			}

			// Convert stamps into geometry
			for (const FSpriteStamp& Stamp : Segment.Stamps)
			{
				SpawnSegment(Stamp.Sprite, Stamp.Time, Stamp.Scale, Stamp.NominalWidth, Segment.Rule);
			}
		}

		// Generate the background if the spline is closed
		if (bClosedSpline && (TerrainMaterial->InteriorFill != nullptr))
		{
			// Create a polygon from the spline
			FBox2D SplineBounds(ForceInit);
			TArray<FVector2D> SplinePolyVertices2D;
			{
				float CurrentTime = 0.0f;
				while (CurrentTime < SplineLength)
				{
					const float Param = AssociatedSpline->SplineReparamTable.Eval(CurrentTime, 0.0f);
					const FVector Position3D = AssociatedSpline->SplineInfo.Eval(Param, FVector::ZeroVector);
					const FVector2D Position2D = FVector2D(FVector::DotProduct(Position3D, PaperAxisX), FVector::DotProduct(Position3D, PaperAxisY));

					SplineBounds += Position2D;
					SplinePolyVertices2D.Add(Position2D);

					CurrentTime += FillRasterizationTimeRate;
				}
			}

			const UPaperSprite* FillSprite = TerrainMaterial->InteriorFill;
			FPaperTerrainMaterialPair& MaterialBatch = *new (GeneratedSpriteGeometry) FPaperTerrainMaterialPair(); //@TODO: Look up the existing one instead
			MaterialBatch.Material = FillSprite->GetDefaultMaterial();

			FSpriteDrawCallRecord& FillDrawCall = *new (MaterialBatch.Records) FSpriteDrawCallRecord();
			FillDrawCall.BuildFromSprite(FillSprite);
			FillDrawCall.RenderVerts.Empty();
			FillDrawCall.Color = TerrainColor;
			FillDrawCall.Destination = PaperAxisZ * 0.1f;

			const FVector2D TextureSize = GetSpriteRenderDataBounds2D(FillSprite->BakedRenderData).GetSize();
			const FVector2D SplineSize = SplineBounds.GetSize();
			
			SpawnFromPoly(FillSprite, FillDrawCall, TextureSize, SplinePolyVertices2D);

			//@TODO: Add support for the fill sprite being smaller than the entire texture
#if NOT_WORKING
			const float StartingDivisionPointX = FMath::CeilToFloat(SplineBounds.Min.X / TextureSize.X);
			const float StartingDivisionPointY = FMath::CeilToFloat(SplineBounds.Min.Y / TextureSize.Y);

			FPoly VerticalRemainder = SplineAsPolygon;
			for (float Y = StartingDivisionPointY; VerticalRemainder.Vertices.Num() > 0; Y += TextureSize.Y)
			{
				FPoly Top;
				FPoly Bottom;
				const FVector SplitBaseOuter = (Y * PaperAxisY);
				VerticalRemainder.SplitWithPlane(SplitBaseOuter, -PaperAxisY, &Top, &Bottom, 1);
				VerticalRemainder = Bottom;

				FPoly HorizontalRemainder = Top;
				for (float X = StartingDivisionPointX; HorizontalRemainder.Vertices.Num() > 0; X += TextureSize.X)
				{
					FPoly Left;
					FPoly Right;
					const FVector SplitBaseInner = (X * PaperAxisX) + (Y * PaperAxisY);
					HorizontalRemainder.SplitWithPlane(SplitBaseInner, -PaperAxisX, &Left, &Right, 1);
					HorizontalRemainder = Right;

					SpawnFromPoly(FillSprite, FillDrawCall, TextureSize, Left);
				}
			}
#endif
		}

		// Draw debug frames at the start and end of the spline
#if PAPER_TERRAIN_DRAW_DEBUG
		{
			const float Time = 5.0f;
			{
				FTransform WorldTransform = GetTransformAtDistance(0.0f) * ComponentToWorld;
				DrawDebugCoordinateSystem(GetWorld(), WorldTransform.GetLocation(), FRotator(WorldTransform.GetRotation()), 30.0f, true, Time, SDPG_Foreground);
			}
			{
				FTransform WorldTransform = GetTransformAtDistance(SplineLength) * ComponentToWorld;
				DrawDebugCoordinateSystem(GetWorld(), WorldTransform.GetLocation(), FRotator(WorldTransform.GetRotation()), 30.0f, true, Time, SDPG_Foreground);
			}
		}
#endif
	}

	RecreateRenderState_Concurrent();
}

void UPaperTerrainComponent::SpawnSegment(const UPaperSprite* NewSprite, float Position, float HorizontalScale, float NominalWidth, const FPaperTerrainMaterialRule* Rule)
{
	if ((Rule->bEnableCollision) && (CachedBodySetup != nullptr))
	{
		const FTransform LocalTransformAtCenter(GetTransformAtDistance(Position));

		FKBoxElem Box;

		if (bClosedSpline)
		{
			//@TODO: Add proper support for this!
			Box.SetTransform(LocalTransformAtCenter);
			Box.X = NominalWidth * HorizontalScale;
			Box.Y = CollisionThickness;
			Box.Z = 30.0f;
		}
		else
		{
			Box.SetTransform(LocalTransformAtCenter);
			Box.X = NominalWidth * HorizontalScale;
			Box.Y = CollisionThickness;
			Box.Z = FMath::Max<float>(1.0f, FMath::Abs<float>(Rule->CollisionOffset * 0.5f));
		}
		CachedBodySetup->AggGeom.BoxElems.Add(Box);
	}

	if (NewSprite)
	{
		FPaperTerrainMaterialPair& MaterialBatch = *new (GeneratedSpriteGeometry) FPaperTerrainMaterialPair(); //@TODO: Look up the existing one instead
		MaterialBatch.Material = NewSprite->GetDefaultMaterial();

		FSpriteDrawCallRecord& Record = *new (MaterialBatch.Records) FSpriteDrawCallRecord();
		Record.BuildFromSprite(NewSprite);
		Record.Color = TerrainColor;

		for (FVector4& XYUV : Record.RenderVerts)
		{
			const FTransform LocalTransformAtX(GetTransformAtDistance(Position + (XYUV.X * HorizontalScale)));
			const FVector SourceVector = (XYUV.Y * PaperAxisY);
			const FVector NewVector = LocalTransformAtX.TransformPosition(SourceVector);

			const float NewX = FVector::DotProduct(NewVector, PaperAxisX);
			const float NewY = FVector::DotProduct(NewVector, PaperAxisY);

			XYUV.X = NewX;
			XYUV.Y = NewY;
		}
	}
}

void UPaperTerrainComponent::SpawnFromPoly(const class UPaperSprite* NewSprite, FSpriteDrawCallRecord& Batch, const FVector2D& TextureSize, const TArray<FVector2D>& SplinePolyVertices2D)
{
	const FVector2D TextureSizeInUnits = TextureSize * NewSprite->GetUnrealUnitsPerPixel();

	// Correct spline vertices so they are always CCW
	//@TODO: Handle overlapping and crossing polygon edges during triangulation
	TArray<FVector2D> CorrectedSpriteVertices;
	PaperGeomTools::CorrectPolygonWinding(/*out*/CorrectedSpriteVertices, SplinePolyVertices2D, false);

	if (SplinePolyVertices2D.Num() >= 3)
	{
		TArray<FVector2D> TriangulatedPolygonVertices;
		PaperGeomTools::TriangulatePoly(/*out*/TriangulatedPolygonVertices, CorrectedSpriteVertices, false);

		// Pack vertex data
		for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < TriangulatedPolygonVertices.Num(); ++TriangleVertexIndex)
		{
			const FVector2D& TriangleVertex = TriangulatedPolygonVertices[TriangleVertexIndex];
			new (Batch.RenderVerts) FVector4(TriangleVertex.X, TriangleVertex.Y, TriangleVertex.X / TextureSizeInUnits.X, -TriangleVertex.Y / TextureSizeInUnits.Y);
		}
	}
}

void UPaperTerrainComponent::SetTerrainColor(FLinearColor NewColor)
{
	// Can't set color on a static component
	if (!(IsRegistered() && (Mobility == EComponentMobility::Static)) && (TerrainColor != NewColor))
	{
		TerrainColor = NewColor;

		// Update the color in the game-thread copy of the render geometry
		for (FPaperTerrainMaterialPair& Batch : GeneratedSpriteGeometry)
		{
			for (FSpriteDrawCallRecord& DrawCall : Batch.Records)
			{
				DrawCall.Color = TerrainColor;
			}
		}

		// Update the render thread copy
		RecreateRenderState_Concurrent();
	}
}
