// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheModulePrivatePCH.h"
#include "GeometryCacheTrackTransformAnimation.h"
#include "GeometryCacheTrackFlipbookAnimation.h"
#include "EditorFramework/AssetImportData.h"

UGeometryCache::UGeometryCache(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/) : UObject(ObjectInitializer)
{
	NumTransformAnimationTracks = 0;
	NumVertexAnimationTracks = 0;
}

void UGeometryCache::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

void UGeometryCache::Serialize(FArchive& Ar)
{
	// Custom serialization
#if WITH_EDITORONLY_DATA
	Ar << AssetImportData;
#endif
	Ar << Tracks;
	Ar << NumVertexAnimationTracks;
	Ar << NumTransformAnimationTracks;
}

FString UGeometryCache::GetDesc()
{
	const int32 NumTracks = Tracks.Num();
	return FString("%d Tracks", NumTracks);
}

SIZE_T UGeometryCache::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;
#if WITH_EDITORONLY_DATA
	ResourceSize += sizeof( AssetImportData );
#endif
	// Calculate Resource Size according to what is serialized
	const int32 NumTracks = Tracks.Num();
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		ResourceSize += Tracks[TrackIndex]->GetResourceSize(Mode);
	}

	ResourceSize += sizeof(Tracks);
	ResourceSize += sizeof(NumVertexAnimationTracks);
	ResourceSize += sizeof(NumTransformAnimationTracks);

	return ResourceSize;
}

void UGeometryCache::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// Information on number total of (per type) tracks
	const int32 NumTracks = Tracks.Num();	
	OutTags.Add(FAssetRegistryTag("Total Tracks", FString::FromInt(NumTracks), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Vertex Animation Tracks", FString::FromInt(NumVertexAnimationTracks), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Transform Animation Tracks", FString::FromInt(NumTransformAnimationTracks), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

void UGeometryCache::BeginDestroy()
{
	Super::BeginDestroy();
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();
	ReleaseResourcesFence.BeginFence();
}

void UGeometryCache::ClearForReimporting()
{
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}

bool UGeometryCache::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.IsFenceComplete();
}

#if WITH_EDITOR
void UGeometryCache::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Release the Geometry Cache resources.
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}
#endif

void UGeometryCache::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UGeometryCache* This = CastChecked<UGeometryCache>(InThis);	
	Super::AddReferencedObjects(This, Collector);
}

void UGeometryCache::AddTrack(UGeometryCacheTrack* Track)
{
	if (Track->GetClass() == UGeometryCacheTrack_TransformAnimation::StaticClass())
	{
		NumTransformAnimationTracks++;
	}
	else if (Track->GetClass() == UGeometryCacheTrack_FlipbookAnimation::StaticClass())
	{
		NumVertexAnimationTracks++;
	}

	Tracks.Add(Track);
}
