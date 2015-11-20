// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"
#include "Exporter.h"
#include "LightmassSwarm.h"
#include "CPUSolver.h"
#include "LightingSystem.h"
#include "MonteCarlo.h"
#include "ExceptionHandling.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include <psapi.h>
#include "HideWindowsPlatformTypes.h"

#pragma comment(lib, "psapi.lib")
#endif

namespace Lightmass
{

/** Calculates incident radiance for a given world space position. */
void FStaticLightingSystem::CalculateVolumeSampleIncidentRadiance(
	const TArray<FVector4>& UniformHemisphereSamples,
	float MaxUnoccludedLength,
	FVolumeLightingSample& LightingSample,
	FLMRandomStream& RandomStream,
	FStaticLightingMappingContext& MappingContext,
	bool bDebugThisSample
	) const
{
	if (bDebugThisSample)
	{
		int32 asdf = 0;
	}

	const FVector4 Position = LightingSample.GetPosition();
	FFullStaticLightingVertex RepresentativeVertex;
	RepresentativeVertex.WorldPosition = Position;
	RepresentativeVertex.TextureCoordinates[0] = FVector2D(0,0);
	RepresentativeVertex.TextureCoordinates[1] = FVector2D(0,0);

	// Construct a vertex to capture incident radiance for the positive Z hemisphere
	RepresentativeVertex.WorldTangentZ = RepresentativeVertex.TriangleNormal = FVector4(0,0,1);
	RepresentativeVertex.GenerateVertexTangents();
	RepresentativeVertex.GenerateTriangleTangents();

	FGatheredLightSample UpperStaticDirectLighting;
	// Stationary point and spot light direct contribution
	FGatheredLightSample UpperToggleableDirectLighting;
	float UpperToggleableDirectionalLightShadowing = 1;

	CalculateApproximateDirectLighting(RepresentativeVertex, 0, .1f, false, false, bDebugThisSample, MappingContext, UpperStaticDirectLighting, UpperToggleableDirectLighting, UpperToggleableDirectionalLightShadowing);

	FLightingCacheGatherInfo GatherInfo;
	TArray<FVector4> ImportancePhotonDirections;

	// Calculate incident radiance with a uniform final gather (also known as hemisphere gathering).  
	// We can't do importance sampled final gathering using photons because they are only stored on surfaces, and Position is an arbitrary point in world space.
	const FFinalGatherSample UpperHemisphereSample = IncomingRadianceUniform<FFinalGatherSample>(
		NULL,
		RepresentativeVertex,
		0.0f,
		0,
		1,
		UniformHemisphereSamples,
		MaxUnoccludedLength,
		ImportancePhotonDirections,
		MappingContext,
		RandomStream,
		GatherInfo,
		bDebugThisSample
		);

	// Construct a vertex to capture incident radiance for the negative Z hemisphere
	RepresentativeVertex.WorldTangentZ = RepresentativeVertex.TriangleNormal = FVector4(0,0,-1);
	RepresentativeVertex.GenerateVertexTangents();
	RepresentativeVertex.GenerateTriangleTangents();

	FGatheredLightSample LowerStaticDirectLighting;
	// Stationary point and spot light direct contribution
	FGatheredLightSample LowerToggleableDirectLighting;
	float LowerToggleableDirectionalLightShadowing = 1;

	CalculateApproximateDirectLighting(RepresentativeVertex, 0, .1f, false, false, bDebugThisSample, MappingContext, LowerStaticDirectLighting, LowerToggleableDirectLighting, LowerToggleableDirectionalLightShadowing);

	const FFinalGatherSample LowerHemisphereSample = IncomingRadianceUniform<FFinalGatherSample>(
		NULL,
		RepresentativeVertex,
		0.0f,
		0,
		1,
		UniformHemisphereSamples,
		MaxUnoccludedLength,
		ImportancePhotonDirections,
		MappingContext,
		RandomStream,
		GatherInfo,
		bDebugThisSample
		);

	const FGatheredLightSample CombinedIndirectLighting = UpperHemisphereSample + LowerHemisphereSample;
	const FGatheredLightSample CombinedHighQualitySample = UpperStaticDirectLighting + LowerStaticDirectLighting + CombinedIndirectLighting;
	 
	// Composite point and spot stationary direct lighting into the low quality volume samples, since we won't be applying them dynamically
	FGatheredLightSample CombinedLowQualitySample = UpperStaticDirectLighting + UpperToggleableDirectLighting + LowerStaticDirectLighting + LowerToggleableDirectLighting + CombinedIndirectLighting;
	// Composite stationary sky light contribution to the low quality volume samples, since we won't be applying it dynamically
	CombinedLowQualitySample = CombinedLowQualitySample + UpperHemisphereSample.StationarySkyLighting + LowerHemisphereSample.StationarySkyLighting;

	for (int32 CoefficientIndex = 0; CoefficientIndex < 4; CoefficientIndex++)
	{
		LightingSample.HighQualityCoefficients[CoefficientIndex][0] = CombinedHighQualitySample.SHVector.R.V[CoefficientIndex];
		LightingSample.HighQualityCoefficients[CoefficientIndex][1] = CombinedHighQualitySample.SHVector.G.V[CoefficientIndex];
		LightingSample.HighQualityCoefficients[CoefficientIndex][2] = CombinedHighQualitySample.SHVector.B.V[CoefficientIndex];

		LightingSample.LowQualityCoefficients[CoefficientIndex][0] = CombinedLowQualitySample.SHVector.R.V[CoefficientIndex];
		LightingSample.LowQualityCoefficients[CoefficientIndex][1] = CombinedLowQualitySample.SHVector.G.V[CoefficientIndex];
		LightingSample.LowQualityCoefficients[CoefficientIndex][2] = CombinedLowQualitySample.SHVector.B.V[CoefficientIndex];
	}

	LightingSample.DirectionalLightShadowing = FMath::Min(UpperToggleableDirectionalLightShadowing, LowerToggleableDirectionalLightShadowing);

	// Only using the upper hemisphere sky bent normal
	LightingSample.SkyBentNormal = UpperHemisphereSample.SkyOcclusion;
}

/** Evaluates the PDF that was used to generate samples for the non-importance sampled final gather for the given direction. */
float FStaticLightingSystem::EvaluatePDF(const FFullStaticLightingVertex& Vertex, const FVector4& IncomingDirection) const
{
	checkSlow(Vertex.TriangleNormal.IsUnit3());
	checkSlow(IncomingDirection.IsUnit3());

	if (ImportanceTracingSettings.bUseCosinePDF)
	{
		const float CosTheta = FMath::Max(Dot3(IncomingDirection, Vertex.TriangleNormal), 0.0f);
		const float CosPDF = CosTheta / (float)PI;
		checkSlow(CosPDF > 0.0f);
		return CosPDF;
	}
	else
	{
		const float UniformPDF = 1.0f / (2.0f * (float)PI);
		checkSlow(UniformPDF > 0.0f);
		return UniformPDF;
	}
}

/** Returns environment lighting for the given direction. */
FLinearColor FStaticLightingSystem::EvaluateEnvironmentLighting(const FVector4& IncomingDirection) const
{
	// Upper hemisphere only
	return IncomingDirection.Z < 0 ? (MaterialSettings.EnvironmentColor / (float)PI) : FLinearColor::Black;
}

void FStaticLightingSystem::EvaluateSkyLighting(const FVector4& IncomingDirection, bool bShadowed, bool bForDirectLighting, FLinearColor& OutStaticLighting, FLinearColor& OutStationaryLighting) const
{
	for (int32 LightIndex = 0; LightIndex < SkyLights.Num(); LightIndex++)
	{
		FSkyLight* SkyLight = SkyLights[LightIndex];

		if (!bShadowed || !(SkyLight->LightFlags & GI_LIGHT_CASTSHADOWS))
		{
			FSHVector3 SH = FSHVector3::SHBasisFunction(IncomingDirection);
			const float LightingScale = bForDirectLighting ? 1.0f : SkyLight->IndirectLightingScale;
			FLinearColor Lighting = (Dot(SkyLight->IrradianceEnvironmentMap, SH) * SkyLight->Brightness * LightingScale) * FLinearColor(SkyLight->Color);

			Lighting.R = FMath::Max(Lighting.R, 0.0f);
			Lighting.G = FMath::Max(Lighting.G, 0.0f);
			Lighting.B = FMath::Max(Lighting.B, 0.0f);

			if (SkyLight->LightFlags & GI_LIGHT_HASSTATICLIGHTING)
			{
				OutStaticLighting += Lighting;
			}
			else if (SkyLight->LightFlags & GI_LIGHT_HASSTATICSHADOWING)
			{
				OutStationaryLighting += Lighting;
			}
		}
	}
}

/** Calculates exitant radiance at a vertex. */
FLinearColor FStaticLightingSystem::CalculateExitantRadiance(
	const FStaticLightingMapping* SourceMapping,
	const FStaticLightingMapping* HitMapping,
	const FStaticLightingMesh* HitMesh,
	const FStaticLightingVertex& Vertex,
	int32 VertexIndex,
	int32 ElementIndex,
	const FVector4& OutgoingDirection,
	int32 BounceNumber,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	bool bIncludeDirectLighting,
	bool bDebugThisTexel) const
{
	FLinearColor AccumulatedRadiance = FLinearColor::Black;
	// Note: Emissive is explicitly sampled and therefore not handled here

	if (PhotonMappingSettings.bUsePhotonMapping)
	{
		// Using photon mapping, so all light interactions will be estimated with the photon map.
		checkSlow(BounceNumber == 1);
		checkSlow(PhotonMappingSettings.bUseFinalGathering);
		if (PhotonMappingSettings.bUseIrradiancePhotons)
		{
			const FIrradiancePhoton* NearestPhoton = NULL;
			FLinearColor DirectLighting;

			if (PhotonMappingSettings.bCacheIrradiancePhotonsOnSurfaces)
			{
				// Find the cached irradiance photon
				NearestPhoton = HitMapping->GetCachedIrradiancePhoton(VertexIndex, Vertex, *this, bDebugThisTexel && PhotonMappingSettings.bVisualizePhotonGathers, DirectLighting);
			}
			else
			{
				// Find the nearest irradiance photon from the irradiance photon map
				TArray<FIrradiancePhoton*> TempIrradiancePhotons;
				NearestPhoton = FindNearestIrradiancePhoton(Vertex, MappingContext, TempIrradiancePhotons, false, bDebugThisTexel);
				FGatheredLightSample DirectLightingSample;
				FGatheredLightSample UnusedSample;
				float UnusedShadowing;
					
				CalculateApproximateDirectLighting(Vertex, 0, .1f, true, true, bDebugThisTexel, MappingContext, DirectLightingSample, UnusedSample, UnusedShadowing);
				DirectLighting = DirectLightingSample.IncidentLighting;
			}
			const FLinearColor& PhotonIrradiance = NearestPhoton ? NearestPhoton->GetIrradiance() : FLinearColor::Black;
			const FLinearColor Reflectance = HitMesh->EvaluateTotalReflectance(Vertex, ElementIndex);
			// Any type of light interaction can be retrieved from the irradiance photons here except direct lighting
			if (GeneralSettings.ViewSingleBounceNumber != 0)
			{
				FLinearColor FinalLighting = PhotonIrradiance;

				if (bIncludeDirectLighting)
				{
					FinalLighting += DirectLighting;
				}

				// Estimate exitant radiance as the irradiance times the surface's hemispherical-hemispherical reflectance divided by PI.
				AccumulatedRadiance += FinalLighting * Reflectance * (float)INV_PI;
			}
		}
		else
		{
			if (GeneralSettings.ViewSingleBounceNumber < 0 || GeneralSettings.ViewSingleBounceNumber == 1)
			{
				// Search the direct photon map for direct photon contribution
				const FLinearColor DirectPhotonExitantRadiance = CalculatePhotonExitantRadiance(DirectPhotonMap, NumPhotonsEmittedDirect, PhotonMappingSettings.DirectPhotonSearchDistance, HitMesh, Vertex, ElementIndex, OutgoingDirection, bDebugThisTexel);
				AccumulatedRadiance += DirectPhotonExitantRadiance;
			}
			if (GeneralSettings.ViewSingleBounceNumber < 0 || GeneralSettings.ViewSingleBounceNumber > 1)
			{
				// Search the indirect photon maps for indirect photon contribution
				const FLinearColor FirstBouncePhotonExitantRadiance = CalculatePhotonExitantRadiance(FirstBouncePhotonMap, NumPhotonsEmittedFirstBounce, PhotonMappingSettings.IndirectPhotonSearchDistance, HitMesh, Vertex, ElementIndex, OutgoingDirection, bDebugThisTexel);
				AccumulatedRadiance += FirstBouncePhotonExitantRadiance;
				if (GeneralSettings.ViewSingleBounceNumber < 0 || GeneralSettings.ViewSingleBounceNumber > 2)
				{
					const FLinearColor SecondBouncePhotonExitantRadiance = CalculatePhotonExitantRadiance(SecondBouncePhotonMap, NumPhotonsEmittedSecondBounce, PhotonMappingSettings.IndirectPhotonSearchDistance, HitMesh, Vertex, ElementIndex, OutgoingDirection, bDebugThisTexel);
					AccumulatedRadiance += SecondBouncePhotonExitantRadiance;
				}
			}
		}

		FLinearColor Emissive = FLinearColor::Black;
		if (HitMesh->IsEmissive(ElementIndex))
		{
			Emissive = HitMesh->EvaluateEmissive(Vertex.TextureCoordinates[0], ElementIndex);
		}

		AccumulatedRadiance += Emissive;
	}

	// So we can compare it against FLinearColor::Black easily
	AccumulatedRadiance.A = 1.0f;
	return AccumulatedRadiance;
}

/** Final gather using first bounce indirect photons to importance sample the incident radiance function. */
FGatheredLightSample FStaticLightingSystem::IncomingRadianceImportancePhotons(
	const FStaticLightingMapping* Mapping,
	const FFullStaticLightingVertex& Vertex,
	float SampleRadius,
	int32 ElementIndex,
	int32 BounceNumber,
	const TArray<FVector4>& ImportancePhotonDirections,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	bool bDebugThisTexel
	) const
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 TempBreak = 0;
	}
#endif

	FGatheredLightSample IncomingRadiance;
	const int32 PhotonImportanceHemisphereSamples = ImportancePhotonDirections.Num() > 0 ? GetNumPhotonImportanceHemisphereSamples() : 0;
	const int32 UniformHemisphereSamples = GetNumUniformHemisphereSamples(BounceNumber);

	if (PhotonImportanceHemisphereSamples > 0)
	{
		// Estimate the indirect part of the light transport equation using importance guided monte carlo integration
		for (int32 SampleIndex = 0; SampleIndex < PhotonImportanceHemisphereSamples; SampleIndex++)
		{
			FVector4 SampleDirection;
			float DirectionPDF = 0.0f;
			{
				LIGHTINGSTAT(FScopedRDTSCTimer GenerateSampleTimer(MappingContext.Stats.CalculateImportanceSampleTime));
				
				// Select a direction with uniform probability in a cone around the photon's incident direction
				// See the "Extended Photon Map Implementation" paper
				//@todo - select a direction with probability proportional to the power of the photon from that direction
				const int32 PhotonIndex = FMath::TruncToInt(RandomStream.GetFraction() * ImportancePhotonDirections.Num());
				checkSlow(PhotonIndex >= 0 && PhotonIndex < ImportancePhotonDirections.Num());
				const FVector4& CurrentPhotonDirection = ImportancePhotonDirections[PhotonIndex];
				checkSlow(Dot3(CurrentPhotonDirection, Vertex.TriangleNormal) > 0);
				FVector4 XAxis;
				FVector4 YAxis;
				GenerateCoordinateSystem(CurrentPhotonDirection, XAxis, YAxis);

				// Generate a direction from the cone around the importance photon direction
				SampleDirection = UniformSampleCone(
					RandomStream, 
					PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle, 
					XAxis, 
					YAxis, 
					CurrentPhotonDirection);

				MappingContext.Stats.NumImportancePDFCalculations++;
				const float ConePDF = UniformConePDF(PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle);
				// Calculate the probability that this sample was generated
				for (int32 OtherPhotonIndex = 0; OtherPhotonIndex < ImportancePhotonDirections.Num(); OtherPhotonIndex++)
				{
					// Accumulate this direction's cone PDF if the direction lies in the cone
					if (Dot3(ImportancePhotonDirections[OtherPhotonIndex], SampleDirection) > (1.0f - DELTA) * PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle)
					{
						DirectionPDF += ConePDF;
					}
				}
				DirectionPDF /= ImportancePhotonDirections.Num();
				checkSlow(DirectionPDF > 0.0f);
			}

			const FVector4 TangentSampleDirection = Vertex.TransformWorldVectorToTangent(SampleDirection);

			// Setup a ray from the point in the sample direction
			const FLightRay PathRay(
				// Apply various offsets to the start of the ray.
				// The offset along the ray direction is to avoid incorrect self-intersection due to floating point precision.
				// The offset along the normal is to push self-intersection patterns (like triangle shape) on highly curved surfaces onto the backfaces.
				Vertex.WorldPosition 
					+ SampleDirection * SceneConstants.VisibilityRayOffsetDistance 
					+ Vertex.WorldTangentZ * SampleRadius * SceneConstants.VisibilityNormalOffsetSampleRadiusScale,
				Vertex.WorldPosition + SampleDirection * MaxRayDistance,
				Mapping,
				NULL
				);

			MappingContext.Stats.NumFirstBounceRaysTraced++;
			const float LastRayTraceTime = MappingContext.RayCache.FirstHitRayTraceTime;
			FLightRayIntersection RayIntersection;
			AggregateMesh.IntersectLightRay(PathRay, true, false, false, MappingContext.RayCache, RayIntersection);
			MappingContext.Stats.FirstBounceRayTraceTime += MappingContext.RayCache.FirstHitRayTraceTime - LastRayTraceTime;

			bool bPositiveSample = false;
			// Calculate the probability that this sample direction would have been generated by the uniform hemisphere final gather
			const float UniformPDF = EvaluatePDF(Vertex, SampleDirection);
			// Calculate the multiple importance sample weight for this sample direction using a power heuristic
			const float MultipleImportanceSamplingWeight = PowerHeuristic(PhotonImportanceHemisphereSamples, DirectionPDF, UniformHemisphereSamples, UniformPDF);
			const float SampleWeight = MultipleImportanceSamplingWeight / (DirectionPDF * PhotonImportanceHemisphereSamples);

			if (RayIntersection.bIntersects)
			{
				// Only continue if the ray hit the frontface of the polygon, otherwise the ray started inside a mesh
				if (Dot3(PathRay.Direction, -RayIntersection.IntersectionVertex.WorldTangentZ) > 0.0f)
				{
					LIGHTINGSTAT(FScopedRDTSCTimer CalculateExitantRadianceTimer(MappingContext.Stats.CalculateExitantRadianceTime));
					FStaticLightingVertex IntersectionVertex = RayIntersection.IntersectionVertex;
					// The ray intersection does not return a Tangent and Binormal so we need to create some in order to have a valid tangent space
					IntersectionVertex.GenerateVertexTangents();

					// Calculate exitant radiance at the final gather ray intersection position.
					const FLinearColor PathVertexOutgoingRadiance = CalculateExitantRadiance(
						Mapping, 
						RayIntersection.Mapping, 
						RayIntersection.Mesh, 
						IntersectionVertex, 
						RayIntersection.VertexIndex, 
						RayIntersection.ElementIndex,
						-SampleDirection, 
						BounceNumber, 
						MappingContext, 
						RandomStream, 
						true,
						bDebugThisTexel && PhotonMappingSettings.bVisualizePhotonImportanceSamples);
					
					checkSlow(FLinearColorUtils::AreFloatsValid(PathVertexOutgoingRadiance));
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
					if (PathVertexOutgoingRadiance.R > DELTA || PathVertexOutgoingRadiance.G > DELTA || PathVertexOutgoingRadiance.B > DELTA)
					{
						bPositiveSample = true;
					}
#endif
					IncomingRadiance.AddWeighted(FGatheredLightSample::PointLightWorldSpace(PathVertexOutgoingRadiance, TangentSampleDirection, SampleDirection), SampleWeight);
					checkSlow(IncomingRadiance.AreFloatsValid());
				}
			}
			else
			{
				// The ray missed any geometry in the scene, calculate the environment contribution in the sample direction
				const FLinearColor EnvironmentLighting = EvaluateEnvironmentLighting(-SampleDirection);
				IncomingRadiance.AddWeighted(FGatheredLightSample::PointLightWorldSpace(EnvironmentLighting, TangentSampleDirection, SampleDirection), SampleWeight);
			}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
			if (bDebugThisTexel 
				&& GeneralSettings.ViewSingleBounceNumber == BounceNumber
				&& PhotonMappingSettings.bVisualizePhotonImportanceSamples)
			{
				FDebugStaticLightingRay DebugRay(PathRay.Start, PathRay.End, RayIntersection.bIntersects, bPositiveSample != 0);
				if (RayIntersection.bIntersects)
				{
					DebugRay.End = RayIntersection.IntersectionVertex.WorldPosition;
				}
				DebugOutput.PathRays.Add(DebugRay);
			}
#endif
		}
	}
	return IncomingRadiance;
}

FLinearColor FStaticLightingSystem::FinalGatherSample(
	const FStaticLightingMapping* Mapping,
	const FFullStaticLightingVertex& Vertex,
	const FVector4& TriangleTangentPathDirection,
	float SampleRadius,
	int32 BounceNumber,
	bool bDebugThisTexel,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	FLightingCacheGatherInfo& RecordGatherInfo,
	FFinalGatherInfo& FinalGatherInfo,
	FVector& OutUnoccludedSkyVector,
	FLinearColor& OutStationarySkyLighting) const
{
	FLinearColor Lighting = FLinearColor::Black;
	OutStationarySkyLighting = FLinearColor::Black;

	checkSlow(TriangleTangentPathDirection.Z >= 0.0f);
	checkSlow(TriangleTangentPathDirection.IsUnit3());

	// Generate the uniform hemisphere samples from a hemisphere based around the triangle normal, not the smoothed vertex normal
	// This is important for cases where the smoothed vertex normal is very different from the triangle normal, in which case
	// Using the smoothed vertex normal would cause self-intersection even on a plane
	const FVector4 WorldPathDirection = Vertex.TransformTriangleTangentVectorToWorld(TriangleTangentPathDirection);
	checkSlow(WorldPathDirection.IsUnit3());

	const FVector4 TangentPathDirection = Vertex.TransformWorldVectorToTangent(WorldPathDirection);
	checkSlow(TangentPathDirection.IsUnit3());

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 asdf = 0;
	}
#endif

	FVector4 SampleOffset(0,0,0);
	if (GeneralSettings.bAccountForTexelSize)
	{
		// Offset the sample's starting point in the tangent XY plane based on the sample's area of influence. 
		// This is particularly effective for large texels with high variance in the incoming radiance over the area of the texel.
		SampleOffset = Vertex.WorldTangentX * TangentPathDirection.X * SampleRadius * SceneConstants.VisibilityTangentOffsetSampleRadiusScale
			+ Vertex.WorldTangentY * TangentPathDirection.Y * SampleRadius * SceneConstants.VisibilityTangentOffsetSampleRadiusScale;
			
		// Experiment to distribute the starting position over the area of the texel to anti-alias, causes incorrect shadowing at intersections though
		//@todo - use consistent sample set between irradiance cache samples
		//const FVector2D DiskPosition = GetUniformUnitDiskPosition(RandomStream);
		//SampleOffset = Vertex.WorldTangentX * DiskPosition.X * SampleRadius * .5f + Vertex.WorldTangentY * DiskPosition.Y * SampleRadius * .5f;
	}

	const FLightRay PathRay(
		// Apply various offsets to the start of the ray.
		// The offset along the ray direction is to avoid incorrect self-intersection due to floating point precision.
		// The offset along the normal is to push self-intersection patterns (like triangle shape) on highly curved surfaces onto the backfaces.
		Vertex.WorldPosition 
		+ WorldPathDirection * SceneConstants.VisibilityRayOffsetDistance 
		+ Vertex.WorldTangentZ * SampleRadius * SceneConstants.VisibilityNormalOffsetSampleRadiusScale 
		+ SampleOffset,
		Vertex.WorldPosition + WorldPathDirection * MaxRayDistance,
		Mapping,
		NULL
		);

	MappingContext.Stats.NumFirstBounceRaysTraced++;
	const float LastRayTraceTime = MappingContext.RayCache.FirstHitRayTraceTime;
	FLightRayIntersection RayIntersection;
	AggregateMesh.IntersectLightRay(PathRay, true, false, false, MappingContext.RayCache, RayIntersection);
	MappingContext.Stats.FirstBounceRayTraceTime += MappingContext.RayCache.FirstHitRayTraceTime - LastRayTraceTime;

	bool bPositiveSample = false;

	OutUnoccludedSkyVector = RayIntersection.bIntersects ? FVector(0) : FVector(WorldPathDirection);

	if (RayIntersection.bIntersects)
	{
		const float IntersectionDistance = (Vertex.WorldPosition - RayIntersection.IntersectionVertex.WorldPosition).Size3();
		RecordGatherInfo.UpdateOnHit(IntersectionDistance);

		if (IntersectionDistance < AmbientOcclusionSettings.MaxOcclusionDistance)
		{
			const float DistanceFraction = IntersectionDistance / AmbientOcclusionSettings.MaxOcclusionDistance;
			const float DistanceWeight = 1.0f - 1.0f * DistanceFraction * DistanceFraction;
			FinalGatherInfo.NumSamplesOccluded += DistanceWeight / RayIntersection.Mesh->GetFullyOccludedSamplesFraction(RayIntersection.ElementIndex);
		}

		// Only continue if the ray hit the frontface of the polygon, otherwise the ray started inside a mesh
		if (Dot3(PathRay.Direction, -RayIntersection.IntersectionVertex.WorldTangentZ) > 0.0f)
		{
			if (GeneralSettings.NumIndirectLightingBounces > 0 && TangentPathDirection.Z > 0)
			{
				LIGHTINGSTAT(FScopedRDTSCTimer CalculateExitantRadianceTimer(MappingContext.Stats.CalculateExitantRadianceTime));
				FStaticLightingVertex IntersectionVertex = RayIntersection.IntersectionVertex;
				// The ray intersection does not return a Tangent and Binormal so we need to create some in order to have a valid tangent space
				IntersectionVertex.GenerateVertexTangents();

				// Calculate exitant radiance at the final gather ray intersection position.
				const FLinearColor PathVertexOutgoingRadiance = CalculateExitantRadiance(
					Mapping, 
					RayIntersection.Mapping, 
					RayIntersection.Mesh, 
					IntersectionVertex, 
					RayIntersection.VertexIndex, 
					RayIntersection.ElementIndex,
					-WorldPathDirection, 
					BounceNumber, 
					MappingContext, 
					RandomStream, 
					true,
					bDebugThisTexel && (!PhotonMappingSettings.bUsePhotonMapping || !PhotonMappingSettings.bVisualizePhotonImportanceSamples));

				checkSlow(FLinearColorUtils::AreFloatsValid(PathVertexOutgoingRadiance));
				Lighting += PathVertexOutgoingRadiance;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
				if (PathVertexOutgoingRadiance.R > DELTA || PathVertexOutgoingRadiance.G > DELTA || PathVertexOutgoingRadiance.B > DELTA)
				{
					if (bDebugThisTexel)
					{
						int32 TempBreak = 0;
					}
					bPositiveSample = true;
				}
#endif
			}
		}
		else
		{
			FinalGatherInfo.NumBackfaceHits++;
		}
	}
	else
	{
		if (TangentPathDirection.Z > 0)
		{
			const FLinearColor EnvironmentLighting = EvaluateEnvironmentLighting(-WorldPathDirection);
			Lighting += EnvironmentLighting;
		}
	}

	EvaluateSkyLighting(WorldPathDirection, RayIntersection.bIntersects, true, Lighting, OutStationarySkyLighting);

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel
		&& GeneralSettings.ViewSingleBounceNumber == BounceNumber
		&& (!PhotonMappingSettings.bUsePhotonMapping || !PhotonMappingSettings.bVisualizePhotonImportanceSamples))
	{
		FDebugStaticLightingRay DebugRay(PathRay.Start, PathRay.End, RayIntersection.bIntersects, bPositiveSample != 0);
		if (RayIntersection.bIntersects)
		{
			DebugRay.End = RayIntersection.IntersectionVertex.WorldPosition;
		}
		DebugOutput.PathRays.Add(DebugRay);
	}
#endif

	return Lighting;
}

/** Stores intermediate data during a traversal of the refinement tree. */
class FRefinementTraversalContext
{
public:
	FSimpleQuadTreeNode<FRefinementElement>* Node;
	FVector2D Min;
	FVector2D Size;
	bool bCausedByBrightnessDifference;

	FRefinementTraversalContext(FSimpleQuadTreeNode<FRefinementElement>* InNode, FVector2D InMin, FVector2D InSize, bool bInCausedByBrightnessDifference) :
		Node(InNode),
		Min(InMin),
		Size(InSize),
		bCausedByBrightnessDifference(bInCausedByBrightnessDifference)
	{}
};

/** Data structure used for adaptive refinement.  This is basically a 2d array of quadtrees. */
class FUniformHemisphereRefinementGrid
{
public:

	FUniformHemisphereRefinementGrid(int32 InNumThetaSteps, int32 InNumPhiSteps)
	{
		NumThetaSteps = InNumThetaSteps;
		NumPhiSteps = InNumPhiSteps;
		Cells.Empty(NumThetaSteps * NumPhiSteps);
		Cells.AddZeroed(NumThetaSteps * NumPhiSteps);
	}

	/** 
	 * Fetches a leaf node value at the desired fractional position.
	 * Expects a UV that is the center of the cell being searched for, not the min. 
	 */
	const FLightingAndOcclusion& GetValue(FVector2D UV)
	{
		// Theta is radius, clamp
		const int32 ThetaIndex = FMath::Clamp(FMath::FloorToInt(UV.X * NumThetaSteps), 0, NumThetaSteps - 1);
		// Phi is angle around the hemisphere axis, wrap on both ends
		const int32 PhiIndex = (FMath::FloorToInt(UV.Y * NumPhiSteps) + NumPhiSteps) % NumPhiSteps;
		const float CellU = FMath::Fractional(FMath::Clamp(UV.X, 0.0f, .9999f) * NumThetaSteps);
		const float CellV = FMath::Abs(FMath::Fractional(UV.Y * NumPhiSteps));
		const FSimpleQuadTree<FRefinementElement>& QuadTree = Cells[ThetaIndex * NumPhiSteps + PhiIndex];

		return QuadTree.GetLeafElement(CellU, CellV).Lighting;
	}

	const FLightingAndOcclusion& GetRootValue(int32 ThetaIndex, int32 PhiIndex)
	{
		return Cells[ThetaIndex * NumPhiSteps + PhiIndex].RootNode.Element.Lighting;
	}

	/** Computes the value for the requested cell by averaging all the leaves inside the cell. */
	FLightingAndOcclusion GetFilteredValue(int32 ThetaIndex, int32 PhiIndex)
	{
		return GetFilteredValueRecursive(&Cells[ThetaIndex * NumPhiSteps + PhiIndex].RootNode);
	}

	void SetRootElement(int32 ThetaIndex, int32 PhiIndex, const FRefinementElement& Element)
	{
		Cells[ThetaIndex * NumPhiSteps + PhiIndex].RootNode.Element = Element;
	}

	void ReturnToFreeList(TArray<FSimpleQuadTreeNode<FRefinementElement>*>& OutNodes)
	{
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); CellIndex++)
		{
			Cells[CellIndex].ReturnToFreeList(OutNodes);
		}
	}

	void RefineIncomingRadiance(
		const FStaticLightingSystem& LightingSystem,
		const FStaticLightingMapping* Mapping,
		const FFullStaticLightingVertex& Vertex,
		float SampleRadius,
		int32 BounceNumber,
		const TArray<FVector4, TInlineAllocator<30> >& TangentImportancePhotonDirections,
		FStaticLightingMappingContext& MappingContext,
		FLMRandomStream& RandomStream,
		bool bDebugThisTexel)
	{
		FIntPoint Neighbors[8];
		Neighbors[0] = FIntPoint(1, 0);
		Neighbors[1] = FIntPoint(-1, 0);
		Neighbors[2] = FIntPoint(0, 1);
		Neighbors[3] = FIntPoint(0, -1);
		Neighbors[4] = FIntPoint(1, 1);
		Neighbors[5] = FIntPoint(1, -1);
		Neighbors[6] = FIntPoint(-1, 1);
		Neighbors[7] = FIntPoint(-1, -1);

		TArray<FRefinementTraversalContext> NodesToRefine[2];
		NodesToRefine[0].Empty(400);
		NodesToRefine[1].Empty(400);

		const int32 NumSubsamples = 2;

		TArray<FRefinementTraversalContext>* CurrentNodesToRefine = &NodesToRefine[0];
		TArray<FRefinementTraversalContext>* NextNodesToRefine = &NodesToRefine[1];

		float ImportanceConeAngle = LightingSystem.ImportanceTracingSettings.AdaptiveFirstBouncePhotonConeAngle;
		// Approximation for the cone angle of a root level cell
		const float RootCellAngle = PI * FMath::Sqrt((.5f / NumThetaSteps) * (.5f / NumThetaSteps) + (.5f / NumPhiSteps) * (.5f / NumPhiSteps));
		const float RootCombinedAngleThreshold = FMath::Cos(ImportanceConeAngle + RootCellAngle);
		const float ConeIntersectionWeight = 1.0f / TangentImportancePhotonDirections.Num();

		float BrightnessThreshold = LightingSystem.ImportanceTracingSettings.AdaptiveBrightnessThreshold;
		float SkyOcclusionThreshold = LightingSystem.ImportanceTracingSettings.AdaptiveBrightnessThreshold;
		bool bRefineForSkyOcclusion = LightingSystem.SkyLights.Num() > 0;

		// This is basically disabled, causes too much noise in worst case scenarios (all GI coming from small bright spot)
		float ConeWeightThreshold = .006f;

		// Operate on all cells at a refinement depth before going deeper
		// This is necessary for the neighbor comparisons to work right
		for (int32 RefinementDepth = 0; RefinementDepth < LightingSystem.ImportanceTracingSettings.NumAdaptiveRefinementLevels; RefinementDepth++)
		{
			if (bDebugThisTexel)
			{
				int32 asdf = 0;
			}

			FLinearColor TotalLighting = FLinearColor::Black;

			// Recalculate total lighting based on the refined results
			for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
			{
				for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
				{
					const FLightingAndOcclusion FilteredLighting = GetFilteredValue(ThetaIndex, PhiIndex);
					TotalLighting += FilteredLighting.Lighting + FilteredLighting.StationarySkyLighting;
				}
			}

			const float TotalBrightness = FMath::Max(TotalLighting.GetLuminance(), .01f);

			// At depth 0 we are operating on the 2d grid 
			if (RefinementDepth == 0)
			{
				for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
				{
					for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
					{
						const FVector4 CellCenterDirection = UniformSampleHemisphere((ThetaIndex + .5f) / (float)NumThetaSteps, (PhiIndex + .5f) / (float)NumPhiSteps);
						float IntersectingImportanceConeWeight = 0;

						// Accumulate weight of intersecting photon cones
						for (int32 ImportanceDirectionIndex = 0; ImportanceDirectionIndex < TangentImportancePhotonDirections.Num(); ImportanceDirectionIndex++)
						{
							const float CosAngleBetweenCones = Dot3(TangentImportancePhotonDirections[ImportanceDirectionIndex], CellCenterDirection);
								
							// Cone intersection by comparing the cosines of angles
							// In the range [0, PI], cosine is always decreasing while the input angle is increasing, so we can just flip the comparison from what we would do on the angle
							if (CosAngleBetweenCones > RootCombinedAngleThreshold)
							{
								IntersectingImportanceConeWeight += ConeIntersectionWeight;

								if (IntersectingImportanceConeWeight >= ConeWeightThreshold)
								{
									break;
								}
							}
						}

						const bool bSuperSampleDueToImportanceCones = IntersectingImportanceConeWeight >= ConeWeightThreshold;

						float MaxRelativeDifference = 0;
						float MaxSkyOcclusionDifference = 0;

						// Determine maximum relative brightness difference
						if (!bSuperSampleDueToImportanceCones)
						{
							const FLightingAndOcclusion RootElementLighting = GetRootValue(ThetaIndex, PhiIndex);
							const FLinearColor Radiance = RootElementLighting.Lighting + RootElementLighting.StationarySkyLighting;
							const float RelativeBrightness = Radiance.ComputeLuminance() / TotalBrightness;

							for (int32 NeighborIndex = 0; NeighborIndex < ARRAY_COUNT(Neighbors); NeighborIndex++)
							{
								int32 NeighborTheta = ThetaIndex + Neighbors[NeighborIndex].X;
								// Wrap phi around, since it is the angle around the hemisphere axis
								// Add NumPhiSteps to handle negative
								int32 NeighborPhi = ((PhiIndex + Neighbors[NeighborIndex].Y) + NumPhiSteps) % NumPhiSteps;

								if (NeighborTheta >= 0 && NeighborTheta < NumThetaSteps)
								{
									const FLightingAndOcclusion NeighborLighting = GetRootValue(NeighborTheta, NeighborPhi);
									const float NeighborBrightness = (NeighborLighting.Lighting + NeighborLighting.StationarySkyLighting).ComputeLuminance();
									const float NeighborRelativeBrightness = NeighborBrightness / TotalBrightness;
									MaxRelativeDifference = FMath::Max(MaxRelativeDifference, FMath::Abs(RelativeBrightness - NeighborRelativeBrightness));
									MaxSkyOcclusionDifference = FMath::Max(MaxSkyOcclusionDifference, FMath::Abs(RootElementLighting.UnoccludedSkyVector.SizeSquared() - NeighborLighting.UnoccludedSkyVector.SizeSquared()));
								}
							}
						}

						// Refine if the importance cone threshold is exceeded or there was a big enough brightness difference
						if (MaxRelativeDifference > BrightnessThreshold 
							|| (bRefineForSkyOcclusion && MaxSkyOcclusionDifference > SkyOcclusionThreshold)
							|| bSuperSampleDueToImportanceCones)
						{
							FSimpleQuadTreeNode<FRefinementElement>* Node = &Cells[ThetaIndex * NumPhiSteps + PhiIndex].RootNode;

							NextNodesToRefine->Add(FRefinementTraversalContext(
								Node, 
								FVector2D(ThetaIndex / (float)NumThetaSteps, PhiIndex / (float)NumPhiSteps),
								FVector2D(1 / (float)NumThetaSteps, 1 / (float)NumPhiSteps),
								MaxRelativeDifference > BrightnessThreshold || MaxSkyOcclusionDifference > SkyOcclusionThreshold));
						}
					}
				}
			}
			// At depth > 0 we are operating on quadtree nodes
			else
			{
				// Reset output without reallocating
				NextNodesToRefine->Reset(); 

				float SubCellCombinedAngleThreshold = 0;
				
				// The cell size will be the same for all cells of this depth, so calculate it once
				if (CurrentNodesToRefine->Num() > 0)
				{
					FRefinementTraversalContext NodeContext = (*CurrentNodesToRefine)[0];
					const FVector2D HalfSubCellSize = NodeContext.Size / 4;
					// Approximate the cone angle of the sub cell
					const float SubCellAngle = PI * FMath::Sqrt(HalfSubCellSize.X * HalfSubCellSize.X + HalfSubCellSize.Y * HalfSubCellSize.Y);
					SubCellCombinedAngleThreshold = FMath::Cos(ImportanceConeAngle + SubCellAngle);
				}

				for (int32 NodeIndex = 0; NodeIndex < CurrentNodesToRefine->Num(); NodeIndex++)
				{
					FRefinementTraversalContext NodeContext = (*CurrentNodesToRefine)[NodeIndex];
					const FVector2D HalfSubCellSize = NodeContext.Size / 4;

					for (int32 SubThetaIndex = 0; SubThetaIndex < NumSubsamples; SubThetaIndex++)
					{
						for (int32 SubPhiIndex = 0; SubPhiIndex < NumSubsamples; SubPhiIndex++)
						{
							FSimpleQuadTreeNode<FRefinementElement>* ChildNode = NodeContext.Node->Children[SubThetaIndex * NumSubsamples + SubPhiIndex];

							const FVector4 CellCenterDirection = UniformSampleHemisphere(
								NodeContext.Min.X + SubThetaIndex * NodeContext.Size.X / 2 + NodeContext.Size.X / 4,
								NodeContext.Min.Y + SubPhiIndex * NodeContext.Size.Y / 2 + NodeContext.Size.Y / 4);

							float IntersectingImportanceConeWeight = 0;

							// Accumulate weight of intersecting photon cones
							for (int32 ImportanceDirectionIndex = 0; ImportanceDirectionIndex < TangentImportancePhotonDirections.Num(); ImportanceDirectionIndex++)
							{
								const float CosAngleBetweenCones = Dot3(TangentImportancePhotonDirections[ImportanceDirectionIndex], CellCenterDirection);

								// Cone intersection by comparing the cosines of angles
								// In the range [0, PI], cosine is always decreasing while the input angle is increasing, so we can just flip the comparison from what we would do on the angle
								if (CosAngleBetweenCones > SubCellCombinedAngleThreshold)
								{
									IntersectingImportanceConeWeight += ConeIntersectionWeight;

									if (IntersectingImportanceConeWeight >= ConeWeightThreshold)
									{
										break;
									}
								}
							}

							const bool bSuperSampleDueToImportanceCones = IntersectingImportanceConeWeight >= ConeWeightThreshold;

							float MaxRelativeDifference = 0;
							float MaxSkyOcclusionDifference = 0;

							// Determine maximum relative brightness difference
							if (!bSuperSampleDueToImportanceCones)
							{
								const FLightingAndOcclusion ChildLighting = ChildNode->Element.Lighting;
								const FLinearColor Radiance = ChildLighting.Lighting + ChildLighting.StationarySkyLighting;
								const float RelativeBrightness = Radiance.ComputeLuminance() / TotalBrightness;

								// Only search the axis neighbors past the first depth
								for (int32 NeighborIndex = 0; NeighborIndex < ARRAY_COUNT(Neighbors) / 2; NeighborIndex++)
								{
									const float NeighborU = NodeContext.Min.X + (SubThetaIndex + Neighbors[NeighborIndex].X) * NodeContext.Size.X / 2;
									const float NeighborV = NodeContext.Min.Y + (SubPhiIndex + Neighbors[NeighborIndex].Y) * NodeContext.Size.Y / 2;

									// Query must be done on the center of the cell
									const FVector2D NeighborUV = FVector2D(NeighborU, NeighborV) + NodeContext.Size / 4;
									const FLightingAndOcclusion NeighborLighting = GetValue(NeighborUV);
									const float NeighborBrightness = (NeighborLighting.Lighting + NeighborLighting.StationarySkyLighting).ComputeLuminance();
									const float NeighborRelativeBrightness = NeighborBrightness / TotalBrightness;
									MaxRelativeDifference = FMath::Max(MaxRelativeDifference, FMath::Abs(RelativeBrightness - NeighborRelativeBrightness));
									MaxSkyOcclusionDifference = FMath::Max(MaxSkyOcclusionDifference, FMath::Abs(ChildLighting.UnoccludedSkyVector.SizeSquared() - NeighborLighting.UnoccludedSkyVector.SizeSquared()));
								}
							}

							// Refine if the importance cone threshold is exceeded or there was a big enough brightness difference
							if (MaxRelativeDifference > BrightnessThreshold 
								|| (bRefineForSkyOcclusion && MaxSkyOcclusionDifference > SkyOcclusionThreshold)
								|| bSuperSampleDueToImportanceCones)
							{
								NextNodesToRefine->Add(FRefinementTraversalContext(
									ChildNode, 
									FVector2D(NodeContext.Min.X + SubThetaIndex * NodeContext.Size.X / 2, NodeContext.Min.Y + SubPhiIndex * NodeContext.Size.Y / 2),
									NodeContext.Size / 2.0f,
									MaxRelativeDifference > BrightnessThreshold || MaxSkyOcclusionDifference > SkyOcclusionThreshold));
							}
						}
					}
				}
			}

			// Swap input and output for the next step
			Swap(CurrentNodesToRefine, NextNodesToRefine);

			if (bDebugThisTexel)
			{
				int32 asdf = 0;
			}

			FStaticLightingMappingStats& Stats = MappingContext.Stats;

			for (int32 NodeIndex = 0; NodeIndex < CurrentNodesToRefine->Num(); NodeIndex++)
			{
				FRefinementTraversalContext NodeContext = (*CurrentNodesToRefine)[NodeIndex];
				FLinearColor SubsampledRadiance = FLinearColor::Black;
				FLightingCacheGatherInfo SubsampleGatherInfo; 

				for (int32 SubThetaIndex = 0; SubThetaIndex < NumSubsamples; SubThetaIndex++)
				{
					for (int32 SubPhiIndex = 0; SubPhiIndex < NumSubsamples; SubPhiIndex++)
					{
						FSimpleQuadTreeNode<FRefinementElement>* FreeNode = NULL;
						
						if (MappingContext.RefinementTreeFreePool.Num() > 0)
						{
							FreeNode = MappingContext.RefinementTreeFreePool.Pop(false);
							*FreeNode = FSimpleQuadTreeNode<FRefinementElement>();
						}
						else
						{
							FreeNode = new FSimpleQuadTreeNode<FRefinementElement>();
						}

						const FVector2D ChildMin = NodeContext.Min + FVector2D(SubThetaIndex, SubPhiIndex) * NodeContext.Size / 2;

						// Reuse the parent sample result in whatever child cell it falls in
						if (NodeContext.Node->Element.Uniforms.X >= ChildMin.X
							&& NodeContext.Node->Element.Uniforms.Y >= ChildMin.Y
							&& NodeContext.Node->Element.Uniforms.X < ChildMin.X + NodeContext.Size.X / 2
							&& NodeContext.Node->Element.Uniforms.Y < ChildMin.Y + NodeContext.Size.Y / 2)
						{
							FreeNode->Element = NodeContext.Node->Element;
						}
						else
						{
							const float U1 = RandomStream.GetFraction();
							const float U2 = RandomStream.GetFraction();
							// Stratified sampling, pick a random position within the target cell
							const float SubStepFraction1 = (SubThetaIndex + U1) / (float)NumSubsamples;
							const float SubStepFraction2 = (SubPhiIndex + U2) / (float)NumSubsamples;
							const float Fraction1 = NodeContext.Min.X + SubStepFraction1 * NodeContext.Size.X;
							const float Fraction2 = NodeContext.Min.Y + SubStepFraction2 * NodeContext.Size.Y;

							const FVector4 SampleDirection = UniformSampleHemisphere(Fraction1, Fraction2);

							FVector UnoccludedSkyVector;
							FLinearColor StationarySkyLighting;
							FFinalGatherInfo SubsampleFinalGatherInfo;

							const FLinearColor SubsampleLighting = LightingSystem.FinalGatherSample(
								Mapping,
								Vertex,
								SampleDirection,
								SampleRadius,
								BounceNumber,
								bDebugThisTexel,
								MappingContext,
								RandomStream,
								SubsampleGatherInfo,
								SubsampleFinalGatherInfo,
								UnoccludedSkyVector,
								StationarySkyLighting);

							FreeNode->Element = FRefinementElement(FLightingAndOcclusion(SubsampleLighting, UnoccludedSkyVector, StationarySkyLighting, SubsampleFinalGatherInfo.NumSamplesOccluded), FVector2D(Fraction1, Fraction2));

							Stats.NumRefiningFinalGatherSamples[RefinementDepth]++;

							if (NodeContext.bCausedByBrightnessDifference)
							{
								Stats.NumRefiningSamplesDueToBrightness++;
							}
							else
							{
								Stats.NumRefiningSamplesDueToImportancePhotons++;
							}
						}

						NodeContext.Node->AddChild(SubThetaIndex * NumSubsamples + SubPhiIndex, FreeNode);
					}
				}
			}

			// Tighten the refinement criteria for the next depth level
			// These have a huge impact on build time with a large depth limit
			ImportanceConeAngle /= 4;
			BrightnessThreshold *= 2;
			ConeWeightThreshold *= 1.5f;
			SkyOcclusionThreshold *= 16;
		}
	}

private:

	// Dimensions of the base 2d grid
	int32 NumThetaSteps;
	int32 NumPhiSteps;

	// 2d grid of quadtrees for refinement
	TArray<FSimpleQuadTree<FRefinementElement> > Cells;

	FLightingAndOcclusion GetFilteredValueRecursive(const FSimpleQuadTreeNode<FRefinementElement>* Parent) const
	{
		if (Parent->Children[0])
		{
			FLightingAndOcclusion FilteredValue;

			for (int32 ChildIndex = 0; ChildIndex < ARRAY_COUNT(Parent->Children); ChildIndex++)
			{
				FilteredValue = FilteredValue + GetFilteredValueRecursive(Parent->Children[ChildIndex]) / 4.0f;
			}

			return FilteredValue;
		}
		else
		{
			return Parent->Element.Lighting;
		}
	}
};

/** 
 * Final gather using adaptive sampling to estimate the incident radiance function. 
 * Adaptive refinement is done on brightness differences and anywhere that a first bounce photon determined lighting was coming from.
 */
template<class SampleType>
SampleType FStaticLightingSystem::IncomingRadianceAdaptive(
	const FStaticLightingMapping* Mapping,
	const FFullStaticLightingVertex& Vertex,
	float SampleRadius,
	bool bIntersectingSurface,
	int32 ElementIndex,
	int32 BounceNumber,
	const TArray<FVector4>& UniformHemisphereSamples,
	const TArray<FVector2D>& UniformHemisphereSampleUniforms,
	float MaxUnoccludedLength,
	const TArray<FVector4>& ImportancePhotonDirections,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	FLightingCacheGatherInfo& GatherInfo,
	bool bDebugThisTexel) const
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 TempBreak = 0;
	}
#endif

	const double StartBaseTraceTime = FPlatformTime::Seconds();

	const int32 NumThetaSteps = FMath::TruncToInt(FMath::Sqrt(UniformHemisphereSamples.Num() / (float)PI) + .5f);
	const int32 NumPhiSteps = UniformHemisphereSamples.Num() / NumThetaSteps;
	checkSlow(NumThetaSteps * NumPhiSteps == UniformHemisphereSamples.Num());

	int32 NumBackfaceHits = 0;
	FUniformHemisphereRefinementGrid RefinementGrid(NumThetaSteps, NumPhiSteps);

	// Initialize the root level of the refinement grid with lighting values
	for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
	{
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			const int32 SampleIndex = ThetaIndex * NumPhiSteps + PhiIndex;
			const FVector4 TriangleTangentPathDirection = UniformHemisphereSamples[SampleIndex];
			
			FVector UnoccludedSkyVector;
			FLinearColor StationarySkyLighting;
			FFinalGatherInfo FinalGatherInfo;

			const FLinearColor Radiance = FinalGatherSample(
				Mapping,
				Vertex,
				TriangleTangentPathDirection,
				SampleRadius,
				BounceNumber,
				bDebugThisTexel,
				MappingContext,
				RandomStream,
				GatherInfo,
				FinalGatherInfo,
				UnoccludedSkyVector,
				StationarySkyLighting);

			NumBackfaceHits += FinalGatherInfo.NumBackfaceHits;
			RefinementGrid.SetRootElement(ThetaIndex, PhiIndex, FRefinementElement(FLightingAndOcclusion(Radiance, UnoccludedSkyVector, StationarySkyLighting, FinalGatherInfo.NumSamplesOccluded), UniformHemisphereSampleUniforms[SampleIndex]));
		}
	}

	const double EndBaseTraceTime = FPlatformTime::Seconds();

	MappingContext.Stats.BaseFinalGatherSampleTime += EndBaseTraceTime - StartBaseTraceTime;
	MappingContext.Stats.NumBaseFinalGatherSamples += NumThetaSteps * NumPhiSteps;
	GatherInfo.BackfacingHitsFraction = NumBackfaceHits / (float)UniformHemisphereSamples.Num();

	// Refine if we are not hidden inside some geometry
	const bool bRefine = GatherInfo.BackfacingHitsFraction < .5f || bIntersectingSurface;

	if (bRefine)
	{
		TArray<FVector4, TInlineAllocator<30> > TangentSpaceImportancePhotonDirections;
		TangentSpaceImportancePhotonDirections.Empty(ImportancePhotonDirections.Num());

		for (int32 PhotonIndex = 0; PhotonIndex < ImportancePhotonDirections.Num(); PhotonIndex++)
		{
			TangentSpaceImportancePhotonDirections.Add(Vertex.TransformWorldVectorToTriangleTangent(ImportancePhotonDirections[PhotonIndex]));
		}

		RefinementGrid.RefineIncomingRadiance(
			*this,
			Mapping,
			Vertex,
			SampleRadius,
			BounceNumber,
			TangentSpaceImportancePhotonDirections,
			MappingContext,
			RandomStream,
			bDebugThisTexel);
	}

	const double EndRefiningTime = FPlatformTime::Seconds();

	MappingContext.Stats.RefiningFinalGatherSampleTime += EndRefiningTime - EndBaseTraceTime;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 TempBreak = 0;
	}
#endif

	SampleType IncomingRadiance;
	FVector CombinedSkyUnoccludedDirection(0);
	float NumSamplesOccluded = 0;

	// Accumulate lighting from all samples
	for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
	{
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			const int32 SampleIndex = ThetaIndex * NumPhiSteps + PhiIndex;

			const FVector4 TriangleTangentPathDirection = UniformHemisphereSamples[SampleIndex];
			checkSlow(TriangleTangentPathDirection.Z >= 0.0f);
			checkSlow(TriangleTangentPathDirection.IsUnit3());

			const FVector4 WorldPathDirection = Vertex.TransformTriangleTangentVectorToWorld(TriangleTangentPathDirection);
			checkSlow(WorldPathDirection.IsUnit3());

			const FVector4 TangentPathDirection = Vertex.TransformWorldVectorToTangent(WorldPathDirection);
			checkSlow(TangentPathDirection.IsUnit3());

			const float PDF = EvaluatePDF(Vertex, WorldPathDirection);
			const float SampleWeight = 1.0f / (PDF * UniformHemisphereSamples.Num());

			const FLightingAndOcclusion FilteredLighting = RefinementGrid.GetFilteredValue(ThetaIndex, PhiIndex);
			// Get the filtered lighting from the leaves of the refinement trees
			const FLinearColor Radiance = FilteredLighting.Lighting;
			CombinedSkyUnoccludedDirection += FilteredLighting.UnoccludedSkyVector;

			IncomingRadiance.AddIncomingRadiance(Radiance, SampleWeight, TangentPathDirection, WorldPathDirection);
			IncomingRadiance.AddIncomingStationarySkyLight(FilteredLighting.StationarySkyLighting, SampleWeight, TangentPathDirection, WorldPathDirection);
			checkSlow(IncomingRadiance.AreFloatsValid());
			NumSamplesOccluded += FilteredLighting.NumSamplesOccluded;
		}
	}

	// Calculate the fraction of samples which were occluded
	const float MaterialElementFullyOccludedSamplesFraction = Mapping ? Mapping->Mesh->GetFullyOccludedSamplesFraction(ElementIndex) : 1.0f;
	const float OcclusionFraction = FMath::Min(NumSamplesOccluded / (AmbientOcclusionSettings.FullyOccludedSamplesFraction * MaterialElementFullyOccludedSamplesFraction * UniformHemisphereSamples.Num()), 1.0f);
	// Constant which maintains an integral of .5 for the unclamped exponential function applied to occlusion below
	// An integral of .5 is important because it makes an image with a uniform distribution of occlusion values stay the same brightness with different exponents.
	// As a result, OcclusionExponent just controls contrast and doesn't affect brightness.
	const float NormalizationConstant = .5f * (AmbientOcclusionSettings.OcclusionExponent + 1);
	IncomingRadiance.SetOcclusion(FMath::Clamp(NormalizationConstant * FMath::Pow(OcclusionFraction, AmbientOcclusionSettings.OcclusionExponent), 0.0f, 1.0f)); 

	const FVector BentNormal = CombinedSkyUnoccludedDirection / (MaxUnoccludedLength * UniformHemisphereSamples.Num());
	IncomingRadiance.SetSkyOcclusion(BentNormal);

	RefinementGrid.ReturnToFreeList(MappingContext.RefinementTreeFreePool);

	return IncomingRadiance;
}


/** Final gather using uniform sampling to estimate the incident radiance function. */
template<class SampleType>
SampleType FStaticLightingSystem::IncomingRadianceUniform(
	const FStaticLightingMapping* Mapping,
	const FFullStaticLightingVertex& Vertex,
	float SampleRadius,
	int32 ElementIndex,
	int32 BounceNumber,
	const TArray<FVector4>& UniformHemisphereSamples,
	float MaxUnoccludedLength,
	const TArray<FVector4>& ImportancePhotonDirections,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	FLightingCacheGatherInfo& GatherInfo,
	bool bDebugThisTexel) const
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 TempBreak = 0;
	}
#endif

	// Photons will only be deposited on shadow casting surfaces
	const bool bSurfaceCanReceiveIndirectPhotons = (!Mapping || (Mapping->Mesh->LightingFlags & GI_INSTANCE_CASTSHADOW)) 
		&& ImportancePhotonDirections.Num() > 0;

	SampleType IncomingRadiance;
	const int32 PhotonImportanceHemisphereSamples = ImportancePhotonDirections.Num() > 0 ? GetNumPhotonImportanceHemisphereSamples() : 0;

	// Initialize the data needed for calculating irradiance gradients
	if (IrradianceCachingSettings.bUseIrradianceGradients)
	{
		GatherInfo.PreviousIncidentRadiances.AddZeroed(UniformHemisphereSamples.Num());
		GatherInfo.PreviousDistances.AddZeroed(UniformHemisphereSamples.Num());
	}
	 
	int32 NumBackfaceHits = 0;
	float NumSamplesOccluded = 0;
	FVector CombinedSkyUnoccludedDirection(0);

	// Estimate the indirect part of the light transport equation using uniform sampled monte carlo integration
	//@todo - use cosine sampling if possible to match the indirect integrand, the irradiance caching algorithm assumes uniform sampling
	for (int32 SampleIndex = 0; SampleIndex < UniformHemisphereSamples.Num(); SampleIndex++)
	{
		const FVector4 TriangleTangentPathDirection = UniformHemisphereSamples[SampleIndex];
		checkSlow(TriangleTangentPathDirection.Z >= 0.0f);
		checkSlow(TriangleTangentPathDirection.IsUnit3());

		// Generate the uniform hemisphere samples from a hemisphere based around the triangle normal, not the smoothed vertex normal
		// This is important for cases where the smoothed vertex normal is very different from the triangle normal, in which case
		// Using the smoothed vertex normal would cause self-intersection even on a plane
		const FVector4 WorldPathDirection = Vertex.TransformTriangleTangentVectorToWorld(TriangleTangentPathDirection);
		checkSlow(WorldPathDirection.IsUnit3());

		const FVector4 TangentPathDirection = Vertex.TransformWorldVectorToTangent(WorldPathDirection);
		checkSlow(TangentPathDirection.IsUnit3());

		FVector4 SampleOffset(0,0,0);
		if (GeneralSettings.bAccountForTexelSize)
		{
			// Offset the sample's starting point in the tangent XY plane based on the sample's area of influence. 
			// This is particularly effective for large texels with high variance in the incoming radiance over the area of the texel.
			SampleOffset = Vertex.WorldTangentX * TangentPathDirection.X * SampleRadius * SceneConstants.VisibilityTangentOffsetSampleRadiusScale
				+ Vertex.WorldTangentY * TangentPathDirection.Y * SampleRadius * SceneConstants.VisibilityTangentOffsetSampleRadiusScale;
		}

		const FLightRay PathRay(
			// Apply various offsets to the start of the ray.
			// The offset along the ray direction is to avoid incorrect self-intersection due to floating point precision.
			// The offset along the normal is to push self-intersection patterns (like triangle shape) on highly curved surfaces onto the backfaces.
			Vertex.WorldPosition 
				+ WorldPathDirection * SceneConstants.VisibilityRayOffsetDistance 
				+ Vertex.WorldTangentZ * SampleRadius * SceneConstants.VisibilityNormalOffsetSampleRadiusScale 
				+ SampleOffset,
			Vertex.WorldPosition + WorldPathDirection * MaxRayDistance,
			Mapping,
			NULL
			);

		MappingContext.Stats.NumFirstBounceRaysTraced++;
		const float LastRayTraceTime = MappingContext.RayCache.FirstHitRayTraceTime;
		FLightRayIntersection RayIntersection;
		AggregateMesh.IntersectLightRay(PathRay, true, false, false, MappingContext.RayCache, RayIntersection);
		MappingContext.Stats.FirstBounceRayTraceTime += MappingContext.RayCache.FirstHitRayTraceTime - LastRayTraceTime;

		float PhotonImportanceSampledPDF = 0.0f;
		{
			LIGHTINGSTAT(FScopedRDTSCTimer CalculateSamplePDFTimer(MappingContext.Stats.CalculateImportanceSampleTime));
			if (PhotonImportanceHemisphereSamples > 0)
			{
				MappingContext.Stats.NumImportancePDFCalculations++;
				const float ConePDF = UniformConePDF(PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle);

				// Calculate the probability that this sample was generated by importance sampling using the nearby photon directions
				for (int32 OtherPhotonIndex = 0; OtherPhotonIndex < ImportancePhotonDirections.Num(); OtherPhotonIndex++)
				{
					if (Dot3(ImportancePhotonDirections[OtherPhotonIndex], WorldPathDirection) > (1.0f - DELTA) * PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle)
					{
						PhotonImportanceSampledPDF += ConePDF;
					}
				}

				PhotonImportanceSampledPDF /= ImportancePhotonDirections.Num();
			}
		}

		const float PDF = EvaluatePDF(Vertex, WorldPathDirection);
		// Calculate the multiple importance sample weight for this sample direction using a power heuristic
		const float MultipleImportanceSamplingWeight = PowerHeuristic(UniformHemisphereSamples.Num(), PDF, PhotonImportanceHemisphereSamples, PhotonImportanceSampledPDF);
		const float SampleWeight = MultipleImportanceSamplingWeight / (PDF * UniformHemisphereSamples.Num());
		bool bPositiveSample = false;

		if (RayIntersection.bIntersects)
		{
			const float IntersectionDistance = (Vertex.WorldPosition - RayIntersection.IntersectionVertex.WorldPosition).Size3();
			GatherInfo.UpdateOnHit(IntersectionDistance);

			if (IntersectionDistance < AmbientOcclusionSettings.MaxOcclusionDistance)
			{
				NumSamplesOccluded += 1.0f / RayIntersection.Mesh->GetFullyOccludedSamplesFraction(RayIntersection.ElementIndex);
			}

			// Only continue if the ray hit the frontface of the polygon, otherwise the ray started inside a mesh
			if (Dot3(PathRay.Direction, -RayIntersection.IntersectionVertex.WorldTangentZ) > 0.0f && TangentPathDirection.Z > 0)
			{
				if (GeneralSettings.NumIndirectLightingBounces > 0)
				{
					LIGHTINGSTAT(FScopedRDTSCTimer CalculateExitantRadianceTimer(MappingContext.Stats.CalculateExitantRadianceTime));
					FStaticLightingVertex IntersectionVertex = RayIntersection.IntersectionVertex;
					// The ray intersection does not return a Tangent and Binormal so we need to create some in order to have a valid tangent space
					IntersectionVertex.GenerateVertexTangents();

					// Calculate exitant radiance at the final gather ray intersection position.
					const FLinearColor PathVertexOutgoingRadiance = CalculateExitantRadiance(
						Mapping, 
						RayIntersection.Mapping, 
						RayIntersection.Mesh, 
						IntersectionVertex, 
						RayIntersection.VertexIndex, 
						RayIntersection.ElementIndex,
						-WorldPathDirection, 
						BounceNumber, 
						MappingContext, 
						RandomStream, 
						// Exclude direct lighting when there are indirect photons, as first bounce lighting from uniform sampling tends to have very high variance
						// While the first bounce lighting from importance sampling will be much cleaner.  This is incorrect as energy is lost, but improves quality.
						!bSurfaceCanReceiveIndirectPhotons,
						bDebugThisTexel && (!PhotonMappingSettings.bUsePhotonMapping || !PhotonMappingSettings.bVisualizePhotonImportanceSamples));

					checkSlow(FLinearColorUtils::AreFloatsValid(PathVertexOutgoingRadiance));
					IncomingRadiance.AddIncomingRadiance(PathVertexOutgoingRadiance, SampleWeight, TangentPathDirection, WorldPathDirection);
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
					if (PathVertexOutgoingRadiance.R > DELTA || PathVertexOutgoingRadiance.G > DELTA || PathVertexOutgoingRadiance.B > DELTA)
					{
						if (bDebugThisTexel)
						{
							int32 TempBreak = 0;
						}
						bPositiveSample = true;
					}
#endif
					checkSlow(IncomingRadiance.AreFloatsValid());
					if (IrradianceCachingSettings.bUseIrradianceGradients)
					{
						GatherInfo.PreviousIncidentRadiances[SampleIndex] = PathVertexOutgoingRadiance;
					}
				}
			}
			else
			{
				NumBackfaceHits++;
			}

			if (IrradianceCachingSettings.bUseIrradianceGradients)
			{
				GatherInfo.PreviousDistances[SampleIndex] = IntersectionDistance;
			}
		}
		else
		{
			const FLinearColor EnvironmentLighting = EvaluateEnvironmentLighting(-WorldPathDirection);

			if (TangentPathDirection.Z > 0)
			{
				IncomingRadiance.AddIncomingRadiance(EnvironmentLighting, SampleWeight, TangentPathDirection, WorldPathDirection);
			}
			
			CombinedSkyUnoccludedDirection += WorldPathDirection;

			if (IrradianceCachingSettings.bUseIrradianceGradients)
			{
				GatherInfo.PreviousIncidentRadiances[SampleIndex] = EnvironmentLighting;
				GatherInfo.PreviousDistances[SampleIndex] = MaxRayDistance;
			}
		}

		FLinearColor StaticSkyLighting = FLinearColor::Black;
		FLinearColor StationarySkyLighting = FLinearColor::Black;
		EvaluateSkyLighting(WorldPathDirection, RayIntersection.bIntersects, true, StaticSkyLighting, StationarySkyLighting);

		if (StaticSkyLighting != FLinearColor::Black)
		{
			IncomingRadiance.AddIncomingRadiance(StaticSkyLighting, SampleWeight, TangentPathDirection, WorldPathDirection);
		}

		if (StationarySkyLighting != FLinearColor::Black)
		{
			IncomingRadiance.AddIncomingStationarySkyLight(StationarySkyLighting, SampleWeight, TangentPathDirection, WorldPathDirection);
		}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
		if (bDebugThisTexel
			&& GeneralSettings.ViewSingleBounceNumber == BounceNumber
			&& (!PhotonMappingSettings.bUsePhotonMapping || !PhotonMappingSettings.bVisualizePhotonImportanceSamples))
		{
			FDebugStaticLightingRay DebugRay(PathRay.Start, PathRay.End, RayIntersection.bIntersects, bPositiveSample != 0);
			if (RayIntersection.bIntersects)
			{
				DebugRay.End = RayIntersection.IntersectionVertex.WorldPosition;
			}
			DebugOutput.PathRays.Add(DebugRay);
		}
#endif
	}

	GatherInfo.BackfacingHitsFraction = NumBackfaceHits / (float)UniformHemisphereSamples.Num();
	
	// Calculate the fraction of samples which were occluded
	const float MaterialElementFullyOccludedSamplesFraction = Mapping ? Mapping->Mesh->GetFullyOccludedSamplesFraction(ElementIndex) : 1.0f;
	const float OcclusionFraction = FMath::Min(NumSamplesOccluded / (AmbientOcclusionSettings.FullyOccludedSamplesFraction * MaterialElementFullyOccludedSamplesFraction * UniformHemisphereSamples.Num()), 1.0f);
	// Constant which maintains an integral of .5 for the unclamped exponential function applied to occlusion below
	// An integral of .5 is important because it makes an image with a uniform distribution of occlusion values stay the same brightness with different exponents.
	// As a result, OcclusionExponent just controls contrast and doesn't affect brightness.
	const float NormalizationConstant = .5f * (AmbientOcclusionSettings.OcclusionExponent + 1);
	IncomingRadiance.SetOcclusion(FMath::Clamp(NormalizationConstant * FMath::Pow(OcclusionFraction, AmbientOcclusionSettings.OcclusionExponent), 0.0f, 1.0f)); 

	const FVector BentNormal = CombinedSkyUnoccludedDirection / (MaxUnoccludedLength * UniformHemisphereSamples.Num());
	IncomingRadiance.SetSkyOcclusion(BentNormal);

	return IncomingRadiance;
}

/** Calculates irradiance gradients for a sample position that will be cached. */
void FStaticLightingSystem::CalculateIrradianceGradients(
	int32 BounceNumber,
	const FLightingCacheGatherInfo& GatherInfo,
	FVector4& RotationalGradient,
	FVector4& TranslationalGradient) const
{
	// Calculate rotational and translational gradients as described in the paper "Irradiance Gradients" by Greg Ward and Paul Heckbert
	FVector4 AccumulatedRotationalGradient(0,0,0);
	FVector4 AccumulatedTranslationalGradient(0,0,0);
	if (IrradianceCachingSettings.bUseIrradianceGradients)
	{
		// Extract Theta and Phi steps from the number of hemisphere samples requested
		const float NumThetaStepsFloat = FMath::Sqrt(GetNumUniformHemisphereSamples(BounceNumber) / (float)PI);
		const int32 NumThetaSteps = FMath::TruncToInt(NumThetaStepsFloat);
		// Using PI times more Phi steps as Theta steps
		const int32 NumPhiSteps = FMath::TruncToInt(NumThetaStepsFloat * (float)PI);
		checkSlow(NumThetaSteps > 0 && NumPhiSteps > 0);

		// Calculate the rotational gradient
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			FVector4 InnerSum(0,0,0);
			for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
			{
				const int32 SampleIndex = ThetaIndex * NumPhiSteps + PhiIndex;
				const FLinearColor& IncidentRadiance = GatherInfo.PreviousIncidentRadiances[SampleIndex];
				// These equations need to be re-derived from the paper for a non-uniform PDF
				checkSlow(!ImportanceTracingSettings.bUseCosinePDF);
				const float TangentTerm = -FMath::Tan(ThetaIndex / (float)NumThetaSteps);
				InnerSum += TangentTerm * FVector4(IncidentRadiance);
			}
			const float CurrentPhi = 2.0f * (float)PI * PhiIndex / (float)NumPhiSteps;
			// Vector in the tangent plane perpendicular to the current Phi
			const FVector4 BasePlaneVector = FVector2D((float)HALF_PI, FMath::Fmod(CurrentPhi + (float)HALF_PI, 2.0f * (float)PI)).SphericalToUnitCartesian();
			AccumulatedRotationalGradient += InnerSum * BasePlaneVector;
		}
		// Normalize the sum
		AccumulatedRotationalGradient *= (float)PI / (NumThetaSteps * NumPhiSteps);

		// Calculate the translational gradient
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			FVector4 PolarWallContribution(0,0,0);
			// Starting from 1 since Theta doesn't wrap around (unlike Phi)
			for (int32 ThetaIndex = 1; ThetaIndex < NumThetaSteps; ThetaIndex++)
			{
				const float CurrentTheta = ThetaIndex / (float)NumThetaSteps;
				const float CosCurrentTheta = FMath::Cos(CurrentTheta);
				const int32 SampleIndex = ThetaIndex * NumPhiSteps + PhiIndex;
				const int32 PreviousThetaSampleIndex = (ThetaIndex - 1) * NumPhiSteps + PhiIndex;
				const float& PreviousThetaDistance = GatherInfo.PreviousDistances[PreviousThetaSampleIndex];
				const float& CurrentThetaDistance = GatherInfo.PreviousDistances[SampleIndex];
				const float MinDistance = FMath::Min(PreviousThetaDistance, CurrentThetaDistance);
				checkSlow(MinDistance > 0);
				const FLinearColor IncomingRadianceDifference = GatherInfo.PreviousIncidentRadiances[SampleIndex] - GatherInfo.PreviousIncidentRadiances[PreviousThetaSampleIndex];
				PolarWallContribution += FMath::Sin(CurrentTheta) * CosCurrentTheta * CosCurrentTheta / MinDistance * FVector4(IncomingRadianceDifference);
				checkSlow(!PolarWallContribution.ContainsNaN());
			}

			// Wrap Phi around for the first Phi index
			const int32 PreviousPhiIndex = PhiIndex == 0 ? NumPhiSteps - 1 : PhiIndex - 1;
			FVector4 RadialWallContribution(0,0,0);
			for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
			{
				const float CurrentTheta = FMath::Acos(ThetaIndex / (float)NumThetaSteps);
				const float NextTheta = FMath::Acos((ThetaIndex + 1) / (float)NumThetaSteps);
				const int32 SampleIndex = ThetaIndex * NumPhiSteps + PhiIndex;
				const int32 PreviousPhiSampleIndex = ThetaIndex * NumPhiSteps + PreviousPhiIndex;
				const float& PreviousPhiDistance = GatherInfo.PreviousDistances[PreviousPhiSampleIndex];
				const float& CurrentPhiDistance = GatherInfo.PreviousDistances[SampleIndex];
				const float MinDistance = FMath::Min(PreviousPhiDistance, CurrentPhiDistance);
				checkSlow(MinDistance > 0);
				const FLinearColor IncomingRadianceDifference = GatherInfo.PreviousIncidentRadiances[SampleIndex] - GatherInfo.PreviousIncidentRadiances[PreviousPhiSampleIndex];
				RadialWallContribution += (FMath::Sin(NextTheta) - FMath::Sin(CurrentTheta)) / MinDistance * FVector4(IncomingRadianceDifference);
				checkSlow(!RadialWallContribution.ContainsNaN());
			}

			const float CurrentPhi = 2.0f * (float)PI * PhiIndex / (float)NumPhiSteps;
			// Vector in the tangent plane in the direction of the current Phi
			const FVector4 PhiDirection = SphericalToUnitCartesian(FVector2D((float)HALF_PI, CurrentPhi));
			// Vector in the tangent plane perpendicular to the current Phi
			const FVector4 PerpendicularPhiDirection = FVector2D((float)HALF_PI, FMath::Fmod(CurrentPhi + (float)HALF_PI, 2.0f * (float)PI)).SphericalToUnitCartesian();

			PolarWallContribution = PhiDirection * 2.0f * (float)PI / (float)NumPhiSteps * PolarWallContribution;
			RadialWallContribution = PerpendicularPhiDirection * RadialWallContribution;
			AccumulatedTranslationalGradient += PolarWallContribution + RadialWallContribution;
		}
	}
	RotationalGradient = AccumulatedRotationalGradient;
	TranslationalGradient = AccumulatedTranslationalGradient;
}

/** 
 * Interpolates incoming radiance from the lighting cache if possible,
 * otherwise estimates incoming radiance for this sample point and adds it to the cache. 
 */
FFinalGatherSample FStaticLightingSystem::CachePointIncomingRadiance(
	const FStaticLightingMapping* Mapping,
	const FFullStaticLightingVertex& Vertex,
	int32 ElementIndex,
	int32 VertexIndex,
	float SampleRadius,
	bool bIntersectingSurface,
	FStaticLightingMappingContext& MappingContext,
	FLMRandomStream& RandomStream,
	bool bDebugThisTexel
	) const
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisTexel)
	{
		int32 TempBreak = 0;
	}
#endif

	const int32 BounceNumber = 1;
	FFinalGatherSample IndirectLighting;
	FFinalGatherSample UnusedSecondLighting;
	// Attempt to interpolate incoming radiance from the lighting cache
	if (!IrradianceCachingSettings.bAllowIrradianceCaching || !MappingContext.FirstBounceCache.InterpolateLighting(Vertex, true, bDebugThisTexel, 1.0f , IndirectLighting, UnusedSecondLighting, MappingContext.DebugCacheRecords))
	{
		// If final gathering is disabled, all indirect lighting will be estimated using photon mapping.
		// This is really only useful for debugging since it requires an excessive number of indirect photons to get indirect shadows for the first bounce.
		if (PhotonMappingSettings.bUsePhotonMapping 
			&& GeneralSettings.NumIndirectLightingBounces > 0
			&& !PhotonMappingSettings.bUseFinalGathering)
		{
			// Use irradiance photons for indirect lighting
			if (PhotonMappingSettings.bUseIrradiancePhotons)
			{
				const FIrradiancePhoton* NearestPhoton = NULL;

				if (PhotonMappingSettings.bCacheIrradiancePhotonsOnSurfaces)
				{
					// Trace a ray into the texel to get a good representation of what the final gather will see,
					// Instead of just calculating lightmap UV's from the current texel's position.
					// Speed does not matter here since !bUseFinalGathering is only used for debugging.
					const FLightRay TexelRay(
						Vertex.WorldPosition + Vertex.WorldTangentZ * SampleRadius,
						Vertex.WorldPosition - Vertex.WorldTangentZ * SampleRadius,
						Mapping,
						NULL
						);

					FLightRayIntersection Intersection;
					AggregateMesh.IntersectLightRay(TexelRay, true, false, false, MappingContext.RayCache, Intersection);
					FStaticLightingVertex CurrentVertex = Vertex;
					// Use the intersection's UV's if found, otherwise use the passed in UV's
					if (Intersection.bIntersects && Mapping == Intersection.Mapping)
					{
						CurrentVertex = Intersection.IntersectionVertex;
						VertexIndex = Intersection.VertexIndex;
					}
					// Lookup the irradiance photon that was cached on this surface point
					// Direct lighting is not desired here, as CachePointIncomingRadiance without final gathering is only responsible for gathering indirect lighting
					FLinearColor DirectLighting;
					NearestPhoton = Mapping->GetCachedIrradiancePhoton(VertexIndex, Intersection.IntersectionVertex, *this, bDebugThisTexel && PhotonMappingSettings.bVisualizePhotonGathers && GeneralSettings.ViewSingleBounceNumber != 0, DirectLighting);
				}
				else
				{
					TArray<FIrradiancePhoton*> TempIrradiancePhotons;
					// Search the irradiance photon map for the nearest one
					NearestPhoton = FindNearestIrradiancePhoton(Vertex, MappingContext, TempIrradiancePhotons, false, bDebugThisTexel);
				}
				const FLinearColor& PhotonIrradiance = NearestPhoton ? NearestPhoton->GetIrradiance() : FLinearColor::Black;
				// Convert irradiance (which is incident radiance over all directions for a point) to incident radiance with the approximation 
				// That the irradiance is actually incident radiance along the surface normal.  This will only be correct for simple lightmaps.
				IndirectLighting.AddWeighted(FGatheredLightSample::AmbientLight(PhotonIrradiance), 1.0f);
			}
			else
			{
				// Use the photons deposited on surfaces to estimate indirect lighting
				const bool bDebugFirstBouncePhotonGather = bDebugThisTexel && GeneralSettings.ViewSingleBounceNumber == BounceNumber;
				const FGatheredLightSample FirstBounceLighting = CalculatePhotonIncidentRadiance(FirstBouncePhotonMap, NumPhotonsEmittedFirstBounce, PhotonMappingSettings.IndirectPhotonSearchDistance, Vertex, bDebugFirstBouncePhotonGather);
				if (GeneralSettings.ViewSingleBounceNumber < 0 || GeneralSettings.ViewSingleBounceNumber == BounceNumber)
				{
					IndirectLighting.AddWeighted(FirstBounceLighting, 1.0f);
				}
				
				if (GeneralSettings.NumIndirectLightingBounces > 1)
				{
					const bool bDebugSecondBouncePhotonGather = bDebugThisTexel && GeneralSettings.ViewSingleBounceNumber > BounceNumber;
					const FGatheredLightSample SecondBounceLighting = CalculatePhotonIncidentRadiance(SecondBouncePhotonMap, NumPhotonsEmittedSecondBounce, PhotonMappingSettings.IndirectPhotonSearchDistance, Vertex, bDebugSecondBouncePhotonGather);
					if (GeneralSettings.ViewSingleBounceNumber < 0 || GeneralSettings.ViewSingleBounceNumber > BounceNumber)
					{
						IndirectLighting.AddWeighted(SecondBounceLighting, 1.0f);
					}
				}
			}
		}
		else if (DynamicObjectSettings.bVisualizeVolumeLightInterpolation
			&& GeneralSettings.NumIndirectLightingBounces > 0)
		{
			const FGatheredLightSample VolumeLighting = InterpolatePrecomputedVolumeIncidentRadiance(Vertex, SampleRadius, MappingContext.RayCache, bDebugThisTexel);
			IndirectLighting.AddWeighted(VolumeLighting, 1.0f);
		}
		else
		{
			// Using final gathering with photon mapping, hemisphere gathering without photon mapping, path tracing and/or just calculating ambient occlusion
			TArray<FVector4> ImportancePhotonDirections;
			const int32 UniformHemisphereSamples = GetNumUniformHemisphereSamples(BounceNumber);
			if (GeneralSettings.NumIndirectLightingBounces > 0)
			{
				if (PhotonMappingSettings.bUsePhotonMapping)
				{
					LIGHTINGSTAT(FScopedRDTSCTimer PhotonGatherTimer(MappingContext.Stats.ImportancePhotonGatherTime));
					TArray<FPhoton> FoundPhotons;
					// Gather nearby first bounce photons, which give an estimate of the first bounce incident radiance function,
					// Which we can use to importance sample the real first bounce incident radiance function.
					// See the "Extended Photon Map Implementation" paper.
					FFindNearbyPhotonStats DummyStats;
					FindNearbyPhotonsIterative(
						FirstBouncePhotonMap, 
						Vertex.WorldPosition, 
						Vertex.TriangleNormal, 
						PhotonMappingSettings.NumImportanceSearchPhotons, 
						PhotonMappingSettings.MinImportancePhotonSearchDistance, 
						PhotonMappingSettings.MaxImportancePhotonSearchDistance, 
						bDebugThisTexel, 
						false,
						FoundPhotons,
						DummyStats);

					MappingContext.Stats.TotalFoundImportancePhotons += FoundPhotons.Num();

					ImportancePhotonDirections.Empty(FoundPhotons.Num());
					for (int32 PhotonIndex = 0; PhotonIndex < FoundPhotons.Num(); PhotonIndex++)
					{
						const FPhoton& CurrentPhoton = FoundPhotons[PhotonIndex];
						// Calculate the direction from the current position to the photon's source
						// Using the photon's incident direction unmodified produces artifacts proportional to the distance to that photon
						const FVector4 NewDirection = CurrentPhoton.GetPosition() + CurrentPhoton.GetIncidentDirection() * CurrentPhoton.GetDistance() - Vertex.WorldPosition;
						// Only use the direction if it is in the hemisphere of the normal
						// FindNearbyPhotons only returns photons whose incident directions lie in this hemisphere, but the recalculated direction might not.
						if (Dot3(NewDirection, Vertex.TriangleNormal) > 0.0f)
						{
							ImportancePhotonDirections.Add(NewDirection.GetUnsafeNormal3());
						}
					}
				}
			}
			
			FLightingCacheGatherInfo GatherInfo;
			FFinalGatherSample UniformSampledIncomingRadiance;

			if (ImportanceTracingSettings.bUseAdaptiveSolver)
			{
				UniformSampledIncomingRadiance = IncomingRadianceAdaptive<FFinalGatherSample>(
					Mapping, 
					Vertex, 
					SampleRadius, 
					bIntersectingSurface,
					ElementIndex, 
					BounceNumber, 
					CachedHemisphereSamples,
					CachedHemisphereSampleUniforms,
					CachedSamplesMaxUnoccludedLength,
					ImportancePhotonDirections, 
					MappingContext, 
					RandomStream, 
					GatherInfo, 
					bDebugThisTexel);
			}
			else
			{
				//@todo - remove old solver
				UniformSampledIncomingRadiance = IncomingRadianceUniform<FFinalGatherSample>(
					Mapping, 
					Vertex, 
					SampleRadius, 
					ElementIndex, 
					BounceNumber, 
					CachedHemisphereSamples,
					CachedSamplesMaxUnoccludedLength,
					ImportancePhotonDirections, 
					MappingContext, 
					RandomStream, 
					GatherInfo, 
					bDebugThisTexel);
			}

			IndirectLighting.AddWeighted(UniformSampledIncomingRadiance, 1.0f);

			const bool bInsideGeometry = GatherInfo.BackfacingHitsFraction > .5f && !bIntersectingSurface;

			if (!ImportanceTracingSettings.bUseAdaptiveSolver
				&& GeneralSettings.NumIndirectLightingBounces > 0 
				// Skip importance sampling if we are inside another object
				// This will create incomplete lighting for texels skipped, but it's assumed they are not visible anyway if they can see so many backfaces
				&& !bInsideGeometry)
			{
				// Use importance sampled final gathering to calculate incident radiance
				const FGatheredLightSample ImportanceSampledIncomingRadiance = IncomingRadianceImportancePhotons(Mapping, Vertex, SampleRadius, ElementIndex, BounceNumber, ImportancePhotonDirections, MappingContext, RandomStream, bDebugThisTexel);
				IndirectLighting.AddWeighted(ImportanceSampledIncomingRadiance, 1.0f);
			}

			if (IrradianceCachingSettings.bAllowIrradianceCaching)
			{
				FVector4 RotationalGradient;
				FVector4 TranslationalGradient;
				CalculateIrradianceGradients(BounceNumber, GatherInfo, RotationalGradient, TranslationalGradient);

				float OverrideRadius = 0;

				if (GeneralSettings.bAccountForTexelSize)
				{
					// Make the irradiance cache sample radius very small for texels whose radius is close to the minimum, 
					// Since those texels are usually in corners and not representative of their neighbors.
					if (SampleRadius < SceneConstants.SmallestTexelRadius * 2.0f)
					{
						OverrideRadius = SceneConstants.SmallestTexelRadius;
					}
					else if (GatherInfo.MinDistance > SampleRadius)
					{
						// When uniform final gather rays are offset from the center of the texel, 
						// It's possible for a perpendicular surface to intersect the center of the texel and none of the final gather rays detect it.
						// The lighting cache sample will be assigned a large radius and the artifact will be interpolated a large distance.
						// Trace a ray from one corner of the texel to the other to detect this edge case, 
						// And set the record radius to the minimum to contain the error.
						// Center of the texel offset along the normal
						const FVector4 TexelCenterOffset = Vertex.WorldPosition + Vertex.TriangleNormal * SampleRadius * SceneConstants.VisibilityNormalOffsetSampleRadiusScale;
						// Vector from the center to one of the corners of the texel
						// The FMath::Sqrt(.5f) is to normalize (Vertex.TriangleTangentX + Vertex.TriangleTangentY), which are orthogonal unit vectors.
						const FVector4 CornerOffset = FMath::Sqrt(.5f) * (Vertex.TriangleTangentX + Vertex.TriangleTangentY) * SampleRadius * SceneConstants.VisibilityTangentOffsetSampleRadiusScale;
						const FLightRay TexelRay(
							TexelCenterOffset + CornerOffset,
							TexelCenterOffset - CornerOffset,
							NULL,
							NULL
							);

						FLightRayIntersection Intersection;
						AggregateMesh.IntersectLightRay(TexelRay, false, false, false, MappingContext.RayCache, Intersection);
						if (Intersection.bIntersects)
						{
							OverrideRadius = SampleRadius;
						}
	#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
						if (bDebugThisTexel 
							&& GeneralSettings.ViewSingleBounceNumber == BounceNumber
							&& (!PhotonMappingSettings.bUsePhotonMapping || !PhotonMappingSettings.bVisualizePhotonImportanceSamples))
						{
							FDebugStaticLightingRay DebugRay(TexelRay.Start, TexelRay.End, Intersection.bIntersects, false);
							if (Intersection.bIntersects)
							{
								DebugRay.End = Intersection.IntersectionVertex.WorldPosition;
							}

							// CachePointIncomingRadiance can be called from multiple threads
							FScopeLock DebugOutputLock(&DebugOutputSync);
							DebugOutput.PathRays.Add(DebugRay);
						}
	#endif
					}
				}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
				if (bDebugThisTexel)
				{
					int32 TempBreak = 0;
				}
#endif

				TLightingCache<FFinalGatherSample>::FRecord<FFinalGatherSample> NewRecord(
					Vertex,
					GatherInfo,
					SampleRadius,
					OverrideRadius,
					IrradianceCachingSettings,
					GeneralSettings,
					IndirectLighting,
					RotationalGradient,
					TranslationalGradient
					);

				// Add the incident radiance sample to the first bounce lighting cache.
				MappingContext.FirstBounceCache.AddRecord(NewRecord, bInsideGeometry, true);

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
				if (IrradianceCachingSettings.bVisualizeIrradianceSamples && Mapping == Scene.DebugMapping && GeneralSettings.ViewSingleBounceNumber == BounceNumber)
				{
					const float DistanceToDebugTexelSq = FVector(Scene.DebugInput.Position - Vertex.WorldPosition).SizeSquared();
					FDebugLightingCacheRecord TempRecord;
					TempRecord.bNearSelectedTexel = DistanceToDebugTexelSq < NewRecord.BoundingRadius * NewRecord.BoundingRadius;
					TempRecord.Radius = GatherInfo.MinDistance;
					TempRecord.Vertex.VertexPosition = Vertex.WorldPosition;
					TempRecord.Vertex.VertexNormal = Vertex.WorldTangentZ;
					TempRecord.RecordId = NewRecord.Id;

					MappingContext.DebugCacheRecords.Add(TempRecord);
				}
#endif
			}
		}
	}

	return IndirectLighting;
}

} //namespace Lightmass

