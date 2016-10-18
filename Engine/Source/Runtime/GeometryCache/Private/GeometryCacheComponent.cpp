// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheModulePrivatePCH.h"
#include "GeometryCacheComponent.h"
#include "MessageLog.h"
#include "ContentStreaming.h"

#include "GeometryCacheSceneProxy.h"
#include "GeometryCacheTrack.h"
#include "GeometryCacheTrackFlipbookAnimation.h"

#define LOCTEXT_NAMESPACE "GeometryCacheComponent"

DECLARE_CYCLE_STAT(TEXT("GeometryCacheTick"), STAT_GeometryCacheComponent_TickComponent, STATGROUP_GeometryCache);

UGeometryCacheComponent::UGeometryCacheComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	ElapsedTime = 0.0f;
	bRunning = false;
	bLooping = true;
	PlayDirection = 1.0f;
	StartTimeOffset = 0.0f;
	PlaybackSpeed = 1.0f;
	Duration = 0.0f;
}

void UGeometryCacheComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();	
}

void UGeometryCacheComponent::OnRegister()
{
	ClearTrackData();
	SetupTrackData();
	Super::OnRegister();
}

void UGeometryCacheComponent::ClearTrackData()
{
	NumTracks = 0;
	TrackMatrixSampleIndices.Empty();
	TrackMatrixSampleIndices.Empty();
	TrackSections.Empty();
	SceneProxy = nullptr;
}

void UGeometryCacheComponent::SetupTrackData()
{
	if (GeometryCache != nullptr)
	{
		// Refresh NumTracks and clear Index Arrays
		NumTracks = GeometryCache->Tracks.Num();
		TrackMeshSampleIndices.Empty(GeometryCache->Tracks.Num());
		TrackMatrixSampleIndices.Empty(GeometryCache->Tracks.Num());

		Duration = 0.0f;
		// Create mesh sections for each GeometryCacheTrack
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			UGeometryCacheTrack* Track = GeometryCache->Tracks[TrackIndex];
			FMatrix WorldMatrix;
			int32 MeshSampleIndex = -1;
			int32 MatrixSampleIndex = -1;
			FGeometryCacheMeshData* MeshData = nullptr;

			// Retrieve the matrix/mesh data and the appropriate sample indices
			Track->UpdateMatrixData(ElapsedTime + StartTimeOffset, bLooping, MatrixSampleIndex, WorldMatrix);
			Track->UpdateMeshData(ElapsedTime + StartTimeOffset, bLooping, MeshSampleIndex, MeshData);

			// First time so create rather than update the mesh sections
			CreateTrackSection(TrackIndex, WorldMatrix, MeshData);

			// Store the sample indices for both the mesh and matrix data
			TrackMeshSampleIndices.Add(MeshSampleIndex);
			TrackMatrixSampleIndices.Add(MatrixSampleIndex);

			const float TrackMaxSampleTime = Track->GetMaxSampleTime();
			Duration = (Duration > TrackMaxSampleTime) ? Duration : TrackMaxSampleTime;
		}
	}
}

void UGeometryCacheComponent::OnUnregister()
{
	Super::OnUnregister();

	NumTracks = 0;
	TrackMatrixSampleIndices.Empty();
	TrackMatrixSampleIndices.Empty();
	TrackSections.Empty();
}

void UGeometryCacheComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_GeometryCacheComponent_TickComponent);
	if (GeometryCache && bRunning)
	{
		// Increase total elapsed time since BeginPlay according to PlayDirection and speed
		ElapsedTime += (DeltaTime * PlayDirection * PlaybackSpeed);

		if (ElapsedTime < 0.0f)
		{
			ElapsedTime += Duration;
		}

		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			// Determine if and which part of the section we have to update
			UGeometryCacheTrack* Track = GeometryCache->Tracks[TrackIndex];
			FMatrix WorldMatrix;
			FGeometryCacheMeshData* MeshData = nullptr;

			const bool bUpdateMatrix = Track->UpdateMatrixData(ElapsedTime + StartTimeOffset, bLooping, TrackMatrixSampleIndices[TrackIndex], WorldMatrix);
			const bool bUpdateMesh = Track->UpdateMeshData(ElapsedTime + StartTimeOffset, bLooping, TrackMeshSampleIndices[TrackIndex], MeshData);

			// Update sections according what is required
			if (bUpdateMatrix)
			{
				UpdateTrackSectionMatrixData(TrackIndex, WorldMatrix);
			}

			if (bUpdateMesh)
			{
				UpdateTrackSectionMeshData(TrackIndex, MeshData);
			}			
		}
	}
}

FBoxSphereBounds UGeometryCacheComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds.TransformBy(LocalToWorld);
}
void UGeometryCacheComponent::UpdateLocalBounds()
{
	FBox LocalBox(0);

	for (const FTrackRenderData& Section : TrackSections)
	{
		// Use World matrix per section for correct bounding box
		LocalBox += (Section.MeshData->BoundingBox.TransformBy(Section.WorldMatrix));
	}

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds

	UpdateBounds();
}

FPrimitiveSceneProxy* UGeometryCacheComponent::CreateSceneProxy()
{
	SceneProxy = new FGeometryCacheSceneProxy(this);
	return SceneProxy;
}

#if WITH_EDITOR
void UGeometryCacheComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InvalidateTrackSampleIndices();
	MarkRenderStateDirty();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

int32 UGeometryCacheComponent::GetNumMaterials() const
{
	return GeometryCache ? GeometryCache->Materials.Num() : 0;
}

UMaterialInterface* UGeometryCacheComponent::GetMaterial(int32 MaterialIndex) const
{
	// If we have a base materials array, use that
	if (MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex];
	}
	// Otherwise get from static mesh
	else
	{
		return GeometryCache ? ( GeometryCache->Materials.IsValidIndex(MaterialIndex) ? GeometryCache->Materials[MaterialIndex] : nullptr) : nullptr;
	}
}

void UGeometryCacheComponent::CreateTrackSection(int32 SectionIndex, const FMatrix& WorldMatrix, FGeometryCacheMeshData* MeshData)
{
	// Ensure sections array is long enough
	if (TrackSections.Num() <= SectionIndex)
	{
		TrackSections.SetNum(SectionIndex + 1, false);
	}

	// Reset this section (in case it already existed)
	FTrackRenderData& NewSection = TrackSections[SectionIndex];
	NewSection.Reset();

	// Number of Vertices
	const int32 NumVerts = MeshData->Vertices.Num();
		
	// Store Matrix and MeshData-pointer
	NewSection.WorldMatrix = WorldMatrix;
	NewSection.MeshData = MeshData;

	UpdateLocalBounds(); // Update overall bounds
	MarkRenderStateDirty(); // Recreate scene proxy
}

void UGeometryCacheComponent::UpdateTrackSectionMeshData(int32 SectionIndex, FGeometryCacheMeshData* MeshData)
{
	// Check if the index is in range and Vertices contains any data
	check(SectionIndex < TrackSections.Num() && MeshData != nullptr && "Invalid SectionIndex or Mesh Data");
	// Reset this section (in case it already existed)
	FTrackRenderData& UpdateSection = TrackSections[SectionIndex];
	
	// Number of Vertices
	const int32 NumVerts = MeshData->Vertices.Num();
	
	// Update MeshDataPointer
	UpdateSection.MeshData = MeshData;

	//if (SceneProxy)
	//{		
	//	UpdateTrackSectionVertexbuffer(SectionIndex, MeshData);
	//	UpdateTrackSectionIndexbuffer(SectionIndex, MeshData->Indices);
	//	// Will update the bounds (used for frustum culling)
	//	MarkRenderTransformDirty();
	//	MarkRenderDynamicDataDirty();
	//}
	//else
	//{
	//	// Recreate scene proxy
	//	MarkRenderStateDirty(); 
	//}

	// Update overall bounds	
	UpdateLocalBounds();

	MarkRenderStateDirty();
}

void UGeometryCacheComponent::UpdateTrackSectionMatrixData(int32 SectionIndex, const FMatrix& WorldMatrix)
{
	check(SectionIndex < TrackSections.Num() && "Invalid SectionIndex");
	// Reset this section (in case it already existed)
	FTrackRenderData& UpdateSection = TrackSections[SectionIndex];

	// Update the WorldMatrix only
	UpdateSection.WorldMatrix = WorldMatrix;

	if (!IsRenderStateDirty() && SceneProxy != nullptr)
	{
		// Update it within the scene proxy
		SceneProxy->UpdateSectionWorldMatrix(SectionIndex, WorldMatrix);
	}

	// Update local bounds
	UpdateLocalBounds();

	// Mark transform Dirty
	MarkRenderTransformDirty();
}

void UGeometryCacheComponent::UpdateTrackSectionVertexbuffer(int32 SectionIndex, FGeometryCacheMeshData* MeshData)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FUpdateVertexBufferCommand,
		FGeometryCacheSceneProxy*, SceneProxy, SceneProxy,
		const int32, Index, SectionIndex,
		FGeometryCacheMeshData*, Data, MeshData,
		{
		SceneProxy->UpdateSectionVertexBuffer(Index, Data);
	});
}

void UGeometryCacheComponent::UpdateTrackSectionIndexbuffer(int32 SectionIndex, const TArray<uint32>& Indices)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FUpdateVertexBufferCommand,
		FGeometryCacheSceneProxy*, SceneProxy, SceneProxy,
		const int32, Index, SectionIndex,
		const TArray<uint32>&, Data, Indices,
		{
		SceneProxy->UpdateSectionIndexBuffer(Index, Data);
	});
}

void UGeometryCacheComponent::OnObjectReimported(UGeometryCache* ImportedGeometryCache)
{
	if (GeometryCache == ImportedGeometryCache)
	{
		ReleaseResources();
		GeometryCache = ImportedGeometryCache;

		if (GeometryCache != nullptr)
		{
			// Refresh NumTracks and clear Index Arrays
			NumTracks = GeometryCache->Tracks.Num();
			TrackMeshSampleIndices.Empty(GeometryCache->Tracks.Num());
			TrackMatrixSampleIndices.Empty(GeometryCache->Tracks.Num());

			Duration = 0.0f;
			// Create mesh sections for each GeometryCacheTrack
			for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
			{
				UGeometryCacheTrack* Track = GeometryCache->Tracks[TrackIndex];
				FMatrix WorldMatrix;
				int32 MeshSampleIndex = -1;
				int32 MatrixSampleIndex = -1;
				FGeometryCacheMeshData* MeshData = nullptr;

				// Retrieve the matrix/mesh data and the appropriate sample indices
				Track->UpdateMatrixData(ElapsedTime + StartTimeOffset, bLooping, MatrixSampleIndex, WorldMatrix);
				Track->UpdateMeshData(ElapsedTime + StartTimeOffset, bLooping, MeshSampleIndex, MeshData);

				// First time so create rather than update the mesh sections
				CreateTrackSection(TrackIndex, WorldMatrix, MeshData);

				// Store the sample indices for both the mesh and matrix data
				TrackMeshSampleIndices.Add(MeshSampleIndex);
				TrackMatrixSampleIndices.Add(MatrixSampleIndex);

				const float TrackMaxSampleTime = Track->GetMaxSampleTime();
				Duration = (Duration > TrackMaxSampleTime) ? Duration : TrackMaxSampleTime;
			}
		}

		MarkRenderStateDirty();
	}
}

void UGeometryCacheComponent::Play()
{
	bRunning = true;
	PlayDirection = 1.0f;
}

void UGeometryCacheComponent::PlayFromStart()
{
	ElapsedTime = 0.0f;
	bRunning = true;
	PlayDirection = 1.0f;
}

void UGeometryCacheComponent::Pause()
{
	bRunning = !bRunning;
}

void UGeometryCacheComponent::Stop()
{
	bRunning = false;
}

bool UGeometryCacheComponent::IsPlaying() const
{
	return bRunning;
}

bool UGeometryCacheComponent::IsLooping() const
{
	return bLooping;
}

void UGeometryCacheComponent::SetLooping(const bool bNewLooping)
{
	bLooping = bNewLooping;
}

bool UGeometryCacheComponent::IsPlayingReversed() const
{
	return FMath::IsNearlyEqual( PlayDirection, -1.0f );
}

float UGeometryCacheComponent::GetPlaybackSpeed() const
{
	return PlaybackSpeed;
}

void UGeometryCacheComponent::SetPlaybackSpeed(const float NewPlaybackSpeed)
{
	// Currently only positive play back speeds are supported
	PlaybackSpeed = fabs( NewPlaybackSpeed );
}

bool UGeometryCacheComponent::SetGeometryCache(UGeometryCache* NewGeomCache)
{
	// Do nothing if we are already using the supplied static mesh
	if (NewGeomCache == GeometryCache)
	{
		return false;
	}

	// Don't allow changing static meshes if "static" and registered
	AActor* Owner = GetOwner();
	if (!AreDynamicDataChangesAllowed() && Owner != nullptr)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetGeometryCache", "Calling SetGeometryCache on '{0}' but Mobility is Static."),
			FText::FromString(GetPathName())));
		return false;
	}


	GeometryCache = NewGeomCache;

	ClearTrackData();
	SetupTrackData();

	// Need to send this to render thread at some point
	MarkRenderStateDirty();

	// Update physics representation right away
	RecreatePhysicsState();
	
	// Notify the streaming system. Don't use Update(), because this may be the first time the geometry cache has been set
	// and the component may have to be added to the streaming system for the first time.
	IStreamingManager::Get().NotifyPrimitiveAttached(this, DPT_Spawned);

	// Since we have new tracks, we need to update bounds
	UpdateBounds();
	return true;
}

UGeometryCache* UGeometryCacheComponent::GetGeometryCache() const
{
	return GeometryCache;
}

void UGeometryCacheComponent::PlayReversedFromEnd()
{
	ElapsedTime = Duration;
	PlayDirection = -1.0f;
	bRunning = true;
}

void UGeometryCacheComponent::PlayReversed()
{
	PlayDirection = -1.0f;
	bRunning = true;
}

void UGeometryCacheComponent::InvalidateTrackSampleIndices()
{
	for (int32& Index : TrackMeshSampleIndices)
	{
		Index = -1;
	}

	for (int32& Index : TrackMatrixSampleIndices)
	{
		Index = -1;
	}
}

void UGeometryCacheComponent::ReleaseResources()
{
	GeometryCache = nullptr;
	NumTracks = 0;
	TrackMatrixSampleIndices.Empty();
	TrackMatrixSampleIndices.Empty();
	TrackSections.Empty();
	DetachFence.BeginFence();
}

#if WITH_EDITOR
void UGeometryCacheComponent::PreEditUndo()
{
	InvalidateTrackSampleIndices();
	MarkRenderStateDirty();
}

void UGeometryCacheComponent::PostEditUndo()
{
	InvalidateTrackSampleIndices();
	MarkRenderStateDirty();
}
#endif

#undef LOCTEXT_NAMESPACE
