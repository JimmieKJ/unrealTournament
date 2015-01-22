// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Lightmass
{

class FIrradianceCacheStats
{
public:

	uint64 NumCacheLookups;
	uint64 NumRecords;
	uint64 NumInsideGeometry;

	FIrradianceCacheStats() :
		NumCacheLookups(0),
		NumRecords(0),
		NumInsideGeometry(0)
	{}

	FIrradianceCacheStats& operator+=(const FIrradianceCacheStats& B)
	{
		NumCacheLookups += B.NumCacheLookups;
		NumRecords += B.NumRecords;
		NumInsideGeometry += B.NumInsideGeometry;
		return *this;
	}
};

/** The information needed by the lighting cache from a uniform sampled integration of the hemisphere in order to create a lighting record at that point. */
class FLightingCacheGatherInfo
{
public:
	float MinDistance;
	float BackfacingHitsFraction;
	/** Incident radiance and distance from each hemisphere sample. */
	TArray<FLinearColor> PreviousIncidentRadiances;
	TArray<float> PreviousDistances;

	FLightingCacheGatherInfo() :
		MinDistance(FLT_MAX),
		BackfacingHitsFraction(0)
	{}

	void UpdateOnHit(float IntersectionDistance)
	{
		MinDistance = FMath::Min(MinDistance, IntersectionDistance);
	}
};

class FLightingCacheBase
{
public:
	/** See FIrradianceCachingSettings for descriptions of these or the variables they are based on. */
	float InterpolationAngleNormalization;
	float InterpolationAngleNormalizationSmooth;
	const float MinCosPointBehindPlane;
	const float DistanceSmoothFactor;
	const bool bUseIrradianceGradients;
	const bool bShowGradientsOnly;
	const bool bVisualizeIrradianceSamples;
	const int32 BounceNumber;
	int32 NextRecordId;
	mutable FIrradianceCacheStats Stats;
	const class FStaticLightingSystem& System;

	/** Initialization constructor. */
	FLightingCacheBase(const FStaticLightingSystem& InSystem, int32 InBounceNumber);
};


/** A lighting cache. */
template<class SampleType>
class TLightingCache : public FLightingCacheBase
{
public:

	/** The irradiance for a single static lighting vertex. */
	template<class RecordSampleType>
	class FRecord
	{
	public:

		/** The static lighting vertex the irradiance record was computed for. */
		FFullStaticLightingVertex Vertex;

		/** Largest radius that the sample will ever have, used for insertion into spatial data structures. */
		float BoundingRadius;

		/** Radius of this irradiance cache record in the cache pass. */
		float Radius;

		/** Radius of this irradiance cache record in the interpolation pass. */
		float InterpolationRadius;

		/** The lighting incident on an infinitely small surface at WorldPosition facing along WorldNormal. */
		RecordSampleType Lighting;

		/** The rotational gradient along the vector perpendicular to both the record normal and the normal of the vertex being interpolated to, used for higher order interpolation. */
		FVector4 RotationalGradient;

		/** The translational gradient from the record to the point being interpolated to, used for higher order interpolation. */
		FVector4 TranslationalGradient;

		/** For debugging */
		int32 Id;

		/** Initialization constructor. */
		FRecord(const FFullStaticLightingVertex& InVertex,const FLightingCacheGatherInfo& GatherInfo,float SampleRadius,float InOverrideRadius,const FIrradianceCachingSettings& IrradianceCachingSettings,const FStaticLightingSettings& GeneralSettings,const RecordSampleType& InLighting, const FVector4& InRotGradient, const FVector4& InTransGradient):
			Vertex(InVertex),
			Lighting(InLighting),
			RotationalGradient(InRotGradient),
			TranslationalGradient(InTransGradient),
			Id(-1)
		{
			// Clamp to be larger than the texel
			Radius = FMath::Clamp(GatherInfo.MinDistance, SampleRadius, IrradianceCachingSettings.MaxRecordRadius) * IrradianceCachingSettings.RecordRadiusScale; 
			// Use a larger radius to interpolate, which smooths the error
			InterpolationRadius = Radius * FMath::Max(IrradianceCachingSettings.DistanceSmoothFactor * GeneralSettings.IndirectLightingSmoothness, 1.0f);

			BoundingRadius = FMath::Max(Radius, InterpolationRadius);
		}
	};

	TLightingCache(const FBox& InBoundingBox, const FStaticLightingSystem& System, int32 InBounceNumber) :
		FLightingCacheBase(System, InBounceNumber),
		Octree(InBoundingBox.GetCenter(),InBoundingBox.GetExtent().GetMax())
	{}

	/** Adds a lighting record to the cache. */
	void AddRecord(FRecord<SampleType>& Record, bool bInsideGeometry, bool bAddToStats)
	{
		Record.Id = NextRecordId;
		NextRecordId++;
		Octree.AddElement(Record);

		if (bAddToStats)
		{
			Stats.NumRecords++;

			if (bInsideGeometry)
			{
				Stats.NumInsideGeometry++;
			}
		}
	}

	void GetAllRecords(TArray<FRecord<SampleType> >& Records) const
	{
		// Gather an array of samples from the octree
		for(typename LightingOctreeType::template TConstIterator<> NodeIt(Octree); NodeIt.HasPendingNodes(); NodeIt.Advance())
		{
			const typename LightingOctreeType::FNode& CurrentNode = NodeIt.GetCurrentNode();

			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if(CurrentNode.HasChild(ChildRef))
				{
					NodeIt.PushChild(ChildRef);
				}
			}

			for (typename LightingOctreeType::ElementConstIt ElementIt(CurrentNode.GetConstElementIt()); ElementIt; ++ElementIt)
			{
				const FRecord<SampleType>& Sample = *ElementIt;
				Records.Add(Sample);
			}
		}
	}

	/**
	 * Interpolates nearby lighting records for a vertex.
	 * @param Vertex - The vertex to interpolate the lighting for.
	 * @param OutLighting - If true is returned, contains the blended lighting records that were found near the point.
	 * @return true if nearby records were found with enough relevance to interpolate this point's lighting.
	 */
	bool InterpolateLighting(
		const FFullStaticLightingVertex& Vertex, 
		bool bFirstPass, 
		bool bDebugThisSample, 
		float SecondInterpolationSmoothnessReduction,
		SampleType& OutLighting,
		SampleType& OutSecondLighting,
		TArray<FDebugLightingCacheRecord>& DebugCacheRecords) const;

private:

	struct FRecordOctreeSemantics;

	/** The type of lighting cache octree nodes. */
	typedef TOctree<FRecord<SampleType>,FRecordOctreeSemantics> LightingOctreeType;

	/** The octree semantics for irradiance records. */
	struct FRecordOctreeSemantics
	{
		enum { MaxElementsPerLeaf = 4 };
		enum { MaxNodeDepth = 12 };
		enum { LoosenessDenominator = 16 };

		typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

		static FBoxCenterAndExtent GetBoundingBox(const FRecord<SampleType>& LightingRecord)
		{
			return FBoxCenterAndExtent(
				LightingRecord.Vertex.WorldPosition,
				FVector4(LightingRecord.BoundingRadius, LightingRecord.BoundingRadius, LightingRecord.BoundingRadius)
				);
		}
	};

	/** The lighting cache octree. */
	LightingOctreeType Octree;
};

/**
 * Interpolates nearby lighting records for a vertex.
 * @param Vertex - The vertex to interpolate the lighting for.
 * @param OutLighting - If true is returned, contains the blended lighting records that were found near the point.
 * @return true if nearby records were found with enough relevance to interpolate this point's lighting.
 */
template<class SampleType>
bool TLightingCache<SampleType>::InterpolateLighting(
	const FFullStaticLightingVertex& Vertex, 
	bool bFirstPass, 
	bool bDebugThisSample, 
	float SecondInterpolationSmoothnessReduction,
	SampleType& OutLighting,
	SampleType& OutSecondLighting,
	TArray<FDebugLightingCacheRecord>& DebugCacheRecords) const
{
	if (bFirstPass)
	{
		Stats.NumCacheLookups++;
	}
	const float AngleNormalization = bFirstPass ? InterpolationAngleNormalization : InterpolationAngleNormalizationSmooth;
	// Initialize the sample to zero
	SampleType AccumulatedLighting(ForceInit);
	float TotalWeight = 0.0f;
	SampleType SecondAccumulatedLighting(ForceInit);
	float SecondTotalWeight = 0.0f;

	// Iterate over the octree nodes containing the query point.
	for( typename LightingOctreeType::template TConstElementBoxIterator<> OctreeIt(
		Octree,
		FBoxCenterAndExtent(Vertex.WorldPosition, FVector4(0,0,0))
		);
		OctreeIt.HasPendingElements();
		OctreeIt.Advance())
	{
		const FRecord<SampleType>& LightingRecord = OctreeIt.GetCurrentElement();

		if (bDebugThisSample)
		{
			int asdf = 0;
		}

		// Check whether the query point is farther than the record's intersection distance for the direction to the query point.
		const float DistanceSquared = (LightingRecord.Vertex.WorldPosition - Vertex.WorldPosition).SizeSquared3();
		if (DistanceSquared > FMath::Square(LightingRecord.BoundingRadius))
		{
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSquared);

		// Don't use a lighting record if it's in front of the query point.
		// Query points behind the lighting record may have nearby occluders that the lighting record does not see.
		const FVector4 RecordToVertexVector = Vertex.WorldPosition - LightingRecord.Vertex.WorldPosition;
		// Use the average normal to handle surfaces with constant concavity
		const FVector4 AverageNormal = (LightingRecord.Vertex.TriangleNormal + Vertex.TriangleNormal).GetSafeNormal();
		const float PlaneDistance = Dot3(AverageNormal, RecordToVertexVector.GetSafeNormal());
		// Setup an error metric that goes from 0 if the points are coplanar, to 1 if the point being shaded is at the angle corresponding to MinCosPointBehindPlane behind the plane
		const float PointBehindPlaneError = FMath::Max(PlaneDistance / MinCosPointBehindPlane, 0.0f);

		const float NormalDot = Dot3(LightingRecord.Vertex.WorldTangentZ, Vertex.WorldTangentZ);

		const float NonGradientLighting = bShowGradientsOnly ? 0.0f : 1.0f;
		float RotationalGradientContribution = 0.0f;
		float TranslationalGradientContribution = 0.0f;

		if (bUseIrradianceGradients)
		{
			// Calculate the gradient's contribution
			RotationalGradientContribution = Dot3((LightingRecord.Vertex.WorldTangentZ ^ Vertex.WorldTangentZ), LightingRecord.RotationalGradient);
			TranslationalGradientContribution = Dot3((Vertex.WorldPosition - LightingRecord.Vertex.WorldPosition), LightingRecord.TranslationalGradient);
		}

		// Error metric from "An Approximate Global Illumination System for Computer Generated Films",
		// This error metric has the advantages (over Ward's original metric from "A Ray Tracing Solution to Diffuse Interreflection")
		// That it goes to 0 at the record's radius, which avoids discontinuities,
		// And it is finite at the record's center, which allows filtering the records to be more effective.
		const float EffectiveRadius = bFirstPass ? LightingRecord.Radius : LightingRecord.InterpolationRadius;

		{
			const float DistanceRatio = Distance / EffectiveRadius;
			const float NormalRatio = AngleNormalization * FMath::Sqrt(FMath::Max(1.0f - NormalDot, 0.0f));
			// The total error is the max of the distance, normal and plane errors
			float RecordError = FMath::Max(DistanceRatio, NormalRatio);
			RecordError = FMath::Max(RecordError, PointBehindPlaneError);

			if (RecordError < 1)
			{
				const float RecordWeight = 1.0f - RecordError;

				//@todo - Rotate the record's lighting into this vertex's tangent basis.  We are linearly combining incident lighting in different coordinate spaces.
				AccumulatedLighting = AccumulatedLighting + LightingRecord.Lighting * RecordWeight * (NonGradientLighting + RotationalGradientContribution + TranslationalGradientContribution);
				// Accumulate the weight of all records
				TotalWeight += RecordWeight;
			}
		}

		// Accumulate a second interpolation with reduced smoothness
		// This is useful for lighting components like AO and sky shadowing where less smoothing is needed to hide noise
		// This interpolation is done in the same pass to prevent another traversal of the octree
		{
			const float DistanceRatio = Distance / FMath::Lerp(LightingRecord.Radius, EffectiveRadius, SecondInterpolationSmoothnessReduction);
			const float SecondAngleNormalization = FMath::Lerp(InterpolationAngleNormalization, AngleNormalization, SecondInterpolationSmoothnessReduction);
			const float NormalRatio = SecondAngleNormalization * FMath::Sqrt(FMath::Max(1.0f - NormalDot, 0.0f));
			// The total error is the max of the distance, normal and plane errors
			float RecordError = FMath::Max(DistanceRatio, NormalRatio);
			RecordError = FMath::Max(RecordError, PointBehindPlaneError);

			if (RecordError < 1)
			{
				const float RecordWeight = 1.0f - RecordError;

				//@todo - Rotate the record's lighting into this vertex's tangent basis.  We are linearly combining incident lighting in different coordinate spaces.
				SecondAccumulatedLighting = SecondAccumulatedLighting + LightingRecord.Lighting * RecordWeight * (NonGradientLighting + RotationalGradientContribution + TranslationalGradientContribution);
				// Accumulate the weight of all records
				SecondTotalWeight += RecordWeight;
			}
		}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
		if (bVisualizeIrradianceSamples && bDebugThisSample && BounceNumber == 1)
		{
			for (int32 i = 0; i < DebugCacheRecords.Num(); i++)
			{
				FDebugLightingCacheRecord& CurrentRecord =  DebugCacheRecords[i];

				if (CurrentRecord.RecordId == LightingRecord.Id)
				{
					CurrentRecord.bAffectsSelectedTexel = true;
				}
			}
		}
#endif
	}

	if (TotalWeight > DELTA)
	{
		// Normalize the accumulated lighting and return success.
		const float InvTotalWeight = 1.0f / TotalWeight;
		OutLighting = OutLighting + AccumulatedLighting * InvTotalWeight;
		OutSecondLighting = OutSecondLighting + SecondAccumulatedLighting * (1.0f / SecondTotalWeight);
		return true;
	}
	else
	{
		// Irradiance for the query vertex couldn't be interpolated from the cache
		return false;
	}
}

} //namespace Lightmass
