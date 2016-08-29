// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrecomputedLightVolume.cpp: Implementation of a precomputed light volume.
=============================================================================*/

#include "EnginePrivate.h"
#include "PrecomputedLightVolume.h"
#include "TargetPlatform.h"
#include "RenderingObjectVersion.h"

template<> TVolumeLightingSample<2>::TVolumeLightingSample(const TVolumeLightingSample<2>& Other)
{
	Position = Other.Position;
	Radius = Other.Radius;
	Lighting = Other.Lighting;
	PackedSkyBentNormal = Other.PackedSkyBentNormal;
	DirectionalLightShadowing = Other.DirectionalLightShadowing;	
}
template<> TVolumeLightingSample<3>::TVolumeLightingSample(const TVolumeLightingSample<2>& Other)
{
	Position = Other.Position;
	Radius = Other.Radius;	
	PackedSkyBentNormal = Other.PackedSkyBentNormal;
	DirectionalLightShadowing = Other.DirectionalLightShadowing;

	for (int i = 0; i < 4; ++i)
	{
		Lighting.R.V[i] = Other.Lighting.R.V[i];
		Lighting.G.V[i] = Other.Lighting.G.V[i];
		Lighting.B.V[i] = Other.Lighting.B.V[i];
	}
	for (int i = 4; i < 9; ++i)
	{
		Lighting.R.V[i] = 0.0f;
		Lighting.G.V[i] = 0.0f;
		Lighting.B.V[i] = 0.0f;
	}
}

template<> TVolumeLightingSample<2>::TVolumeLightingSample(const TVolumeLightingSample<3>& Other)
{
	Position = Other.Position;
	Radius = Other.Radius;
	PackedSkyBentNormal = Other.PackedSkyBentNormal;
	DirectionalLightShadowing = Other.DirectionalLightShadowing;

	for (int i = 0; i < 4; ++i)
	{
		Lighting.R.V[i] = Other.Lighting.R.V[i];
		Lighting.G.V[i] = Other.Lighting.G.V[i];
		Lighting.B.V[i] = Other.Lighting.B.V[i];
	}
}

template<> TVolumeLightingSample<3>::TVolumeLightingSample(const TVolumeLightingSample<3>& Other)
{
	Position = Other.Position;
	Radius = Other.Radius;
	Lighting = Other.Lighting;
	PackedSkyBentNormal = Other.PackedSkyBentNormal;
	DirectionalLightShadowing = Other.DirectionalLightShadowing;
}


FArchive& operator<<(FArchive& Ar, TVolumeLightingSample<2>& Sample)
{
	Ar << Sample.Position;
	Ar << Sample.Radius;
	Ar << Sample.Lighting;

	if (Ar.UE4Ver() >= VER_UE4_SKY_BENT_NORMAL)
	{
		Ar << Sample.PackedSkyBentNormal;
	}

	if (Ar.UE4Ver() >= VER_UE4_VOLUME_SAMPLE_LOW_QUALITY_SUPPORT)
	{
		Ar << Sample.DirectionalLightShadowing;
	}
	
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TVolumeLightingSample<3>& Sample)
{
	// Less version check since TVolumeLightingSample<3> require a more recent version. 
	Ar << Sample.Position;
	Ar << Sample.Radius;
	Ar << Sample.Lighting;
	Ar << Sample.PackedSkyBentNormal;
	Ar << Sample.DirectionalLightShadowing;
	return Ar;
}

FPrecomputedLightVolume::FPrecomputedLightVolume() :
	bInitialized(false),
	bAddedToScene(false),
	HighQualityLightmapOctree(FVector::ZeroVector, HALF_WORLD_MAX),
	LowQualityLightmapOctree(FVector::ZeroVector, HALF_WORLD_MAX),
	OctreeForRendering(NULL)
{}

FPrecomputedLightVolume::~FPrecomputedLightVolume()
{
	if (bInitialized)
	{
		const SIZE_T VolumeBytes = GetAllocatedBytes();
		DEC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
	}
}

static void LoadVolumeLightSamples(FArchive& Ar, int32 ArchiveNumSHSamples, TArray<FVolumeLightingSample>& Samples)
{
	// If it's the same number as what is currently compiled
	if (ArchiveNumSHSamples == NUM_INDIRECT_LIGHTING_SH_COEFFICIENTS)
	{
		Ar << Samples;		
	}
	else if (ArchiveNumSHSamples == 9)
	{
		TArray< TVolumeLightingSample<3> > DummySamples;
		Ar << DummySamples;
		for (TVolumeLightingSample<3>& LoadedSample : DummySamples)
		{
			Samples.Add(FVolumeLightingSample(LoadedSample));
		}
	}
	else
	{
		checkf(ArchiveNumSHSamples == 4, TEXT("NumSHSamples: %i"), ArchiveNumSHSamples);

		TArray< TVolumeLightingSample<2> > DummySamples;
		Ar << DummySamples;

		for (TVolumeLightingSample<2>& LoadedSample : DummySamples)
		{
			Samples.Add(FVolumeLightingSample(LoadedSample));
		}
	}	
}

FArchive& operator<<(FArchive& Ar,FPrecomputedLightVolume& Volume)
{
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.IsCountingMemory())
	{
		const int32 AllocatedBytes = Volume.GetAllocatedBytes();
		Ar.CountBytes(AllocatedBytes, AllocatedBytes);
	}
	else if (Ar.IsLoading())
	{
		Ar << Volume.bInitialized;
		if (Volume.bInitialized)
		{
			FBox Bounds;
			Ar << Bounds;
			float SampleSpacing = 0.0f;
			Ar << SampleSpacing;
			Volume.Initialize(Bounds);

			// Before adding support for SH3, it was always using SH2
			int32 NumSHSamples = 4; 			
			if (Ar.CustomVer(FRenderingObjectVersion::GUID) >= FRenderingObjectVersion::IndirectLightingCache3BandSupport)
			{
				Ar << NumSHSamples;
			}

			TArray<FVolumeLightingSample> HighQualitySamples;
			LoadVolumeLightSamples(Ar, NumSHSamples, HighQualitySamples);

			// Deserialize samples as an array, and add them to the octree
			if (FPlatformProperties::SupportsHighQualityLightmaps() 
				&& (GIsEditor || AllowHighQualityLightmaps(GMaxRHIFeatureLevel)))
			{
				for(int32 SampleIndex = 0; SampleIndex < HighQualitySamples.Num(); SampleIndex++)
				{
					Volume.AddHighQualityLightingSample(HighQualitySamples[SampleIndex]);
				}
			}

			TArray<FVolumeLightingSample> LowQualitySamples;

			if (Ar.UE4Ver() >= VER_UE4_VOLUME_SAMPLE_LOW_QUALITY_SUPPORT)
			{
				LoadVolumeLightSamples(Ar, NumSHSamples, LowQualitySamples);
			}

			if (FPlatformProperties::SupportsLowQualityLightmaps() 
				&& (GIsEditor || !AllowHighQualityLightmaps(GMaxRHIFeatureLevel)))
			{
				for(int32 SampleIndex = 0; SampleIndex < LowQualitySamples.Num(); SampleIndex++)
				{
					Volume.AddLowQualityLightingSample(LowQualitySamples[SampleIndex]);
				}
			}

			Volume.FinalizeSamples();
		}
	}
	else if (Ar.IsSaving())
	{
		Ar << Volume.bInitialized;
		if (Volume.bInitialized)
		{
			Ar << Volume.Bounds;
			float SampleSpacing = 0.0f;
			Ar << SampleSpacing;

			int32 NumSHSamples = NUM_INDIRECT_LIGHTING_SH_COEFFICIENTS; 
			Ar << NumSHSamples;

			TArray<FVolumeLightingSample> HighQualitySamples;

			if (!Ar.IsCooking() || Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps))
			{
				// Gather an array of samples from the octree
				for(FLightVolumeOctree::TConstIterator<> NodeIt(Volume.HighQualityLightmapOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
				{
					const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();

					FOREACH_OCTREE_CHILD_NODE(ChildRef)
					{
						if(CurrentNode.HasChild(ChildRef))
						{
							NodeIt.PushChild(ChildRef);
						}
					}

					for (FLightVolumeOctree::ElementConstIt ElementIt(CurrentNode.GetElementIt()); ElementIt; ++ElementIt)
					{
						const FVolumeLightingSample& Sample = *ElementIt;
						HighQualitySamples.Add(Sample);
					}
				}
			}
			
			Ar << HighQualitySamples;

			TArray<FVolumeLightingSample> LowQualitySamples;

			if (!Ar.IsCooking() || Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::LowQualityLightmaps))
			{
				// Gather an array of samples from the octree
				for(FLightVolumeOctree::TConstIterator<> NodeIt(Volume.LowQualityLightmapOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
				{
					const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();

					FOREACH_OCTREE_CHILD_NODE(ChildRef)
					{
						if(CurrentNode.HasChild(ChildRef))
						{
							NodeIt.PushChild(ChildRef);
						}
					}

					for (FLightVolumeOctree::ElementConstIt ElementIt(CurrentNode.GetElementIt()); ElementIt; ++ElementIt)
					{
						const FVolumeLightingSample& Sample = *ElementIt;
						LowQualitySamples.Add(Sample);
					}
				}
			}

			Ar << LowQualitySamples;
		}
	}
	return Ar;
}

/** Frees any previous samples, prepares the volume to have new samples added. */
void FPrecomputedLightVolume::Initialize(const FBox& NewBounds)
{
	InvalidateLightingCache();
	bInitialized = true;
	Bounds = NewBounds;
	// Initialize the octree based on the passed in bounds
	HighQualityLightmapOctree = FLightVolumeOctree(NewBounds.GetCenter(), NewBounds.GetExtent().GetMax());
	LowQualityLightmapOctree = FLightVolumeOctree(NewBounds.GetCenter(), NewBounds.GetExtent().GetMax());

	OctreeForRendering = AllowHighQualityLightmaps(GMaxRHIFeatureLevel) ? &HighQualityLightmapOctree : &LowQualityLightmapOctree;
}

/** Adds a lighting sample. */
void FPrecomputedLightVolume::AddHighQualityLightingSample(const FVolumeLightingSample& NewHighQualitySample)
{
	check(bInitialized);
	HighQualityLightmapOctree.AddElement(NewHighQualitySample);
}

void FPrecomputedLightVolume::AddLowQualityLightingSample(const FVolumeLightingSample& NewLowQualitySample)
{
	check(bInitialized);
	LowQualityLightmapOctree.AddElement(NewLowQualitySample);
}

/** Shrinks the octree and updates memory stats. */
void FPrecomputedLightVolume::FinalizeSamples()
{
	check(bInitialized);
	// No more samples will be added, shrink octree node element arrays
	HighQualityLightmapOctree.ShrinkElements();
	LowQualityLightmapOctree.ShrinkElements();
	const SIZE_T VolumeBytes = GetAllocatedBytes();
	INC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
}

/** Invalidates anything produced by the last lighting build. */
void FPrecomputedLightVolume::InvalidateLightingCache()
{
	if (bInitialized)
	{
		check(!bAddedToScene);

		// Release existing samples
		const SIZE_T VolumeBytes = GetAllocatedBytes();
		DEC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
		HighQualityLightmapOctree.Destroy();
		LowQualityLightmapOctree.Destroy();
		OctreeForRendering = NULL;
		bInitialized = false;
	}
}

void FPrecomputedLightVolume::AddToScene(FSceneInterface* Scene)
{
	check(!bAddedToScene);
	bAddedToScene = true;

	if (bInitialized && Scene)
	{
		Scene->AddPrecomputedLightVolume(this);
		OctreeForRendering = AllowHighQualityLightmaps(Scene->GetFeatureLevel()) ? &HighQualityLightmapOctree : &LowQualityLightmapOctree;
	}
}

void FPrecomputedLightVolume::RemoveFromScene(FSceneInterface* Scene)
{
	if (bAddedToScene)
	{
		bAddedToScene = false;

		if (bInitialized && Scene)
		{
			Scene->RemovePrecomputedLightVolume(this);
		}
	}
}

void FPrecomputedLightVolume::InterpolateIncidentRadiancePoint(
		const FVector& WorldPosition, 
		float& AccumulatedWeight,
		float& AccumulatedDirectionalLightShadowing,
		FSHVectorRGB3& AccumulatedIncidentRadiance,
		FVector& SkyBentNormal) const
{
	// This could be called on a NULL volume for a newly created level. This is now checked at the call site, but this check provides extra safety
	checkf(this, TEXT("FPrecomputedLightVolume::InterpolateIncidentRadiancePoint() is called on a null volume. Fix the call site."));

	// Handle being called on a volume that hasn't been initialized yet, which can happen if lighting hasn't been built yet.
	if (bInitialized)
	{
		FBoxCenterAndExtent BoundingBox( WorldPosition, FVector::ZeroVector );
		// Iterate over the octree nodes containing the query point.
		for (FLightVolumeOctree::TConstElementBoxIterator<> OctreeIt(*OctreeForRendering, BoundingBox);
			OctreeIt.HasPendingElements();
			OctreeIt.Advance())
		{
			const FVolumeLightingSample& VolumeSample = OctreeIt.GetCurrentElement();
			const float DistanceSquared = (VolumeSample.Position - WorldPosition).SizeSquared();
			const float RadiusSquared = FMath::Square(VolumeSample.Radius);

			if (DistanceSquared < RadiusSquared)
			{
				const float InvRadiusSquared = 1.0f / RadiusSquared;
				// Weight each sample with the fraction that Position is to the center of the sample, and inversely to the sample radius.
				// The weight goes to 0 when Position is on the bounding radius of the sample, so the interpolated result is continuous.
				// The sample's radius size is a factor so that smaller samples contribute more than larger, low detail ones.
				const float SampleWeight = (1.0f - DistanceSquared * InvRadiusSquared) * InvRadiusSquared;
				// Accumulate weighted results and the total weight for normalization later
				AccumulatedIncidentRadiance += VolumeSample.Lighting * SampleWeight;
				SkyBentNormal += VolumeSample.GetSkyBentNormalUnpacked() * SampleWeight;
				AccumulatedDirectionalLightShadowing += VolumeSample.DirectionalLightShadowing * SampleWeight;
				AccumulatedWeight += SampleWeight;
			}
		}
	}
}

/** Interpolates incident radiance to Position. */
void FPrecomputedLightVolume::InterpolateIncidentRadianceBlock(
	const FBoxCenterAndExtent& BoundingBox, 
	const FIntVector& QueryCellDimensions,
	const FIntVector& DestCellDimensions,
	const FIntVector& DestCellPosition,
	TArray<float>& AccumulatedWeights,
	TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance) const
{	
	// This could be called on a NULL volume for a newly created level. This is now checked at the callsite, but this check provides extra safety
	checkf(this, TEXT("FPrecomputedLightVolume::InterpolateIncidentRadianceBlock() is called on a null volume. Fix the call site."));

	// Handle being called on a volume that hasn't been initialized yet, which can happen if lighting hasn't been built yet.
	if (bInitialized)
	{
		static TArray<const FVolumeLightingSample*> PotentiallyIntersectingSamples;
		PotentiallyIntersectingSamples.Reset(100);

		// Iterate over the octree nodes containing the query point.
		for (FLightVolumeOctree::TConstElementBoxIterator<> OctreeIt(*OctreeForRendering, BoundingBox);
			OctreeIt.HasPendingElements();
			OctreeIt.Advance())
		{
			PotentiallyIntersectingSamples.Add(&OctreeIt.GetCurrentElement());
		}
		
		const int32 LinearIndexBase = DestCellPosition.Z * DestCellDimensions.Y * DestCellDimensions.X
			+ DestCellPosition.Y * DestCellDimensions.X
			+ DestCellPosition.X;

		const int32 CenterIndex = LinearIndexBase + (QueryCellDimensions.Z / 2 * DestCellDimensions.Y + QueryCellDimensions.Y / 2) * DestCellDimensions.X + QueryCellDimensions.X / 2;

		for (int32 SampleIndex = 0; SampleIndex < PotentiallyIntersectingSamples.Num(); SampleIndex++)
		{
			const FVolumeLightingSample& VolumeSample = *PotentiallyIntersectingSamples[SampleIndex];
				
			const float RadiusSquared = FMath::Square(VolumeSample.Radius);
			const float WeightBase  = 1.0f / RadiusSquared;
			const float WeightMultiplier = -1.0f / (RadiusSquared * RadiusSquared);
				
			const FVector BaseTranslationFromSample = BoundingBox.Center - BoundingBox.Extent - VolumeSample.Position;
			const FVector QuerySteps = BoundingBox.Extent / FVector(QueryCellDimensions) * 2;
			FVector TranslationFromSample = BaseTranslationFromSample;

			for (int32 Z = 0; Z < QueryCellDimensions.Z; Z++)
			{
				for (int32 Y = 0; Y < QueryCellDimensions.Y; Y++)
				{
					for (int32 X = 0; X < QueryCellDimensions.X; X++)
					{
						const float DistanceSquared = TranslationFromSample.SizeSquared();

						if (DistanceSquared < RadiusSquared)
						{
							const int32 LinearIndex = LinearIndexBase + (Z * DestCellDimensions.Y + Y) * DestCellDimensions.X + X;
							// Weight each sample with the fraction that Position is to the center of the sample, and inversely to the sample radius.
							// The weight goes to 0 when Position is on the bounding radius of the sample, so the interpolated result is continuous.
							// The sample's radius size is a factor so that smaller samples contribute more than larger, low detail ones.
							const float SampleWeight = DistanceSquared * WeightMultiplier + WeightBase;
							// Accumulate weighted results and the total weight for normalization later													
							AccumulatedIncidentRadiance[LinearIndex] += TSHVectorRGB<2>(VolumeSample.Lighting) * SampleWeight;
							AccumulatedWeights[LinearIndex] += SampleWeight;							
						}

						TranslationFromSample.X += QuerySteps.X;
					}

					TranslationFromSample.X = BaseTranslationFromSample.X;
					TranslationFromSample.Y += QuerySteps.Y;
				}

				TranslationFromSample.Y = BaseTranslationFromSample.Y;
				TranslationFromSample.Z += QuerySteps.Z;
			}
		}
	}
}

void FPrecomputedLightVolume::DebugDrawSamples(FPrimitiveDrawInterface* PDI, bool bDrawDirectionalShadowing) const
{
	for (FLightVolumeOctree::TConstElementBoxIterator<> OctreeIt(*OctreeForRendering, OctreeForRendering->GetRootBounds());
		OctreeIt.HasPendingElements();
		OctreeIt.Advance())
	{
		const FVolumeLightingSample& VolumeSample = OctreeIt.GetCurrentElement();
		const FLinearColor AverageColor = bDrawDirectionalShadowing
			? FLinearColor(VolumeSample.DirectionalLightShadowing, VolumeSample.DirectionalLightShadowing, VolumeSample.DirectionalLightShadowing)
			: VolumeSample.Lighting.CalcIntegral() / (FSHVector2::ConstantBasisIntegral * PI);
		PDI->DrawPoint(VolumeSample.Position, AverageColor, 10, SDPG_World);
	}
}

SIZE_T FPrecomputedLightVolume::GetAllocatedBytes() const
{
	SIZE_T NodeBytes = 0;

	for (FLightVolumeOctree::TConstIterator<> NodeIt(HighQualityLightmapOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
	{
		const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();
		NodeBytes += sizeof(FLightVolumeOctree::FNode);
		NodeBytes += CurrentNode.GetElements().GetAllocatedSize();

		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if(CurrentNode.HasChild(ChildRef))
			{
				NodeIt.PushChild(ChildRef);
			}
		}
	}

	for (FLightVolumeOctree::TConstIterator<> NodeIt(LowQualityLightmapOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
	{
		const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();
		NodeBytes += sizeof(FLightVolumeOctree::FNode);
		NodeBytes += CurrentNode.GetElements().GetAllocatedSize();

		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if(CurrentNode.HasChild(ChildRef))
			{
				NodeIt.PushChild(ChildRef);
			}
		}
	}

	return NodeBytes;
}

void FPrecomputedLightVolume::ApplyWorldOffset(const FVector& InOffset)
{
	Bounds.Min+= InOffset;
	Bounds.Max+= InOffset;
	HighQualityLightmapOctree.ApplyOffset(InOffset);
	LowQualityLightmapOctree.ApplyOffset(InOffset);
}
