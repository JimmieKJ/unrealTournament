// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMesh.cpp: Static mesh class implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/AssetUserData.h"
#include "StaticMeshResources.h"
#include "MeshBuild.h"
#include "GenericOctree.h"
#include "TessellationRendering.h"
#include "StaticMeshVertexData.h"
#include "TargetPlatform.h"
#include "SpeedTreeWind.h"
#include "DistanceFieldAtlas.h"
#include "UObject/DevObjectVersion.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "DerivedDataCacheInterface.h"
#include "UObjectAnnotation.h"
#endif // #if WITH_EDITOR

#include "Engine/StaticMeshSocket.h"
#include "EditorFramework/AssetImportData.h"
#include "AI/Navigation/NavCollision.h"
#include "CookStats.h"
#include "ReleaseObjectVersion.h"

DEFINE_LOG_CATEGORY(LogStaticMesh);	

DECLARE_MEMORY_STAT( TEXT( "StaticMesh Total Memory" ), STAT_StaticMeshTotalMemory2, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh Vertex Memory" ), STAT_StaticMeshVertexMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh VxColor Resource Mem" ), STAT_ResourceVertexColorMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh Index Memory" ), STAT_StaticMeshIndexMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh Distance Field Memory" ), STAT_StaticMeshDistanceFieldMemory, STATGROUP_MemoryStaticMesh );

DECLARE_MEMORY_STAT( TEXT( "StaticMesh Total Memory" ), STAT_StaticMeshTotalMemory, STATGROUP_Memory );

/** Package name, that if set will cause only static meshes in that package to be rebuilt based on SM version. */
ENGINE_API FName GStaticMeshPackageNameToRebuild = NAME_None;

#if ENABLE_COOK_STATS
namespace StaticMeshCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("StaticMesh.Usage"), TEXT(""));
	});
}
#endif

/*-----------------------------------------------------------------------------
	FStaticMeshVertexBuffer
-----------------------------------------------------------------------------*/

FStaticMeshVertexBuffer::FStaticMeshVertexBuffer():
	VertexData(NULL),
	NumTexCoords(0),
	Data(NULL),
	Stride(0),
	NumVertices(0),
	bUseFullPrecisionUVs(false),
	bUseHighPrecisionTangentBasis(false)
{}

FStaticMeshVertexBuffer::~FStaticMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
* @param InNumTexCoords - The number of texture coordinate to store in the buffer.
*/
void FStaticMeshVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices,uint32 InNumTexCoords)
{
	NumTexCoords = InNumTexCoords;
	NumVertices = InVertices.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the vertices into the buffer.
	for(int32 VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		SetVertexTangents(DestVertexIndex, SourceVertex.TangentX, SourceVertex.TangentY, SourceVertex.TangentZ);

		for(uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
		{
			SetVertexUV(DestVertexIndex,UVIndex,SourceVertex.UVs[UVIndex]);
		}
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FStaticMeshVertexBuffer::Init(const FStaticMeshVertexBuffer& InVertexBuffer)
{
	NumTexCoords = InVertexBuffer.GetNumTexCoords();
	NumVertices = InVertexBuffer.GetNumVertices();
	bUseFullPrecisionUVs = InVertexBuffer.GetUseFullPrecisionUVs();
	bUseHighPrecisionTangentBasis = InVertexBuffer.GetUseHighPrecisionTangentBasis();

	if ( NumVertices )
	{
		AllocateData();
		check( GetStride() == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const uint8* InData = InVertexBuffer.GetRawVertexData();
		FMemory::Memcpy( Data, InData, Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FStaticMeshVertexBuffer::RemoveLegacyShadowVolumeVertices(uint32 InNumVertices)
{
	check(VertexData);
	VertexData->ResizeBuffer(InNumVertices);
	NumVertices = InNumVertices;

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

template<typename SrcVertexTypeT, typename DstVertexTypeT>
void FStaticMeshVertexBuffer::ConvertVertexFormat()
{
	CA_SUPPRESS(6326);
	if (SrcVertexTypeT::TangentBasisType == DstVertexTypeT::TangentBasisType &&
		SrcVertexTypeT::UVType == DstVertexTypeT::UVType)
	{
		return;
	}

	static_assert(SrcVertexTypeT::NumTexCoords == DstVertexTypeT::NumTexCoords, "NumTexCoords don't match");

	auto& SrcVertexData = *static_cast<TStaticMeshVertexData<SrcVertexTypeT>*>(VertexData);

	TArray<DstVertexTypeT> DstVertexData;
	DstVertexData.AddUninitialized(SrcVertexData.Num());

	for (int32 VertIdx = 0; VertIdx < SrcVertexData.Num(); VertIdx++)
	{
		SrcVertexTypeT& SrcVert = SrcVertexData[VertIdx];
		DstVertexTypeT& DstVert = DstVertexData[VertIdx];

		DstVert.SetTangents(SrcVert.GetTangentX(), SrcVert.GetTangentY(), SrcVert.GetTangentZ());

		for (int32 UVIdx = 0; UVIdx < DstVertexTypeT::NumTexCoords; UVIdx++)
		{
			DstVert.SetUV(UVIdx, SrcVert.GetUV(UVIdx));
		}
	}

	CA_SUPPRESS(6326);
	bUseHighPrecisionTangentBasis = DstVertexTypeT::TangentBasisType == EStaticMeshVertexTangentBasisType::HighPrecision;
	CA_SUPPRESS(6326);
	bUseFullPrecisionUVs = DstVertexTypeT::UVType == EStaticMeshVertexUVType::HighPrecision;

	AllocateData();
	*static_cast<TStaticMeshVertexData<DstVertexTypeT>*>(VertexData) = DstVertexData;

	Data = VertexData->GetDataPointer();
	Stride = VertexData->GetStride();
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FStaticMeshVertexBuffer::Serialize( FArchive& Ar, bool bNeedsCPUAccess )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStaticMeshVertexBuffer::Serialize"), STAT_StaticMeshVertexBuffer_Serialize, STATGROUP_LoadTime );

	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	Ar << NumTexCoords << Stride << NumVertices;
	Ar << bUseFullPrecisionUVs;	
	Ar << bUseHighPrecisionTangentBasis;

	if( Ar.IsLoading() )
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if( VertexData != NULL )
		{
			// Serialize the vertex data.
			VertexData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			Data = VertexData->GetDataPointer();
		}
	}
}


/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FStaticMeshVertexBuffer::operator=(const FStaticMeshVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
	bUseHighPrecisionTangentBasis = Other.bUseHighPrecisionTangentBasis;
}

void FStaticMeshVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),BUF_Static,CreateInfo);
	}
}

void FStaticMeshVertexBuffer::AllocateData( bool bNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	SELECT_STATIC_MESH_VERTEX_TYPE(
		GetUseHighPrecisionTangentBasis(),
		GetUseFullPrecisionUVs(),
		GetNumTexCoords(),
		VertexData = new TStaticMeshVertexData<VertexType>(bNeedsCPUAccess);
		);

	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

/*-----------------------------------------------------------------------------
	FStaticMeshLODResources
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive& Ar, FStaticMeshSection& Section)
{
	Ar << Section.MaterialIndex;
	Ar << Section.FirstIndex;
	Ar << Section.NumTriangles;
	Ar << Section.MinVertexIndex;
	Ar << Section.MaxVertexIndex;
	Ar << Section.bEnableCollision;
	Ar << Section.bCastShadow;
	return Ar;
}

void FStaticMeshLODResources::Serialize(FArchive& Ar, UObject* Owner, int32 Index)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStaticMeshLODResources::Serialize"), STAT_StaticMeshLODResources_Serialize, STATGROUP_LoadTime );

	// See if the mesh wants to keep resources CPU accessible
	UStaticMesh* OwnerStaticMesh = Cast<UStaticMesh>(Owner);
	bool bMeshCPUAcces = OwnerStaticMesh ? OwnerStaticMesh->bAllowCPUAccess : false;

	// Note: this is all derived data, native versioning is not needed, but be sure to bump STATICMESH_DERIVEDDATA_VER when modifying!

	// On cooked platforms we never need the resource data.
	// TODO: Not needed in uncooked games either after PostLoad!
	bool bNeedsCPUAccess = !FPlatformProperties::RequiresCookedData() || bMeshCPUAcces;

	bHasAdjacencyInfo = false;
	bHasDepthOnlyIndices = false;
	bHasReversedIndices = false;
	bHasReversedDepthOnlyIndices = false;
	DepthOnlyNumTriangles = 0;

    // Defined class flags for possible stripping
	const uint8 AdjacencyDataStripFlag = 1;

    // Actual flags used during serialization
	uint8 ClassDataStripFlags = 0;
	ClassDataStripFlags |= (Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::Tessellation)) ? AdjacencyDataStripFlag : 0;

	FStripDataFlags StripFlags( Ar, ClassDataStripFlags );

	Ar << Sections;
	Ar << MaxDeviation;

	if( !StripFlags.IsDataStrippedForServer() )
	{
		PositionVertexBuffer.Serialize( Ar, bNeedsCPUAccess );
		VertexBuffer.Serialize( Ar, bNeedsCPUAccess );
		ColorVertexBuffer.Serialize( Ar, bNeedsCPUAccess );
		IndexBuffer.Serialize( Ar, bNeedsCPUAccess );
		ReversedIndexBuffer.Serialize( Ar, bNeedsCPUAccess );
		DepthOnlyIndexBuffer.Serialize(Ar, bNeedsCPUAccess);
		ReversedDepthOnlyIndexBuffer.Serialize( Ar, bNeedsCPUAccess );

		if( !StripFlags.IsEditorDataStripped() )
		{
			WireframeIndexBuffer.Serialize(Ar, bNeedsCPUAccess);
		}

		if ( !StripFlags.IsClassDataStripped( AdjacencyDataStripFlag ) )
		{
			AdjacencyIndexBuffer.Serialize( Ar, bNeedsCPUAccess );
			bHasAdjacencyInfo = AdjacencyIndexBuffer.GetNumIndices() != 0;
		}

		// Needs to be done now because on cooked platform, indices are discarded after RHIInit.
		bHasDepthOnlyIndices = DepthOnlyIndexBuffer.GetNumIndices() != 0;
		bHasReversedIndices = ReversedIndexBuffer.GetNumIndices() != 0;
		bHasReversedDepthOnlyIndices = ReversedDepthOnlyIndexBuffer.GetNumIndices() != 0;
		DepthOnlyNumTriangles = DepthOnlyIndexBuffer.GetNumIndices() / 3;
	}
}

int32 FStaticMeshLODResources::GetNumTriangles() const
{
	int32 NumTriangles = 0;
	for(int32 SectionIndex = 0;SectionIndex < Sections.Num();SectionIndex++)
	{
		NumTriangles += Sections[SectionIndex].NumTriangles;
	}
	return NumTriangles;
}

int32 FStaticMeshLODResources::GetNumVertices() const
{
	return VertexBuffer.GetNumVertices();
}

int32 FStaticMeshLODResources::GetNumTexCoords() const
{
	return VertexBuffer.GetNumTexCoords();
}

void FStaticMeshLODResources::InitVertexFactory(
	FLocalVertexFactory& InOutVertexFactory,
	UStaticMesh* InParentMesh,
	bool bInOverrideColorVertexBuffer
	)
{
	check( InParentMesh != NULL );

	struct InitStaticMeshVertexFactoryParams
	{
		FLocalVertexFactory* VertexFactory;
		FStaticMeshLODResources* LODResources;
		bool bOverrideColorVertexBuffer;
		UStaticMesh* Parent;
	} Params;

	Params.VertexFactory = &InOutVertexFactory;
	Params.LODResources = this;
	Params.bOverrideColorVertexBuffer = bInOverrideColorVertexBuffer;
	Params.Parent = InParentMesh;

	uint32 TangentXOffset = 0;
	uint32 TangetnZOffset = 0;
	uint32 UVsBaseOffset = 0;

	SELECT_STATIC_MESH_VERTEX_TYPE(
		Params.LODResources->VertexBuffer.GetUseHighPrecisionTangentBasis(),
		Params.LODResources->VertexBuffer.GetUseFullPrecisionUVs(),
		Params.LODResources->VertexBuffer.GetNumTexCoords(),
		{
			TangentXOffset = STRUCT_OFFSET(VertexType, TangentX);
			TangetnZOffset = STRUCT_OFFSET(VertexType, TangentZ);
			UVsBaseOffset = STRUCT_OFFSET(VertexType, UVs);
		});

	// Initialize the static mesh's vertex factory.
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
		InitStaticMeshVertexFactory,
		InitStaticMeshVertexFactoryParams, Params, Params,
		uint32, TangentXOffset, TangentXOffset,
		uint32, TangetnZOffset, TangetnZOffset,
		uint32, UVsBaseOffset, UVsBaseOffset,
		{
			FLocalVertexFactory::FDataType Data;
			Data.PositionComponent = FVertexStreamComponent(
				&Params.LODResources->PositionVertexBuffer,
				STRUCT_OFFSET(FPositionVertex,Position),
				Params.LODResources->PositionVertexBuffer.GetStride(),
				VET_Float3
				);

			Data.TangentBasisComponents[0] = FVertexStreamComponent(
				&Params.LODResources->VertexBuffer,
				TangentXOffset,
				Params.LODResources->VertexBuffer.GetStride(),
				Params.LODResources->VertexBuffer.GetUseHighPrecisionTangentBasis() ? 
					TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
					TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
				);

			Data.TangentBasisComponents[1] = FVertexStreamComponent(
				&Params.LODResources->VertexBuffer,
				TangetnZOffset,
				Params.LODResources->VertexBuffer.GetStride(),
				Params.LODResources->VertexBuffer.GetUseHighPrecisionTangentBasis() ?
					TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
					TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
				);

			// Use the "override" color vertex buffer if one was supplied.  Otherwise, the color vertex stream
			// associated with the static mesh is used.
			if (Params.bOverrideColorVertexBuffer)
			{
				Data.ColorComponent = FVertexStreamComponent(
					&GNullColorVertexBuffer,
					0,	// Struct offset to color
					sizeof(FColor), //asserted elsewhere
					VET_Color,
					false, // not instanced
					true // set in SetMesh
					);
			}
			else 
			{
				FColorVertexBuffer* LODColorVertexBuffer = &Params.LODResources->ColorVertexBuffer;
				if (LODColorVertexBuffer->GetNumVertices() > 0)
				{
					Data.ColorComponent = FVertexStreamComponent(
						LODColorVertexBuffer,
						0,	// Struct offset to color
						LODColorVertexBuffer->GetStride(),
						VET_Color
						);
				}
			}

			Data.TextureCoordinates.Empty();

			uint32 UVSizeInBytes = Params.LODResources->VertexBuffer.GetUseFullPrecisionUVs() ?
				sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT) : sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);

			EVertexElementType UVDoubleWideVertexElementType = Params.LODResources->VertexBuffer.GetUseFullPrecisionUVs() ?
				VET_Float4 : VET_Half4;

			EVertexElementType UVVertexElementType = Params.LODResources->VertexBuffer.GetUseFullPrecisionUVs() ?
				VET_Float2 : VET_Half2;

			int32 UVIndex;
			for (UVIndex = 0; UVIndex < (int32)Params.LODResources->VertexBuffer.GetNumTexCoords() - 1; UVIndex += 2)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&Params.LODResources->VertexBuffer,
					UVsBaseOffset + UVSizeInBytes * UVIndex,
					Params.LODResources->VertexBuffer.GetStride(),
					UVDoubleWideVertexElementType
					));
			}

			// possible last UV channel if we have an odd number
			if (UVIndex < (int32)Params.LODResources->VertexBuffer.GetNumTexCoords())
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&Params.LODResources->VertexBuffer,
					UVsBaseOffset + UVSizeInBytes * UVIndex,
					Params.LODResources->VertexBuffer.GetStride(),
					UVVertexElementType
					));
			}

			if(	Params.Parent->LightMapCoordinateIndex >= 0 && (uint32)Params.Parent->LightMapCoordinateIndex < Params.LODResources->VertexBuffer.GetNumTexCoords())
			{
				Data.LightMapCoordinateComponent = FVertexStreamComponent(
					&Params.LODResources->VertexBuffer,
					UVsBaseOffset + UVSizeInBytes * Params.Parent->LightMapCoordinateIndex,
					Params.LODResources->VertexBuffer.GetStride(),
					UVVertexElementType
					);
			}

			Params.VertexFactory->SetData(Data);
		});
}

FStaticMeshLODResources::FStaticMeshLODResources()
	: DistanceFieldData(NULL)
	, MaxDeviation(0.0f)
	, bHasAdjacencyInfo(false)
	, bHasDepthOnlyIndices(false)
	, bHasReversedIndices(false)
	, bHasReversedDepthOnlyIndices(false)
	, DepthOnlyNumTriangles(0)
	, SplineVertexFactory(nullptr)
	, SplineVertexFactoryOverrideColorVertexBuffer(nullptr)
{
}

FStaticMeshLODResources::~FStaticMeshLODResources()
{
	delete (FRenderResource*)SplineVertexFactory;
	delete (FRenderResource*)SplineVertexFactoryOverrideColorVertexBuffer;
	delete DistanceFieldData;
}

void FStaticMeshLODResources::InitResources(UStaticMesh* Parent)
{
	const auto MaxShaderPlatform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];

	// Initialize the vertex and index buffers.
	if (IsES2Platform(MaxShaderPlatform))
	{
		if (IndexBuffer.Is32Bit())
		{
			//TODO: Show this as an error in the static mesh editor when doing a Mobile preview so gets fixed in content
			TArray<uint32> Indices;
			IndexBuffer.GetCopy(Indices);
			IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force16Bit);
			UE_LOG(LogStaticMesh, Warning, TEXT("[%s] Mesh has more that 65535 vertices, incompatible with mobile; forcing 16-bit (will probably cause rendering issues)." ), *Parent->GetName());
		}
	}
	BeginInitResource(&IndexBuffer);
	if( WireframeIndexBuffer.GetNumIndices() > 0 )
	{
		BeginInitResource(&WireframeIndexBuffer);
	}	
	BeginInitResource(&VertexBuffer);
	BeginInitResource(&PositionVertexBuffer);
	if( ColorVertexBuffer.GetNumVertices() > 0 )
	{
		BeginInitResource(&ColorVertexBuffer);
	}

	if (ReversedIndexBuffer.GetNumIndices() > 0)
	{
		BeginInitResource(&ReversedIndexBuffer);
	}

	if (DepthOnlyIndexBuffer.GetNumIndices() > 0)
	{
		BeginInitResource(&DepthOnlyIndexBuffer);
	}

	if (ReversedDepthOnlyIndexBuffer.GetNumIndices() > 0)
	{
		BeginInitResource(&ReversedDepthOnlyIndexBuffer);
	}

	if (RHISupportsTessellation(MaxShaderPlatform))
	{
		BeginInitResource(&AdjacencyIndexBuffer);
	}

	InitVertexFactory(VertexFactory, Parent, false);
	BeginInitResource(&VertexFactory);

	InitVertexFactory(VertexFactoryOverrideColorVertexBuffer, Parent, true);
	BeginInitResource(&VertexFactoryOverrideColorVertexBuffer);

	if (DistanceFieldData)
	{
		DistanceFieldData->VolumeTexture.Initialize();
		INC_DWORD_STAT_BY( STAT_StaticMeshDistanceFieldMemory, DistanceFieldData->GetResourceSize() );
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateMemoryStats,
		FStaticMeshLODResources*, This, this,
		{		
			const uint32 StaticMeshVertexMemory =
			This->VertexBuffer.GetStride() * This->VertexBuffer.GetNumVertices() +
			This->PositionVertexBuffer.GetStride() * This->PositionVertexBuffer.GetNumVertices();
			const uint32 StaticMeshIndexMemory = This->IndexBuffer.GetAllocatedSize()
				+ This->WireframeIndexBuffer.GetAllocatedSize()
				+ (RHISupportsTessellation( GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel] ) ? This->AdjacencyIndexBuffer.GetAllocatedSize() : 0);
			const uint32 ResourceVertexColorMemory = This->ColorVertexBuffer.GetStride() * This->ColorVertexBuffer.GetNumVertices();

			INC_DWORD_STAT_BY( STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
			INC_DWORD_STAT_BY( STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
			INC_DWORD_STAT_BY( STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );
		});
}

void FStaticMeshLODResources::ReleaseResources()
{
	const uint32 StaticMeshVertexMemory = 
		VertexBuffer.GetStride() * VertexBuffer.GetNumVertices() + 
		PositionVertexBuffer.GetStride() * PositionVertexBuffer.GetNumVertices();
	const uint32 StaticMeshIndexMemory = IndexBuffer.GetAllocatedSize()
		+ WireframeIndexBuffer.GetAllocatedSize()
		+ (RHISupportsTessellation( GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel] ) ? AdjacencyIndexBuffer.GetAllocatedSize() : 0);
	const uint32 ResourceVertexColorMemory = ColorVertexBuffer.GetStride() * ColorVertexBuffer.GetNumVertices();

	DEC_DWORD_STAT_BY( STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
	DEC_DWORD_STAT_BY( STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
	DEC_DWORD_STAT_BY( STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );

	// Release the vertex and index buffers.
	
	// AdjacencyIndexBuffer may not be initialized at this time, but it is safe to release it anyway.
	// The bInitialized flag will be safely checked in the render thread.
	// This avoids a race condition regarding releasing this resource.
	BeginReleaseResource(&AdjacencyIndexBuffer);

	BeginReleaseResource(&IndexBuffer);
	BeginReleaseResource(&WireframeIndexBuffer);
	BeginReleaseResource(&VertexBuffer);
	BeginReleaseResource(&PositionVertexBuffer);
	BeginReleaseResource(&ColorVertexBuffer);
	BeginReleaseResource(&ReversedIndexBuffer);
	BeginReleaseResource(&DepthOnlyIndexBuffer);
	BeginReleaseResource(&ReversedDepthOnlyIndexBuffer);

	// Release the vertex factories.
	BeginReleaseResource(&VertexFactory);
	BeginReleaseResource(&VertexFactoryOverrideColorVertexBuffer);

	if (SplineVertexFactory)
	{
		BeginReleaseResource((FRenderResource *)SplineVertexFactory);
	}
	if (SplineVertexFactoryOverrideColorVertexBuffer)
	{
		BeginReleaseResource((FRenderResource *)SplineVertexFactoryOverrideColorVertexBuffer);
	}

	if (DistanceFieldData)
	{
		DEC_DWORD_STAT_BY( STAT_StaticMeshDistanceFieldMemory, DistanceFieldData->GetResourceSize() );
		DistanceFieldData->VolumeTexture.Release();
	}
}

/*------------------------------------------------------------------------------
	FStaticMeshRenderData
------------------------------------------------------------------------------*/

FStaticMeshRenderData::FStaticMeshRenderData()
	: MaxStreamingTextureFactor(0.0f)
	, bLODsShareStaticLighting(false)
{
	for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
	{
		ScreenSize[LODIndex] = 0.0f;
	}

	for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
	{
		StreamingTextureFactors[TexCoordIndex] = 0.0f;
	}
}

void FStaticMeshRenderData::Serialize(FArchive& Ar, UStaticMesh* Owner, bool bCooked)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStaticMeshRenderData::Serialize"), STAT_StaticMeshRenderData_Serialize, STATGROUP_LoadTime );

	// Note: this is all derived data, native versioning is not needed, but be sure to bump STATICMESH_DERIVEDDATA_VER when modifying!
#if WITH_EDITOR
	const bool bHasEditorData = !Owner->GetOutermost()->bIsCookedForEditor;
	if (Ar.IsSaving() && bHasEditorData)
	{
		ResolveSectionInfo(Owner);
	}
#endif
#if WITH_EDITORONLY_DATA
	if (!bCooked)
	{
		Ar << WedgeMap;
		Ar << MaterialIndexToImportIndex;
	}

#endif // #if WITH_EDITORONLY_DATA

	LODResources.Serialize(Ar, Owner);

	// Inline the distance field derived data for cooked builds
	if (bCooked)
	{
		FStripDataFlags StripFlags( Ar );
		if ( !StripFlags.IsDataStrippedForServer() )
		{
			if (Ar.IsSaving())
			{
				GDistanceFieldAsyncQueue->BlockUntilBuildComplete(Owner, false);
			}

			for (int32 ResourceIndex = 0; ResourceIndex < LODResources.Num(); ResourceIndex++)
			{
				FStaticMeshLODResources& LOD = LODResources[ResourceIndex];

				bool bValid = LOD.DistanceFieldData != NULL;

				Ar << bValid;

				if (bValid)
				{
					if (!LOD.DistanceFieldData)
					{
						LOD.DistanceFieldData = new FDistanceFieldVolumeData();
					}

					Ar << *(LOD.DistanceFieldData);
				}
			}
		}
	}

	Ar << Bounds;
	Ar << bLODsShareStaticLighting;
	Ar << bReducedBySimplygon;

	for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
	{
		Ar << StreamingTextureFactors[TexCoordIndex];
	}
	Ar << MaxStreamingTextureFactor;

	if (bCooked)
	{
		for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
		{
			Ar << ScreenSize[LODIndex];
		}
	}
}

void FStaticMeshRenderData::InitResources(UStaticMesh* Owner)
{
#if WITH_EDITOR
	ResolveSectionInfo(Owner);
#endif // #if WITH_EDITOR

	for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
	{
		LODResources[LODIndex].InitResources(Owner);
	}
}

void FStaticMeshRenderData::ReleaseResources()
{
	for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
	{
		LODResources[LODIndex].ReleaseResources();
	}
}

void FStaticMeshRenderData::AllocateLODResources(int32 NumLODs)
{
	check(LODResources.Num() == 0);
	while (LODResources.Num() < NumLODs)
	{
		new(LODResources) FStaticMeshLODResources();
	}
}

#if WITH_EDITOR
/**
 * Calculates the view distance that a mesh should be displayed at.
 * @param MaxDeviation - The maximum surface-deviation between the reduced geometry and the original. This value should be acquired from Simplygon
 * @returns The calculated view distance	 
 */
static float CalculateViewDistance(float MaxDeviation, float AllowedPixelError)
{
	// We want to solve for the depth in world space given the screen space distance between two pixels
	//
	// Assumptions:
	//   1. There is no scaling in the view matrix.
	//   2. The horizontal FOV is 90 degrees.
	//   3. The backbuffer is 1920x1080.
	//
	// If we project two points at (X,Y,Z) and (X',Y,Z) from view space, we get their screen
	// space positions: (X/Z, Y'/Z) and (X'/Z, Y'/Z) where Y' = Y * AspectRatio.
	//
	// The distance in screen space is then sqrt( (X'-X)^2/Z^2 + (Y'-Y')^2/Z^2 )
	// or (X'-X)/Z. This is in clip space, so PixelDist = 1280 * 0.5 * (X'-X)/Z.
	//
	// Solving for Z: ViewDist = (X'-X * 640) / PixelDist

	const float ViewDistance = (MaxDeviation * 960.0f) / AllowedPixelError;
	return ViewDistance;
}

void FStaticMeshRenderData::ResolveSectionInfo(UStaticMesh* Owner)
{
	int32 LODIndex = 0;
	int32 MaxLODs = LODResources.Num();
	check(MaxLODs <= MAX_STATIC_MESH_LODS);
	for (; LODIndex < MaxLODs; ++LODIndex)
	{
		FStaticMeshLODResources& LOD = LODResources[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
		{
			FMeshSectionInfo Info = Owner->SectionInfoMap.Get(LODIndex,SectionIndex);
			FStaticMeshSection& Section = LOD.Sections[SectionIndex];
			Section.MaterialIndex = Info.MaterialIndex;
			Section.bEnableCollision = Info.bEnableCollision;
			Section.bCastShadow = Info.bCastShadow;
		}

		if (Owner->bAutoComputeLODScreenSize)
		{
			if (LODIndex == 0)
			{
				ScreenSize[LODIndex] = 1.0f;
			}
			else if(LOD.MaxDeviation <= 0.0f)
			{
				ScreenSize[LODIndex] = 1.0f / (MaxLODs * LODIndex);
			}
			else
			{
				const float ViewDistance = CalculateViewDistance(LOD.MaxDeviation, Owner->AutoLODPixelError);
				ScreenSize[LODIndex] = 2.0f * Bounds.SphereRadius / ViewDistance;
			}
		}
		else if (Owner->SourceModels.IsValidIndex(LODIndex))
		{
			ScreenSize[LODIndex] = Owner->SourceModels[LODIndex].ScreenSize;
		}
		else
		{
			check(LODIndex > 0);

			// No valid source model and we're not auto-generating. Auto-generate in this case
			// because we have nothing else to go on.
			const float Tolerance = 0.01f;
			float AutoDisplayFactor = 1.0f / (MaxLODs * LODIndex);

			// Make sure this fits in with the previous LOD
			ScreenSize[LODIndex] = FMath::Clamp(AutoDisplayFactor, 0.0f, ScreenSize[LODIndex-1] - Tolerance);
		}
	}
	for (; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
	{
		ScreenSize[LODIndex] = 0.0f;
	}
}

/*------------------------------------------------------------------------------
	FStaticMeshLODSettings
------------------------------------------------------------------------------*/

void FStaticMeshLODSettings::Initialize(const FConfigFile& IniFile)
{
	// Ensure there is a default LOD group.
	Groups.FindOrAdd(NAME_None);

	// Read individual entries from a config file.
	const TCHAR* IniSection = TEXT("StaticMeshLODSettings");
	const FConfigSection* Section = IniFile.Find(IniSection);
	if (Section)
	{
		for (TMultiMap<FName,FConfigValue>::TConstIterator It(*Section); It; ++It)
		{
			FName GroupName = It.Key();
			FStaticMeshLODGroup& Group = Groups.FindOrAdd(GroupName);
			ReadEntry(Group, It.Value().GetValue());
		};
	}

	// Do some per-group initialization.
	for (TMap<FName,FStaticMeshLODGroup>::TIterator It(Groups); It; ++It)
	{
		FStaticMeshLODGroup& Group = It.Value();
		float PercentTrianglesPerLOD = Group.DefaultSettings[1].PercentTriangles;
		for (int32 LODIndex = 1; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
		{
			float PercentTriangles = Group.DefaultSettings[LODIndex-1].PercentTriangles;
			Group.DefaultSettings[LODIndex] = Group.DefaultSettings[LODIndex - 1];
			Group.DefaultSettings[LODIndex].PercentTriangles = PercentTriangles * PercentTrianglesPerLOD;
		}
	}
}

void FStaticMeshLODSettings::ReadEntry(FStaticMeshLODGroup& Group, FString Entry)
{
	FMeshReductionSettings& Settings = Group.DefaultSettings[0];
	FMeshReductionSettings& Bias = Group.SettingsBias;
	int32 Importance = EMeshFeatureImportance::Normal;

	// Trim whitespace at the beginning.
	Entry = Entry.Trim();

	FParse::Value(*Entry, TEXT("Name="), Group.DisplayName, TEXT("StaticMeshLODSettings"));

	// Remove brackets.
	Entry = Entry.Replace( TEXT("("), TEXT("") );
	Entry = Entry.Replace( TEXT(")"), TEXT("") );
		
	if (FParse::Value(*Entry, TEXT("NumLODs="), Group.DefaultNumLODs))
	{
		Group.DefaultNumLODs = FMath::Clamp<int32>(Group.DefaultNumLODs, 1, MAX_STATIC_MESH_LODS);
	}

	if (FParse::Value(*Entry, TEXT("LightMapResolution="), Group.DefaultLightMapResolution))
	{
		Group.DefaultLightMapResolution = FMath::Max<int32>(Group.DefaultLightMapResolution, 0);
		Group.DefaultLightMapResolution = (Group.DefaultLightMapResolution + 3) & (~3);
	}

	float BasePercentTriangles = 100.0f;
	if (FParse::Value(*Entry, TEXT("BasePercentTriangles="), BasePercentTriangles))
	{
		BasePercentTriangles = FMath::Clamp<float>(BasePercentTriangles, 0.0f, 100.0f);
	}
	Group.DefaultSettings[0].PercentTriangles = BasePercentTriangles * 0.01f;

	float LODPercentTriangles = 100.0f;
	if (FParse::Value(*Entry, TEXT("LODPercentTriangles="), LODPercentTriangles))
	{
		LODPercentTriangles = FMath::Clamp<float>(LODPercentTriangles, 0.0f, 100.0f);
	}
	Group.DefaultSettings[1].PercentTriangles = LODPercentTriangles * 0.01f;

	if (FParse::Value(*Entry, TEXT("MaxDeviation="), Settings.MaxDeviation))
	{
		Settings.MaxDeviation = FMath::Clamp<float>(Settings.MaxDeviation, 0.0f, 1000.0f);
	}

	if (FParse::Value(*Entry, TEXT("WeldingThreshold="), Settings.WeldingThreshold))
	{
		Settings.WeldingThreshold = FMath::Clamp<float>(Settings.WeldingThreshold, 0.0f, 10.0f);
	}

	if (FParse::Value(*Entry, TEXT("HardAngleThreshold="), Settings.HardAngleThreshold))
	{
		Settings.HardAngleThreshold = FMath::Clamp<float>(Settings.HardAngleThreshold, 0.0f, 180.0f);
	}

	if (FParse::Value(*Entry, TEXT("SilhouetteImportance="), Importance))
	{
		Settings.SilhouetteImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, 0, EMeshFeatureImportance::Highest);
	}

	if (FParse::Value(*Entry, TEXT("TextureImportance="), Importance))
	{
		Settings.TextureImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, 0, EMeshFeatureImportance::Highest);
	}

	if (FParse::Value(*Entry, TEXT("ShadingImportance="), Importance))
	{
		Settings.ShadingImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, 0, EMeshFeatureImportance::Highest);
	}

	float BasePercentTrianglesMult = 100.0f;
	if (FParse::Value(*Entry, TEXT("BasePercentTrianglesMult="), BasePercentTrianglesMult))
	{
		BasePercentTrianglesMult = FMath::Clamp<float>(BasePercentTrianglesMult, 0.0f, 100.0f);
	}
	Group.BasePercentTrianglesMult = BasePercentTrianglesMult * 0.01f;

	float LODPercentTrianglesMult = 100.0f;
	if (FParse::Value(*Entry, TEXT("LODPercentTrianglesMult="), LODPercentTrianglesMult))
	{
		LODPercentTrianglesMult = FMath::Clamp<float>(LODPercentTrianglesMult, 0.0f, 100.0f);
	}
	Bias.PercentTriangles = LODPercentTrianglesMult * 0.01f;

	if (FParse::Value(*Entry, TEXT("MaxDeviationBias="), Bias.MaxDeviation))
	{
		Bias.MaxDeviation = FMath::Clamp<float>(Bias.MaxDeviation, -1000.0f, 1000.0f);
	}

	if (FParse::Value(*Entry, TEXT("WeldingThresholdBias="), Bias.WeldingThreshold))
	{
		Bias.WeldingThreshold = FMath::Clamp<float>(Bias.WeldingThreshold, -10.0f, 10.0f);
	}

	if (FParse::Value(*Entry, TEXT("HardAngleThresholdBias="), Bias.HardAngleThreshold))
	{
		Bias.HardAngleThreshold = FMath::Clamp<float>(Bias.HardAngleThreshold, -180.0f, 180.0f);
	}

	if (FParse::Value(*Entry, TEXT("SilhouetteImportanceBias="), Importance))
	{
		Bias.SilhouetteImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, -EMeshFeatureImportance::Highest, EMeshFeatureImportance::Highest);
	}

	if (FParse::Value(*Entry, TEXT("TextureImportanceBias="), Importance))
	{
		Bias.TextureImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, -EMeshFeatureImportance::Highest, EMeshFeatureImportance::Highest);
	}

	if (FParse::Value(*Entry, TEXT("ShadingImportanceBias="), Importance))
	{
		Bias.ShadingImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(Importance, -EMeshFeatureImportance::Highest, EMeshFeatureImportance::Highest);
	}
}

void FStaticMeshLODSettings::GetLODGroupNames(TArray<FName>& OutNames) const
{
	for (TMap<FName,FStaticMeshLODGroup>::TConstIterator It(Groups); It; ++It)
	{
		OutNames.Add(It.Key());
	}
}

void FStaticMeshLODSettings::GetLODGroupDisplayNames(TArray<FText>& OutDisplayNames) const
{
	for (TMap<FName,FStaticMeshLODGroup>::TConstIterator It(Groups); It; ++It)
	{
		OutDisplayNames.Add( It.Value().DisplayName );
	}
}

FMeshReductionSettings FStaticMeshLODGroup::GetSettings(const FMeshReductionSettings& InSettings, int32 LODIndex) const
{
	check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);

	FMeshReductionSettings FinalSettings = InSettings;

	// PercentTriangles is actually a multiplier.
	float PercentTrianglesMult = (LODIndex == 0) ? BasePercentTrianglesMult : SettingsBias.PercentTriangles;
	FinalSettings.PercentTriangles = FMath::Clamp(InSettings.PercentTriangles * PercentTrianglesMult, 0.0f, 1.0f);

	// Bias the remaining settings.
	FinalSettings.MaxDeviation = FMath::Max(InSettings.MaxDeviation + SettingsBias.MaxDeviation, 0.0f);
	FinalSettings.WeldingThreshold = FMath::Max(InSettings.WeldingThreshold + SettingsBias.WeldingThreshold, 0.0f);
	FinalSettings.HardAngleThreshold = FMath::Clamp(InSettings.HardAngleThreshold + SettingsBias.HardAngleThreshold, 0.0f, 180.0f);
	FinalSettings.SilhouetteImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(InSettings.SilhouetteImportance + SettingsBias.SilhouetteImportance, EMeshFeatureImportance::Off, EMeshFeatureImportance::Highest);
	FinalSettings.TextureImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(InSettings.TextureImportance + SettingsBias.TextureImportance, EMeshFeatureImportance::Off, EMeshFeatureImportance::Highest);
	FinalSettings.ShadingImportance = (EMeshFeatureImportance::Type)FMath::Clamp<int32>(InSettings.ShadingImportance + SettingsBias.ShadingImportance, EMeshFeatureImportance::Off, EMeshFeatureImportance::Highest);
	return FinalSettings;
}

void UStaticMesh::GetLODGroups(TArray<FName>& OutLODGroups)
{
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);
	RunningPlatform->GetStaticMeshLODSettings().GetLODGroupNames(OutLODGroups);
}

void UStaticMesh::GetLODGroupsDisplayNames(TArray<FText>& OutLODGroupsDisplayNames)
{
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);
	RunningPlatform->GetStaticMeshLODSettings().GetLODGroupDisplayNames(OutLODGroupsDisplayNames);
}

/*------------------------------------------------------------------------------
	FStaticMeshRenderData
------------------------------------------------------------------------------*/

FArchive& operator<<(FArchive& Ar, FMeshReductionSettings& ReductionSettings)
{
	Ar << ReductionSettings.PercentTriangles;
	Ar << ReductionSettings.MaxDeviation;
	Ar << ReductionSettings.WeldingThreshold;
	Ar << ReductionSettings.HardAngleThreshold;
	Ar << ReductionSettings.SilhouetteImportance;
	Ar << ReductionSettings.TextureImportance;
	Ar << ReductionSettings.ShadingImportance;
	Ar << ReductionSettings.bRecalculateNormals;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FMeshBuildSettings& BuildSettings)
{
	// Note: this serializer is currently only used to build the mesh DDC key, no versioning is required
	Ar << BuildSettings.bRecomputeNormals;
	Ar << BuildSettings.bRecomputeTangents;
	Ar << BuildSettings.bUseMikkTSpace;
	Ar << BuildSettings.bRemoveDegenerates;
	Ar << BuildSettings.bBuildAdjacencyBuffer;
	Ar << BuildSettings.bBuildReversedIndexBuffer;
	Ar << BuildSettings.bUseHighPrecisionTangentBasis;
	Ar << BuildSettings.bUseFullPrecisionUVs;
	Ar << BuildSettings.bGenerateLightmapUVs;

	Ar << BuildSettings.MinLightmapResolution;
	Ar << BuildSettings.SrcLightmapIndex;
	Ar << BuildSettings.DstLightmapIndex;

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_BUILD_SCALE_VECTOR)
	{
		float BuildScale(1.0f);
		Ar << BuildScale;
		BuildSettings.BuildScale3D = FVector( BuildScale );
	}
	else
	{
		Ar << BuildSettings.BuildScale3D;
	}
	
	Ar << BuildSettings.DistanceFieldResolutionScale;
	Ar << BuildSettings.bGenerateDistanceFieldAsIfTwoSided;

	if (BuildSettings.DistanceFieldReplacementMesh)
	{
		FString ReplacementMeshName = BuildSettings.DistanceFieldReplacementMesh->GetPathName();
		Ar << ReplacementMeshName;
	}

	return Ar;
}

// If static mesh derived data needs to be rebuilt (new format, serialization
// differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID
// and set this new GUID as the version.                                       
#define STATICMESH_DERIVEDDATA_VER TEXT("6F4494992CB61A37E4B1403C6156E6A")

static const FString& GetStaticMeshDerivedDataVersion()
{
	static FString CachedVersionString;
	if (CachedVersionString.IsEmpty())
	{
		// Static mesh versioning is controlled by the version reported by the mesh utilities module.
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
		CachedVersionString = FString::Printf(TEXT("%s_%s"),
			STATICMESH_DERIVEDDATA_VER,
			*MeshUtilities.GetVersionString()
			);
	}
	return CachedVersionString;
}

class FStaticMeshStatusMessageContext : public FScopedSlowTask
{
public:
	explicit FStaticMeshStatusMessageContext(const FText& InMessage)
		: FScopedSlowTask(0, InMessage)
	{
		UE_LOG(LogStaticMesh,Log,TEXT("%s"),*InMessage.ToString());
		MakeDialog();
	}
};

namespace StaticMeshDerivedDataTimings
{
	int64 GetCycles = 0;
	int64 BuildCycles = 0;
	int64 ConvertCycles = 0;

	static void DumpTimings()
	{
		UE_LOG(LogStaticMesh,Log,TEXT("Derived Data Times: Get=%.3fs Build=%.3fs ConvertLegacy=%.3fs"),
			FPlatformTime::ToSeconds(GetCycles),
			FPlatformTime::ToSeconds(BuildCycles),
			FPlatformTime::ToSeconds(ConvertCycles)
			);
	}

	static FAutoConsoleCommand DumpTimingsCmd(
		TEXT("sm.DerivedDataTimings"),
		TEXT("Dumps derived data timings to the log."),
		FConsoleCommandDelegate::CreateStatic(DumpTimings)
		);
}

static FString BuildStaticMeshDerivedDataKey(UStaticMesh* Mesh, const FStaticMeshLODGroup& LODGroup)
{
	FString KeySuffix(TEXT(""));
	TArray<uint8> TempBytes;
	TempBytes.Reserve(64);

	int32 NumLODs = Mesh->SourceModels.Num();
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = Mesh->SourceModels[LODIndex];
		KeySuffix += SrcModel.RawMeshBulkData->GetIdString();

		// Serialize the build and reduction settings into a temporary array. The archive
		// is flagged as persistent so that machines of different endianness produce
		// identical binary results.
		TempBytes.Reset();
		FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
		Ar << SrcModel.BuildSettings;

		FMeshReductionSettings FinalReductionSettings = LODGroup.GetSettings(SrcModel.ReductionSettings, LODIndex);
		Ar << FinalReductionSettings;

		// Now convert the raw bytes to a string.
		const uint8* SettingsAsBytes = TempBytes.GetData();
		KeySuffix.Reserve(KeySuffix.Len() + TempBytes.Num() + 1);
		for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
		{
			ByteToHex(SettingsAsBytes[ByteIndex], KeySuffix);
		}
	}

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("STATICMESH"),
		*GetStaticMeshDerivedDataVersion(),
		*KeySuffix
		);
}

static FString BuildDistanceFieldDerivedDataKey(const FString& InMeshKey)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.MaxPerMeshResolution"));
	const int32 PerMeshMax = CVar->GetValueOnAnyThread();
	const FString PerMeshMaxString = PerMeshMax == 128 ? TEXT("") : FString(TEXT("_%u"), PerMeshMax);

	static const auto CVarDensity = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.DistanceFields.DefaultVoxelDensity"));
	const float VoxelDensity = CVarDensity->GetValueOnAnyThread();
	const FString VoxelDensityString = VoxelDensity == .1f ? TEXT("") : FString(TEXT("_%.3f"), VoxelDensity);

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("DIST"),
		*FString::Printf(TEXT("%s_%s%s%s"), *InMeshKey, DISTANCEFIELD_DERIVEDDATA_VER, *PerMeshMaxString, *VoxelDensityString),
		TEXT(""));
}

void FStaticMeshRenderData::Cache(UStaticMesh* Owner, const FStaticMeshLODSettings& LODSettings)
{
	if (Owner->GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
	{
		// Don't cache for cooked packages
		return;
	}


	{
		COOK_STAT(auto Timer = StaticMeshCookStats::UsageStats.TimeSyncWork());
		int32 T0 = FPlatformTime::Cycles();
		int32 NumLODs = Owner->SourceModels.Num();
		const FStaticMeshLODGroup& LODGroup = LODSettings.GetLODGroup(Owner->LODGroup);
		DerivedDataKey = BuildStaticMeshDerivedDataKey(Owner, LODGroup);

		TArray<uint8> DerivedData;
		if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData))
		{
			COOK_STAT(Timer.AddHit(DerivedData.Num()));
			FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
			Serialize(Ar, Owner, /*bCooked=*/ false);

			int32 T1 = FPlatformTime::Cycles();
			UE_LOG(LogStaticMesh,Verbose,TEXT("Static mesh found in DDC [%fms] %s"),
				FPlatformTime::ToMilliseconds(T1-T0),
				*Owner->GetPathName()
				);
			FPlatformAtomics::InterlockedAdd(&StaticMeshDerivedDataTimings::GetCycles, T1 - T0);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("StaticMeshName"), FText::FromString( Owner->GetName() ) );
			FStaticMeshStatusMessageContext StatusContext( FText::Format( NSLOCTEXT("Engine", "BuildingStaticMeshStatus", "Building static mesh {StaticMeshName}..."), Args ) );

			IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
			MeshUtilities.BuildStaticMesh(*this, Owner->SourceModels, LODGroup);
			bLODsShareStaticLighting = Owner->CanLODsShareStaticLighting();
			FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
			Serialize(Ar, Owner, /*bCooked=*/ false);
			GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData);

			int32 T1 = FPlatformTime::Cycles();
			UE_LOG(LogStaticMesh,Log,TEXT("Built static mesh [%.2fs] %s"),
				FPlatformTime::ToMilliseconds(T1-T0) / 1000.0f,
				*Owner->GetPathName()
				);
			FPlatformAtomics::InterlockedAdd(&StaticMeshDerivedDataTimings::BuildCycles, T1 - T0);
			COOK_STAT(Timer.AddMiss(DerivedData.Num()));
		}
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));

	if (CVar->GetValueOnGameThread() != 0)
	{
		FString DistanceFieldKey = BuildDistanceFieldDerivedDataKey(DerivedDataKey);
		
		if (!LODResources[0].DistanceFieldData)
		{
			LODResources[0].DistanceFieldData = new FDistanceFieldVolumeData();
		}

		const FMeshBuildSettings& BuildSettings = Owner->SourceModels[0].BuildSettings;
		UStaticMesh* MeshToGenerateFrom = BuildSettings.DistanceFieldReplacementMesh ? BuildSettings.DistanceFieldReplacementMesh : Owner;

		if (BuildSettings.DistanceFieldReplacementMesh)
		{
			// Make sure dependency is postloaded
			BuildSettings.DistanceFieldReplacementMesh->ConditionalPostLoad();
		}

		LODResources[0].DistanceFieldData->CacheDerivedData(DistanceFieldKey, Owner, MeshToGenerateFrom, BuildSettings.DistanceFieldResolutionScale, BuildSettings.bGenerateDistanceFieldAsIfTwoSided);
	}
}
#endif // #if WITH_EDITOR

/*-----------------------------------------------------------------------------
UStaticMesh
-----------------------------------------------------------------------------*/

UStaticMesh::UStaticMesh(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	ElementToIgnoreForTexFactor = -1;
	StreamingDistanceMultiplier=1.0f;
	bHasNavigationData=true;
#if WITH_EDITORONLY_DATA
	AutoLODPixelError = 1.0f;
	bAutoComputeLODScreenSize=true;
#endif // #if WITH_EDITORONLY_DATA
	LightMapResolution = 4;
	LpvBiasMultiplier = 1.0f;
	MinLOD = 0;
}

void UStaticMesh::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

/**
 * Initializes the static mesh's render resources.
 */
void UStaticMesh::InitResources()
{
	if (RenderData)
	{
		RenderData->InitResources(this);
	}

#if	STATS
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateMemoryStats,
		UStaticMesh*, This, this,
		{
 			const uint32 StaticMeshResourceSize = This->GetResourceSize( EResourceSizeMode::Exclusive );
 			INC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory, StaticMeshResourceSize );
 			INC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory2, StaticMeshResourceSize );
		} );
#endif // STATS
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
SIZE_T UStaticMesh::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;
	if (RenderData)
	{
		ResourceSize += RenderData->GetResourceSize();
	}
	if (Mode == EResourceSizeMode::Inclusive)
	{
		TSet<UMaterialInterface*> UniqueMaterials;
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			UMaterialInterface* Material = Materials[MaterialIndex];
			bool bAlreadyCounted = false;
			UniqueMaterials.Add(Material,&bAlreadyCounted);
			if (!bAlreadyCounted && Material)
			{
				ResourceSize += Material->GetResourceSize(Mode);
			}
		}

		if(BodySetup)
		{
			ResourceSize += BodySetup->GetResourceSize(Mode);
		}
	}
	return ResourceSize;
}

SIZE_T FStaticMeshRenderData::GetResourceSize() const
{
	SIZE_T ResourceSize = sizeof(*this);

	// Count dynamic arrays.
	ResourceSize += LODResources.GetAllocatedSize();
#if WITH_EDITORONLY_DATA
	ResourceSize += DerivedDataKey.GetAllocatedSize();
	ResourceSize += WedgeMap.GetAllocatedSize();
#endif // #if WITH_EDITORONLY_DATA

	for(int32 LODIndex = 0;LODIndex < LODResources.Num();LODIndex++)
	{
		const FStaticMeshLODResources& LODRenderData = LODResources[LODIndex];

		const int32 VBSize = LODRenderData.VertexBuffer.GetStride()	* LODRenderData.VertexBuffer.GetNumVertices() + 
			LODRenderData.PositionVertexBuffer.GetStride()			* LODRenderData.PositionVertexBuffer.GetNumVertices() + 
			LODRenderData.ColorVertexBuffer.GetStride()				* LODRenderData.ColorVertexBuffer.GetNumVertices();
		const int32 IBSize = LODRenderData.IndexBuffer.GetAllocatedSize()
			+ LODRenderData.WireframeIndexBuffer.GetAllocatedSize()
			+ (RHISupportsTessellation(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) ? LODRenderData.AdjacencyIndexBuffer.GetAllocatedSize() : 0);

		ResourceSize += VBSize + IBSize;
		ResourceSize += LODRenderData.Sections.GetAllocatedSize();

		if (LODRenderData.DistanceFieldData)
		{
			ResourceSize += LODRenderData.DistanceFieldData->GetResourceSize();
		}
	}

#if WITH_EDITORONLY_DATA
	// If render data for multiple platforms is loaded, count it all.
	if (NextCachedRenderData)
	{
		ResourceSize += NextCachedRenderData->GetResourceSize();
	}
#endif // #if WITH_EDITORONLY_DATA

	return ResourceSize;
}

int32 UStaticMesh::GetNumVertices(int32 LODIndex) const
{
	int32 NumVertices = 0;
	if (RenderData && RenderData->LODResources.IsValidIndex(LODIndex))
	{
		NumVertices = RenderData->LODResources[LODIndex].VertexBuffer.GetNumVertices();
	}
	return NumVertices;
}

int32 UStaticMesh::GetNumLODs() const
{
	int32 NumLODs = 0;
	if (RenderData)
	{
		NumLODs = RenderData->LODResources.Num();
	}
	return NumLODs;
}

bool UStaticMesh::HasValidRenderData() const
{
	return RenderData != NULL
		&& RenderData->LODResources.Num() > 0
		&& RenderData->LODResources.GetData() != NULL
		&& RenderData->LODResources[0].VertexBuffer.GetNumVertices() > 0;
}

FBoxSphereBounds UStaticMesh::GetBounds() const
{
	return ExtendedBounds;
}

FBox UStaticMesh::GetBoundingBox() const
{
	return ExtendedBounds.GetBox();
}

int32 UStaticMesh::GetNumSections(int32 InLOD) const
{
	int32 NumSections = 0;
	if (RenderData != NULL && RenderData->LODResources.IsValidIndex(InLOD))
	{
		const FStaticMeshLODResources& LOD = RenderData->LODResources[InLOD];
		NumSections = LOD.Sections.Num();
	}
	return NumSections;
}

float UStaticMesh::GetStreamingTextureFactor(int32 RequestedUVIndex) const
{
	check(RequestedUVIndex >= 0);
	check(RequestedUVIndex < MAX_STATIC_TEXCOORDS);

	float StreamingTextureFactor = 0.0f;
	if (RenderData)
	{
		if( bUseMaximumStreamingTexelRatio )
		{
			StreamingTextureFactor = RenderData->MaxStreamingTextureFactor * FMath::Max(0.0f, StreamingDistanceMultiplier);
		}
		else if( RequestedUVIndex == 0 )
		{
			StreamingTextureFactor = RenderData->StreamingTextureFactors[RequestedUVIndex] * FMath::Max(0.0f, StreamingDistanceMultiplier);
		}
		else
		{
			StreamingTextureFactor = RenderData->StreamingTextureFactors[RequestedUVIndex];
		}
	}
	return StreamingTextureFactor;
}

bool UStaticMesh::GetStreamingTextureFactor(float& OutTexelFactor, FBoxSphereBounds& OutBounds, int32 CoordinateIndex, int32 LODIndex, int32 ElementIndex, const FTransform& Transform) const 
{
#if WITH_EDITORONLY_DATA
	if (!GIsEditor || !RenderData || !RenderData->LODResources.IsValidIndex(LODIndex))
	{
		return false;
	}

	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

	if (CoordinateIndex < 0 || CoordinateIndex >= LODModel.GetNumTexCoords())
	{
		return false;
	}

	if (!LODModel.Sections.IsValidIndex(ElementIndex))
	{
		return false;
	}

	struct FTriangleInfo
	{
		FTriangleInfo(float InAera, float InTexelRatio) : Aera(InAera), TexelRatio(InTexelRatio) {}
		float Aera;
		float TexelRatio;
	};

	struct FCompareAera
	{
		FORCEINLINE bool operator()(FTriangleInfo const& A, FTriangleInfo const& B) const { return A.Aera < B.Aera; }
	};

	struct FCompareTexelRatio
	{
		FORCEINLINE bool operator()(FTriangleInfo const& A, FTriangleInfo const& B) const { return A.TexelRatio < B.TexelRatio; }
	};

	TArray<FTriangleInfo> TriangleInfos;

	const FStaticMeshSection& SectionInfo = LODModel.Sections[ElementIndex];
	const int32 SectionIndexCount = SectionInfo.NumTriangles * 3;
	FIndexArrayView IndexBuffer = LODModel.IndexBuffer.GetArrayView();
	
	FBox TransformedSectionBox(ForceInit);

	for (uint32 TriangleIndex = 0; TriangleIndex < SectionInfo.NumTriangles; ++TriangleIndex)
	{
		const int32 Index0 = IndexBuffer[SectionInfo.FirstIndex + TriangleIndex * 3 + 0];
		const int32 Index1 = IndexBuffer[SectionInfo.FirstIndex + TriangleIndex * 3 + 1];
		const int32 Index2 = IndexBuffer[SectionInfo.FirstIndex + TriangleIndex * 3 + 2];

		FVector Pos0 = Transform.TransformPosition(LODModel.PositionVertexBuffer.VertexPosition(Index0));
		FVector Pos1 = Transform.TransformPosition(LODModel.PositionVertexBuffer.VertexPosition(Index1));
		FVector Pos2 = Transform.TransformPosition(LODModel.PositionVertexBuffer.VertexPosition(Index2));

		TransformedSectionBox += Pos0;
		TransformedSectionBox += Pos1;
		TransformedSectionBox += Pos2;

		FVector2D UV0 = LODModel.VertexBuffer.GetVertexUV(Index0, CoordinateIndex);
		FVector2D UV1 = LODModel.VertexBuffer.GetVertexUV(Index1, CoordinateIndex);
		FVector2D UV2 = LODModel.VertexBuffer.GetVertexUV(Index2, CoordinateIndex);

		FVector P01 = Pos1 - Pos0;
		FVector P02 = Pos2 - Pos0;

		float Aera = FVector::CrossProduct(P01, P02).Size();

		if (Aera > SMALL_NUMBER)
		{
			float L1 = P01.Size();
			float L2 = P02.Size();

			float T1 = (UV1 - UV0).Size();
			float T2 = (UV2 - UV0).Size();

			if (T1 > SMALL_NUMBER && T2 > SMALL_NUMBER)
			{
				float TexelRatio = FMath::Max(L1 / T1, L2 / T2);
				TriangleInfos.Push(FTriangleInfo(Aera, TexelRatio));
			}
		}
	}

	TriangleInfos.Sort(FCompareTexelRatio());

	float WeightedTexelFactorSum = 0;
	float AreaSum = 0;

	// Remove 10% of higher and lower texel factors.
	int32 Threshold = FMath::FloorToInt(.10f * (float)TriangleInfos.Num());
	for (int32 Index = Threshold; Index < TriangleInfos.Num() - Threshold; ++Index)
	{
		WeightedTexelFactorSum += TriangleInfos[Index].TexelRatio * TriangleInfos[Index].Aera;
		AreaSum += TriangleInfos[Index].Aera;
	}

	if (AreaSum == 0)
	{
		return false;
	}

	OutBounds = TransformedSectionBox;
	OutTexelFactor = WeightedTexelFactorSum / AreaSum; 
	// Don't take into account StreamingDistanceMultiplier here (but rather in the components using it). That allows realtime feedback without requiring a TextureStreamingBuild.
	return true;

#else
	return false;
#endif
}	


/**
 * Releases the static mesh's render resources.
 */
void UStaticMesh::ReleaseResources()
{
#if STATS
	uint32 StaticMeshResourceSize = GetResourceSize(EResourceSizeMode::Exclusive);
	DEC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory, StaticMeshResourceSize );
	DEC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory2, StaticMeshResourceSize );
#endif

	if (RenderData)
	{
		RenderData->ReleaseResources();
	}

	// insert a fence to signal when these commands completed
	ReleaseResourcesFence.BeginFence();
}

#if WITH_EDITOR
void UStaticMesh::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// Release the static mesh's resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	ReleaseResourcesFence.Wait();
}

void UStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, bHasNavigationData) && !bHasNavigationData)
	{
		NavCollision = nullptr;
	}

#if WITH_EDITORONLY_DATA
	LightMapResolution = FMath::Max(LightMapResolution, 0);

	if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("StreamingDistanceMultiplier") )
	{
		GEngine->TriggerStreamingDataRebuild();
	}

	if (PropertyChangedEvent.MemberProperty && ( (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, PositiveBoundsExtension) ) || ( PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, NegativeBoundsExtension) ) ))
	{
		// Update the extended bounds
		CalculateExtendedBounds();
	}
	AutoLODPixelError = FMath::Max(AutoLODPixelError, 1.0f);

	if (!bAutoComputeLODScreenSize
		&& RenderData
		&& PropertyThatChanged
		&& PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, bAutoComputeLODScreenSize))
		{
		for (int32 LODIndex = 1; LODIndex < SourceModels.Num(); ++LODIndex)
		{
			SourceModels[LODIndex].ScreenSize = RenderData->ScreenSize[LODIndex];
		}
	}

	EnforceLightmapRestrictions();

	Build(/*bSilent=*/ true);

	// Only unbuild lighting for properties which affect static lighting
	if (!PropertyThatChanged 
		|| PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, LightMapResolution)
		|| PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMesh, LightMapCoordinateIndex))
	{
		FStaticMeshComponentRecreateRenderStateContext Context(this, true);		
		SetLightingGuid();
	}
#endif // #if WITH_EDITORONLY_DATA
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UStaticMesh::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

bool UStaticMesh::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.IsFenceComplete();
}

int32 UStaticMesh::GetNumSectionsWithCollision() const
{
#if WITH_EDITORONLY_DATA
	int32 NumSectionsWithCollision = 0;

	if (RenderData && RenderData->LODResources.Num() > 0)
	{
		// Find how many sections have collision enabled
		const int32 UseLODIndex = FMath::Clamp(LODForCollision, 0, RenderData->LODResources.Num() - 1);
		const FStaticMeshLODResources& CollisionLOD = RenderData->LODResources[UseLODIndex];
		for (int32 SectionIndex = 0; SectionIndex < CollisionLOD.Sections.Num(); ++SectionIndex)
		{
			if (SectionInfoMap.Get(UseLODIndex, SectionIndex).bEnableCollision)
			{
				NumSectionsWithCollision++;
			}
		}
	}

	return NumSectionsWithCollision;
#else
	return 0;
#endif
}

void UStaticMesh::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	int32 NumTriangles = 0;
	int32 NumVertices = 0;
	int32 NumUVChannels = 0;
	int32 NumLODs = 0;

	if (RenderData && RenderData->LODResources.Num() > 0)
	{
		const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
		NumTriangles = LOD.IndexBuffer.GetNumIndices() / 3;
		NumVertices = LOD.VertexBuffer.GetNumVertices();
		NumUVChannels = LOD.VertexBuffer.GetNumTexCoords();
		NumLODs = RenderData->LODResources.Num();
	}

	int32 NumSectionsWithCollision = GetNumSectionsWithCollision();

	int32 NumCollisionPrims = 0;
	if ( BodySetup != NULL )
	{
		NumCollisionPrims = BodySetup->AggGeom.GetElementCount();
	}

	FBoxSphereBounds Bounds(ForceInit);
	if (RenderData)
	{
		Bounds = RenderData->Bounds;
	}
	const FString ApproxSizeStr = FString::Printf(TEXT("%dx%dx%d"), FMath::RoundToInt(Bounds.BoxExtent.X * 2.0f), FMath::RoundToInt(Bounds.BoxExtent.Y * 2.0f), FMath::RoundToInt(Bounds.BoxExtent.Z * 2.0f));

	// Get name of default collision profile
	FName DefaultCollisionName = NAME_None;
	if(BodySetup != nullptr)
	{
		DefaultCollisionName = BodySetup->DefaultInstance.GetCollisionProfileName();
	}

	OutTags.Add( FAssetRegistryTag("Triangles", FString::FromInt(NumTriangles), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("Vertices", FString::FromInt(NumVertices), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("UVChannels", FString::FromInt(NumUVChannels), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("Materials", FString::FromInt(Materials.Num()), FAssetRegistryTag::TT_Numerical) );
	OutTags.Add( FAssetRegistryTag("ApproxSize", ApproxSizeStr, FAssetRegistryTag::TT_Dimensional) );
	OutTags.Add( FAssetRegistryTag("CollisionPrims", FString::FromInt(NumCollisionPrims), FAssetRegistryTag::TT_Numerical));
	OutTags.Add( FAssetRegistryTag("LODs", FString::FromInt(NumLODs), FAssetRegistryTag::TT_Numerical));
	OutTags.Add( FAssetRegistryTag("SectionsWithCollision", FString::FromInt(NumSectionsWithCollision), FAssetRegistryTag::TT_Numerical));
	OutTags.Add( FAssetRegistryTag("DefaultCollision", DefaultCollisionName.ToString(), FAssetRegistryTag::TT_Alphabetical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void UStaticMesh::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);

	OutMetadata.Add("CollisionPrims",
		FAssetRegistryTagMetadata()
			.SetTooltip(NSLOCTEXT("UStaticMesh", "CollisionPrimsTooltip", "The number of collision primitives in the static mesh"))
			.SetImportantValue(TEXT("0"))
		);
}
#endif


/*------------------------------------------------------------------------------
	FStaticMeshSourceModel
------------------------------------------------------------------------------*/

FStaticMeshSourceModel::FStaticMeshSourceModel()
{
#if WITH_EDITOR
	RawMeshBulkData = new FRawMeshBulkData();
	ScreenSize = 0.0f;
#endif // #if WITH_EDITOR
}

FStaticMeshSourceModel::~FStaticMeshSourceModel()
{
#if WITH_EDITOR
	if (RawMeshBulkData)
	{
		delete RawMeshBulkData;
		RawMeshBulkData = NULL;
	}
#endif // #if WITH_EDITOR
	}

#if WITH_EDITOR
void FStaticMeshSourceModel::SerializeBulkData(FArchive& Ar, UObject* Owner)
{
	check(RawMeshBulkData != NULL);
	RawMeshBulkData->Serialize(Ar, Owner);
}
#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	FMeshSectionInfoMap
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
	
bool operator==(const FMeshSectionInfo& A, const FMeshSectionInfo& B)
{
	return A.MaterialIndex == B.MaterialIndex
		&& A.bCastShadow == B.bCastShadow
		&& A.bEnableCollision == B.bEnableCollision;
}

bool operator!=(const FMeshSectionInfo& A, const FMeshSectionInfo& B)
{
	return !(A == B);
}
	
static uint32 GetMeshMaterialKey(int32 LODIndex, int32 SectionIndex)
{
	return ((LODIndex & 0xffff) << 16) | (SectionIndex & 0xffff);
}

void FMeshSectionInfoMap::Clear()
{
	Map.Empty();
}

FMeshSectionInfo FMeshSectionInfoMap::Get(int32 LODIndex, int32 SectionIndex) const
{
	uint32 Key = GetMeshMaterialKey(LODIndex, SectionIndex);
	const FMeshSectionInfo* InfoPtr = Map.Find(Key);
	if (InfoPtr == NULL)
	{
		Key = GetMeshMaterialKey(0, SectionIndex);
		InfoPtr = Map.Find(Key);
	}
	if (InfoPtr != NULL)
	{
		return *InfoPtr;
	}
	return FMeshSectionInfo(SectionIndex);
}

void FMeshSectionInfoMap::Set(int32 LODIndex, int32 SectionIndex, FMeshSectionInfo Info)
{
	uint32 Key = GetMeshMaterialKey(LODIndex, SectionIndex);
	Map.Add(Key, Info);
}

void FMeshSectionInfoMap::Remove(int32 LODIndex, int32 SectionIndex)
{
	uint32 Key = GetMeshMaterialKey(LODIndex, SectionIndex);
	Map.Remove(Key);
}

void FMeshSectionInfoMap::CopyFrom(const FMeshSectionInfoMap& Other)
{
	for (TMap<uint32,FMeshSectionInfo>::TConstIterator It(Other.Map); It; ++It)
	{
		Map.Add(It.Key(), It.Value());
	}
}

bool FMeshSectionInfoMap::AnySectionHasCollision() const
{
	for (TMap<uint32,FMeshSectionInfo>::TConstIterator It(Map); It; ++It)
	{
		uint32 Key = It.Key();
		int32 LODIndex = (int32)(Key >> 16);
		if (LODIndex == 0 && It.Value().bEnableCollision)
		{
			return true;
		}
	}
	return false;
}

FArchive& operator<<(FArchive& Ar, FMeshSectionInfo& Info)
{
	Ar << Info.MaterialIndex;
	Ar << Info.bEnableCollision;
	Ar << Info.bCastShadow;
	return Ar;
}

void FMeshSectionInfoMap::Serialize(FArchive& Ar)
{
	Ar << Map;
}

#endif // #if WITH_EDITORONLY_DATA

#if WITH_EDITOR
static FStaticMeshRenderData& GetPlatformStaticMeshRenderData(UStaticMesh* Mesh, const ITargetPlatform* Platform)
{
	check(Mesh && Mesh->RenderData);
	const FStaticMeshLODSettings& PlatformLODSettings = Platform->GetStaticMeshLODSettings();
	FString PlatformDerivedDataKey = BuildStaticMeshDerivedDataKey(Mesh, PlatformLODSettings.GetLODGroup(Mesh->LODGroup));
	FStaticMeshRenderData* PlatformRenderData = Mesh->RenderData;

	if (Mesh->GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
	{
		check(PlatformRenderData);
		return *PlatformRenderData;
	}

	while (PlatformRenderData && PlatformRenderData->DerivedDataKey != PlatformDerivedDataKey)
	{
		PlatformRenderData = PlatformRenderData->NextCachedRenderData;
	}
	if (PlatformRenderData == NULL)
	{
		// Cache render data for this platform and insert it in to the linked list.
		PlatformRenderData = new FStaticMeshRenderData();
		PlatformRenderData->Cache(Mesh, PlatformLODSettings);
		check(PlatformRenderData->DerivedDataKey == PlatformDerivedDataKey);
		PlatformRenderData->NextCachedRenderData.Swap(Mesh->RenderData->NextCachedRenderData);
		Mesh->RenderData->NextCachedRenderData = PlatformRenderData;
	}
	check(PlatformRenderData);
	return *PlatformRenderData;
}

void UStaticMesh::CacheDerivedData()
{
	// Cache derived data for the running platform.
	ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
	ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
	check(RunningPlatform);
	const FStaticMeshLODSettings& LODSettings = RunningPlatform->GetStaticMeshLODSettings();

	if (RenderData)
	{
		// Finish any previous async builds before modifying RenderData
		// This can happen during import as the mesh is rebuilt redundantly
		GDistanceFieldAsyncQueue->BlockUntilBuildComplete(this, true);

		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++LODIndex)
		{
			FDistanceFieldVolumeData* DistanceFieldData = RenderData->LODResources[LODIndex].DistanceFieldData;

			if (DistanceFieldData)
			{
				// Release before destroying RenderData
				DistanceFieldData->VolumeTexture.Release();
			}
		}
	}

	RenderData = new FStaticMeshRenderData();
	RenderData->Cache(this, LODSettings);

	// Additionally cache derived data for any other platforms we care about.
	const TArray<ITargetPlatform*>& TargetPlatforms = TargetPlatformManager.GetActiveTargetPlatforms();
	for (int32 PlatformIndex = 0; PlatformIndex < TargetPlatforms.Num(); ++PlatformIndex)
	{
		ITargetPlatform* Platform = TargetPlatforms[PlatformIndex];
		if (Platform != RunningPlatform)
		{
			GetPlatformStaticMeshRenderData(this, Platform);
		}
	}
}

#endif // #if WITH_EDITORONLY_DATA

void UStaticMesh::CalculateExtendedBounds()
{
	FBoxSphereBounds Bounds(ForceInit);
	if (RenderData)
	{
		Bounds = RenderData->Bounds;
	}

	// Only apply bound extension if necessary, as it will result in a larger bounding sphere radius than retrieved from the render data
	if (!NegativeBoundsExtension.IsZero() || !PositiveBoundsExtension.IsZero())
	{
		// Convert to Min and Max
		FVector Min = Bounds.Origin - Bounds.BoxExtent;
		FVector Max = Bounds.Origin + Bounds.BoxExtent;
		// Apply bound extensions
		Min -= NegativeBoundsExtension;
		Max += PositiveBoundsExtension;
		// Convert back to Origin, Extent and update SphereRadius
		Bounds.Origin = (Min + Max) / 2;
		Bounds.BoxExtent = (Max - Min) / 2;
		Bounds.SphereRadius = Bounds.BoxExtent.Size();
	}

	ExtendedBounds = Bounds;
}


#if WITH_EDITORONLY_DATA
FUObjectAnnotationSparseBool GStaticMeshesThatNeedMaterialFixup;
#endif // #if WITH_EDITORONLY_DATA

#if WITH_EDITOR
COREUOBJECT_API extern bool GOutputCookingWarnings;
#endif


/**
 *	UStaticMesh::Serialize
 */
void UStaticMesh::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("UStaticMesh::Serialize"), STAT_StaticMesh_Serialize, STATGROUP_LoadTime );

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	FStripDataFlags StripFlags( Ar );

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REMOVE_ZERO_TRIANGLE_SECTIONS)
	{
		GStaticMeshesThatNeedMaterialFixup.Set(this);
	}
#endif // #if WITH_EDITORONLY_DATA

	Ar << BodySetup;

	if (Ar.UE4Ver() >= VER_UE4_STATIC_MESH_STORE_NAV_COLLISION)
	{
		Ar << NavCollision;
#if WITH_EDITOR
		if ((BodySetup != nullptr) && 
			bHasNavigationData && 
			(NavCollision == nullptr))
		{
			if (Ar.IsPersistent() && Ar.IsLoading() && (Ar.GetDebugSerializationFlags() & DSF_EnableCookerWarnings))
			{
				UE_LOG(LogStaticMesh, Warning, TEXT("Serialized NavCollision but it was null (%s) NavCollision will be created dynamicaly at cook time.  Please resave package %s."), *GetName(), *GetOutermost()->GetPathName());
			}
		}
#endif
	}
#if WITH_EDITOR
	else if (bHasNavigationData && BodySetup && GOutputCookingWarnings)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("This StaticMeshes (%s) NavCollision will be created dynamicaly at cook time.  Please resave %s."), *GetName(), *GetOutermost()->GetPathName())
	}
#endif

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if(Ar.IsLoading() && Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::UseBodySetupCollisionProfile && BodySetup)
	{
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}

#if WITH_EDITORONLY_DATA
	if( !StripFlags.IsEditorDataStripped() )
	{
		if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_DEPRECATED_STATIC_MESH_THUMBNAIL_PROPERTIES_REMOVED )
		{
			FRotator DummyThumbnailAngle;
			float DummyThumbnailDistance;
			Ar << DummyThumbnailAngle;
			Ar << DummyThumbnailDistance;
		}
	}

	if( !StripFlags.IsEditorDataStripped() )
	{
		Ar << HighResSourceMeshName;
		Ar << HighResSourceMeshCRC;
	}
#endif // #if WITH_EDITORONLY_DATA

	if( Ar.IsCountingMemory() )
	{
		// Include collision as part of memory used
		if ( BodySetup )
		{
			BodySetup->Serialize( Ar );
		}

		if ( NavCollision )
		{
			NavCollision->Serialize( Ar );
		}

		//TODO: Count these members when calculating memory used
		//Ar << ReleaseResourcesFence;
	}

	Ar << LightingGuid;
	Ar << Sockets;

#if WITH_EDITOR
	if (!StripFlags.IsEditorDataStripped())
	{
		for (int32 i = 0; i < SourceModels.Num(); ++i)
		{
			FStaticMeshSourceModel& SrcModel = SourceModels[i];
			SrcModel.SerializeBulkData(Ar, this);
		}
		SectionInfoMap.Serialize(Ar);

		// Need to set a flag rather than do conversion in place as RenderData is not
		// created until postload and it is needed for bounding information
		bRequiresLODDistanceConversion = Ar.UE4Ver() < VER_UE4_STATIC_MESH_SCREEN_SIZE_LODS;
	}
#endif // #if WITH_EDITOR

	// Inline the derived data for cooked builds. Never include render data when
	// counting memory as it is included by GetResourceSize.
	if (bCooked && !IsTemplate() && !Ar.IsCountingMemory())
	{	
		if (Ar.IsLoading())
		{
			RenderData = new FStaticMeshRenderData();
			RenderData->Serialize(Ar, this, bCooked);
		}

#if WITH_EDITOR
		else if (Ar.IsSaving())
		{
			FStaticMeshRenderData& PlatformRenderData = GetPlatformStaticMeshRenderData(this, Ar.CookingTarget());
			PlatformRenderData.Serialize(Ar, this, bCooked);
		}
#endif
	}

	if (Ar.UE4Ver() >= VER_UE4_SPEEDTREE_STATICMESH)
	{
		bool bHasSpeedTreeWind = SpeedTreeWind.IsValid();
		Ar << bHasSpeedTreeWind;

		if (bHasSpeedTreeWind)
		{
			if (!SpeedTreeWind.IsValid())
			{
				SpeedTreeWind = TSharedPtr<FSpeedTreeWind>(new FSpeedTreeWind);
			}

			Ar << *SpeedTreeWind;
		}
	}

#if WITH_EDITORONLY_DATA
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !AssetImportData)
	{
		// AssetImportData should always be valid
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	
	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA && AssetImportData )
	{
		// AssetImportData should always have been set up in the constructor where this is relevant
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
		
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}
#endif // WITH_EDITORONLY_DATA
}

//
//	UStaticMesh::PostLoad
//
void UStaticMesh::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
	{
		// Needs to happen before 'CacheDerivedData'
		if (GetLinkerUE4Version() < VER_UE4_BUILD_SCALE_VECTOR)
		{
			int32 NumLODs = SourceModels.Num();
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
				SrcModel.BuildSettings.BuildScale3D = FVector(SrcModel.BuildSettings.BuildScale_DEPRECATED);
			}
		}

		if (GetLinkerUE4Version() < VER_UE4_LIGHTMAP_MESH_BUILD_SETTINGS)
		{
			for (int32 i = 0; i < SourceModels.Num(); i++)
			{
				SourceModels[i].BuildSettings.bGenerateLightmapUVs = false;
			}
		}

		if (GetLinkerUE4Version() < VER_UE4_MIKKTSPACE_IS_DEFAULT)
		{
			for (int32 i = 0; i < SourceModels.Num(); ++i)
			{
				SourceModels[i].BuildSettings.bUseMikkTSpace = true;
			}
		}

		if (GetLinkerUE4Version() < VER_UE4_BUILD_MESH_ADJ_BUFFER_FLAG_EXPOSED)
		{
			FRawMesh TempRawMesh;
			uint32 TotalIndexCount = 0;

			for (int32 i = 0; i < SourceModels.Num(); ++i)
			{
				FRawMeshBulkData* RawMeshBulkData = SourceModels[i].RawMeshBulkData;
				if (RawMeshBulkData)
				{
					RawMeshBulkData->LoadRawMesh(TempRawMesh);
					TotalIndexCount += TempRawMesh.WedgeIndices.Num();
				}
			}

			for (int32 i = 0; i < SourceModels.Num(); ++i)
			{
				SourceModels[i].BuildSettings.bBuildAdjacencyBuffer = (TotalIndexCount < 50000);
			}
		}

		CacheDerivedData();
		
		// Only required in an editor build as other builds process this in a different place
		if (bRequiresLODDistanceConversion)
		{
			// Convert distances to Display Factors
			ConvertLegacyLODDistance();
		}

		if (RenderData && GStaticMeshesThatNeedMaterialFixup.Get(this))
		{
			FixupZeroTriangleSections();
		}
	}
#endif // #if WITH_EDITOR

	EnforceLightmapRestrictions();

	if (!GVertexElementTypeSupport.IsSupported(VET_Half2))
	{
		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++LODIndex)
		{
			if (RenderData->LODResources.IsValidIndex(LODIndex))
			{
				FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
				
				SELECT_STATIC_MESH_VERTEX_TYPE(
					LOD.VertexBuffer.GetUseHighPrecisionTangentBasis(),
					LOD.VertexBuffer.GetUseFullPrecisionUVs(),
					LOD.VertexBuffer.GetNumTexCoords(),
					{
						typedef TStaticMeshFullVertex<VertexType::TangentBasisType, EStaticMeshVertexUVType::HighPrecision, VertexType::NumTexCoords> DstVertexType;
						LOD.VertexBuffer.ConvertVertexFormat<VertexType, DstVertexType>();
					});
			}
		}
	}

	if( FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		InitResources();
	}

#if WITH_EDITOR
	if (GetLinkerUE4Version() < VER_UE4_STATIC_MESH_EXTENDED_BOUNDS)
	{
		CalculateExtendedBounds();
	}

	// New fix for incorrect extended bounds
	const int32 CustomVersion = GetLinkerCustomVersion(FReleaseObjectVersion::GUID);
	if (CustomVersion < FReleaseObjectVersion::StaticMeshExtendedBoundsFix)
	{
		CalculateExtendedBounds();
	}
#endif // #if WITH_EDITOR

	// We want to always have a BodySetup, its used for per-poly collision as well
	if(BodySetup == NULL)
	{
		CreateBodySetup();
	}

	if (NavCollision == nullptr && bHasNavigationData)
	{
		CreateNavCollision();
	}
	else if (NavCollision && !bHasNavigationData)
	{
		NavCollision = nullptr;
	}
}

//
//	UStaticMesh::GetDesc
//

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UStaticMesh::GetDesc()
{
	int32 NumTris = 0;
	int32 NumVerts = 0;
	int32 NumLODs = RenderData ? RenderData->LODResources.Num() : 0;
	if (NumLODs > 0)
	{
		NumTris = RenderData->LODResources[0].GetNumTriangles();
		NumVerts = RenderData->LODResources[0].GetNumVertices();
	}
	return FString::Printf(
		TEXT("%d LODs, %d Tris, %d Verts"),
		NumLODs,
		NumTris,
		NumVerts
		);
}


static int32 GetCollisionVertIndexForMeshVertIndex(int32 MeshVertIndex, TMap<int32, int32>& MeshToCollisionVertMap, TArray<FVector>& OutPositions, TArray< TArray<FVector2D> >& OutUVs, FPositionVertexBuffer& InPosVertBuffer, FStaticMeshVertexBuffer& InVertBuffer)
{
	int32* CollisionIndexPtr = MeshToCollisionVertMap.Find(MeshVertIndex);
	if (CollisionIndexPtr != nullptr)
	{
		return *CollisionIndexPtr;
	}
	else
	{
		// Copy UVs for vert if desired
		for (int32 ChannelIdx = 0; ChannelIdx < OutUVs.Num(); ChannelIdx++)
		{
			check(OutPositions.Num() == OutUVs[ChannelIdx].Num());
			OutUVs[ChannelIdx].Add(InVertBuffer.GetVertexUV(MeshVertIndex, ChannelIdx));
		}

		// Copy position
		int32 CollisionVertIndex = OutPositions.Add(InPosVertBuffer.VertexPosition(MeshVertIndex));

		// Add indices to map
		MeshToCollisionVertMap.Add(MeshVertIndex, CollisionVertIndex);

		return CollisionVertIndex;
	}
}

bool UStaticMesh::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool bInUseAllTriData)
{
#if WITH_EDITORONLY_DATA
	check(HasValidRenderData());

	// Get the LOD level to use for collision
	// Always use 0 if asking for 'all tri data'
	const int32 UseLODIndex = bInUseAllTriData ? 0 : FMath::Clamp(LODForCollision, 0, RenderData->LODResources.Num()-1);

	FStaticMeshLODResources& LOD = RenderData->LODResources[UseLODIndex];
	FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

	TMap<int32, int32> MeshToCollisionVertMap; // map of static mesh verts to collision verts

	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; // See if we should copy UVs

	// If copying UVs, allocate array for storing them
	if (bCopyUVs)
	{
		CollisionData->UVs.AddZeroed(LOD.GetNumTexCoords());
	}

	for(int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

		if (bInUseAllTriData || SectionInfoMap.Get(UseLODIndex,SectionIndex).bEnableCollision)
		{
			const uint32 OnePastLastIndex  = Section.FirstIndex + Section.NumTriangles*3;

			for (uint32 TriIdx = Section.FirstIndex; TriIdx < OnePastLastIndex; TriIdx += 3)
			{
				FTriIndices TriIndex;
				TriIndex.v0 = GetCollisionVertIndexForMeshVertIndex(Indices[TriIdx +0], MeshToCollisionVertMap, CollisionData->Vertices, CollisionData->UVs, LOD.PositionVertexBuffer, LOD.VertexBuffer);
				TriIndex.v1 = GetCollisionVertIndexForMeshVertIndex(Indices[TriIdx +1], MeshToCollisionVertMap, CollisionData->Vertices, CollisionData->UVs, LOD.PositionVertexBuffer, LOD.VertexBuffer);
				TriIndex.v2 = GetCollisionVertIndexForMeshVertIndex(Indices[TriIdx +2], MeshToCollisionVertMap, CollisionData->Vertices, CollisionData->UVs, LOD.PositionVertexBuffer, LOD.VertexBuffer);

				CollisionData->Indices.Add(TriIndex);
				CollisionData->MaterialIndices.Add(Section.MaterialIndex);
			}
		}
	}
	CollisionData->bFlipNormals = true;
	
	// We only have a valid TriMesh if the CollisionData has vertices AND indices. For meshes with disabled section collision, it
	// can happen that the indices will be empty, in which case we do not want to consider that as valid trimesh data
	return CollisionData->Vertices.Num() > 0 && CollisionData->Indices.Num() > 0;
#else // #if WITH_EDITORONLY_DATA
	return false;
#endif // #if WITH_EDITORONLY_DATA
}

bool UStaticMesh::ContainsPhysicsTriMeshData(bool bInUseAllTriData) const 
{
	if(RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return false;
	}

	// Get the LOD level to use for collision
	// Always use 0 if asking for 'all tri data'
	const int32 UseLODIndex = bInUseAllTriData ? 0 : FMath::Clamp(LODForCollision, 0, RenderData->LODResources.Num() - 1);

	if (RenderData->LODResources[UseLODIndex].PositionVertexBuffer.GetNumVertices() > 0)
	{
		// In non-cooked builds we need to look at the section info map to get
		// accurate per-section info.
#if WITH_EDITORONLY_DATA
		return bInUseAllTriData || SectionInfoMap.AnySectionHasCollision();
#else
		// Get the LOD level to use for collision
		FStaticMeshLODResources& LOD = RenderData->LODResources[UseLODIndex];
		for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
		{
			const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
			if ((bInUseAllTriData || Section.bEnableCollision) && Section.NumTriangles > 0)
			{
				return true;
			}
		}
#endif
	}
	return false; 
}

void UStaticMesh::GetMeshId(FString& OutMeshId)
{
#if WITH_EDITORONLY_DATA
	if (RenderData)
	{
		OutMeshId = RenderData->DerivedDataKey;
	}
#endif
}

void UStaticMesh::AddAssetUserData(UAssetUserData* InUserData)
{
	if(InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if(ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UStaticMesh::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UStaticMesh::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UStaticMesh::GetAssetUserDataArray() const 
{
	return &AssetUserData;
}

/**
 * Create BodySetup for this staticmesh 
 */
void UStaticMesh::CreateBodySetup()
{
	if (BodySetup==NULL)
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
}

void UStaticMesh::CreateNavCollision()
{
	if (NavCollision == NULL && BodySetup != NULL)
	{
		NavCollision = NewObject<UNavCollision>(this);
		NavCollision->Setup(BodySetup);
	}
}

/**
 * Returns vertex color data by position.
 * For matching to reimported meshes that may have changed or copying vertex paint data from mesh to mesh.
 *
 *	@param	VertexColorData		(out)A map of vertex position data and its color. The method fills this map.
 */
void UStaticMesh::GetVertexColorData(TMap<FVector, FColor>& VertexColorData)
{
	VertexColorData.Empty();
#if WITH_EDITOR
	// What LOD to get vertex colors from.  
	// Currently mesh painting only allows for painting on the first lod.
	const uint32 PaintingMeshLODIndex = 0;
	if (SourceModels.IsValidIndex(PaintingMeshLODIndex)
		&& SourceModels[PaintingMeshLODIndex].RawMeshBulkData->IsEmpty() == false)
	{
		// Extract the raw mesh.
		FRawMesh Mesh;
		SourceModels[PaintingMeshLODIndex].RawMeshBulkData->LoadRawMesh(Mesh);

		// Nothing to copy if there are no colors stored.
		if (Mesh.WedgeColors.Num() != 0 && Mesh.WedgeColors.Num() == Mesh.WedgeIndices.Num())
		{
			// Build a mapping of vertex positions to vertex colors.
			for (int32 WedgeIndex = 0; WedgeIndex < Mesh.WedgeIndices.Num(); ++WedgeIndex)
			{
				FVector Position = Mesh.VertexPositions[Mesh.WedgeIndices[WedgeIndex]];
				FColor Color = Mesh.WedgeColors[WedgeIndex];
				if (!VertexColorData.Contains(Position))
				{
					VertexColorData.Add(Position, Color);
				}
			}
		}	
	}
#endif // #if WITH_EDITORONLY_DATA
}

/**
 * Sets vertex color data by position.
 * Map of vertex color data by position is matched to the vertex position in the mesh
 * and nearest matching vertex color is used.
 *
 *	@param	VertexColorData		A map of vertex position data and color.
 */
void UStaticMesh::SetVertexColorData(const TMap<FVector, FColor>& VertexColorData)
{
#if WITH_EDITOR
	// What LOD to get vertex colors from.  
	// Currently mesh painting only allows for painting on the first lod.
	const uint32 PaintingMeshLODIndex = 0;
	if (SourceModels.IsValidIndex(PaintingMeshLODIndex)
		&& SourceModels[PaintingMeshLODIndex].RawMeshBulkData->IsEmpty() == false)
	{
		// Extract the raw mesh.
		FRawMesh Mesh;
		SourceModels[PaintingMeshLODIndex].RawMeshBulkData->LoadRawMesh(Mesh);

		// Reserve space for the new vertex colors.
		if (Mesh.WedgeColors.Num() == 0 || Mesh.WedgeColors.Num() != Mesh.WedgeIndices.Num())
		{
			Mesh.WedgeColors.Empty(Mesh.WedgeIndices.Num());
			Mesh.WedgeColors.AddUninitialized(Mesh.WedgeIndices.Num());
		}

		// Build a mapping of vertex positions to vertex colors.
		for (int32 WedgeIndex = 0; WedgeIndex < Mesh.WedgeIndices.Num(); ++WedgeIndex)
		{
			FVector Position = Mesh.VertexPositions[Mesh.WedgeIndices[WedgeIndex]];
			const FColor* Color = VertexColorData.Find(Position);
			if (Color)
			{
				Mesh.WedgeColors[WedgeIndex] = *Color;
			}
			else
			{
				Mesh.WedgeColors[WedgeIndex] = FColor(255,255,255,255);
			}
		}

		// Save the new raw mesh.
		SourceModels[PaintingMeshLODIndex].RawMeshBulkData->SaveRawMesh(Mesh);
	}
	// TODO_STATICMESH: Build?
#endif // #if WITH_EDITOR
}

void UStaticMesh::EnforceLightmapRestrictions()
{
	// Legacy content may contain a lightmap resolution of 0, which was valid when vertex lightmaps were supported, but not anymore with only texture lightmaps
	LightMapResolution = FMath::Max(LightMapResolution, 4);

	int32 NumUVs = 16;

	if (RenderData)
	{
		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++LODIndex)
		{
			NumUVs = FMath::Min(RenderData->LODResources[LODIndex].GetNumTexCoords(),NumUVs);
		}
	}
	else
	{
		NumUVs = 1;
	}

	// Clamp LightMapCoordinateIndex to be valid for all lightmap uvs
	LightMapCoordinateIndex = FMath::Clamp(LightMapCoordinateIndex, 0, NumUVs - 1);
}

/**
 * Static: Processes the specified static mesh for light map UV problems
 *
 * @param	InStaticMesh					Static mesh to process
 * @param	InOutAssetsWithMissingUVSets	Array of assets that we found with missing UV sets
 * @param	InOutAssetsWithBadUVSets		Array of assets that we found with bad UV sets
 * @param	InOutAssetsWithValidUVSets		Array of assets that we found with valid UV sets
 * @param	bInVerbose						If true, log the items as they are found
 */
void UStaticMesh::CheckLightMapUVs( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets, bool bInVerbose )
{
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);
	if (!bAllowStaticLighting)
	{
		// We do not need to check for lightmap UV problems when we do not allow static lighting
		return;
	}

	struct FLocal
	{
		/**
		 * Checks to see if a point overlaps a triangle
		 *
		 * @param	P	Point
		 * @param	A	First triangle vertex
		 * @param	B	Second triangle vertex
		 * @param	C	Third triangle vertex
		 *
		 * @return	true if the point overlaps the triangle
		 */
		bool IsPointInTriangle( const FVector& P, const FVector& A, const FVector& B, const FVector& C, const float Epsilon )
		{
			struct
			{
				bool SameSide( const FVector& P1, const FVector& P2, const FVector& InA, const FVector& InB, const float InEpsilon )
				{
					const FVector Cross1((InB - InA) ^ (P1 - InA));
					const FVector Cross2((InB - InA) ^ (P2 - InA));
					return (Cross1 | Cross2) >= -InEpsilon;
				}
			} Local;

			return ( Local.SameSide( P, A, B, C, Epsilon ) &&
					 Local.SameSide( P, B, A, C, Epsilon ) &&
					 Local.SameSide( P, C, A, B, Epsilon ) );
		}
		
		/**
		 * Checks to see if a point overlaps a triangle
		 *
		 * @param	P	Point
		 * @param	Triangle	triangle vertices
		 *
		 * @return	true if the point overlaps the triangle
		 */
		bool IsPointInTriangle(const FVector2D & P, const FVector2D (&Triangle)[3])
		{
			// Bias toward non-overlapping so sliver triangles won't overlap their adjoined neighbors
			const float TestEpsilon = -0.001f;
			// Test for overlap
			if( IsPointInTriangle(
				FVector( P, 0.0f ),
				FVector( Triangle[0], 0.0f ),
				FVector( Triangle[1], 0.0f ),
				FVector( Triangle[2], 0.0f ),
				TestEpsilon ) )
			{
				return true;
			}
			return false;
		}

		/**
		 * Checks for UVs outside of a 0.0 to 1.0 range.
		 *
		 * @param	TriangleUVs	a referenced array of 3 UV coordinates.
		 *
		 * @return	true if UVs are <0.0 or >1.0
		 */
		bool AreUVsOutOfRange(const FVector2D (&TriangleUVs)[3])
		{
			// Test for UVs outside of the 0.0 to 1.0 range (wrapped/clamped)
			for(int32 UVIndex = 0; UVIndex < 3; UVIndex++)
			{
				const FVector2D& CurVertUV = TriangleUVs[UVIndex];
				const float TestEpsilon = 0.001f;
				for( int32 CurDimIndex = 0; CurDimIndex < 2; ++CurDimIndex )
				{
					if( CurVertUV[ CurDimIndex ] < ( 0.0f - TestEpsilon ) || CurVertUV[ CurDimIndex ] > ( 1.0f + TestEpsilon ) )
					{
						return true;
					}
				}
			}
			return false;
		}

		/**
		 * Fills an array with 3 UV coordinates for a specified triangle from a FStaticMeshLODResources object.
		 *
		 * @param	MeshLOD	Source mesh.
		 * @param	TriangleIndex	triangle to get UV data from
		 * @param	UVChannel UV channel to extract
		 * @param	TriangleUVsOUT an array which is filled with the UV data
		 */
		void GetTriangleUVs( const FStaticMeshLODResources& MeshLOD, const int32 TriangleIndex, const int32 UVChannel, FVector2D (&TriangleUVsOUT)[3])
		{
			check( TriangleIndex < MeshLOD.GetNumTriangles());
			
			FIndexArrayView Indices = MeshLOD.IndexBuffer.GetArrayView();
			const int32 StartIndex = TriangleIndex*3;			
			const uint32 VertexIndices[] = {Indices[StartIndex + 0], Indices[StartIndex + 1], Indices[StartIndex + 2]};
			for(int i = 0; i<3;i++)
			{
				TriangleUVsOUT[i] = MeshLOD.VertexBuffer.GetVertexUV(VertexIndices[i], UVChannel);		
			}
		}

		enum UVCheckResult { UVCheck_Missing, UVCheck_Bad, UVCheck_OK, UVCheck_NoTriangles};
		/**
		 * Performs a UV check on a specific LOD from a UStaticMesh.
		 *
		 * @param	MeshLOD	a referenced array of 3 UV coordinates.
		 * @param	LightMapCoordinateIndex The UV channel containing the light map UVs.
		 * @param	OverlappingLightMapUVTriangleCountOUT Filled with the number of triangles that overlap one another.
		 * @param	OutOfBoundsTriangleCountOUT Filled with the number of triangles whose UVs are out of 0..1 range.
		 * @return	UVCheckResult UVCheck_Missing: light map UV channel does not exist in the data. UVCheck_Bad: one or more triangles break UV mapping rules. UVCheck_NoTriangle: The specified mesh has no triangles. UVCheck_OK: no problems were found.
		 */
		UVCheckResult CheckLODLightMapUVs( const FStaticMeshLODResources& MeshLOD, const int32 InLightMapCoordinateIndex, int32& OverlappingLightMapUVTriangleCountOUT, int32& OutOfBoundsTriangleCountOUT)
		{
			const int32 TriangleCount = MeshLOD.GetNumTriangles();
			if(TriangleCount==0)
			{
				return UVCheck_NoTriangles;
			}
			OverlappingLightMapUVTriangleCountOUT = 0;
			OutOfBoundsTriangleCountOUT = 0;

			TArray< int32 > TriangleOverlapCounts;
			TriangleOverlapCounts.AddZeroed( TriangleCount );

			if (InLightMapCoordinateIndex >= MeshLOD.GetNumTexCoords())
			{
				return UVCheck_Missing;
			}

			for(int32 CurTri = 0; CurTri<TriangleCount;CurTri++)
			{
				FVector2D CurTriangleUVs[3];
				GetTriangleUVs(MeshLOD, CurTri, InLightMapCoordinateIndex, CurTriangleUVs);
				FVector2D CurTriangleUVCentroid = ( CurTriangleUVs[0] + CurTriangleUVs[1] + CurTriangleUVs[2] ) / 3.0f;
		
				if( AreUVsOutOfRange(CurTriangleUVs) )
				{
					++OutOfBoundsTriangleCountOUT;
				}

				if(TriangleOverlapCounts[CurTri] != 0)
				{
					continue;
				}
				for(int32 OtherTri = CurTri+1; OtherTri<TriangleCount;OtherTri++)
				{
					if(TriangleOverlapCounts[OtherTri] != 0)
					{
						continue;
					}

					FVector2D OtherTriangleUVs[3];
					GetTriangleUVs(MeshLOD, OtherTri, InLightMapCoordinateIndex, OtherTriangleUVs);
					FVector2D OtherTriangleUVCentroid = ( OtherTriangleUVs[0] + OtherTriangleUVs[1] + OtherTriangleUVs[2] ) / 3.0f;

					bool result1 = IsPointInTriangle(CurTriangleUVCentroid, OtherTriangleUVs );
					bool result2 = IsPointInTriangle(OtherTriangleUVCentroid, CurTriangleUVs );

					if( result1 || result2)
					{
						++OverlappingLightMapUVTriangleCountOUT;
						++TriangleOverlapCounts[ CurTri ];
						++OverlappingLightMapUVTriangleCountOUT;
						++TriangleOverlapCounts[ OtherTri ];
					}
				}
			}

			return (OutOfBoundsTriangleCountOUT != 0 || OverlappingLightMapUVTriangleCountOUT !=0 ) ? UVCheck_Bad : UVCheck_OK;
		}
	} Local;

	check( InStaticMesh != NULL );

	TArray< int32 > TriangleOverlapCounts;

	const int32 NumLods = InStaticMesh->GetNumLODs();
	for( int32 CurLODModelIndex = 0; CurLODModelIndex < NumLods; ++CurLODModelIndex )
	{
		const FStaticMeshLODResources& RenderData = InStaticMesh->RenderData->LODResources[CurLODModelIndex];
		int32 LightMapTextureCoordinateIndex = InStaticMesh->LightMapCoordinateIndex;

		// We expect the light map texture coordinate to be greater than zero, as the first UV set
		// should never really be used for light maps, unless this mesh was exported as a light mapped uv set.
		if( LightMapTextureCoordinateIndex <= 0 && RenderData.GetNumTexCoords() > 1 )
		{	
			LightMapTextureCoordinateIndex = 1;
		}

		int32 OverlappingLightMapUVTriangleCount = 0;
		int32 OutOfBoundsTriangleCount = 0;

		const FLocal::UVCheckResult result = Local.CheckLODLightMapUVs( RenderData, LightMapTextureCoordinateIndex, OverlappingLightMapUVTriangleCount, OutOfBoundsTriangleCount);
		switch(result)
		{
			case FLocal::UVCheck_OK:
				InOutAssetsWithValidUVSets.Add( InStaticMesh->GetFullName() );
			break;
			case FLocal::UVCheck_Bad:
				InOutAssetsWithBadUVSets.Add( InStaticMesh->GetFullName() );
			break;
			case FLocal::UVCheck_Missing:
				InOutAssetsWithMissingUVSets.Add( InStaticMesh->GetFullName() );
			break;
			default:
			break;
		}

		if(bInVerbose == true)
		{
			switch(result)
			{
				case FLocal::UVCheck_OK:
					UE_LOG(LogStaticMesh, Log, TEXT( "[%s, LOD %i] light map UVs OK" ), *InStaticMesh->GetName(), CurLODModelIndex );
					break;
				case FLocal::UVCheck_Bad:
					if( OverlappingLightMapUVTriangleCount > 0 )
					{
						UE_LOG(LogStaticMesh, Warning, TEXT( "[%s, LOD %i] %i triangles with overlapping UVs (of %i) (UV set %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, OverlappingLightMapUVTriangleCount, RenderData.GetNumTriangles(), LightMapTextureCoordinateIndex );
					}
					if( OutOfBoundsTriangleCount > 0 )
					{
						UE_LOG(LogStaticMesh, Warning, TEXT( "[%s, LOD %i] %i triangles with out-of-bound UVs (of %i) (UV set %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, OutOfBoundsTriangleCount, RenderData.GetNumTriangles(), LightMapTextureCoordinateIndex );
					}
					break;
				case FLocal::UVCheck_Missing:
					UE_LOG(LogStaticMesh, Warning, TEXT( "[%s, LOD %i] missing light map UVs (Res %i, CoordIndex %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, InStaticMesh->LightMapResolution, InStaticMesh->LightMapCoordinateIndex );
					break;
				case FLocal::UVCheck_NoTriangles:
					UE_LOG(LogStaticMesh, Warning, TEXT( "[%s, LOD %i] doesn't have any triangles" ), *InStaticMesh->GetName(), CurLODModelIndex );
					break;
				default:
					break;
			}
		}
	}
}

UMaterialInterface* UStaticMesh::GetMaterial(int32 MaterialIndex) const
{
	if (Materials.IsValidIndex(MaterialIndex))
	{
		return Materials[MaterialIndex];
	}

	return NULL;
}

/**
 * Returns the render data to use for exporting the specified LOD. This method should always
 * be called when exporting a static mesh.
 */
const FStaticMeshLODResources& UStaticMesh::GetLODForExport(int32 LODIndex) const
{
	check(RenderData);
	LODIndex = FMath::Clamp<int32>( LODIndex, 0, RenderData->LODResources.Num()-1 );
	// TODO_STATICMESH: Don't allow exporting simplified meshes?
	return RenderData->LODResources[LODIndex];
}

#if WITH_EDITOR
bool UStaticMesh::CanLODsShareStaticLighting() const
{
	bool bCanShareData = true;
	for (int32 LODIndex = 1; bCanShareData && LODIndex < SourceModels.Num(); ++LODIndex)
	{
		bCanShareData = bCanShareData && SourceModels[LODIndex].RawMeshBulkData->IsEmpty();
	}

	if (SpeedTreeWind.IsValid())
	{
		// SpeedTrees are set up for lighting to share between LODs
		bCanShareData = true;
	}

	return bCanShareData;
}

void UStaticMesh::ConvertLegacyLODDistance()
{
	check(SourceModels.Num() > 0);
	check(SourceModels.Num() <= MAX_STATIC_MESH_LODS);

	if(SourceModels.Num() == 1)
	{
		// Only one model, 
		SourceModels[0].ScreenSize = 1.0f;
	}
	else
	{
		// Multiple models, we should have LOD distance data.
		// Assuming an FOV of 90 and a screen size of 1920x1080 to estimate an appropriate display factor.
		const float HalfFOV = PI / 4.0f;
		const float ScreenWidth = 1920.0f;
		const float ScreenHeight = 1080.0f;

		for(int32 ModelIndex = 0 ; ModelIndex < SourceModels.Num() ; ++ModelIndex)
		{
			FStaticMeshSourceModel& SrcModel = SourceModels[ModelIndex];

			if(SrcModel.LODDistance_DEPRECATED == 0.0f)
			{
				SrcModel.ScreenSize = 1.0f;
				RenderData->ScreenSize[ModelIndex] = SrcModel.ScreenSize;
			}
			else
			{
				// Create a screen position from the LOD distance
				const FVector4 PointToTest(0.0f, 0.0f, SrcModel.LODDistance_DEPRECATED, 1.0f);
				FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);
				FVector4 ScreenPosition = ProjMatrix.TransformFVector4(PointToTest);
				// Convert to a percentage of the screen
				const float ScreenMultiple = ScreenWidth / 2.0f * ProjMatrix.M[0][0];
				const float ScreenRadius = ScreenMultiple * GetBounds().SphereRadius / FMath::Max(ScreenPosition.W, 1.0f);
				const float ScreenArea = ScreenWidth * ScreenHeight;
				const float BoundsArea = PI * ScreenRadius * ScreenRadius;
				SrcModel.ScreenSize = FMath::Clamp(BoundsArea / ScreenArea, 0.0f, 1.0f);
				RenderData->ScreenSize[ModelIndex] = SrcModel.ScreenSize;
			}
		}
	}
}

void UStaticMesh::GenerateLodsInPackage()
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("StaticMeshName"), FText::FromString(GetName()));
	FStaticMeshStatusMessageContext StatusContext(FText::Format(NSLOCTEXT("Engine", "SavingStaticMeshLODsStatus", "Saving generated LODs for static mesh {StaticMeshName}..."), Args));

	// Get LODGroup info
	ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
	ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
	check(RunningPlatform);
	const FStaticMeshLODSettings& LODSettings = RunningPlatform->GetStaticMeshLODSettings();

	// Generate the reduced models
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	if (MeshUtilities.GenerateStaticMeshLODs(SourceModels, LODSettings.GetLODGroup(LODGroup)))
	{
		// Clear LOD settings
		LODGroup = NAME_None;
		const auto& NewGroup = LODSettings.GetLODGroup(LODGroup);
		for (int32 Index = 0; Index < SourceModels.Num(); ++Index)
		{
			SourceModels[Index].ReductionSettings = NewGroup.GetDefaultSettings(0);
		}

		Build(true);

		// Raw mesh is now dirty, so the package has to be resaved
		MarkPackageDirty();
	}
}

#endif // #if WITH_EDITOR

UStaticMeshSocket* UStaticMesh::FindSocket(FName InSocketName)
{
	if(InSocketName == NAME_None)
	{
		return NULL;
	}

	for(int32 i=0; i<Sockets.Num(); i++)
	{
		UStaticMeshSocket* Socket = Sockets[i];
		if(Socket && Socket->SocketName == InSocketName)
		{
			return Socket;
		}
	}
	return NULL;
}

/*-----------------------------------------------------------------------------
UStaticMeshSocket
-----------------------------------------------------------------------------*/

UStaticMeshSocket::UStaticMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RelativeScale = FVector(1.0f, 1.0f, 1.0f);
}

/** Utility that returns the current matrix for this socket. */
bool UStaticMeshSocket::GetSocketMatrix(FMatrix& OutMatrix, UStaticMeshComponent const* MeshComp) const
{
	check( MeshComp );
	OutMatrix = FScaleRotationTranslationMatrix( RelativeScale, RelativeRotation, RelativeLocation ) * MeshComp->ComponentToWorld.ToMatrixWithScale();
	return true;
}

bool UStaticMeshSocket::GetSocketTransform(FTransform& OutTransform, class UStaticMeshComponent const* MeshComp) const
{
	check( MeshComp );
	OutTransform = FTransform(RelativeRotation, RelativeLocation, RelativeScale) * MeshComp->ComponentToWorld;
	return true;
}

bool UStaticMeshSocket::AttachActor(AActor* Actor,  UStaticMeshComponent* MeshComp) const
{
	bool bAttached = false;

	// Don't support attaching to own socket
	if (Actor != MeshComp->GetOwner() && Actor->GetRootComponent())
	{
		FMatrix SocketTM;
		if( GetSocketMatrix( SocketTM, MeshComp ) )
		{
			Actor->Modify();

			Actor->SetActorLocation(SocketTM.GetOrigin(), false);
			Actor->SetActorRotation(SocketTM.Rotator());
			Actor->GetRootComponent()->SnapTo( MeshComp, SocketName );

#if WITH_EDITOR
			if (GIsEditor)
			{
				Actor->PreEditChange(NULL);
				Actor->PostEditChange();
			}
#endif // WITH_EDITOR

			bAttached = true;
		}
	}
	return bAttached;
}

#if WITH_EDITOR
void UStaticMeshSocket::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if( PropertyChangedEvent.Property )
	{
		ChangedEvent.Broadcast( this, PropertyChangedEvent.MemberProperty );
	}
}
#endif

void UStaticMeshSocket::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if(Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MeshSocketScaleUtilization)
	{
		// Set the relative scale to 1.0. As it was not used before this should allow existing data
		// to work as expected.
		RelativeScale = FVector(1.0f, 1.0f, 1.0f);
	}
}
