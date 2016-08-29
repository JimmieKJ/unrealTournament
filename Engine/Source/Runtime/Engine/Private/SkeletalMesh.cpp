// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMesh.cpp: Unreal skeletal mesh and animation implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Animation/SkeletalMeshActor.h"
#include "EditorSupportDelegates.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalMeshSorting.h"
#include "MeshBuild.h"
#include "ParticleDefinitions.h"
#include "TessellationRendering.h"
#include "SkeletalRenderPublic.h"
#include "SoundDefinitions.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "SkeletalRenderGPUSkin.h"
#include "RawIndexBuffer.h"
#include "PhysicsPublic.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "ComponentReregisterContext.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/AssetUserData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

#if WITH_EDITOR
#include "MeshUtilities.h"

#if WITH_APEX_CLOTHING
#include "ApexClothingUtils.h"
#endif

#endif // #if WITH_EDITOR

#include "Net/UnrealNetwork.h"
#include "TargetPlatform.h"

#if WITH_APEX
#include "PhysicsEngine/PhysXSupport.h"
#endif// #if WITH_APEX

#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/BrushComponent.h"
#include "FrameworkObjectVersion.h"

#define LOCTEXT_NAMESPACE "SkeltalMesh"

DEFINE_LOG_CATEGORY(LogSkeletalMesh);
DECLARE_CYCLE_STAT(TEXT("GetShadowShapes"), STAT_GetShadowShapes, STATGROUP_Anim);

// Custom serialization version for RecomputeTangent
struct FSkeletalMeshCustomVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// Remove Chunks array in FStaticLODModel and combine with Sections array
		CombineSectionWithChunk = 1,
		// Remove FRigidSkinVertex and combine with FSoftSkinVertex array
		CombineSoftAndRigidVerts = 2,
		// Need to recalc max bone influences
		RecalcMaxBoneInfluences = 3,
		// Add NumVertices that can be accessed when stripping editor data
		SaveNumVertices = 4,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSkeletalMeshCustomVersion() {}
};

const FGuid FSkeletalMeshCustomVersion::GUID(0xD78A4A00, 0xE8584697, 0xBAA819B5, 0x487D46B4);
FCustomVersionRegistration GRegisterSkeletalMeshCustomVersion(FSkeletalMeshCustomVersion::GUID, FSkeletalMeshCustomVersion::LatestVersion, TEXT("SkeletalMeshVer"));

#if WITH_APEX_CLOTHING
/*-----------------------------------------------------------------------------
	utility functions for apex clothing 
-----------------------------------------------------------------------------*/

static NxClothingAsset* LoadApexClothingAssetFromBlob(const TArray<uint8>& Buffer)
{
	// Wrap this blob with the APEX read stream class
	physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream( Buffer.GetData(), Buffer.Num() );
	// Create an NxParameterized serializer
	NxParameterized::Serializer* Serializer = GApexSDK->createSerializer(NxParameterized::Serializer::NST_BINARY);
	// Deserialize into a DeserializedData buffer
	NxParameterized::Serializer::DeserializedData DeserializedData;
	Serializer->deserialize( *Stream, DeserializedData );
	NxApexAsset* ApexAsset = NULL;
	if( DeserializedData.size() > 0 )
	{
		// The DeserializedData has something in it, so create an APEX asset from it
		ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL);
		// Make sure it's a Clothing asset
		if (ApexAsset 
			&& ApexAsset->getObjTypeID() != GApexModuleClothing->getModuleID()
			)
		{
			GPhysCommandHandler->DeferredRelease(ApexAsset);
			ApexAsset = NULL;
		}
	}

	NxClothingAsset* ApexClothingAsset = static_cast<NxClothingAsset*>(ApexAsset);
	// Release our temporary objects
	Serializer->release();
	GApexSDK->releaseMemoryReadStream( *Stream );

	return ApexClothingAsset;
}

static bool SaveApexClothingAssetToBlob(const NxClothingAsset *InAsset, TArray<uint8>& OutBuffer)
{
	bool bResult = false;
	uint32 Size = 0;
	// Get the NxParameterized data for our Clothing asset
	if( InAsset != NULL )
	{
		// Create an APEX write stream
		physx::PxFileBuf* Stream = GApexSDK->createMemoryWriteStream();
		// Create an NxParameterized serializer
		NxParameterized::Serializer* Serializer = GApexSDK->createSerializer(NxParameterized::Serializer::NST_BINARY);

		const NxParameterized::Interface* AssetParameterized = InAsset->getAssetNxParameterized();
		if( AssetParameterized != NULL )
		{
			// Serialize the data into the stream
			Serializer->serialize( *Stream, &AssetParameterized, 1 );
			// Read the stream data into our buffer for UE serialzation
			Size = Stream->getFileLength();
			OutBuffer.AddUninitialized( Size );
			Stream->read( OutBuffer.GetData(), Size );
			bResult = true;
		}

		// Release our temporary objects
		Serializer->release();
		Stream->release();
	}

	return bResult;
}

#endif//#if WITH_APEX_CLOTHING

////////////////////////////////////////////////
SIZE_T FClothingAssetData::GetResourceSize() const
{
	SIZE_T ResourceSize = 0;
#if WITH_APEX_CLOTHING
	if (ApexClothingAsset)
	{
		physx::PxU32 LODLevel = ApexClothingAsset->getNumGraphicalLodLevels();
		for(physx::PxU32 LODId=0; LODId < LODLevel; ++LODId)
		{
			if(const NxRenderMeshAsset* RenderAsset = ApexClothingAsset->getRenderMeshAsset(LODId))
			{
				NxRenderMeshAssetStats AssetStats;
				RenderAsset->getStats(AssetStats);

				ResourceSize += AssetStats.totalBytes;
			}
		}
	}

	ResourceSize += ClothCollisionVolumes.GetAllocatedSize();
	ResourceSize += ClothCollisionConvexPlaneIndices.GetAllocatedSize();
	ResourceSize += ClothCollisionVolumePlanes.GetAllocatedSize();
	ResourceSize += ClothBoneSpheres.GetAllocatedSize();
	ResourceSize += BoneSphereConnections.GetAllocatedSize();
	ResourceSize += ClothVisualizationInfos.GetAllocatedSize();
#endif // #if WITH_APEX_CLOTHING

	return ResourceSize;
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshVertexBuffer
-----------------------------------------------------------------------------*/

/**
* Constructor
*/
FSkeletalMeshVertexBuffer::FSkeletalMeshVertexBuffer() 
:	bInfluencesByteSwapped(false)
,	bUseFullPrecisionUVs(false)
,	bNeedsCPUAccess(false)
,	bExtraBoneInfluences(false)
,	VertexData(nullptr)
,	Data(nullptr)
,	Stride(0)
,	NumVertices(0)
,	MeshOrigin(FVector::ZeroVector)
, 	MeshExtension(FVector(1.f,1.f,1.f))
{
}

/**
* Destructor
*/
FSkeletalMeshVertexBuffer::~FSkeletalMeshVertexBuffer()
{
	CleanUp();
}

/**
* Assignment. Assumes that vertex buffer will be rebuilt 
*/
FSkeletalMeshVertexBuffer& FSkeletalMeshVertexBuffer::operator=(const FSkeletalMeshVertexBuffer& Other)
{
	VertexData = NULL;
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
	bNeedsCPUAccess = Other.bNeedsCPUAccess;
	bExtraBoneInfluences = Other.bExtraBoneInfluences;
	return *this;
}

/**
* Constructor (copy)
*/
FSkeletalMeshVertexBuffer::FSkeletalMeshVertexBuffer(const FSkeletalMeshVertexBuffer& Other)
:	bInfluencesByteSwapped(false)
,	bUseFullPrecisionUVs(Other.bUseFullPrecisionUVs)
,	bNeedsCPUAccess(Other.bNeedsCPUAccess)
,	bExtraBoneInfluences(Other.bExtraBoneInfluences)
,	VertexData(NULL)
,	Data(NULL)
,	Stride(0)
,	NumVertices(0)
,	MeshOrigin(Other.MeshOrigin)
, 	MeshExtension(Other.MeshExtension)
{
}

/**
 * @return text description for the resource type
 */
FString FSkeletalMeshVertexBuffer::GetFriendlyName() const
{ 
	return TEXT("Skeletal-mesh vertex buffer"); 
}

/** 
 * Delete existing resources 
 */
void FSkeletalMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

void FSkeletalMeshVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if( ResourceArray->GetResourceDataSize() > 0 )
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		
		// BUF_ShaderResource is needed for support of the SkinCache (we could make is dependent on GEnableGPUSkinCacheShaders or are there other users?)
		VertexBufferRHI = RHICreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
	}
}

void FSkeletalMeshVertexBuffer::ReleaseRHI()
{
	FVertexBuffer::ReleaseRHI();

	SRVValue.SafeRelease();
}

/**
* Serializer for this class
* @param Ar - archive to serialize to
* @param B - data to serialize
*/
FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexBuffer& VertexBuffer)
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	Ar << VertexBuffer.NumTexCoords;
	Ar << VertexBuffer.bUseFullPrecisionUVs;
	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES)
	{
		Ar << VertexBuffer.bExtraBoneInfluences;
	}
	else
	{
		if (Ar.IsLoading())
		{
			// Make sure nothing slipped through the cracks
			check(!VertexBuffer.bExtraBoneInfluences);
		}
	}

	// Serialize MeshExtension and Origin
	// I need to save them for console to pick it up later
	Ar << VertexBuffer.MeshExtension << VertexBuffer.MeshOrigin;

	if( Ar.IsLoading() )
	{
		// allocate vertex data on load
		VertexBuffer.AllocateData();
	}

	// if Ar is counting, it still should serialize. Need to count VertexData
	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if( VertexBuffer.VertexData != NULL )
		{
			VertexBuffer.VertexData->Serialize(Ar);	

			// update cached buffer info
			VertexBuffer.NumVertices = VertexBuffer.VertexData->GetNumVertices();
			VertexBuffer.Data = (VertexBuffer.NumVertices > 0) ? VertexBuffer.VertexData->GetDataPointer() : nullptr;
			VertexBuffer.Stride = VertexBuffer.VertexData->GetStride();
		}
	}

	return Ar;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
*/
void FSkeletalMeshVertexBuffer::Init(const TArray<FSoftSkinVertex>& InVertices)
{
	// Make sure if this is console, use compressed otherwise, use not compressed
	AllocateData();
	
	VertexData->ResizeBuffer(InVertices.Num());
	
	if (InVertices.Num() > 0)
	{
		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();
	}
	
	if (bExtraBoneInfluences)
	{
		for( int32 VertIdx=0; VertIdx < InVertices.Num(); VertIdx++ )
		{
			const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
			SetVertexFast<true>(VertIdx,SrcVertex);
		}
	}
	else
	{
		for( int32 VertIdx=0; VertIdx < InVertices.Num(); VertIdx++ )
		{
			const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
			SetVertexFast<false>(VertIdx,SrcVertex);
		}
	}
}

void FSkeletalMeshVertexBuffer::SetNeedsCPUAccess(bool bInNeedsCPUAccess)
{
	bNeedsCPUAccess = bInNeedsCPUAccess;
}

// Handy macro for allocating the correct vertex data class (which has to be known at compile time) depending on the data type and number of UVs.  
#define ALLOCATE_VERTEX_DATA_TEMPLATE( VertexDataType, NumUVs, bExtraBoneInfluences )											\
	switch(NumUVs)																						\
	{																									\
		case 1: VertexData = new TSkeletalMeshVertexData< VertexDataType<1, bExtraBoneInfluences> >(bNeedsCPUAccess); break;	\
		case 2: VertexData = new TSkeletalMeshVertexData< VertexDataType<2, bExtraBoneInfluences> >(bNeedsCPUAccess); break;	\
		case 3: VertexData = new TSkeletalMeshVertexData< VertexDataType<3, bExtraBoneInfluences> >(bNeedsCPUAccess); break;	\
		case 4: VertexData = new TSkeletalMeshVertexData< VertexDataType<4, bExtraBoneInfluences> >(bNeedsCPUAccess); break;	\
		default: UE_LOG(LogSkeletalMesh, Fatal,TEXT("Invalid number of texture coordinates"));								\
	}																									\

/** 
* Allocates the vertex data storage type. 
*/
void FSkeletalMeshVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	if( !bUseFullPrecisionUVs )
	{
		if (bExtraBoneInfluences)
		{
			ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat16Uvs, NumTexCoords, true );
		}
		else
		{
			ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat16Uvs, NumTexCoords, false );
		}
	}
	else
	{
		if (bExtraBoneInfluences)
		{
			ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat32Uvs, NumTexCoords, true );
		}
		else
		{
			ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat32Uvs, NumTexCoords, false );
		}
	}
}

template <bool bExtraBoneInfluencesT>
void FSkeletalMeshVertexBuffer::SetVertexFast(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex)
{
	checkSlow(VertexIndex < GetNumVertices());
	auto* VertBase = (TGPUSkinVertexBase<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	VertBase->TangentX = SrcVertex.TangentX;
	VertBase->TangentZ = SrcVertex.TangentZ;
	// store the sign of the determinant in TangentZ.W
	VertBase->TangentZ.Vector.W = GetBasisDeterminantSignByte( SrcVertex.TangentX, SrcVertex.TangentY, SrcVertex.TangentZ );
	FMemory::Memcpy(VertBase->InfluenceBones,SrcVertex.InfluenceBones, TGPUSkinVertexBase<bExtraBoneInfluencesT>::NumInfluences);
	FMemory::Memcpy(VertBase->InfluenceWeights,SrcVertex.InfluenceWeights, TGPUSkinVertexBase<bExtraBoneInfluencesT>::NumInfluences);
	if( !bUseFullPrecisionUVs )
	{
		auto* Vertex = (TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)VertBase;
		Vertex->Position = SrcVertex.Position;
		for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
		{
			Vertex->UVs[UVIndex] = FVector2DHalf( SrcVertex.UVs[UVIndex] );
		}
	}
	else
	{
		auto* Vertex = (TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS, bExtraBoneInfluencesT>*)VertBase;
		Vertex->Position = SrcVertex.Position;
		for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
		{
			Vertex->UVs[UVIndex] = FVector2D( SrcVertex.UVs[UVIndex] );
		}
	}
}

/**
* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
* Without rebuilding the mesh (loss of precision)
*/
template<uint32 NumTexCoordsT, bool bExtraBoneInfluencesT>
void FSkeletalMeshVertexBuffer::ConvertToFullPrecisionUVsTyped()
{
	if( !bUseFullPrecisionUVs )
	{
		TArray< TGPUSkinVertexFloat32Uvs<NumTexCoordsT, bExtraBoneInfluencesT> > DestVertexData;
		TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >& SrcVertexData = *(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT, bExtraBoneInfluencesT> >*)VertexData;			
		DestVertexData.AddUninitialized(SrcVertexData.Num());
		for( int32 VertIdx=0; VertIdx < SrcVertexData.Num(); VertIdx++ )
		{
			TGPUSkinVertexFloat16Uvs<NumTexCoordsT, bExtraBoneInfluencesT>& SrcVert = SrcVertexData[VertIdx];
			TGPUSkinVertexFloat32Uvs<NumTexCoordsT, bExtraBoneInfluencesT>& DestVert = DestVertexData[VertIdx];
			FMemory::Memcpy(&DestVert,&SrcVert,sizeof(TGPUSkinVertexBase<bExtraBoneInfluencesT>));
			DestVert.Position = SrcVert.Position;
			for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
			{
				DestVert.UVs[UVIndex] = FVector2D(SrcVert.UVs[UVIndex]);
			}

		}

		bUseFullPrecisionUVs = true;
		*this = DestVertexData;
	}
}

/*-----------------------------------------------------------------------------
FSkeletalMeshVertexColorBuffer
-----------------------------------------------------------------------------*/

/**
 * Constructor
 */
FSkeletalMeshVertexColorBuffer::FSkeletalMeshVertexColorBuffer() 
:	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{

}

/**
 * Destructor
 */
FSkeletalMeshVertexColorBuffer::~FSkeletalMeshVertexColorBuffer()
{
	// clean up everything
	CleanUp();
}

/**
 * Assignment. Assumes that vertex buffer will be rebuilt 
 */

FSkeletalMeshVertexColorBuffer& FSkeletalMeshVertexColorBuffer::operator=(const FSkeletalMeshVertexColorBuffer& Other)
{
	VertexData = NULL;
	return *this;
}

/**
 * Copy Constructor
 */
FSkeletalMeshVertexColorBuffer::FSkeletalMeshVertexColorBuffer(const FSkeletalMeshVertexColorBuffer& Other)
:	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{

}

/**
 * @return text description for the resource type
 */
FString FSkeletalMeshVertexColorBuffer::GetFriendlyName() const
{
	return TEXT("Skeletal0-mesh vertex color buffer");
}

/** 
 * Delete existing resources 
 */
void FSkeletalMeshVertexColorBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

/**
 * Initialize the RHI resource for this vertex buffer
 */
void FSkeletalMeshVertexColorBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if( ResourceArray->GetResourceDataSize() > 0 )
	{
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static, CreateInfo);
	}
}

/**
 * Serializer for this class
 * @param Ar - archive to serialize to
 * @param B - data to serialize
 */
FArchive& operator<<( FArchive& Ar, FSkeletalMeshVertexColorBuffer& VertexBuffer )
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
	if( Ar.IsLoading() )
	{
		VertexBuffer.AllocateData();
	}

	if ( !StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory() )
	{
		if( VertexBuffer.VertexData != NULL )
		{
			VertexBuffer.VertexData->Serialize( Ar );
			VertexBuffer.UpdateCachedInfo();
		}
	}

	return Ar;
}

/**
 * Initializes the buffer with the given vertices.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FSkeletalMeshVertexColorBuffer::Init( const TArray<FSoftSkinVertex>& InVertices )
{
	AllocateData();
	ResizeData(InVertices.Num());

	// Copy color info from each vertex
	for( int32 VertIdx=0; VertIdx < InVertices.Num(); ++VertIdx )
	{
		const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
		SetColor(VertIdx,SrcVertex.Color);
	}
}

/** 
 * Allocates the vertex data storage type. 
 */
void FSkeletalMeshVertexColorBuffer::AllocateData()
{
	CleanUp();

	VertexData = new TSkeletalMeshVertexData<FGPUSkinVertexColor>(true);
}

FSkeletalMeshVertexColorBuffer& FSkeletalMeshVertexColorBuffer::operator=(const TArray<FColor>& InColors)
{
	AllocateData();
	ResizeData(InColors.Num());

	// Copy the color values for each vertex.
	for (int32 VertIdx = 0; VertIdx < InColors.Num(); ++VertIdx)
	{
		SetColor(VertIdx, InColors[VertIdx]);
	}

	return *this;
}

void FSkeletalMeshVertexColorBuffer::ResizeData(int32 InNumVertices)
{
	VertexData->ResizeBuffer(InNumVertices);
	UpdateCachedInfo();
}

void FSkeletalMeshVertexColorBuffer::UpdateCachedInfo()
{
	Data = VertexData->GetDataPointer();
	Stride = VertexData->GetStride();
	NumVertices = VertexData->GetNumVertices();
}

/** 
 * Copy the contents of the source color to the destination vertex in the buffer 
 *
 * @param VertexIndex - index into the vertex buffer
 * @param SrcColor - source color to copy from
 */
void FSkeletalMeshVertexColorBuffer::SetColor( uint32 VertexIndex, const FColor& SrcColor )
{
	checkSlow( VertexIndex < GetNumVertices() );
	uint8* VertBase = Data + VertexIndex * Stride;
	((FGPUSkinVertexColor*)(VertBase))->VertexColor = SrcColor;
}

/*-----------------------------------------------------------------------------
FSkeletalMeshVertexAPEXClothBuffer
-----------------------------------------------------------------------------*/

/**
 * Constructor
 */
FSkeletalMeshVertexAPEXClothBuffer::FSkeletalMeshVertexAPEXClothBuffer() 
:	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{

}

/**
 * Destructor
 */
FSkeletalMeshVertexAPEXClothBuffer::~FSkeletalMeshVertexAPEXClothBuffer()
{
	// clean up everything
	CleanUp();
}

/**
 * Assignment. Assumes that vertex buffer will be rebuilt 
 */

FSkeletalMeshVertexAPEXClothBuffer& FSkeletalMeshVertexAPEXClothBuffer::operator=(const FSkeletalMeshVertexAPEXClothBuffer& Other)
{
	VertexData = NULL;
	return *this;
}

/**
 * Copy Constructor
 */
FSkeletalMeshVertexAPEXClothBuffer::FSkeletalMeshVertexAPEXClothBuffer(const FSkeletalMeshVertexAPEXClothBuffer& Other)
:	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{

}

/**
 * @return text description for the resource type
 */
FString FSkeletalMeshVertexAPEXClothBuffer::GetFriendlyName() const
{
	return TEXT("Skeletal-mesh vertex APEX cloth mesh-mesh mapping buffer");
}

/** 
 * Delete existing resources 
 */
void FSkeletalMeshVertexAPEXClothBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

/**
 * Initialize the RHI resource for this vertex buffer
 */
void FSkeletalMeshVertexAPEXClothBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if( ResourceArray->GetResourceDataSize() > 0 )
	{
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static, CreateInfo);
	}
}

/**
 * Serializer for this class
 * @param Ar - archive to serialize to
 * @param B - data to serialize
 */
FArchive& operator<<( FArchive& Ar, FSkeletalMeshVertexAPEXClothBuffer& VertexBuffer )
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	if( Ar.IsLoading() )
	{
		VertexBuffer.AllocateData();
	}

	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if( VertexBuffer.VertexData != NULL )
		{
			VertexBuffer.VertexData->Serialize( Ar );

			// update cached buffer info
			VertexBuffer.NumVertices = VertexBuffer.VertexData->GetNumVertices();
			VertexBuffer.Data = (VertexBuffer.NumVertices > 0) ? VertexBuffer.VertexData->GetDataPointer() : nullptr;
			VertexBuffer.Stride = VertexBuffer.VertexData->GetStride();
		}
	}

	return Ar;
}






/**
 * Initializes the buffer with the given vertices.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FSkeletalMeshVertexAPEXClothBuffer::Init(const TArray<FApexClothPhysToRenderVertData>& InMappingData)
{
	// Allocate new data
	AllocateData();

	// Resize the buffer to hold enough data for all passed in vertices
	VertexData->ResizeBuffer( InMappingData.Num() );

	Data = VertexData->GetDataPointer();
	Stride = VertexData->GetStride();
	NumVertices = VertexData->GetNumVertices();

	// Copy the vertices into the buffer.
	checkSlow(Stride*NumVertices == sizeof(FApexClothPhysToRenderVertData) * InMappingData.Num());
	//appMemcpy(Data, &InMappingData(0), Stride*NumVertices);
	for(int32 Index = 0;Index < InMappingData.Num();Index++)
	{
		const FApexClothPhysToRenderVertData& SourceMapping = InMappingData[Index];
		const int32 DestVertexIndex = Index;
		MappingData(DestVertexIndex) = SourceMapping;
	}
}

/** 
 * Allocates the vertex data storage type. 
 */
void FSkeletalMeshVertexAPEXClothBuffer::AllocateData()
{
	CleanUp();

	VertexData = new TSkeletalMeshVertexData<FApexClothPhysToRenderVertData>(true);
}


/*-----------------------------------------------------------------------------
FGPUSkinVertexBase
-----------------------------------------------------------------------------*/

/**
* Serializer
*
* @param Ar - archive to serialize with
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexBase<bExtraBoneInfluencesT>::Serialize(FArchive& Ar)
{
	Ar << TangentX;
	Ar << TangentZ;

	// serialize bone and weight uint8 arrays in order
	// this is required when serializing as bulk data memory (see TArray::BulkSerialize notes)
	for(uint32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; InfluenceIndex++)
	{
		Ar << InfluenceBones[InfluenceIndex];
	}
	for(uint32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; InfluenceIndex++)
	{
		Ar << InfluenceWeights[InfluenceIndex];
	}
}

/*-----------------------------------------------------------------------------
	FSoftSkinVertex
-----------------------------------------------------------------------------*/

/**
* Serializer
*
* @param Ar - archive to serialize with
* @param V - vertex to serialize
* @return archive that was used
*/
FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V)
{
	Ar << V.Position;
	Ar << V.TangentX << V.TangentY << V.TangentZ;

	for( int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
	{
		Ar << V.UVs[UVIdx];
	}

	Ar << V.Color;

	// serialize bone and weight uint8 arrays in order
	// this is required when serializing as bulk data memory (see TArray::BulkSerialize notes)
	for(uint32 InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES_PER_STREAM;InfluenceIndex++)
	{
		Ar << V.InfluenceBones[InfluenceIndex];
	}

	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES)
	{
		for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
		{
			Ar << V.InfluenceBones[InfluenceIndex];
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
			{
				V.InfluenceBones[InfluenceIndex] = 0;
			}
		}
	}

	for(uint32 InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES_PER_STREAM;InfluenceIndex++)
	{
		Ar << V.InfluenceWeights[InfluenceIndex];
	}

	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES)
	{
		for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
		{
			Ar << V.InfluenceWeights[InfluenceIndex];
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
			{
				V.InfluenceWeights[InfluenceIndex] = 0;
			}
		}
	}

	return Ar;
}

bool FSoftSkinVertex::GetRigidWeightBone(uint8& OutBoneIndex) const
{
	bool bIsRigid = false;

	for (int32 WeightIdx = 0; WeightIdx < MAX_TOTAL_INFLUENCES; WeightIdx++)
	{
		if (InfluenceWeights[WeightIdx] == 255)
		{
			bIsRigid = true;
			OutBoneIndex = InfluenceBones[WeightIdx];
			break;
		}
	}

	return bIsRigid;
}

/*-----------------------------------------------------------------------------
	FMultiSizeIndexBuffer
-------------------------------------------------------------------------------*/
FMultiSizeIndexContainer::~FMultiSizeIndexContainer()
{
	if (IndexBuffer)
	{
		delete IndexBuffer;
	}
}

/**
 * Initialize the index buffer's render resources.
 */
void FMultiSizeIndexContainer::InitResources()
{
	check(IsInGameThread());
	if( IndexBuffer )
	{
		BeginInitResource( IndexBuffer );
	}
}

/**
 * Releases the index buffer's render resources.
 */	
void FMultiSizeIndexContainer::ReleaseResources()
{
	check(IsInGameThread());
	if( IndexBuffer )
	{
		BeginReleaseResource( IndexBuffer );
	}
}

/**
 * Creates a new index buffer
 */
void FMultiSizeIndexContainer::CreateIndexBuffer(uint8 InDataTypeSize)
{
	check( IndexBuffer == NULL );
	bool bNeedsCPUAccess = true;

	DataTypeSize = InDataTypeSize;

	if (InDataTypeSize == sizeof(uint16))
	{
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
	}
	else
	{
#if !DISALLOW_32BIT_INDICES
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
		UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
	}
}

/**
 * Repopulates the index buffer
 */
void FMultiSizeIndexContainer::RebuildIndexBuffer( const FMultiSizeIndexContainerData& InData )
{
	bool bNeedsCPUAccess = true;

	if( IndexBuffer )
	{
		delete IndexBuffer;
	}
	DataTypeSize = InData.DataTypeSize;

	if (DataTypeSize == sizeof(uint16))
	{
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
	}
	else
	{
#if !DISALLOW_32BIT_INDICES
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
		UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
	}

	CopyIndexBuffer( InData.Indices );
}

/**
 * Returns a 32 bit version of the index buffer
 */
void FMultiSizeIndexContainer::GetIndexBuffer( TArray<uint32>& OutArray ) const
{
	check( IndexBuffer );

	OutArray.Reset();
	int32 NumIndices = IndexBuffer->Num();
	OutArray.AddUninitialized( NumIndices );

	for (int32 I = 0; I < NumIndices; ++I)
	{
		OutArray[I] = IndexBuffer->Get(I);
	}
}

/**
 * Populates the index buffer with a new set of indices
 */
void FMultiSizeIndexContainer::CopyIndexBuffer(const TArray<uint32>& NewArray)
{
	check( IndexBuffer );

	// On console the resource arrays can't have items added directly to them
	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		if (DataTypeSize == sizeof(uint16))
		{
			TArray<uint16> WordArray;
			for (int32 i = 0; i < NewArray.Num(); ++i)
			{
				WordArray.Add((uint16)NewArray[i]);
			}

			((FRawStaticIndexBuffer16or32<uint16>*)IndexBuffer)->AssignNewBuffer(WordArray);
		}
		else
		{
			((FRawStaticIndexBuffer16or32<uint32>*)IndexBuffer)->AssignNewBuffer(NewArray);
		}
	}
	else
	{
		IndexBuffer->Empty();
		for (int32 i = 0; i < NewArray.Num(); ++i)
		{
			IndexBuffer->AddItem(NewArray[i]);
		}
	}
}

void FMultiSizeIndexContainer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FMultiSizeIndexContainer::Serialize"), STAT_MultiSizeIndexContainer_Serialize, STATGROUP_LoadTime );
	if (Ar.UE4Ver() < VER_UE4_KEEP_SKEL_MESH_INDEX_DATA)
	{
		bool bOldNeedsCPUAccess = true;
		Ar << bOldNeedsCPUAccess;
	}
	Ar << DataTypeSize;

	if (!IndexBuffer)
	{
		if (DataTypeSize == sizeof(uint16))
		{
			IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
		}
		else
		{
#if !DISALLOW_32BIT_INDICES
			IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
			UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
		}
	}
	
	IndexBuffer->Serialize( Ar );
}

#if WITH_EDITOR
/**
 * Retrieves index buffer related data
 */
void FMultiSizeIndexContainer::GetIndexBufferData( FMultiSizeIndexContainerData& OutData ) const
{
	OutData.DataTypeSize = DataTypeSize;
	GetIndexBuffer( OutData.Indices );
}

FMultiSizeIndexContainer::FMultiSizeIndexContainer(const FMultiSizeIndexContainer& Other)
: DataTypeSize(sizeof(uint16))
, IndexBuffer(NULL)
{
	// Cant copy this index buffer, assumes it will be rebuilt later
	IndexBuffer = NULL;
}

FMultiSizeIndexContainer& FMultiSizeIndexContainer::operator=(const FMultiSizeIndexContainer& Buffer)
{
	// Cant copy this index buffer.  Delete the index buffer type.
	// assumes it will be rebuilt later
	if( IndexBuffer )
	{
		delete IndexBuffer;
		IndexBuffer = NULL;
	}

	return *this;
}
#endif


/*-----------------------------------------------------------------------------
	FSkelMeshSection
-----------------------------------------------------------------------------*/

// Custom serialization version for RecomputeTangent
struct FRecomputeTangentCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// We serialize the RecomputeTangent Option
		RuntimeRecomputeTangent = 1,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FRecomputeTangentCustomVersion() {}
};

const FGuid FRecomputeTangentCustomVersion::GUID(0x5579F886, 0x933A4C1F, 0x83BA087B, 0x6361B92F);
// Register the custom version with core
FCustomVersionRegistration GRegisterRecomputeTangentCustomVersion(FRecomputeTangentCustomVersion::GUID, FRecomputeTangentCustomVersion::LatestVersion, TEXT("RecomputeTangentCustomVer"));

/** Legacy 'rigid' skin vertex */
struct FLegacyRigidSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
		TangentY,	// Binormal, V-direction
		TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// Vertex color.
	uint8			Bone;

	friend FArchive& operator<<(FArchive& Ar, FLegacyRigidSkinVertex& V)
	{
		Ar << V.Position;
		Ar << V.TangentX << V.TangentY << V.TangentZ;

		for (int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx)
		{
			Ar << V.UVs[UVIdx];
		}

		Ar << V.Color;
		Ar << V.Bone;

		return Ar;
	}

	/** Util to convert from legacy */
	void ConvertToSoftVert(FSoftSkinVertex& DestVertex)
	{
		DestVertex.Position = Position;
		DestVertex.TangentX = TangentX;
		DestVertex.TangentY = TangentY;
		DestVertex.TangentZ = TangentZ;
		// store the sign of the determinant in TangentZ.W
		DestVertex.TangentZ.Vector.W = GetBasisDeterminantSignByte(TangentX, TangentY, TangentZ);

		// copy all texture coordinate sets
		FMemory::Memcpy(DestVertex.UVs, UVs, sizeof(FVector2D)*MAX_TEXCOORDS);

		DestVertex.Color = Color;
		DestVertex.InfluenceBones[0] = Bone;
		DestVertex.InfluenceWeights[0] = 255;
		for (int32 InfluenceIndex = 1; InfluenceIndex < MAX_TOTAL_INFLUENCES; InfluenceIndex++)
		{
			DestVertex.InfluenceBones[InfluenceIndex] = 0;
			DestVertex.InfluenceWeights[InfluenceIndex] = 0;
		}
	}
};



// Serialization.
FArchive& operator<<(FArchive& Ar,FSkelMeshSection& S)
{		
	// When data is cooked for server platform some of the
	// variables are not serialized so that they're always
	// set to their initial values (for safety)
	FStripDataFlags StripFlags( Ar );

	Ar << S.MaterialIndex;

	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{
		uint16 DummyChunkIndex;
		Ar << DummyChunkIndex;
	}

	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << S.BaseIndex;
	}
		
	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << S.NumTriangles;
	}
		
	Ar << S.TriangleSorting;

	// for clothing info
	if( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH )
	{
		Ar << S.bDisabled;
		Ar << S.CorrespondClothSectionIndex;
	}

	if( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH_LOD )
	{
		Ar << S.bEnableClothLOD_DEPRECATED;
	}

	Ar.UsingCustomVersion(FRecomputeTangentCustomVersion::GUID);
	if (Ar.CustomVer(FRecomputeTangentCustomVersion::GUID) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
	{
		Ar << S.bRecomputeTangent;
	}

	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{

		if (!StripFlags.IsDataStrippedForServer())
		{
			// This is so that BaseVertexIndex is never set to anything else that 0 (for safety)
			Ar << S.BaseVertexIndex;
		}

		if (!StripFlags.IsEditorDataStripped())
		{
			// For backwards compat, read rigid vert array into array
			TArray<FLegacyRigidSkinVertex> LegacyRigidVertices;
			if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				Ar << LegacyRigidVertices;
			}

			Ar << S.SoftVertices;

			// Once we have read in SoftVertices, convert and insert legacy rigid verts (if present) at start
			const int32 NumRigidVerts = LegacyRigidVertices.Num();
			if (NumRigidVerts > 0 && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				S.SoftVertices.InsertUninitialized(0, NumRigidVerts);

				for (int32 VertIdx = 0; VertIdx < NumRigidVerts; VertIdx++)
				{
					LegacyRigidVertices[VertIdx].ConvertToSoftVert(S.SoftVertices[VertIdx]);
				}
			}
		}

		// If loading content newer than CombineSectionWithChunk but older than SaveNumVertices, update NumVertices here
		if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::SaveNumVertices)
		{
			if (!StripFlags.IsDataStrippedForServer())
			{
				S.NumVertices = S.SoftVertices.Num();
			}
			else
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Cannot set FSkelMeshSection::NumVertices for older content, loading in non-editor build."));
				S.NumVertices = 0;
			}
		}

		Ar << S.BoneMap;

		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::SaveNumVertices)
		{
			Ar << S.NumVertices;
		}

		// Removed NumRigidVertices and NumSoftVertices
		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
		{
			int32 DummyNumRigidVerts, DummyNumSoftVerts;
			Ar << DummyNumRigidVerts;
			Ar << DummyNumSoftVerts;

			if (DummyNumRigidVerts + DummyNumSoftVerts != S.SoftVertices.Num())
			{
				UE_LOG(LogSkeletalMesh, Error, TEXT("Legacy NumSoftVerts + NumRigidVerts != SoftVertices.Num()"));
			}
		}

		Ar << S.MaxBoneInfluences;

#if WITH_EDITOR
		// If loading content where we need to recalc 'max bone influences' instead of using loaded version, do that now
		if (!StripFlags.IsEditorDataStripped() && Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RecalcMaxBoneInfluences)
		{
			S.CalcMaxBoneInfluences();
		}
#endif

		Ar << S.ApexClothMappingData;
		Ar << S.PhysicalMeshVertices;
		Ar << S.PhysicalMeshNormals;
		Ar << S.CorrespondClothAssetIndex;
		Ar << S.ClothAssetSubmeshIndex;
	}

	return Ar;
}

void FMorphTargetVertexInfoBuffers::InitRHI()
{
	if (PerVertexInfoList.Num() > 0)
	{
		FRHIResourceCreateInfo CreateInfo;
		void* PerVertexInfoListVBData = nullptr;
		PerVertexInfoVB = RHICreateAndLockVertexBuffer(PerVertexInfoList.GetAllocatedSize(), BUF_Static | BUF_ShaderResource, CreateInfo, PerVertexInfoListVBData);
		FMemory::Memcpy(PerVertexInfoListVBData, PerVertexInfoList.GetData(), PerVertexInfoList.GetAllocatedSize());
		RHIUnlockVertexBuffer(PerVertexInfoVB);
		PerVertexInfoSRV = RHICreateShaderResourceView(PerVertexInfoVB, sizeof(uint32), PF_R32_UINT);

		void* FlattenedDeltasVBData = nullptr;
		FlattenedDeltasVB = RHICreateAndLockVertexBuffer(FlattenedDeltaList.GetAllocatedSize(), BUF_Static | BUF_ShaderResource, CreateInfo, FlattenedDeltasVBData);
		FMemory::Memcpy(FlattenedDeltasVBData, FlattenedDeltaList.GetData(), FlattenedDeltaList.GetAllocatedSize());
		RHIUnlockVertexBuffer(FlattenedDeltasVB);
		FlattenedDeltasSRV = RHICreateShaderResourceView(FlattenedDeltasVB, sizeof(uint32), PF_R32_UINT);

		NumInfluencedVerticesByMorphs = (uint32)PerVertexInfoList.Num();

		PerVertexInfoList.Empty();
		FlattenedDeltaList.Empty();
	}
}

void FMorphTargetVertexInfoBuffers::ReleaseRHI()
{
	PerVertexInfoVB.SafeRelease();
	PerVertexInfoSRV.SafeRelease();
	FlattenedDeltasVB.SafeRelease();
	FlattenedDeltasSRV.SafeRelease();
}

/*-----------------------------------------------------------------------------
	FStaticLODModel
-----------------------------------------------------------------------------*/

/** Legacy Chunk struct, now merged with FSkelMeshSection */
struct FLegacySkelMeshChunk
{
	uint32 BaseVertexIndex;
	TArray<FSoftSkinVertex> SoftVertices;
	TArray<FApexClothPhysToRenderVertData> ApexClothMappingData;
	TArray<FVector> PhysicalMeshVertices;
	TArray<FVector> PhysicalMeshNormals;
	TArray<FBoneIndexType> BoneMap;
	int32 MaxBoneInfluences;

	int16 CorrespondClothAssetIndex;
	int16 ClothAssetSubmeshIndex;

	FLegacySkelMeshChunk()
		: BaseVertexIndex(0)
		, MaxBoneInfluences(4)
		, CorrespondClothAssetIndex(INDEX_NONE)
		, ClothAssetSubmeshIndex(INDEX_NONE)
	{}

	void CopyToSection(FSkelMeshSection& Section)
	{
		Section.BaseVertexIndex = BaseVertexIndex;
		Section.SoftVertices = SoftVertices;
		Section.ApexClothMappingData = ApexClothMappingData;
		Section.PhysicalMeshVertices = PhysicalMeshVertices;
		Section.PhysicalMeshNormals = PhysicalMeshNormals;
		Section.BoneMap = BoneMap;
		Section.MaxBoneInfluences = MaxBoneInfluences;
		Section.CorrespondClothAssetIndex = CorrespondClothAssetIndex;
		Section.ClothAssetSubmeshIndex = ClothAssetSubmeshIndex;
	}


	friend FArchive& operator<<(FArchive& Ar, FLegacySkelMeshChunk& C)
	{
		FStripDataFlags StripFlags(Ar);

		if (!StripFlags.IsDataStrippedForServer())
		{
			// This is so that BaseVertexIndex is never set to anything else that 0 (for safety)
			Ar << C.BaseVertexIndex;
		}
		if (!StripFlags.IsEditorDataStripped())
		{
			// For backwards compat, read rigid vert array into array
			TArray<FLegacyRigidSkinVertex> LegacyRigidVertices;
			if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				Ar << LegacyRigidVertices;
			}

			Ar << C.SoftVertices;

			// Once we have read in SoftVertices, convert and insert legacy rigid verts (if present) at start
			const int32 NumRigidVerts = LegacyRigidVertices.Num();
			if (NumRigidVerts > 0 && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				C.SoftVertices.InsertUninitialized(0, NumRigidVerts);

				for (int32 VertIdx = 0; VertIdx < NumRigidVerts; VertIdx++)
				{
					LegacyRigidVertices[VertIdx].ConvertToSoftVert(C.SoftVertices[VertIdx]);
				}
			}
		}
		Ar << C.BoneMap;

		// Removed NumRigidVertices and NumSoftVertices, just use array size
		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
		{
			int32 DummyNumRigidVerts, DummyNumSoftVerts;
			Ar << DummyNumRigidVerts;
			Ar << DummyNumSoftVerts;

			if (DummyNumRigidVerts + DummyNumSoftVerts != C.SoftVertices.Num())
			{
				UE_LOG(LogSkeletalMesh, Error, TEXT("Legacy NumSoftVerts + NumRigidVerts != SoftVertices.Num()"));
			}
		}

		Ar << C.MaxBoneInfluences;


		if (Ar.UE4Ver() >= VER_UE4_APEX_CLOTH)
		{
			Ar << C.ApexClothMappingData;
			Ar << C.PhysicalMeshVertices;
			Ar << C.PhysicalMeshNormals;
			Ar << C.CorrespondClothAssetIndex;
			Ar << C.ClothAssetSubmeshIndex;
		}

		return Ar;
	}
};

void FStaticLODModel::Serialize( FArchive& Ar, UObject* Owner, int32 Idx )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStaticLODModel::Serialize"), STAT_StaticLODModel_Serialize, STATGROUP_LoadTime );

	const uint8 LodAdjacencyStripFlag = 1;
	FStripDataFlags StripFlags( Ar, Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::Tessellation) ? LodAdjacencyStripFlag : 0 );

	// Skeletal mesh buffers are kept in CPU memory after initialization to support merging of skeletal meshes.
	bool bKeepBuffersInCPUMemory = true;
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FreeSkeletalMeshBuffers"));
	if(CVar)
	{
		bKeepBuffersInCPUMemory = !CVar->GetValueOnAnyThread();
	}

	Ar << Sections;
	MultiSizeIndexContainer.Serialize(Ar, bKeepBuffersInCPUMemory);
	Ar << ActiveBoneIndices;

	// Array of Sections for backwards compat
	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{
		TArray<FLegacySkelMeshChunk> LegacyChunks;

		Ar << LegacyChunks;

		check(LegacyChunks.Num() == Sections.Num());
		for (int32 ChunkIdx = 0; ChunkIdx < LegacyChunks.Num(); ChunkIdx++)
		{
			FSkelMeshSection& Section = Sections[ChunkIdx];

			LegacyChunks[ChunkIdx].CopyToSection(Section);

			// Set NumVertices for older content on load
			if (!StripFlags.IsDataStrippedForServer())
			{
				Section.NumVertices = Section.SoftVertices.Num();
			}
			else
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Cannot set FSkelMeshSection::NumVertices for older content, loading in non-editor build."));
				Section.NumVertices = 0;
			}
		}
	}
	
	// no longer in use
	{
		uint32 LegacySize = 0;
		Ar << LegacySize;
	}

	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << NumVertices;
	}
	Ar << RequiredBones;

	if( !StripFlags.IsEditorDataStripped() )
	{
		RawPointIndices.Serialize( Ar, Owner );
	}

	Ar << MeshToImportVertexMap;
	Ar << MaxImportVertex;

	if( !StripFlags.IsDataStrippedForServer() )
	{
		USkeletalMesh* SkelMeshOwner = CastChecked<USkeletalMesh>(Owner);

		if( Ar.IsLoading() )
		{
			// set cpu skinning flag on the vertex buffer so that the resource arrays know if they need to be CPU accessible
			VertexBufferGPUSkin.SetNeedsCPUAccess(bKeepBuffersInCPUMemory || SkelMeshOwner->GetImportedResource()->RequiresCPUSkinning(GMaxRHIFeatureLevel));
		}
		Ar << NumTexCoords;
		Ar << VertexBufferGPUSkin;
		if( SkelMeshOwner->bHasVertexColors)
		{
			Ar << ColorVertexBuffer;
		}

		if ( !StripFlags.IsClassDataStripped( LodAdjacencyStripFlag ) )
		{
			AdjacencyMultiSizeIndexContainer.Serialize(Ar,bKeepBuffersInCPUMemory);
		}

		if ( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH && HasApexClothData() )
		{
			Ar << APEXClothVertexBuffer;
		}

		// validate sections and reset incorrect sorting mode
		if ( Ar.IsLoading() )
		{
			const int32 kNumIndicesPerPrimitive = 3;
			const int32 kNumSetsOfIndices = 2;
			for (int32 IdxSection = 0, PreLastSection = Sections.Num() - 1; IdxSection < PreLastSection; ++IdxSection)
			{
				FSkelMeshSection & Section = Sections[ IdxSection ];
				if (TRISORT_CustomLeftRight == Section.TriangleSorting)
				{
					uint32 IndicesInSection = Sections[ IdxSection + 1 ].BaseIndex - Section.BaseIndex;
					if (Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices > IndicesInSection)
					{
						UE_LOG(LogSkeletalMesh, Warning, TEXT( "Section %d in LOD model %d of object %s doesn't have enough indices (%d, while %d are needed) to allow TRISORT_CustomLeftRight mode, resetting to TRISORT_None" ),
							IdxSection, Idx, *Owner->GetName(),
							IndicesInSection, Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices
							);
						Section.TriangleSorting = TRISORT_None;
					}
				}
			}

			// last section is special case
			FSkelMeshSection & Section = Sections[ Sections.Num() - 1 ];
			if (TRISORT_CustomLeftRight == Section.TriangleSorting)
			{
				uint32 IndicesInSection = MultiSizeIndexContainer.GetIndexBuffer()->Num() - Sections[ Sections.Num() - 1 ].BaseIndex;
				if (Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices > IndicesInSection)
				{
					UE_LOG(LogSkeletalMesh, Warning, TEXT( "Section %d in LOD model %d of object %s doesn't have enough indices (%d, while %d are needed) to allow TRISORT_CustomLeftRight mode, resetting to TRISORT_None" ),
						Sections.Num() - 1, Idx, *Owner->GetName(),
						IndicesInSection, Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices
						);
					Section.TriangleSorting = TRISORT_None;
				}
			}
		}
	}
}

void FStaticLODModel::InitResources(bool bNeedsVertexColors, int32 LODIndex, TArray<UMorphTarget*>& InMorphTargets)
{
	INC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	
	MultiSizeIndexContainer.InitResources();

	INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, VertexBufferGPUSkin.GetVertexDataSize() );
	BeginInitResource(&VertexBufferGPUSkin);

	if( bNeedsVertexColors )
	{	
		// Only init the color buffer if the mesh has vertex colors
		INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ColorVertexBuffer.GetVertexDataSize() );
		BeginInitResource(&ColorVertexBuffer);
	}

	if ( HasApexClothData() )
	{
		// Only init the color buffer if the mesh has vertex colors
		INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, APEXClothVertexBuffer.GetVertexDataSize() );
		BeginInitResource(&APEXClothVertexBuffer);
	}

	if( RHISupportsTessellation(GMaxRHIShaderPlatform) ) 
	{
		AdjacencyMultiSizeIndexContainer.InitResources();
		INC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	}

	if (RHISupportsComputeShaders(GMaxRHIShaderPlatform) && InMorphTargets.Num() > 0)
	{
		// Populate the arrays to be filled in later in the render thread

		// Auxiliary mapping for affected vertex indices by morph to its weights
		TMap<uint32, TArray<FMorphTargetVertexInfoBuffers::FFlattenedDelta>> AuxDeltaList;

		int32 TotalNumDeltas = 0;
		for (int32 AnimIdx = 0; AnimIdx < InMorphTargets.Num(); ++AnimIdx)
		{
			UMorphTarget* MorphTarget = InMorphTargets[AnimIdx];

			int32 NumSrcDeltas = 0;
			FMorphTargetDelta* SrcDelta = MorphTarget->GetMorphTargetDelta(LODIndex, NumSrcDeltas);
			for (int32 SrcDeltaIndex = 0; SrcDeltaIndex < NumSrcDeltas; ++SrcDeltaIndex, ++SrcDelta)
			{
				TArray<FMorphTargetVertexInfoBuffers::FFlattenedDelta>& FlattenedDeltas = AuxDeltaList.FindOrAdd(SrcDelta->SourceIdx);
				FMorphTargetVertexInfoBuffers::FFlattenedDelta* NewDelta = new(FlattenedDeltas) FMorphTargetVertexInfoBuffers::FFlattenedDelta;
				NewDelta->PosDelta = SrcDelta->PositionDelta;
				NewDelta->TangentDelta = SrcDelta->TangentZDelta;
				NewDelta->WeightIndex = AnimIdx;
				++TotalNumDeltas;
			}
		}

		MorphTargetVertexInfoBuffers.FlattenedDeltaList.Empty(TotalNumDeltas);
		MorphTargetVertexInfoBuffers.PerVertexInfoList.AddUninitialized(AuxDeltaList.Num());
		int32 StartDelta = 0;
		FMorphTargetVertexInfoBuffers::FPerVertexInfo* NewPerVertexInfo = MorphTargetVertexInfoBuffers.PerVertexInfoList.GetData();
		for (auto& Pair : AuxDeltaList)
		{
			NewPerVertexInfo->DestVertexIndex = Pair.Key;
			NewPerVertexInfo->StartDelta = StartDelta;
			NewPerVertexInfo->NumDeltas = Pair.Value.Num();
			MorphTargetVertexInfoBuffers.FlattenedDeltaList.Append(Pair.Value);
			StartDelta += Pair.Value.Num();
			++NewPerVertexInfo;
		}

		if (AuxDeltaList.Num() > 0)
		{
			BeginInitResource(&MorphTargetVertexInfoBuffers);
		}
	}
}

void FStaticLODModel::ReleaseResources()
{
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, VertexBufferGPUSkin.GetVertexDataSize() );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ColorVertexBuffer.GetVertexDataSize() );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, APEXClothVertexBuffer.GetVertexDataSize() );

	MultiSizeIndexContainer.ReleaseResources();
	AdjacencyMultiSizeIndexContainer.ReleaseResources();

	BeginReleaseResource(&VertexBufferGPUSkin);
	BeginReleaseResource(&ColorVertexBuffer);
	BeginReleaseResource(&APEXClothVertexBuffer);
	BeginReleaseResource(&MorphTargetVertexInfoBuffers);
}

int32 FStaticLODModel::GetTotalFaces() const
{
	int32 TotalFaces = 0;
	for(int32 i=0; i<Sections.Num(); i++)
	{
		TotalFaces += Sections[i].NumTriangles;
	}

	return TotalFaces;
}

void FStaticLODModel::GetSectionFromVertexIndex(int32 InVertIndex, int32& OutSectionIndex, int32& OutVertIndex, bool& bOutHasExtraBoneInfluences) const
{
	OutSectionIndex = 0;
	OutVertIndex = 0;
	bOutHasExtraBoneInfluences = false;

	int32 VertCount = 0;

	// Iterate over each chunk
	for(int32 SectionCount = 0; SectionCount < Sections.Num(); SectionCount++)
	{
		const FSkelMeshSection& Section = Sections[SectionCount];
		OutSectionIndex = SectionCount;

		// Is it in Soft vertex range?
		if(InVertIndex < VertCount + Section.GetNumVertices())
		{
			OutVertIndex = InVertIndex - VertCount;
			bOutHasExtraBoneInfluences = VertexBufferGPUSkin.HasExtraBoneInfluences();
			return;
		}
		VertCount += Section.GetNumVertices();
	}

	// InVertIndex should always be in some chunk!
	//check(false);
}

void FStaticLODModel::GetVertices(TArray<FSoftSkinVertex>& Vertices) const
{
	Vertices.Empty(NumVertices);
	Vertices.AddUninitialized(NumVertices);
	
	// Initialize the vertex data
	// All chunks are combined into one (rigid first, soft next)
	FSoftSkinVertex* DestVertex = (FSoftSkinVertex*)Vertices.GetData();
	for(int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];
		FMemory::Memcpy(DestVertex, Section.SoftVertices.GetData(), Section.SoftVertices.Num() * sizeof(FSoftSkinVertex));
		DestVertex += Section.SoftVertices.Num();
	}
}

void FStaticLODModel::GetApexClothMappingData(TArray<FApexClothPhysToRenderVertData>& MappingData) const
{
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];

		if(Section.ApexClothMappingData.Num() == 0 )
		{
			int32 PrevNum = MappingData.Num();

			MappingData.AddZeroed(Section.SoftVertices.Num());
			for(int32 i=PrevNum; i<MappingData.Num(); i++)
			{
				MappingData[i].PositionBaryCoordsAndDist[0] = 1.0f;
				MappingData[i].NormalBaryCoordsAndDist[0] = 1.0f;
				MappingData[i].TangentBaryCoordsAndDist[0] = 1.0f;
				// set max number to verify this is not the clothing section
				MappingData[i].SimulMeshVertIndices[0] = 0xFFFF;
			}
		}
		else
		{
			MappingData += Section.ApexClothMappingData;
		}
	}
}

void FStaticLODModel::BuildVertexBuffers(uint32 BuildFlags)
{
	bool bUseFullPrecisionUVs = (BuildFlags & EVertexFlags::UseFullPrecisionUVs) != 0;
	bool bHasVertexColors = (BuildFlags & EVertexFlags::HasVertexColors) != 0;

	TArray<FSoftSkinVertex> Vertices;
	GetVertices(Vertices);

	// match UV precision for mesh vertex buffer to setting from parent mesh
	VertexBufferGPUSkin.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
	// keep the buffer in CPU memory
	VertexBufferGPUSkin.SetNeedsCPUAccess(true);
	// Set the number of texture coordinate sets
	VertexBufferGPUSkin.SetNumTexCoords( NumTexCoords );

	VertexBufferGPUSkin.SetHasExtraBoneInfluences(DoSectionsNeedExtraBoneInfluences());

	// init vertex buffer with the vertex array
	VertexBufferGPUSkin.Init(Vertices);

	// Init the color buffer if this mesh has vertex colors.
	if( bHasVertexColors )
	{
		ColorVertexBuffer.Init(Vertices);
	}

	if( HasApexClothData() )
	{
		TArray<FApexClothPhysToRenderVertData> MappingData;
		GetApexClothMappingData(MappingData);
		APEXClothVertexBuffer.Init(MappingData);
	}
}

void FStaticLODModel::SortTriangles( FVector SortCenter, bool bUseSortCenter, int32 SectionIndex, ETriangleSortOption NewTriangleSorting )
{
#if WITH_EDITOR
	FSkelMeshSection& Section = Sections[SectionIndex];
	if( NewTriangleSorting == Section.TriangleSorting )
	{
		return;
	}

	if( NewTriangleSorting == TRISORT_CustomLeftRight )
	{
		// Make a second copy of index buffer data for this section
		int32 NumNewIndices = Section.NumTriangles*3;
		MultiSizeIndexContainer.GetIndexBuffer()->Insert(Section.BaseIndex, NumNewIndices);
		FMemory::Memcpy( MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(Section.BaseIndex), MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(Section.BaseIndex+NumNewIndices), NumNewIndices*MultiSizeIndexContainer.GetDataTypeSize() );

		// Fix up BaseIndex for indices in other sections
		for( int32 OtherSectionIdx = 0; OtherSectionIdx < Sections.Num(); OtherSectionIdx++ )
		{
			if( Sections[OtherSectionIdx].BaseIndex > Section.BaseIndex )
			{
				Sections[OtherSectionIdx].BaseIndex += NumNewIndices;
			}
		}
	}
	else if( Section.TriangleSorting == TRISORT_CustomLeftRight )
	{
		// Remove the second copy of index buffer data for this section
		int32 NumRemovedIndices = Section.NumTriangles*3;
		MultiSizeIndexContainer.GetIndexBuffer()->Remove(Section.BaseIndex, NumRemovedIndices);
		// Fix up BaseIndex for indices in other sections
		for( int32 OtherSectionIdx = 0; OtherSectionIdx < Sections.Num(); OtherSectionIdx++ )
		{
			if( Sections[OtherSectionIdx].BaseIndex > Section.BaseIndex )
			{
				Sections[OtherSectionIdx].BaseIndex -= NumRemovedIndices;
			}
		}
	}

	TArray<FSoftSkinVertex> Vertices;
	GetVertices(Vertices);

	switch( NewTriangleSorting )
	{
	case TRISORT_None:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_None( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_CenterRadialDistance:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			if (bUseSortCenter)
			{
				SortTriangles_CenterRadialDistance( SortCenter, Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			}
			else
			{
				SortTriangles_CenterRadialDistance( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			}
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_Random:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_Random( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_MergeContiguous:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_MergeContiguous( Section.NumTriangles, NumVertices, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_Custom:
	case TRISORT_CustomLeftRight:
		break;
	}

	Section.TriangleSorting = NewTriangleSorting;
#endif
}

void FStaticLODModel::ReleaseCPUResources()
{	
	if(!GIsEditor && !IsRunningCommandlet())
	{
		if(MultiSizeIndexContainer.IsIndexBufferValid())
		{
			MultiSizeIndexContainer.GetIndexBuffer()->Empty();
		}
		if(AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
		{
			AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Empty();
		}
		if(VertexBufferGPUSkin.IsVertexDataValid())
		{
			VertexBufferGPUSkin.CleanUp();
		}
	}
}

SIZE_T FStaticLODModel::GetResourceSize() const
{
	SIZE_T ResourceSize = 0;

	ResourceSize += Sections.GetAllocatedSize();
	ResourceSize += ActiveBoneIndices.GetAllocatedSize();  
	ResourceSize += RequiredBones.GetAllocatedSize();

	if(MultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* IndexBuffer = MultiSizeIndexContainer.GetIndexBuffer();
		if (IndexBuffer)
		{
			ResourceSize += IndexBuffer->GetResourceDataSize(); 
		}
	}

	if(AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* AdjacentIndexBuffer = AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
		if(AdjacentIndexBuffer)
		{
			ResourceSize += AdjacentIndexBuffer->GetResourceDataSize();
		}
	}

	ResourceSize += VertexBufferGPUSkin.GetVertexDataSize();
	ResourceSize += ColorVertexBuffer.GetVertexDataSize();
	ResourceSize += APEXClothVertexBuffer.GetVertexDataSize();

	ResourceSize += RawPointIndices.GetBulkDataSize();
	ResourceSize += LegacyRawPointIndices.GetBulkDataSize();
	ResourceSize += MeshToImportVertexMap.GetAllocatedSize();

	// I suppose we add everything we could
	ResourceSize += sizeof(int32);

	return ResourceSize;
}

void FStaticLODModel::RebuildIndexBuffer(FMultiSizeIndexContainerData* IndexBufferData, FMultiSizeIndexContainerData* AdjacencyIndexBufferData)
{
	if (IndexBufferData)
	{
		MultiSizeIndexContainer.RebuildIndexBuffer(*IndexBufferData);
	}

	if (AdjacencyIndexBufferData)
	{
		AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(*AdjacencyIndexBufferData);
	}
}

#if WITH_EDITOR
void FStaticLODModel::RebuildIndexBuffer()
{
	// The index buffer needs to be rebuilt on copy.
	FMultiSizeIndexContainerData IndexBufferData;
	MultiSizeIndexContainer.GetIndexBufferData(IndexBufferData);

	FMultiSizeIndexContainerData AdjacencyIndexBufferData;
	AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyIndexBufferData);

	RebuildIndexBuffer(&IndexBufferData, &AdjacencyIndexBufferData);
}
#endif
/*-----------------------------------------------------------------------------
FStaticMeshSourceData
-----------------------------------------------------------------------------*/

/**
 * FSkeletalMeshSourceData - Source triangles and render data, editor-only.
 */
class FSkeletalMeshSourceData
{
public:
	FSkeletalMeshSourceData();
	~FSkeletalMeshSourceData();

#if WITH_EDITOR
	/** Initialize from static mesh render data. */
	void Init( const class USkeletalMesh* SkeletalMesh, FStaticLODModel& LODModel );

	/** Retrieve render data. */
	FORCEINLINE FStaticLODModel* GetModel() { return LODModel; }
#endif // #if WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Free source data. */
	void Clear();
#endif // WITH_EDITORONLY_DATA

	/** Returns true if the source data has been initialized. */
	FORCEINLINE bool IsInitialized() const { return LODModel != NULL; }

	/** Serialization. */
	void Serialize( FArchive& Ar, USkeletalMesh* SkeletalMesh );

private:
	FStaticLODModel* LODModel;
};


FSkeletalMeshSourceData::FSkeletalMeshSourceData() : LODModel( NULL )
{
}

FSkeletalMeshSourceData::~FSkeletalMeshSourceData()
{
	delete LODModel;
	LODModel = NULL;
}

#if WITH_EDITOR

/** Initialize from static mesh render data. */
void FSkeletalMeshSourceData::Init( const USkeletalMesh* SkeletalMesh, FStaticLODModel& InLODModel )
{
	check( LODModel == NULL );

	/** Bulk data arrays need to be locked before a copy can be made. */
	InLODModel.RawPointIndices.Lock( LOCK_READ_ONLY );
	InLODModel.LegacyRawPointIndices.Lock( LOCK_READ_ONLY );

	/** Allocate a new LOD model to hold the data and copy everything over. */
	LODModel = new FStaticLODModel();
	*LODModel = InLODModel;

	/** Unlock the arrays as the copy has been made. */
	InLODModel.RawPointIndices.Unlock();
	InLODModel.LegacyRawPointIndices.Unlock();

	/** The index buffer needs to be rebuilt on copy. */
	FMultiSizeIndexContainerData IndexBufferData, AdjacencyIndexBufferData;
	InLODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexBufferData );
	InLODModel.AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyIndexBufferData);
	LODModel->RebuildIndexBuffer( &IndexBufferData, &AdjacencyIndexBufferData );

	/** Vertex buffers also need to be rebuilt. Source data is always stored with full precision position data. */
	LODModel->BuildVertexBuffers(SkeletalMesh->GetVertexBufferFlags());
}

#endif // #if WITH_EDITOR

#if WITH_EDITORONLY_DATA

/** Free source data. */
void FSkeletalMeshSourceData::Clear()
{
	delete LODModel;
	LODModel = NULL;
}

#endif // #if WITH_EDITORONLY_DATA

/** Serialization. */
void FSkeletalMeshSourceData::Serialize( FArchive& Ar, USkeletalMesh* SkeletalMesh )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FSkeletalMeshSourceData::Serialize"), STAT_SkeletalMeshSourceData_Serialize, STATGROUP_LoadTime );

	if ( Ar.IsLoading() )
	{
		bool bHaveSourceData = false;
		Ar << bHaveSourceData;
		if ( bHaveSourceData )
		{
			if ( LODModel != NULL )
			{
				delete LODModel;
				LODModel = NULL;
			}
			LODModel = new FStaticLODModel();
			LODModel->Serialize( Ar, SkeletalMesh, INDEX_NONE );
		}
	}
	else
	{
		bool bHaveSourceData = IsInitialized();
		Ar << bHaveSourceData;
		if ( bHaveSourceData )
		{
			LODModel->Serialize( Ar, SkeletalMesh, INDEX_NONE );
		}
	}
}



/*-----------------------------------------------------------------------------
FreeSkeletalMeshBuffersSinkCallback
-----------------------------------------------------------------------------*/

void FreeSkeletalMeshBuffersSinkCallback()
{
	// If r.FreeSkeletalMeshBuffers==1 then CPU buffer copies are to be released.
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FreeSkeletalMeshBuffers"));
	bool bFreeSkeletalMeshBuffers = CVar->GetValueOnGameThread() == 1;
	if(bFreeSkeletalMeshBuffers)
	{
		FlushRenderingCommands();
		for (TObjectIterator<USkeletalMesh> It;It;++It)
		{
			if (!It->GetImportedResource()->RequiresCPUSkinning(GMaxRHIFeatureLevel))
			{
				It->ReleaseCPUResources();
			}
		}
	}
}

/*-----------------------------------------------------------------------------
USkeletalMesh
-----------------------------------------------------------------------------*/

/**
* Calculate max # of bone influences used by this skel mesh chunk
*/
void FSkelMeshSection::CalcMaxBoneInfluences()
{
	// if we only have rigid verts then there is only one bone
	MaxBoneInfluences = 1;
	// iterate over all the soft vertices for this chunk and find max # of bones used
	for( int32 VertIdx=0; VertIdx < SoftVertices.Num(); VertIdx++ )
	{
		FSoftSkinVertex& SoftVert = SoftVertices[VertIdx];

		// calc # of bones used by this soft skinned vertex
		int32 BonesUsed=0;
		for( int32 InfluenceIdx=0; InfluenceIdx < MAX_TOTAL_INFLUENCES; InfluenceIdx++ )
		{
			if( SoftVert.InfluenceWeights[InfluenceIdx] > 0 )
			{
				BonesUsed++;
			}
		}
		// reorder bones so that there aren't any unused influence entries within the [0,BonesUsed] range
		for( int32 InfluenceIdx=0; InfluenceIdx < BonesUsed; InfluenceIdx++ )
		{
			if( SoftVert.InfluenceWeights[InfluenceIdx] == 0 )
			{
				for( int32 ExchangeIdx=InfluenceIdx+1; ExchangeIdx < MAX_TOTAL_INFLUENCES; ExchangeIdx++ )
				{
					if( SoftVert.InfluenceWeights[ExchangeIdx] != 0 )
					{
						Exchange(SoftVert.InfluenceWeights[InfluenceIdx],SoftVert.InfluenceWeights[ExchangeIdx]);
						Exchange(SoftVert.InfluenceBones[InfluenceIdx],SoftVert.InfluenceBones[ExchangeIdx]);
						break;
					}
				}
			}
		}

		// maintain max bones used
		MaxBoneInfluences = FMath::Max(MaxBoneInfluences,BonesUsed);			
	}
}

/*-----------------------------------------------------------------------------
	FClothingAssetData
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive& Ar, FClothingAssetData& A)
{
	// Serialization to load and save ApexClothingAsset
	if( Ar.IsLoading() )
	{
		uint32 AssetSize;
		Ar << AssetSize;

		if( AssetSize > 0 )
		{
			// Load the binary blob data
			TArray<uint8> Buffer;
			Buffer.AddUninitialized( AssetSize );
			Ar.Serialize( Buffer.GetData(), AssetSize );
#if WITH_APEX_CLOTHING
			A.ApexClothingAsset = LoadApexClothingAssetFromBlob(Buffer);
#endif //#if WITH_APEX_CLOTHING
		}
	}
	else
	if( Ar.IsSaving() )
	{
#if WITH_APEX_CLOTHING
		if (A.ApexClothingAsset)
		{
			TArray<uint8> Buffer;
			SaveApexClothingAssetToBlob(A.ApexClothingAsset, Buffer);
			uint32 AssetSize = Buffer.Num();
			Ar << AssetSize;
			Ar.Serialize(Buffer.GetData(), AssetSize);
		}
		else
#endif// #if WITH_APEX_CLOTHING
		{
			uint32 AssetSize = 0;
			Ar << AssetSize;
		}
	}

	return Ar;
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshResource
-----------------------------------------------------------------------------*/

FSkeletalMeshResource::FSkeletalMeshResource()
	: bInitialized(false)
{
}

void FSkeletalMeshResource::InitResources(bool bNeedsVertexColors, TArray<UMorphTarget*>& InMorphTargets)
{
	if (!bInitialized)
	{
		// initialize resources for each lod
		for( int32 LODIndex = 0;LODIndex < LODModels.Num();LODIndex++ )
		{
			LODModels[LODIndex].InitResources(bNeedsVertexColors, LODIndex, InMorphTargets);
		}
		bInitialized = true;
	}
}

void FSkeletalMeshResource::ReleaseResources()
{
	if (bInitialized)
	{
		// release resources for each lod
		for( int32 LODIndex = 0;LODIndex < LODModels.Num();LODIndex++ )
		{
			LODModels[LODIndex].ReleaseResources();
		}
		bInitialized = false;
	}
}

void FSkeletalMeshResource::Serialize(FArchive& Ar, USkeletalMesh* Owner)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FSkeletalMeshResource::Serialize"), STAT_SkeletalMeshResource_Serialize, STATGROUP_LoadTime );

	LODModels.Serialize(Ar,Owner);
}

bool FSkeletalMeshResource::HasExtraBoneInfluences() const
{
	for (int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];
		if (Model.DoSectionsNeedExtraBoneInfluences())
		{
			return true;
		}
	}

	return false;
}

int32 FSkeletalMeshResource::GetMaxBonesPerSection() const
{
	int32 MaxBonesPerSection = 0;
	for (int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < Model.Sections.Num(); ++SectionIndex)
		{
			MaxBonesPerSection = FMath::Max<int32>(MaxBonesPerSection, Model.Sections[SectionIndex].BoneMap.Num());
		}
	}
	return MaxBonesPerSection;
}

bool FSkeletalMeshResource::RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel) const
{
	const int32 MaxGPUSkinBones = GetFeatureLevelMaxNumberOfBones(FeatureLevel);
	const int32 MaxBonesPerChunk = GetMaxBonesPerSection();
	// Do CPU skinning if we need too many bones per chunk, or if we have too many influences per vertex on lower end
	return (MaxBonesPerChunk > MaxGPUSkinBones) || (HasExtraBoneInfluences() && FeatureLevel < ERHIFeatureLevel::ES3_1);
}

SIZE_T FSkeletalMeshResource::GetResourceSize()
{
	SIZE_T ResourceSize = 0;
	for(int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];

		ResourceSize += Model.GetResourceSize();
	}

	return ResourceSize;
}

/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/
USkeletalMesh::USkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkelMirrorAxis = EAxis::X;
	SkelMirrorFlipAxis = EAxis::Z;
	StreamingDistanceMultiplier = 1.0f;
#if WITH_EDITORONLY_DATA
	SelectedEditorSection = INDEX_NONE;
#endif
	ImportedResource = MakeShareable(new FSkeletalMeshResource());
}

void USkeletalMesh::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

FBoxSphereBounds USkeletalMesh::GetBounds()
{
	return ExtendedBounds;
}

FBoxSphereBounds USkeletalMesh::GetImportedBounds()
{
	return ImportedBounds;
}

void USkeletalMesh::SetImportedBounds(const FBoxSphereBounds& InBounds)
{
	ImportedBounds = InBounds;
	CalculateExtendedBounds();
}

void USkeletalMesh::SetPositiveBoundsExtension(const FVector& InExtension)
{
	PositiveBoundsExtension = InExtension;
	CalculateExtendedBounds();
}

void USkeletalMesh::SetNegativeBoundsExtension(const FVector& InExtension)
{
	NegativeBoundsExtension = InExtension;
	CalculateExtendedBounds();
}

void USkeletalMesh::CalculateExtendedBounds()
{
	FBoxSphereBounds CalculatedBounds = ImportedBounds;

	// Convert to Min and Max
	FVector Min = CalculatedBounds.Origin - CalculatedBounds.BoxExtent;
	FVector Max = CalculatedBounds.Origin + CalculatedBounds.BoxExtent;
	// Apply bound extensions
	Min -= NegativeBoundsExtension;
	Max += PositiveBoundsExtension;
	// Convert back to Origin, Extent and update SphereRadius
	CalculatedBounds.Origin = (Min + Max) / 2;
	CalculatedBounds.BoxExtent = (Max - Min) / 2;
	CalculatedBounds.SphereRadius = CalculatedBounds.BoxExtent.GetAbsMax();

	ExtendedBounds = CalculatedBounds;
}

void USkeletalMesh::ValidateBoundsExtension()
{
	FVector HalfExtent = ImportedBounds.BoxExtent;

	PositiveBoundsExtension.X = FMath::Clamp(PositiveBoundsExtension.X, -HalfExtent.X, MAX_flt);
	PositiveBoundsExtension.Y = FMath::Clamp(PositiveBoundsExtension.Y, -HalfExtent.Y, MAX_flt);
	PositiveBoundsExtension.Z = FMath::Clamp(PositiveBoundsExtension.Z, -HalfExtent.Z, MAX_flt);

	NegativeBoundsExtension.X = FMath::Clamp(NegativeBoundsExtension.X, -HalfExtent.X, MAX_flt);
	NegativeBoundsExtension.Y = FMath::Clamp(NegativeBoundsExtension.Y, -HalfExtent.Y, MAX_flt);
	NegativeBoundsExtension.Z = FMath::Clamp(NegativeBoundsExtension.Z, -HalfExtent.Z, MAX_flt);
}

void USkeletalMesh::InitResources()
{
	ImportedResource->InitResources(bHasVertexColors, MorphTargets);
}


void USkeletalMesh::ReleaseResources()
{
	ImportedResource->ReleaseResources();
	// insert a fence to signal when these commands completed
	ReleaseResourcesFence.BeginFence();
}

// @param InStreamingDistanceMultiplier should be >= 0
template <bool bExtraBoneInfluencesT>
static void GetStreamingTextureFactorForLOD(FStaticLODModel& LODModel, TArray<float>& CachedStreamingTextureFactors, float InStreamingDistanceMultiplier)
{
	int32 NumTotalTriangles = LODModel.GetTotalFaces();

	struct FTriangleInfo
	{
		FTriangleInfo(float InAera, float InTexelRatio) : Aera(InAera), TexelRatio(InTexelRatio) {}
		float Aera;
		float TexelRatio;
	};


	TArray<FTriangleInfo> TexelRatios[MAX_TEXCOORDS];
	float MaxTexelRatio = 0.0f;
	for(int32 UVIndex = 0;UVIndex < MAX_TEXCOORDS;UVIndex++)
	{
		TexelRatios[UVIndex].Empty( NumTotalTriangles );
	}

	for( int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num();SectionIndex++ )
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		TArray<uint32> Indices;
		LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );

		if (Indices.Num())
		{
			const uint32* SrcIndices = Indices.GetData() + Section.BaseIndex;
			uint32 NumTriangles = Section.NumTriangles;

			// Figure out Unreal unit per texel ratios.
			for (uint32 TriangleIndex=0; TriangleIndex < NumTriangles; TriangleIndex++ )
			{
				//retrieve indices
				uint32 Index0 = SrcIndices[TriangleIndex*3];
				uint32 Index1 = SrcIndices[TriangleIndex*3+1];
				uint32 Index2 = SrcIndices[TriangleIndex*3+2];

				const FVector Pos0 = LODModel.VertexBufferGPUSkin.GetVertexPositionFast<bExtraBoneInfluencesT>(Index0);
				const FVector Pos1 = LODModel.VertexBufferGPUSkin.GetVertexPositionFast<bExtraBoneInfluencesT>(Index1);
				const FVector Pos2 = LODModel.VertexBufferGPUSkin.GetVertexPositionFast<bExtraBoneInfluencesT>(Index2);

				FVector P01 = Pos1 - Pos0;
				FVector P02 = Pos2 - Pos0;

				float Aera = FVector::CrossProduct(P01, P02).Size();

				if (Aera > SMALL_NUMBER)
				{
					float L1 = (Pos0 - Pos1).Size();
					float L2 = (Pos0 - Pos2).Size();

					int32 NumUVs = LODModel.NumTexCoords;
					for(int32 UVIndex = 0;UVIndex < FMath::Min(NumUVs,(int32)MAX_TEXCOORDS);UVIndex++)
					{
						const FVector2D UV0 = LODModel.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluencesT>(Index0, UVIndex);
						const FVector2D UV1 = LODModel.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluencesT>(Index1, UVIndex);
						const FVector2D UV2 = LODModel.VertexBufferGPUSkin.GetVertexUVFast<bExtraBoneInfluencesT>(Index2, UVIndex);

						float T1 = (UV0 - UV1).Size();
						float T2 = (UV0 - UV2).Size();

						if( FMath::Abs(T1 * T2) > FMath::Square(SMALL_NUMBER) )
						{
							const float TexelRatio = FMath::Max( L1 / T1, L2 / T2 );
							TexelRatios[UVIndex].Add( FTriangleInfo(Aera, TexelRatio) );

							// Update max texel ratio
							if( TexelRatio > MaxTexelRatio )
							{
								MaxTexelRatio = TexelRatio;
							}
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogSkeletalMesh,Warning,TEXT("GetStreamingTextureFactor called but section %d has no indices."),SectionIndex);
		}
	}

	for(int32 UVIndex = 0;UVIndex < MAX_TEXCOORDS;UVIndex++)
	{
		TArray<FTriangleInfo>& TriangleInfos = TexelRatios[UVIndex];
		float WeightedTexelFactorSum = 0;
		float AreaSum = 0;

		if( TriangleInfos.Num() )
		{
			struct FCompareTexelRatio
			{
				FORCEINLINE bool operator()(FTriangleInfo const& A, FTriangleInfo const& B) const { return A.TexelRatio < B.TexelRatio; }
			};

			TriangleInfos.Sort( FCompareTexelRatio() );

			// Disregard upper 10% of texel ratios.
			// This is to ignore backfacing surfaces or other non-visible surfaces that tend to map a small number of texels to a large surface.
			int32 Threshold = FMath::FloorToInt(.10f * (float)TriangleInfos.Num());
			for (int32 Index = Threshold; Index < TriangleInfos.Num() - Threshold; ++Index)
			{
				WeightedTexelFactorSum += TriangleInfos[Index].TexelRatio * TriangleInfos[Index].Aera;
				AreaSum += TriangleInfos[Index].Aera;
			}

			if (AreaSum != 0)
			{
				CachedStreamingTextureFactors[UVIndex] = WeightedTexelFactorSum / AreaSum;

				if ( UVIndex == 0 )
				{
					CachedStreamingTextureFactors[UVIndex] *= InStreamingDistanceMultiplier;
				}
			}

		}
	}
}


float USkeletalMesh::GetStreamingTextureFactor( int32 RequestedUVIndex )
{
	check(RequestedUVIndex >= 0);
	check(RequestedUVIndex < MAX_TEXCOORDS);

	// If the streaming texture factor cache doesn't have the right number of entries, it needs to be updated.
	if(CachedStreamingTextureFactors.Num() != MAX_TEXCOORDS)
	{
		if(FPlatformProperties::HasEditorOnlyData())
		{
			// Reset the cached texture factors.
			CachedStreamingTextureFactors.Empty(MAX_TEXCOORDS);
			CachedStreamingTextureFactors.AddZeroed(MAX_TEXCOORDS);

			FSkeletalMeshResource* Resource = GetImportedResource();
			FStaticLODModel& LODModel = Resource->LODModels[0];
			if (LODModel.DoesVertexBufferHaveExtraBoneInfluences())
			{
				GetStreamingTextureFactorForLOD<true>(LODModel, CachedStreamingTextureFactors, FMath::Max(0.0f, StreamingDistanceMultiplier));
			}
			else
			{
				GetStreamingTextureFactorForLOD<false>(LODModel, CachedStreamingTextureFactors, FMath::Max(0.0f, StreamingDistanceMultiplier));
			}
		}
		else
		{
			// Streaming texture factors cannot be computed on consoles, since the raw data has been cooked out.
			UE_LOG(LogSkeletalMesh, Log,  TEXT("USkeletalMesh::GetStreamingTextureFactor is being called on the console which is slow.  You need to resave the map to have the editor precalculate the StreamingTextureFactor for:  %s  Please resave your map. If you are calling this directly then we just return 0.0f instead of crashing"), *GetFullName() );
			return 0.0f;
		}		
	}

	return CachedStreamingTextureFactors[RequestedUVIndex];
}


SIZE_T USkeletalMesh::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;
	if (ImportedResource.IsValid())
	{
		ResourceSize += ImportedResource->GetResourceSize();
	}

	if (Mode == EResourceSizeMode::Inclusive)
	{
		for (const auto& MorphTarget : MorphTargets)
		{
			ResourceSize += MorphTarget->GetResourceSize(Mode);
		}

		for (const auto& ClothingAsset : ClothingAssets)
		{
			ResourceSize += ClothingAsset.GetResourceSize();
		}

		TSet<UMaterialInterface*> UniqueMaterials;
		for(int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			UMaterialInterface* Material = Materials[MaterialIndex].MaterialInterface;
			bool bAlreadyCounted = false;
			UniqueMaterials.Add(Material, &bAlreadyCounted);
			if(!bAlreadyCounted && Material)
			{
				ResourceSize += Material->GetResourceSize(Mode);
			}
		}

#if WITH_EDITORONLY_DATA
		ResourceSize += RetargetBasePose.GetAllocatedSize();
#endif

		ResourceSize += RefBasesInvMatrix.GetAllocatedSize();
		ResourceSize += RefSkeleton.GetDataSize();

		if (BodySetup)
		{
			ResourceSize += BodySetup->GetResourceSize(Mode);
		}

		if (PhysicsAsset)
		{
			ResourceSize += PhysicsAsset->GetResourceSize(Mode);
		}
	}

	return ResourceSize;
}

/**
 * Operator for MemCount only
 */
FArchive &operator<<( FArchive& Ar, FTriangleSortSettings& S )
{
	Ar << S.TriangleSorting;
	Ar << S.CustomLeftRightAxis;
	Ar << S.CustomLeftRightBoneName;
	return Ar;
}

/**
 * Operator for MemCount only, so it only serializes the arrays that needs to be counted.
 */
FArchive &operator<<( FArchive& Ar, FSkeletalMeshLODInfo& I )
{
	Ar << I.LODMaterialMap;

	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		Ar << I.bEnableShadowCasting_DEPRECATED;
	}

	Ar << I.TriangleSortSettings;

	return Ar;
}

void RefreshSkelMeshOnPhysicsAssetChange(const USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh)
	{
		for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
			// if PhysicsAssetOverride is NULL, it uses SkeletalMesh Physics Asset, so I'll need to update here
			if  (SkeletalMeshComponent->SkeletalMesh == InSkeletalMesh &&
				 SkeletalMeshComponent->PhysicsAssetOverride == NULL)
			{
				// it needs to recreate IF it already has been created
				if (SkeletalMeshComponent->IsPhysicsStateCreated())
				{
					// do not call SetPhysAsset as it will setup physics asset override
					SkeletalMeshComponent->RecreatePhysicsState();
					SkeletalMeshComponent->UpdateHasValidBodies();
				}
			}
		}
#if WITH_EDITOR
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif // WITH_EDITOR
	}
}

#if WITH_EDITOR
void USkeletalMesh::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (GIsEditor &&
		PropertyAboutToChange &&
		PropertyAboutToChange->GetOuterUField() &&
		PropertyAboutToChange->GetOuterUField()->GetFName() == FName(TEXT("ClothPhysicsProperties")))
	{
		// if this is a member property of ClothPhysicsProperties, don't release render resources to drag sliders smoothly 
		return;
	}
	FlushRenderState();
}

void USkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bFullPrecisionUVsReallyChanged = false;

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	// if this is a member property of ClothPhysicsProperties, skip RestartRenderState to drag ClothPhysicsProperties sliders smoothly
	bool bSkipRestartRenderState = 
		GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetOuterUField() &&
		PropertyThatChanged->GetOuterUField()->GetFName() == FName(TEXT("ClothPhysicsProperties"));	

	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("bUseFullPrecisionUVs")) )
	{
		bFullPrecisionUVsReallyChanged = true;
		if (!bUseFullPrecisionUVs && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
		{
			bUseFullPrecisionUVs = true;
			UE_LOG(LogSkeletalMesh, Warning, TEXT("16 bit UVs not supported. Reverting to 32 bit UVs"));			
			bFullPrecisionUVsReallyChanged = false;
		}
	}

	// Apply any triangle sorting changes
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("TriangleSorting")) )
	{
		FVector SortCenter;
		FSkeletalMeshResource* Resource = GetImportedResource();
		bool bHaveSortCenter = GetSortCenterPoint(SortCenter);
		for( int32 LODIndex = 0;LODIndex < Resource->LODModels.Num();LODIndex++ )
		{
			for( int32 SectionIndex = 0; SectionIndex < Resource->LODModels[LODIndex].Sections.Num();SectionIndex++ )
			{
				Resource->LODModels[LODIndex].SortTriangles( SortCenter, bHaveSortCenter, SectionIndex, (ETriangleSortOption)LODInfo[LODIndex].TriangleSortSettings[SectionIndex].TriangleSorting );
			}
		}
	}
	
	if ( GIsEditor && PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("StreamingDistanceMultiplier") )
	{
		// Allow recalculating the texture factor.
		CachedStreamingTextureFactors.Empty();
		// Recalculate in a few seconds.
		GEngine->TriggerStreamingDataRebuild();
	}

	if (!bSkipRestartRenderState)
	{
		RestartRenderState();
	}

	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("PhysicsAsset")) )
	{
		RefreshSkelMeshOnPhysicsAssetChange(this);
	}

	if( GIsEditor &&
		Cast<UObjectProperty>(PropertyThatChanged) &&
		Cast<UObjectProperty>(PropertyThatChanged)->PropertyClass == UMorphTarget::StaticClass() )
	{
		// A morph target has changed, reinitialize morph target maps
		InitMorphTargets();
	}

	if ( GIsEditor &&
		 PropertyThatChanged &&
		 PropertyThatChanged->GetFName() == FName(TEXT("bEnablePerPolyCollision"))
		)
	{
		BuildPhysicsData();
	}

	if(UProperty* MemberProperty = PropertyChangedEvent.MemberProperty)
	{
		if(MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMesh, PositiveBoundsExtension) ||
			MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMesh, NegativeBoundsExtension))
		{
			// If the bounds extensions change, recalculate extended bounds.
			ValidateBoundsExtension();
			CalculateExtendedBounds();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USkeletalMesh::PostEditUndo()
{
	Super::PostEditUndo();
	for( TObjectIterator<USkinnedMeshComponent> It; It; ++It )
	{
		USkinnedMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == this )
		{
			FComponentReregisterContext Context(MeshComponent);
		}
	}

	if(MorphTargets.Num() > MorphTargetIndexMap.Num())
	{
		// A morph target remove has been undone, reinitialise
		InitMorphTargets();
	}
}
#endif // WITH_EDITOR

static void RecreateRenderState_Internal(const USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh)
	{
		for( TObjectIterator<USkinnedMeshComponent> It; It; ++It )
		{
			USkinnedMeshComponent* MeshComponent = *It;
			if( MeshComponent && 
				!MeshComponent->IsTemplate() &&
				MeshComponent->SkeletalMesh == InSkeletalMesh )
			{
				MeshComponent->RecreateRenderState_Concurrent();
			}
		}
	}
}

void USkeletalMesh::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_APEX_CLOTHING
	// release clothing assets
	for (FClothingAssetData& Data : ClothingAssets)
	{
		if (Data.ApexClothingAsset)
		{
			GPhysCommandHandler->DeferredRelease(Data.ApexClothingAsset);
			Data.ApexClothingAsset = NULL;
		}
	}
#endif // #if WITH_APEX_CLOTHING

	// Release the mesh's render resources.
	ReleaseResources();


}

bool USkeletalMesh::IsReadyForFinishDestroy()
{
	// see if we have hit the resource flush fence
	return ReleaseResourcesFence.IsFenceComplete();
}

#if WITH_APEX_CLOTHING
// convert a bone name from APEX stype to FBX style
FName GetConvertedBoneName(NxClothingAsset* ApexClothingAsset, int32 BoneIndex);


void USkeletalMesh::BuildApexToUnrealBoneMapping()
{
	for(FClothingAssetData& ClothingAsset : ClothingAssets)
	{
		NxClothingAsset* ApexClothingAsset = ClothingAsset.ApexClothingAsset;
		uint32 NumUsedBones = ClothingAsset.ApexClothingAsset->getNumUsedBones();

		ClothingAsset.ApexToUnrealBoneMapping.Empty();
		ClothingAsset.ApexToUnrealBoneMapping.AddUninitialized(NumUsedBones);

		for (uint32 Index = 0; Index < NumUsedBones; Index++)
		{
			FName BoneName = GetConvertedBoneName(ApexClothingAsset, Index);
			int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
			ClothingAsset.ApexToUnrealBoneMapping[Index] = BoneIndex;
		}
	}
}
#endif

void USkeletalMesh::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USkeletalMesh::Serialize"), STAT_SkeletalMesh_Serialize, STATGROUP_LoadTime );

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	FStripDataFlags StripFlags( Ar );

	Ar << ImportedBounds;
	Ar << Materials;

	Ar << RefSkeleton;

	// Serialize the default resource.
	ImportedResource->Serialize( Ar, this );

	// Build adjacency information for meshes that have not yet had it built.
#if WITH_EDITOR
	for ( int32 LODIndex = 0; LODIndex < ImportedResource->LODModels.Num(); ++LODIndex )
	{
		FStaticLODModel& LODModel = ImportedResource->LODModels[ LODIndex ];

		if ( !LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid()
#if WITH_APEX_CLOTHING
		|| (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_APEX_CLOTH_TESSELLATION && LODModel.HasApexClothData())
#endif // WITH_APEX_CLOTHING
			)
		{
			TArray<FSoftSkinVertex> Vertices;
			FMultiSizeIndexContainerData IndexData;
			FMultiSizeIndexContainerData AdjacencyIndexData;
			IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

			UE_LOG(LogSkeletalMesh, Warning, TEXT("Building adjacency information for skeletal mesh '%s'. Please resave the asset."), *GetPathName() );
			LODModel.GetVertices( Vertices );
			LODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexData );
			AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;
			MeshUtilities.BuildSkeletalAdjacencyIndexBuffer( Vertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices );
			LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer( AdjacencyIndexData );
		}
	}
#endif // #if WITH_EDITOR

	// make sure we're counting properly
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << RefBasesInvMatrix;
	}

	if( Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		TMap<FName, int32> DummyNameIndexMap;
		Ar << DummyNameIndexMap;
	}

	//@todo legacy
	TArray<UObject*> DummyObjs;
	Ar << DummyObjs;

	Ar << CachedStreamingTextureFactors;

	if ( !StripFlags.IsEditorDataStripped() )
	{
		FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
		SkelSourceData.Serialize( Ar, this );
	}

#if WITH_EDITORONLY_DATA
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !AssetImportData)
	{
		// AssetImportData should always be valid
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	
	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA && AssetImportData)
	{
		// AssetImportData should always have been set up in the constructor where this is relevant
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
		
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}
#endif // WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() >= VER_UE4_APEX_CLOTH)
	{
		// Serialize non-UPROPERTY ApexClothingAsset data.
		for( int32 Idx=0;Idx<ClothingAssets.Num();Idx++ )
		{
			Ar << ClothingAssets[Idx];
		}

		if (Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR)
		{
			RebuildRefSkeletonNameToIndexMap();
		}

#if WITH_APEX_CLOTHING
		if (Ar.IsLoading())
		{
			BuildApexToUnrealBoneMapping();
		}
#endif
	}

	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		// Previous to this version, shadowcasting flags were stored in the LODInfo array
		// now they're in the Materials array so we need to move them over
		MoveDeprecatedShadowFlagToMaterials();
	}
#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() < VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE)
	{
		PreviewAttachedAssetContainer.SaveAttachedObjectsFromDeprecatedProperties();
	}
#endif

	if (bEnablePerPolyCollision)
	{
		Ar << BodySetup;
	}

#if WITH_APEX_CLOTHING && WITH_EDITORONLY_DATA
	if(GIsEditor && Ar.IsLoading() && Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::AddInternalClothingGraphicalSkinning)
	{
		ApexClothingUtils::BackupClothingDataFromSkeletalMesh(this);
		ApexClothingUtils::ReapplyClothingDataToSkeletalMesh(this);
	}
#endif
}

void USkeletalMesh::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	USkeletalMesh* This = CastChecked<USkeletalMesh>(InThis);
#if WITH_EDITOR
	if( GIsEditor )
	{
		// Required by the unified GC when running in the editor
		for( int32 Index = 0; Index < This->Materials.Num(); Index++ )
		{
			Collector.AddReferencedObject( This->Materials[ Index ].MaterialInterface, This );
		}
	}
#endif
	Super::AddReferencedObjects( This, Collector );
}

void USkeletalMesh::FlushRenderState()
{
	//TComponentReregisterContext<USkeletalMeshComponent> ReregisterContext;

	// Release the mesh's render resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still
	// allocated, and potentially accessing the mesh data.
	ReleaseResourcesFence.Wait();
}

bool USkeletalMesh::GetSortCenterPoint(FVector& OutSortCenter) const
{
	OutSortCenter = FVector::ZeroVector;
	bool bFoundCenter = false;
	USkeletalMeshSocket const* Socket = FindSocket( FName(TEXT("SortCenter")) );
	if( Socket )
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);
		if( BoneIndex != INDEX_NONE )
		{
			bFoundCenter = true;
			OutSortCenter = RefSkeleton.GetRefBonePose()[BoneIndex].GetTranslation() + Socket->RelativeLocation;
		}
	}
	return bFoundCenter;
}

uint32 USkeletalMesh::GetVertexBufferFlags() const
{
	uint32 VertexFlags = FStaticLODModel::EVertexFlags::None;
	if (bUseFullPrecisionUVs)
	{
		VertexFlags |= FStaticLODModel::EVertexFlags::UseFullPrecisionUVs;
	}
	if (bHasVertexColors)
	{
		VertexFlags |= FStaticLODModel::EVertexFlags::HasVertexColors;
	}
	return VertexFlags;
}

void USkeletalMesh::RestartRenderState()
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	// rebuild vertex buffers
	uint32 VertexFlags = GetVertexBufferFlags();
	for( int32 LODIndex = 0;LODIndex < Resource->LODModels.Num();LODIndex++ )
	{
		Resource->LODModels[LODIndex].BuildVertexBuffers(VertexFlags);
	}

	// reinitialize resource
	InitResources();

	RecreateRenderState_Internal(this);
}

void USkeletalMesh::PreSave(const class ITargetPlatform* TargetPlatform)
{
	// check the parent index of the root bone is invalid
	check((RefSkeleton.GetNum() == 0) || (RefSkeleton.GetRefBoneInfo()[0].ParentIndex == INDEX_NONE));

	Super::PreSave(TargetPlatform);
	// Make sure streaming texture factors have been cached. Calling GetStreamingTextureFactor
	// with editor-only data available will calculate it if it has not been cached.
	GetStreamingTextureFactor(0);
}

// Pre-calculate refpose-to-local transforms
void USkeletalMesh::CalculateInvRefMatrices()
{
	if( RefBasesInvMatrix.Num() != RefSkeleton.GetNum() )
	{
		RefBasesInvMatrix.Empty(RefSkeleton.GetNum());
		RefBasesInvMatrix.AddUninitialized(RefSkeleton.GetNum());

		// Reset cached mesh-space ref pose
		CachedComposedRefPoseMatrices.Empty( RefSkeleton.GetNum() );
		CachedComposedRefPoseMatrices.AddUninitialized( RefSkeleton.GetNum() );

		// Precompute the Mesh.RefBasesInverse.
		for( int32 b=0; b<RefSkeleton.GetNum(); b++)
		{
			// Render the default pose.
			CachedComposedRefPoseMatrices[b] = GetRefPoseMatrix(b);

			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				int32 Parent = RefSkeleton.GetParentIndex(b);
				CachedComposedRefPoseMatrices[b] = CachedComposedRefPoseMatrices[b] * CachedComposedRefPoseMatrices[Parent];
			}

			FVector XAxis, YAxis, ZAxis;

			CachedComposedRefPoseMatrices[b].GetScaledAxes(XAxis, YAxis, ZAxis);
			if(	XAxis.IsNearlyZero(SMALL_NUMBER) &&
				YAxis.IsNearlyZero(SMALL_NUMBER) &&
				ZAxis.IsNearlyZero(SMALL_NUMBER))
			{
				// this is not allowed, warn them 
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Reference Pose for joint (%s) includes NIL matrix. Zero scale isn't allowed on ref pose. "), *RefSkeleton.GetBoneName(b).ToString());
			}

			// Precompute inverse so we can use from-refpose-skin vertices.
			RefBasesInvMatrix[b] = CachedComposedRefPoseMatrices[b].Inverse(); 
		}

#if WITH_EDITORONLY_DATA
		if(RetargetBasePose.Num() == 0)
		{
			RetargetBasePose = RefSkeleton.GetRefBonePose();
		}
#endif // WITH_EDITORONLY_DATA
	}
}

void USkeletalMesh::CalculateRequiredBones(class FStaticLODModel& LODModel, const struct FReferenceSkeleton& RefSkeleton, const TMap<FBoneIndexType, FBoneIndexType> * BonesToRemove)
{
	// RequiredBones for base model includes all bones.
	int32 RequiredBoneCount = RefSkeleton.GetNum();
	LODModel.RequiredBones.Empty(RequiredBoneCount);
	for(int32 i=0; i<RequiredBoneCount; i++)
	{
		// Make sure it's not in BonesToRemove
		// @Todo change this to one TArray
		if (!BonesToRemove || BonesToRemove->Find(i) == NULL)
		{
			LODModel.RequiredBones.Add(i);
		}
	}

	LODModel.RequiredBones.Shrink();	
}

void USkeletalMesh::PostLoad()
{
	Super::PostLoad();

	// If LODInfo is missing - create array of correct size.
	if( LODInfo.Num() != ImportedResource->LODModels.Num() )
	{
		LODInfo.Empty(ImportedResource->LODModels.Num());
		LODInfo.AddZeroed(ImportedResource->LODModels.Num());

		for(int32 i=0; i<LODInfo.Num(); i++)
		{
			LODInfo[i].LODHysteresis = 0.02f;
		}
	}

	int32 TotalLODNum = LODInfo.Num();
	for (int32 LodIndex = 0; LodIndex<TotalLODNum; LodIndex++)
	{
		FSkeletalMeshLODInfo& ThisLODInfo = LODInfo[LodIndex];
		FStaticLODModel& ThisLODModel = ImportedResource->LODModels[LodIndex];

		// Presize the per-section TriangleSortSettings array
		if( ThisLODInfo.TriangleSortSettings.Num() > ThisLODModel.Sections.Num() )
		{
			ThisLODInfo.TriangleSortSettings.RemoveAt( ThisLODModel.Sections.Num(), ThisLODInfo.TriangleSortSettings.Num()-ThisLODModel.Sections.Num() );
		}
		else
		if( ThisLODModel.Sections.Num() > ThisLODInfo.TriangleSortSettings.Num() )
		{
			ThisLODInfo.TriangleSortSettings.AddZeroed( ThisLODModel.Sections.Num()-ThisLODInfo.TriangleSortSettings.Num() );
		}

#if WITH_EDITOR
		if (ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Num() > 0)
		{
			for (auto& BoneToRemove : ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED)
			{
				AddBoneToReductionSetting(LodIndex, BoneToRemove.BoneName);
			}

			// since in previous system, we always removed from previous LOD, I'm adding this 
			// here for previous LODs
			for (int32 CurLodIndx = LodIndex + 1; CurLodIndx < TotalLODNum; ++CurLodIndx)
			{
				AddBoneToReductionSetting(CurLodIndx, ThisLODInfo.RemovedBones);
			}

			// we don't apply this change here, but this will be applied when you re-gen simplygon
			ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Empty();
		}
#endif
	}

	// Revert to using 32 bit Float UVs on hardware that doesn't support rendering with 16 bit Float UVs 
	if( !bUseFullPrecisionUVs && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
	{
		bUseFullPrecisionUVs=true;
		// convert each LOD level to 32 bit UVs
		for( int32 LODIdx=0; LODIdx < ImportedResource->LODModels.Num(); LODIdx++ )
		{
			FStaticLODModel& LODModel = ImportedResource->LODModels[LODIdx];
			// Determine the correct version of ConvertToFullPrecisionUVs based on the number of UVs in the vertex buffer
			const uint32 NumTexCoords = LODModel.VertexBufferGPUSkin.GetNumTexCoords();
			switch(NumTexCoords)
			{
			case 1: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<1>(); break;
			case 2: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<2>(); break; 
			case 3: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<3>(); break; 
			case 4: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<4>(); break; 
			}
		}
	}

	// Call GetStreamingTextureFactor once before initializing resources.
	// Initializing resources will dump the mesh data we need to compute texture
	// streaming factors.
	GetStreamingTextureFactor(0);

	// initialize rendering resources
	if (FApp::CanEverRender())
	{
#if WITH_EDITOR
		// If we needed to recalc max bone influences, also need to recreate gpu vertex buffer with that info
		if (GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RecalcMaxBoneInfluences)
		{
			FSkeletalMeshResource* Resource = GetImportedResource();
			uint32 VertexFlags = GetVertexBufferFlags();
			for (int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); LODIndex++)
			{
				Resource->LODModels[LODIndex].BuildVertexBuffers(VertexFlags);
			}
		}
#endif // WITH_EDITOR

		InitResources();
	}

	CalculateInvRefMatrices();

	// init morph targets
	InitMorphTargets();

#if WITH_APEX_CLOTHING
	// load clothing section collision
	for( int32 AssetIdx=0; AssetIdx<ClothingAssets.Num();AssetIdx++ )
	{
		if( ClothingAssets[AssetIdx].ApexClothingAsset )
		{
			LoadClothCollisionVolumes(AssetIdx, ClothingAssets[AssetIdx].ApexClothingAsset);
		}
#if WITH_EDITOR
		// Remove any clothing sections that have invalid APEX data.
		// This can occur if you load and re-save content in a build with APEX disabled.
		else
		{
			// Actually need to call ApexClothingUtils::Restore... here but we can't as it's in the editor package

			ClothingAssets.RemoveAt(AssetIdx);
			--AssetIdx;
		}
#endif // WITH_EDITOR
	}

	// validate influences for existing clothing
	if(FSkeletalMeshResource* SkelResource = GetImportedResource())
	{
		for(int32 LODIndex = 0; LODIndex < SkelResource->LODModels.Num(); ++LODIndex)
		{
			FStaticLODModel& CurLODModel = SkelResource->LODModels[LODIndex];

			for (int32 SectionIdx = 0; SectionIdx< CurLODModel.Sections.Num(); SectionIdx++)
			{
				FSkelMeshSection& CurSection = CurLODModel.Sections[SectionIdx];

				if(CurSection.CorrespondClothSectionIndex != INDEX_NONE && CurSection.MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM)
				{
					UE_LOG(LogSkeletalMesh, Warning, TEXT("Section %d for LOD %d in skeletal mesh %s has clothing associated but has %d influences. Clothing only supports a maximum of %d influences - reduce influences on chunk and reimport mesh."),
						SectionIdx,
						LODIndex,
						*GetName(),
						CurSection.MaxBoneInfluences,
						MAX_INFLUENCES_PER_STREAM);
				}
			}
		}
	}

#endif // WITH_APEX_CLOTHING

	if( GetLinkerUE4Version() < VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		RebuildRefSkeletonNameToIndexMap();
	}

#if WITH_APEX_CLOTHING
	//We can't build apex map until this point because old content needs to call RebuildNameToIndexMap
	if(GetLinkerUE4Version() >= VER_UE4_APEX_CLOTH)
	{
		BuildApexToUnrealBoneMapping();
	}
#endif

	if (GetLinkerUE4Version() < VER_UE4_SORT_ACTIVE_BONE_INDICES)
	{
		for (int32 LodIndex = 0; LodIndex < LODInfo.Num(); LodIndex++)
		{
			FStaticLODModel & ThisLODModel = ImportedResource->LODModels[LodIndex];
			ThisLODModel.ActiveBoneIndices.Sort();
		}
	}

#if WITH_EDITORONLY_DATA
	if (RetargetBasePose.Num() == 0)
	{
		RetargetBasePose = RefSkeleton.GetRefBonePose();
	}
#endif

	// Bounds have been loaded - apply extensions.
	CalculateExtendedBounds();
}

void USkeletalMesh::RebuildRefSkeletonNameToIndexMap()
{
	TArray<FBoneIndexType> DuplicateBones;
	// Make sure we have no duplicate bones. Some content got corrupted somehow. :(
	RefSkeleton.RemoveDuplicateBones(this, DuplicateBones);

	// If we have removed any duplicate bones, we need to fix up any broken LODs as well.
	// Duplicate bones are given from highest index to lowest. 
	// so it's safe to decrease indices for children, we're not going to lose the index of the remaining duplicate bones.
	for (int32 Index = 0; Index < DuplicateBones.Num(); Index++)
	{
		const FBoneIndexType& DuplicateBoneIndex = DuplicateBones[Index];
		for (int32 LodIndex = 0; LodIndex < LODInfo.Num(); LodIndex++)
		{
			FStaticLODModel & ThisLODModel = ImportedResource->LODModels[LodIndex];
			{
				int32 FoundIndex;
				if (ThisLODModel.RequiredBones.Find(DuplicateBoneIndex, FoundIndex))
				{
					ThisLODModel.RequiredBones.RemoveAt(FoundIndex, 1);
					// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.RequiredBones.Num(); j++)
					{
						ThisLODModel.RequiredBones[j] = ThisLODModel.RequiredBones[j] - 1;
					}
				}
			}

			{
				int32 FoundIndex;
				if (ThisLODModel.ActiveBoneIndices.Find(DuplicateBoneIndex, FoundIndex))
				{
					ThisLODModel.ActiveBoneIndices.RemoveAt(FoundIndex, 1);
					// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.ActiveBoneIndices.Num(); j++)
					{
						ThisLODModel.ActiveBoneIndices[j] = ThisLODModel.ActiveBoneIndices[j] - 1;
					}
				}
			}
		}
	}

	// Rebuild name table.
	RefSkeleton.RebuildNameToIndexMap();
}

void USkeletalMesh::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	int32 NumTriangles = 0;
	if ( ImportedResource->LODModels.Num() > 0 && &ImportedResource->LODModels[0] != NULL )
	{
		const FStaticLODModel& LODModel = ImportedResource->LODModels[0];
		NumTriangles = LODModel.GetTotalFaces();
	}

	OutTags.Add(FAssetRegistryTag("Triangles", FString::FromInt(NumTriangles), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Bones", FString::FromInt(RefSkeleton.GetNum()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("MorphTargets", FString::FromInt(MorphTargets.Num()), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}
#endif
	
	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void USkeletalMesh::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);
	OutMetadata.Add("PhysicsAsset", FAssetRegistryTagMetadata().SetImportantValue(TEXT("None")));
}
#endif

void USkeletalMesh::DebugVerifySkeletalMeshLOD()
{
	// if LOD do not have displayfactor set up correctly
	if (LODInfo.Num() > 1)
	{
		for(int32 i=1; i<LODInfo.Num(); i++)
		{
			if (LODInfo[i].ScreenSize <= 0.1f)
			{
				// too small
				UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : ScreenSize for LOD %d may be too small (%0.5f)"), *GetPathName(), i, LODInfo[i].ScreenSize);
			}
		}
	}
	else
	{
		// no LODInfo
		UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : LOD does not exist"), *GetPathName());
	}
}

void USkeletalMesh::RegisterMorphTarget(UMorphTarget* MorphTarget)
{
	if ( MorphTarget )
	{
		// if MorphTarget has SkelMesh, make sure you unregister before registering yourself
		if ( MorphTarget->BaseSkelMesh && MorphTarget->BaseSkelMesh!=this )
		{
			MorphTarget->BaseSkelMesh->UnregisterMorphTarget(MorphTarget);
		}

		MorphTarget->BaseSkelMesh = this;

		bool bRegistered = false;

		for ( int32 Index = 0; Index < MorphTargets.Num(); ++Index )
		{
			if ( MorphTargets[Index]->GetFName() == MorphTarget->GetFName() )
			{
				UE_LOG( LogSkeletalMesh, Log, TEXT("RegisterMorphTarget: %s already exists, replacing"), *MorphTarget->GetName() );
				MorphTargets[Index] = MorphTarget;
				bRegistered = true;
				break;
			}
		}

		if (!bRegistered)
		{
			MorphTargets.Add( MorphTarget );
			bRegistered = true;
		}

		if (bRegistered)
		{
			MarkPackageDirty();
			// need to refresh the map
			InitMorphTargets();
		}
	}
}

void USkeletalMesh::UnregisterMorphTarget(UMorphTarget* MorphTarget)
{
	if ( MorphTarget )
	{
		// Do not remove with MorphTarget->GetFName(). The name might have changed
		// Search the value, and delete	
		for ( int32 I=0; I<MorphTargets.Num(); ++I)
		{
			if ( MorphTargets[I] == MorphTarget )
			{
				MorphTargets.RemoveAt(I);
				--I;
				MarkPackageDirty();
				// need to refresh the map
				InitMorphTargets();
				return;
			}
		}

		UE_LOG( LogSkeletalMesh, Log, TEXT("UnregisterMorphTarget: %s not found."), *MorphTarget->GetName() );
	}
}

void USkeletalMesh::InitMorphTargets()
{
	MorphTargetIndexMap.Empty();

	for (int32 Index = 0; Index < MorphTargets.Num(); ++Index)
	{
		UMorphTarget* MorphTarget = MorphTargets[Index];
		FName const ShapeName = MorphTarget->GetFName();
		if (MorphTargetIndexMap.Find(ShapeName) == nullptr)
		{
			MorphTargetIndexMap.Add(ShapeName, Index);
		}
	}
}

UMorphTarget* USkeletalMesh::FindMorphTarget(FName MorphTargetName) const
{
	int32 Index;
	return FindMorphTargetAndIndex(MorphTargetName, Index);
}

UMorphTarget* USkeletalMesh::FindMorphTargetAndIndex(FName MorphTargetName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if( MorphTargetName != NAME_None )
	{
		const int32* Found = MorphTargetIndexMap.Find(MorphTargetName);
		if (Found)
		{
			OutIndex = *Found;
			return MorphTargets[*Found];
		}
	}

	return nullptr;
}

USkeletalMeshSocket* USkeletalMesh::FindSocket(FName InSocketName) const
{
	int32 DummyIdx;
	return FindSocketAndIndex(InSocketName, DummyIdx);
}

USkeletalMeshSocket* USkeletalMesh::FindSocketAndIndex(FName InSocketName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if (InSocketName == NAME_None)
	{
		return NULL;
	}

	for (int32 i = 0; i < Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = Sockets[i];
		if (Socket && Socket->SocketName == InSocketName)
		{
			OutIndex = i;
			return Socket;
		}
	}

	// If the socket isn't on the mesh, try to find it on the skeleton
	if (Skeleton)
	{
		for (int32 i = 0; i < Skeleton->Sockets.Num(); ++i)
		{
			USkeletalMeshSocket* Socket = Skeleton->Sockets[i];
			if (Socket && Socket->SocketName == InSocketName)
			{
				OutIndex = Sockets.Num() + i;
				return Socket;
			}
		}
	}

	return NULL;
}

int32 USkeletalMesh::NumSockets() const
{
	return Sockets.Num() + (Skeleton ? Skeleton->Sockets.Num() : 0);
}

USkeletalMeshSocket* USkeletalMesh::GetSocketByIndex(int32 Index) const
{
	if (Index < Sockets.Num())
	{
		return Sockets[Index];
	}

	if (Skeleton && Index < Skeleton->Sockets.Num())
	{
		return Skeleton->Sockets[Index];
	}

	return nullptr;
}



/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 */
FString USkeletalMesh::GetDetailedInfoInternal() const
{
	return GetPathName( NULL );
}


FMatrix USkeletalMesh::GetRefPoseMatrix( int32 BoneIndex ) const
{
	check( BoneIndex >= 0 && BoneIndex < RefSkeleton.GetNum() );
	FTransform BoneTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
	// Make sure quaternion is normalized!
	BoneTransform.NormalizeRotation();
	return BoneTransform.ToMatrixWithScale();
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix( FName InBoneName ) const
{
	FMatrix LocalPose( FMatrix::Identity );

	if ( InBoneName != NAME_None )
	{
		int32 BoneIndex = RefSkeleton.FindBoneIndex(InBoneName);
		if (BoneIndex != INDEX_NONE)
		{
			return GetComposedRefPoseMatrix(BoneIndex);
		}
		else
		{
			USkeletalMeshSocket const* Socket = FindSocket(InBoneName);

			if(Socket != NULL)
			{
				BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);

				if(BoneIndex != INDEX_NONE)
				{
					const FRotationTranslationMatrix SocketMatrix(Socket->RelativeRotation, Socket->RelativeLocation);
					LocalPose = SocketMatrix * GetComposedRefPoseMatrix(BoneIndex);
				}
			}
		}
	}

	return LocalPose;
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix(int32 InBoneIndex) const
{
	return CachedComposedRefPoseMatrices[InBoneIndex];
}

TArray<USkeletalMeshSocket*>& USkeletalMesh::GetMeshOnlySocketList()
{
	return Sockets;
}

void USkeletalMesh::MoveDeprecatedShadowFlagToMaterials()
{
	// First, the easy case where there's no LOD info (in which case, default to true!)
	if ( LODInfo.Num() == 0 )
	{
		for ( auto Material = Materials.CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting = true;
		}

		return;
	}
	
	TArray<bool> PerLodShadowFlags;
	bool bDifferenceFound = false;

	// Second, detect whether the shadow casting flag is the same for all sections of all lods
	for ( auto LOD = LODInfo.CreateConstIterator(); LOD; ++LOD )
	{
		if ( LOD->bEnableShadowCasting_DEPRECATED.Num() )
		{
			PerLodShadowFlags.Add( LOD->bEnableShadowCasting_DEPRECATED[0] );
		}

		if ( !AreAllFlagsIdentical( LOD->bEnableShadowCasting_DEPRECATED ) )
		{
			// We found a difference in the sections of this LOD!
			bDifferenceFound = true;
			break;
		}
	}

	if ( !bDifferenceFound && !AreAllFlagsIdentical( PerLodShadowFlags ) )
	{
		// Difference between LODs
		bDifferenceFound = true;
	}

	if ( !bDifferenceFound )
	{
		// All the same, so just copy the shadow casting flag to all materials
		for ( auto Material = Materials.CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting = PerLodShadowFlags.Num() ? PerLodShadowFlags[0] : true;
		}
	}
	else
	{
		FSkeletalMeshResource* Resource = GetImportedResource();
		check( Resource->LODModels.Num() == LODInfo.Num() );

		TArray<FSkeletalMaterial> NewMaterialArray;

		// There was a difference, so we need to build a new material list which has all the combinations of UMaterialInterface and shadow casting flag required
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				NewMaterialArray.Add( FSkeletalMaterial( Materials[ Resource->LODModels[LODIndex].Sections[i].MaterialIndex ].MaterialInterface, LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED[i] ) );
			}
		}

		// Reassign the materials array to the new one
		Materials = NewMaterialArray;
		int32 NewIndex = 0;

		// Remap the existing LODModels to point at the correct new material index
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				Resource->LODModels[LODIndex].Sections[i].MaterialIndex = NewIndex;
				++NewIndex;
			}
		}
	}
}

bool USkeletalMesh::AreAllFlagsIdentical( const TArray<bool>& BoolArray ) const
{
	if ( BoolArray.Num() == 0 )
	{
		return true;
	}

	for ( int32 i = 0; i < BoolArray.Num() - 1; ++i )
	{
		if ( BoolArray[i] != BoolArray[i + 1] )
		{
			return false;
		}
	}

	return true;
}

bool operator== ( const FSkeletalMaterial& LHS, const FSkeletalMaterial& RHS )
{
	return ( LHS.MaterialInterface == RHS.MaterialInterface && 
		LHS.bEnableShadowCasting == RHS.bEnableShadowCasting );
}

bool operator== ( const FSkeletalMaterial& LHS, const UMaterialInterface& RHS )
{
	return ( LHS.MaterialInterface == &RHS );
}

bool operator== ( const UMaterialInterface& LHS, const FSkeletalMaterial& RHS )
{
	return ( RHS.MaterialInterface == &LHS );
}

FArchive& operator<<( FArchive& Ar, FSkeletalMaterial& Elem )
{
	Ar << Elem.MaterialInterface;

	if ( Ar.UE4Ver() >= VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		Ar << Elem.bEnableShadowCasting;
	}

	Ar.UsingCustomVersion(FRecomputeTangentCustomVersion::GUID);
	if (Ar.CustomVer(FRecomputeTangentCustomVersion::GUID) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
	{
		Ar << Elem.bRecomputeTangent;
	}

	return Ar;
}

TArray<USkeletalMeshSocket*> USkeletalMesh::GetActiveSocketList() const
{
	TArray<USkeletalMeshSocket*> ActiveSockets = Sockets;

	// Then the skeleton sockets that aren't in the mesh
	if (Skeleton)
	{
		for (auto SkeletonSocketIt = Skeleton->Sockets.CreateConstIterator(); SkeletonSocketIt; ++SkeletonSocketIt)
		{
			USkeletalMeshSocket* Socket = *(SkeletonSocketIt);

			if (!IsSocketOnMesh(Socket->SocketName))
			{
				ActiveSockets.Add(Socket);
			}
		}
	}
	return ActiveSockets;
}

bool USkeletalMesh::IsSocketOnMesh(const FName& InSocketName) const
{
	for(int32 SocketIdx=0; SocketIdx < Sockets.Num(); SocketIdx++)
	{
		USkeletalMeshSocket* Socket = Sockets[SocketIdx];

		if(Socket != NULL && Socket->SocketName == InSocketName)
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR


FStaticLODModel& USkeletalMesh::GetSourceModel()
{
	check( ImportedResource->LODModels.Num() );
	FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
	if ( SkelSourceData.IsInitialized() )
	{
		return *SkelSourceData.GetModel();
	}
	return ImportedResource->LODModels[0];
}


FStaticLODModel& USkeletalMesh::PreModifyMesh()
{
	FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
	if ( !SkelSourceData.IsInitialized() && ImportedResource->LODModels.Num() )
	{
		SkelSourceData.Init( this, ImportedResource->LODModels[0] );
	}
	check( SkelSourceData.IsInitialized() );
	return GetSourceModel();
}

int32 USkeletalMesh::ValidatePreviewAttachedObjects()
{
	int32 NumBrokenAssets = PreviewAttachedAssetContainer.ValidatePreviewAttachedObjects();

	if (NumBrokenAssets > 0)
	{
		MarkPackageDirty();
	}
	return NumBrokenAssets;
}

#endif // #if WITH_EDITOR

void USkeletalMesh::ReleaseCPUResources()
{
	FSkeletalMeshResource* Resource = GetImportedResource();
	for(int32 Index = 0; Index < Resource->LODModels.Num(); ++Index)
	{
		Resource->LODModels[Index].ReleaseCPUResources();
	}
}

/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
void USkeletalMesh::InitBoneMirrorInfo()
{
	SkelMirrorTable.Empty(RefSkeleton.GetNum());
	SkelMirrorTable.AddZeroed(RefSkeleton.GetNum());

	// By default, no bone mirroring, and source is ourself.
	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		SkelMirrorTable[i].SourceIndex = i;
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::CopyMirrorTableFrom(USkeletalMesh* SrcMesh)
{
	// Do nothing if no mirror table in source mesh
	if(SrcMesh->SkelMirrorTable.Num() == 0)
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed( SrcMesh->SkelMirrorTable.Num() );

	// Mirror table must always be size of ref skeleton.
	check(SrcMesh->SkelMirrorTable.Num() == SrcMesh->RefSkeleton.GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<SrcMesh->SkelMirrorTable.Num(); i++)
	{
		if(!EntryCopied[i])
		{
			// Get name of source and dest bone for this entry in the source table.
			FName DestBoneName = SrcMesh->RefSkeleton.GetBoneName(i);
			int32 SrcBoneIndex = SrcMesh->SkelMirrorTable[i].SourceIndex;
			FName SrcBoneName = SrcMesh->RefSkeleton.GetBoneName(SrcBoneIndex);
			EAxis::Type FlipAxis = SrcMesh->SkelMirrorTable[i].BoneFlipAxis;

			// Look up bone names in target mesh (this one)
			int32 DestBoneIndexTarget = RefSkeleton.FindBoneIndex(DestBoneName);
			int32 SrcBoneIndexTarget = RefSkeleton.FindBoneIndex(SrcBoneName);

			// If both bones found, copy data to this mesh's mirror table.
			if( DestBoneIndexTarget != INDEX_NONE && SrcBoneIndexTarget != INDEX_NONE )
			{
				SkelMirrorTable[DestBoneIndexTarget].SourceIndex = SrcBoneIndexTarget;
				SkelMirrorTable[DestBoneIndexTarget].BoneFlipAxis = FlipAxis;


				SkelMirrorTable[SrcBoneIndexTarget].SourceIndex = DestBoneIndexTarget;
				SkelMirrorTable[SrcBoneIndexTarget].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[i] = true;
				EntryCopied[SrcBoneIndex] = true;
			}
		}
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( SkelMirrorTable.Num() == 0 )
	{
		return;
	}
	
	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.GetNum());

	MirrorExportInfo.Empty(SkelMirrorTable.Num());
	MirrorExportInfo.AddZeroed(SkelMirrorTable.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		MirrorExportInfo[i].BoneName		= RefSkeleton.GetBoneName(i);
		MirrorExportInfo[i].SourceBoneName	= RefSkeleton.GetBoneName(SkelMirrorTable[i].SourceIndex);
		MirrorExportInfo[i].BoneFlipAxis	= SkelMirrorTable[i].BoneFlipAxis;
	}
}


/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( MirrorExportInfo.Num() == 0 )
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed( RefSkeleton.GetNum() );

	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<MirrorExportInfo.Num(); i++)
	{
		FName DestBoneName	= MirrorExportInfo[i].BoneName;
		int32 DestBoneIndex	= RefSkeleton.FindBoneIndex(DestBoneName);

		if( DestBoneIndex != INDEX_NONE && !EntryCopied[DestBoneIndex] )
		{
			FName SrcBoneName	= MirrorExportInfo[i].SourceBoneName;
			int32 SrcBoneIndex	= RefSkeleton.FindBoneIndex(SrcBoneName);
			EAxis::Type FlipAxis		= MirrorExportInfo[i].BoneFlipAxis;

			// If both bones found, copy data to this mesh's mirror table.
			if( SrcBoneIndex != INDEX_NONE )
			{
				SkelMirrorTable[DestBoneIndex].SourceIndex = SrcBoneIndex;
				SkelMirrorTable[DestBoneIndex].BoneFlipAxis = FlipAxis;

				SkelMirrorTable[SrcBoneIndex].SourceIndex = DestBoneIndex;
				SkelMirrorTable[SrcBoneIndex].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[DestBoneIndex]	= true;
				EntryCopied[SrcBoneIndex]	= true;
			}
		}
	}
}

/** 
 *	Utility for checking that the bone mirroring table of this mesh is good.
 *	Return true if mirror table is OK, false if there are problems.
 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
 */
bool USkeletalMesh::MirrorTableIsGood(FString& ProblemBones)
{
	TArray<int32>	BadBoneMirror;

	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		int32 SrcIndex = SkelMirrorTable[i].SourceIndex;
		if( SkelMirrorTable[SrcIndex].SourceIndex != i)
		{
			BadBoneMirror.Add(i);
		}
	}

	if(BadBoneMirror.Num() > 0)
	{
		for(int32 i=0; i<BadBoneMirror.Num(); i++)
		{
			int32 BoneIndex = BadBoneMirror[i];
			FName BoneName = RefSkeleton.GetBoneName(BoneIndex);

			ProblemBones += FString::Printf( TEXT("%s (%d)\n"), *BoneName.ToString(), BoneIndex );
		}

		return false;
	}
	else
	{
		return true;
	}
}

void USkeletalMesh::CreateBodySetup()
{
	if (BodySetup == nullptr)
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->bSharedCookedData = true;
	}
}

UBodySetup* USkeletalMesh::GetBodySetup()
{
	CreateBodySetup();
	return BodySetup;
}

#if WITH_EDITOR
void USkeletalMesh::BuildPhysicsData()
{
	CreateBodySetup();
	BodySetup->CookedFormatData.FlushData();	//we need to force a re-cook because we're essentially re-creating the bodysetup so that it swaps whether or not it has a trimesh
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();
}
#endif

bool USkeletalMesh::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return bEnablePerPolyCollision;
}

bool USkeletalMesh::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool bInUseAllTriData)
{
#if WITH_EDITORONLY_DATA

	// Fail if no mesh or not per poly collision
	if (!ImportedResource.IsValid() || !bEnablePerPolyCollision)
	{
		return false;
	}

	const FStaticLODModel& Model = ImportedResource->LODModels[0];

	{
		// Copy all verts into collision vertex buffer.
		CollisionData->Vertices.Empty();
		CollisionData->Vertices.AddUninitialized(Model.NumVertices);
		const uint32 NumSections = Model.Sections.Num();

		for (uint32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
		{
			const FSkelMeshSection& Section = Model.Sections[SectionIdx];
			{
				//soft
				const uint32 SoftOffset = Section.GetVertexBufferIndex();
				const uint32 NumSoftVerts = Section.GetNumVertices();
				for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
				{
					CollisionData->Vertices[SoftIdx + SoftOffset] = Section.SoftVertices[SoftIdx].Position;
				}
			}

		}
	}

	{
		// Copy indices into collision index buffer
		const FMultiSizeIndexContainer& IndexBufferContainer = Model.MultiSizeIndexContainer;

		TArray<uint32> Indices;
		IndexBufferContainer.GetIndexBuffer(Indices);

		const uint32 NumTris = Indices.Num() / 3;
		CollisionData->Indices.Empty();
		CollisionData->Indices.Reserve(NumTris);

		FTriIndices TriIndex;
		for (int32 SectionIndex = 0; SectionIndex < Model.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = Model.Sections[SectionIndex];

			const uint32 OnePastLastIndex = Section.BaseIndex + Section.NumTriangles * 3;

			for (uint32 i = Section.BaseIndex; i < OnePastLastIndex; i += 3)
			{
				TriIndex.v0 = Indices[i];
				TriIndex.v1 = Indices[i + 1];
				TriIndex.v2 = Indices[i + 2];

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

void USkeletalMesh::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* USkeletalMesh::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void USkeletalMesh::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* USkeletalMesh::GetAssetUserDataArray() const
{
	return &AssetUserData;
}

////// SKELETAL MESH THUMBNAIL SUPPORT ////////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString USkeletalMesh::GetDesc()
{
	FSkeletalMeshResource* Resource = GetImportedResource();
	check(Resource->LODModels.Num() > 0);
	return FString::Printf( TEXT("%d Triangles, %d Bones"), Resource->LODModels[0].GetTotalFaces(), RefSkeleton.GetNum() );
}

bool USkeletalMesh::IsSectionUsingCloth(int32 InSectionIndex, bool bCheckCorrespondingSections) const
{
#if WITH_APEX_CLOTHING
	if(ImportedResource.IsValid())
	{
		for(FStaticLODModel& LodModel : ImportedResource->LODModels)
		{
			if(LodModel.Sections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshSection* SectionToCheck = &LodModel.Sections[InSectionIndex];
				if(SectionToCheck->bDisabled && bCheckCorrespondingSections)
				{
					SectionToCheck = &LodModel.Sections[SectionToCheck->CorrespondClothSectionIndex];
				}
			
				if(SectionToCheck->HasApexClothData())
				{
					return true;
				}
			}
		}
	}
#endif

	return false;
}

#if WITH_EDITOR
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, const TArray<FName>& BoneNames)
{
	if (LODInfo.IsValidIndex(LODIndex))
	{
		for (auto& BoneName : BoneNames)
		{
			LODInfo[LODIndex].RemovedBones.AddUnique(BoneName);
		}
	}
}
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, FName BoneName)
{
	if (LODInfo.IsValidIndex(LODIndex))
	{
		LODInfo[LODIndex].RemovedBones.AddUnique(BoneName);
	}
}

#endif // WITH_EDITOR
/*-----------------------------------------------------------------------------
USkeletalMeshSocket
-----------------------------------------------------------------------------*/
USkeletalMeshSocket::USkeletalMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bForceAlwaysAnimated(true)
{
	RelativeScale = FVector(1.0f, 1.0f, 1.0f);
}

void USkeletalMeshSocket::InitializeSocketFromLocation(const class USkeletalMeshComponent* SkelComp, FVector WorldLocation, FVector WorldNormal)
{
	BoneName = SkelComp->FindClosestBone(WorldLocation);
	if( BoneName != NAME_None )
	{
		SkelComp->TransformToBoneSpace(BoneName, WorldLocation, WorldNormal.Rotation(), RelativeLocation, RelativeRotation);
	}
}

FVector USkeletalMeshSocket::GetSocketLocation(const class USkeletalMeshComponent* SkelComp) const
{
	if( SkelComp )
	{
		FMatrix SocketMatrix;
		if( GetSocketMatrix(SocketMatrix, SkelComp) )
		{
			return SocketMatrix.GetOrigin();
		}

		// Fall back to MeshComp origin, so it's visible in case of failure.
		return SkelComp->GetComponentLocation();
	}
	return FVector(0.f);
}

bool USkeletalMeshSocket::GetSocketMatrix(FMatrix& OutMatrix, const class USkeletalMeshComponent* SkelComp) const
{
	int32 BoneIndex = SkelComp->GetBoneIndex(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix( RelativeScale, RelativeRotation, RelativeLocation );
		OutMatrix = RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}

FTransform USkeletalMeshSocket::GetSocketLocalTransform() const
{
	return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
}

FTransform USkeletalMeshSocket::GetSocketTransform(const class USkeletalMeshComponent* SkelComp) const
{
	FTransform OutTM;

	int32 BoneIndex = SkelComp->GetBoneIndex(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FTransform BoneTM = SkelComp->GetBoneTransform(BoneIndex);
		FTransform RelSocketTM( RelativeRotation, RelativeLocation, RelativeScale );
		OutTM = RelSocketTM * BoneTM;
	}

	return OutTM;
}

bool USkeletalMeshSocket::GetSocketMatrixWithOffset(FMatrix& OutMatrix, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	int32 BoneIndex = SkelComp->GetBoneIndex(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		OutMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}


bool USkeletalMeshSocket::GetSocketPositionWithOffset(FVector& OutPosition, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	int32 BoneIndex = SkelComp->GetBoneIndex(BoneName);
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		FMatrix SocketMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		OutPosition = SocketMatrix.GetOrigin();
		return true;
	}

	return false;
}

/** 
 *	Utility to associate an actor with a socket
 *	
 *	@param	Actor			The actor to attach to the socket
 *	@param	SkelComp		The skeletal mesh component that the socket comes from
 *
 *	@return	bool			true if successful, false if not
 */
bool USkeletalMeshSocket::AttachActor(AActor* Actor, class USkeletalMeshComponent* SkelComp) const
{
	bool bAttached = false;

	// Don't support attaching to own socket
	if (Actor != SkelComp->GetOwner() && Actor->GetRootComponent())
	{
		FMatrix SocketTM;
		if( GetSocketMatrix( SocketTM, SkelComp ) )
		{
			Actor->Modify();

			Actor->SetActorLocation(SocketTM.GetOrigin(), false);
			Actor->SetActorRotation(SocketTM.Rotator());
			Actor->GetRootComponent()->SnapTo( SkelComp, SocketName );

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
void USkeletalMeshSocket::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ChangedEvent.Broadcast(this, PropertyChangedEvent.MemberProperty);
	}
}

void USkeletalMeshSocket::CopyFrom(const class USkeletalMeshSocket* OtherSocket)
{
	if (OtherSocket)
	{
		SocketName = OtherSocket->SocketName;
		BoneName = OtherSocket->BoneName;
		RelativeLocation = OtherSocket->RelativeLocation;
		RelativeRotation = OtherSocket->RelativeRotation;
		RelativeScale = OtherSocket->RelativeScale;
		bForceAlwaysAnimated = OtherSocket->bForceAlwaysAnimated;
	}
}

#endif

void USkeletalMeshSocket::Serialize(FArchive& Ar)
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


/*-----------------------------------------------------------------------------
	ASkeletalMeshActor
-----------------------------------------------------------------------------*/
ASkeletalMeshActor::ASkeletalMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent0"));
	SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
	// check BaseEngine.ini for profile setup
	SkeletalMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	RootComponent = SkeletalMeshComponent;

	bShouldDoAnimNotifies = true;
}

void ASkeletalMeshActor::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( ASkeletalMeshActor, ReplicatedMesh );
	DOREPLIFETIME( ASkeletalMeshActor, ReplicatedPhysAsset );
	DOREPLIFETIME( ASkeletalMeshActor, ReplicatedMaterial0 );
	DOREPLIFETIME( ASkeletalMeshActor, ReplicatedMaterial1 );
}

void ASkeletalMeshActor::PreviewBeginAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInst)
		{
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
	}
}
}


void ASkeletalMeshActor::PreviewFinishAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		// if in editor, reset the Animations, makes easier for artist to see them visually and align them
		// in game, we keep the last pose that matinee kept. If you'd like it to have animation, you'll need to have AnimTree or AnimGraph to handle correctly
		if (SkeletalMeshComponent->GetAnimationMode()== EAnimationMode::Type::AnimationBlueprint)
		{
			UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
			if(AnimInst)
			{
				AnimInst->Montage_Stop(0.f);
				AnimInst->UpdateAnimation(0.f, false);
			}
		}
		// Update space bases to reset it back to ref pose
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
	}
}


void ASkeletalMeshActor::PreviewSetAnimPosition(FName SlotName, int32 ChannelIndex, UAnimSequence* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime)
{
	if(CanPlayAnimation(InAnimSequence))
	{
		TWeakObjectPtr<class UAnimMontage>& CurrentlyPlayingMontage = CurrentlyPlayingMontages.FindOrAdd(SlotName);
		CurrentlyPlayingMontage = FAnimMontageInstance::PreviewMatineeSetAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping, bFireNotifies, DeltaTime);
	}
}

void ASkeletalMeshActor::PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos)
{
	//no support yet
}

void ASkeletalMeshActor::SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos )
{
	//no support yet
}

/** Check SkeletalMeshActor for errors. */
#if WITH_EDITOR
void ASkeletalMeshActor::CheckForErrors()
{
	Super::CheckForErrors();

	if (SkeletalMeshComponent)
	{
		UPhysicsAsset * const PhysicsAsset = SkeletalMeshComponent->GetPhysicsAsset();
		if(!PhysicsAsset)
		{
			if (SkeletalMeshComponent->CastShadow 
				&& SkeletalMeshComponent->bCastDynamicShadow)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("DetailedInfo"), FText::FromString(GetDetailedInfo()));
				FMessageLog("MapCheck").PerformanceWarning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create( FText::Format( LOCTEXT( "MapCheck_Message_SkelMeshActorNoPhysAsset", "SkeletalMeshActor '{DetailedInfo}' casts shadow but has no PhysicsAsset assigned.  The shadow will be low res and inefficient" ), Arguments ) ))
					->AddToken(FMapErrorToken::Create(FMapErrors::SkelMeshActorNoPhysAsset));
			}
		}

		if(SkeletalMeshComponent->CastShadow 
			&& SkeletalMeshComponent->bCastDynamicShadow 
			&& SkeletalMeshComponent->IsRegistered() 
			&& SkeletalMeshComponent->Bounds.SphereRadius > 2000.0f)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("DetailedInfo"), FText::FromString(GetDetailedInfo()));
			// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
			FMessageLog("MapCheck").PerformanceWarning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_ActorLargeShadowCaster", "{DetailedInfo} : Large actor casts a shadow and will cause an extreme performance hit unless shadow casting is disabled" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::ActorLargeShadowCaster));
		}

		if (SkeletalMeshComponent->SkeletalMesh == NULL)
		{
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_SkeletalMeshNull", "Skeletal mesh actor has NULL SkeletalMesh property")))
				->AddToken(FMapErrorToken::Create(FMapErrors::SkeletalMeshNull));
		}
	}
	else 
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_SkeletalMeshComponent", "Skeletal mesh actor has NULL SkeletalMeshComponent property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::SkeletalMeshComponent));
	}
}
#endif

FString ASkeletalMeshActor::GetDetailedInfoInternal() const
{
	FString Result;  

	if( SkeletalMeshComponent )
	{
		Result = SkeletalMeshComponent->GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("No_SkeletalMeshComponent");
	}

	return Result;  
}


void ASkeletalMeshActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// grab the current mesh for replication
	if (Role == ROLE_Authority && SkeletalMeshComponent)
	{
		ReplicatedMesh = SkeletalMeshComponent->SkeletalMesh;
	}

	// Unfix bodies flagged as 'full anim weight'
	if( SkeletalMeshComponent )
	{
		ReplicatedPhysAsset = SkeletalMeshComponent->GetPhysicsAsset();
	}
}

void ASkeletalMeshActor::OnRep_ReplicatedMesh()
{
	SkeletalMeshComponent->SetSkeletalMesh(ReplicatedMesh);
}

void ASkeletalMeshActor::OnRep_ReplicatedPhysAsset()
{
	SkeletalMeshComponent->SetPhysicsAsset(ReplicatedPhysAsset);
}

void ASkeletalMeshActor::OnRep_ReplicatedMaterial0()
{
	SkeletalMeshComponent->SetMaterial(0, ReplicatedMaterial0);
}

void ASkeletalMeshActor::OnRep_ReplicatedMaterial1()
{
	SkeletalMeshComponent->SetMaterial(1, ReplicatedMaterial1);
}

void ASkeletalMeshActor::BeginAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInst)
		{
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
	}
}
}

bool ASkeletalMeshActor::CanPlayAnimation(class UAnimSequenceBase* AnimAssetBase/*=NULL*/) const
{
	return (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton && 
		(!AnimAssetBase || SkeletalMeshComponent->SkeletalMesh->Skeleton->IsCompatible(AnimAssetBase->GetSkeleton())));
}

void ASkeletalMeshActor::SetAnimPosition(FName SlotName, int32 ChannelIndex, UAnimSequence* InAnimSequence, float InPosition, bool bFireNotifies, bool bLooping)
{
	if (CanPlayAnimation(InAnimSequence))
	{
		TWeakObjectPtr<class UAnimMontage>& CurrentlyPlayingMontage = CurrentlyPlayingMontages.FindOrAdd(SlotName);
		CurrentlyPlayingMontage = FAnimMontageInstance::SetMatineeAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping);
	}
}

void ASkeletalMeshActor::FinishAnimControl(UInterpGroup* InInterpGroup)
{
	if(SkeletalMeshComponent->GetAnimationMode()== EAnimationMode::Type::AnimationBlueprint)
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if(AnimInst)
	{
			AnimInst->Montage_Stop(0.f);
			AnimInst->UpdateAnimation(0.f, false);
		}

		// Update space bases to reset it back to ref pose
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
	}
}


#if WITH_EDITOR

bool ASkeletalMeshActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);

	if (SkeletalMeshComponent->SkeletalMesh)
	{
		Objects.Add(SkeletalMeshComponent->SkeletalMesh);
	}
	return true;
}

void ASkeletalMeshActor::EditorReplacedActor(AActor* OldActor)
{
	Super::EditorReplacedActor(OldActor);

	if (ASkeletalMeshActor * OldSkelMeshActor = Cast<ASkeletalMeshActor>(OldActor))
	{
		// if no skeletal mesh set, take one from previous actor
		if (SkeletalMeshComponent && OldSkelMeshActor->SkeletalMeshComponent &&
			SkeletalMeshComponent->SkeletalMesh == NULL)
		{
			SkeletalMeshComponent->SetSkeletalMesh(OldSkelMeshActor->SkeletalMeshComponent->SkeletalMesh);
		}
	}
}

void ASkeletalMeshActor::LoadedFromAnotherClass(const FName& OldClassName)
{
	Super::LoadedFromAnotherClass(OldClassName);

	if(GetLinkerUE4Version() < VER_UE4_REMOVE_SKELETALPHYSICSACTOR)
	{
		static FName SkeletalPhysicsActor_NAME(TEXT("SkeletalPhysicsActor"));
		static FName KAsset_NAME(TEXT("KAsset"));

		if(OldClassName == SkeletalPhysicsActor_NAME || OldClassName == KAsset_NAME)
		{
			SkeletalMeshComponent->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
			SkeletalMeshComponent->BodyInstance.bSimulatePhysics = true;
			SkeletalMeshComponent->bBlendPhysics = true;

			bAlwaysRelevant = true;
			bReplicateMovement = true;
			SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
			bReplicates = true;
		}
	}
}
#endif

/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/
#include "LevelUtils.h"
#include "SkeletalRender.h"

const FQuat SphylBasis(FVector(1.0f / FMath::Sqrt(2.0f), 0.0f, 1.0f / FMath::Sqrt(2.0f)), PI);

/** 
 * Constructor. 
 * @param	Component - skeletal mesh primitive being added
 */
FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshResource* InSkelMeshResource)
		:	FPrimitiveSceneProxy(Component, Component->SkeletalMesh->GetFName())
		,	Owner(Component->GetOwner())
		,	MeshObject(Component->MeshObject)
		,	SkelMeshResource(InSkelMeshResource)
		,	SkeletalMeshForDebug(Component->SkeletalMesh)
		,	PhysicsAssetForDebug(Component->GetPhysicsAsset())
		,	bForceWireframe(Component->bForceWireframe)
		,	bCanHighlightSelectedSections(Component->bCanHighlightSelectedSections)
		,	MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		,	bMaterialsNeedMorphUsage_GameThread(false)
#if WITH_EDITORONLY_DATA
		,	StreamingDistanceMultiplier(FMath::Max(0.0f, Component->StreamingDistanceMultiplier))
		,	StreamingTexelFactor(Component->SkeletalMesh->GetStreamingTextureFactor(0) * Component->ComponentToWorld.GetMaximumAxisScale())
#endif
{
	check(MeshObject);
	check(SkelMeshResource);
	check(SkeletalMeshForDebug);

	bIsCPUSkinned = MeshObject->IsCPUSkinned();

	bCastCapsuleDirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleDirectShadow;
	bCastCapsuleIndirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleIndirectShadow;

	// Force inset shadows if capsule shadows are requested, as they can't be supported with full scene shadows
	bCastInsetShadow = bCastInsetShadow || bCastCapsuleDirectShadow;

	const USkeletalMeshComponent* SkeletalMeshComponent = Cast<const USkeletalMeshComponent>(Component);
	if(SkeletalMeshComponent && SkeletalMeshComponent->bPerBoneMotionBlur)
	{
		bAlwaysHasVelocity = true;
	}

	const auto FeatureLevel = GetScene().GetFeatureLevel();

	// setup materials and performance classification for each LOD.
	extern bool GForceDefaultMaterial;
	bool bCastShadow = Component->CastShadow;
	bool bAnySectionCastsShadow = false;
	LODSections.Reserve(SkelMeshResource->LODModels.Num());
	LODSections.AddZeroed(SkelMeshResource->LODModels.Num());
	for(int32 LODIdx=0; LODIdx < SkelMeshResource->LODModels.Num(); LODIdx++)
	{
		const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIdx];
		const FSkeletalMeshLODInfo& Info = Component->SkeletalMesh->LODInfo[LODIdx];

		FLODSectionElements& LODSection = LODSections[LODIdx];

		// Presize the array
		LODSection.SectionElements.Empty( LODModel.Sections.Num() );
		for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

			// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
			int32 UseMaterialIndex = Section.MaterialIndex;			
			if(LODIdx > 0)
			{
				if(Section.MaterialIndex < Info.LODMaterialMap.Num())
				{
					UseMaterialIndex = Info.LODMaterialMap[Section.MaterialIndex];
					UseMaterialIndex = FMath::Clamp( UseMaterialIndex, 0, Component->SkeletalMesh->Materials.Num() );
				}
			}

			// If Section is hidden, do not cast shadow
			bool bSectionHidden = MeshObject->IsMaterialHidden(LODIdx,UseMaterialIndex);

			// Disable rendering for cloth mapped sections
			bSectionHidden |= Section.bDisabled;

			// If the material is NULL, or isn't flagged for use with skeletal meshes, it will be replaced by the default material.
			UMaterialInterface* Material = Component->GetMaterial(UseMaterialIndex);
			if (GForceDefaultMaterial && Material && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			// if this is a clothing section, then enabled and will be drawn but the corresponding original section should be disabled
			bool bClothSection = (!Section.bDisabled && Section.CorrespondClothSectionIndex >= 0);

			if(bClothSection)
			{
				// the cloth section's material index must be same as the original section's material index
				check(Section.MaterialIndex == LODModel.Sections[Section.CorrespondClothSectionIndex].MaterialIndex);
			}

			if(!Material || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh) ||
			  (bClothSection && !Material->CheckMaterialUsage_Concurrent(MATUSAGE_Clothing)))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation( Material, &TGPUSkinVertexFactory<false>::StaticType, FeatureLevel );
			if ( bRequiresAdjacencyInformation && LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() == false )
			{
				UE_LOG(LogSkeletalMesh, Warning, 
					TEXT("Material %s requires adjacency information, but skeletal mesh %s does not have adjacency information built. The mesh must be rebuilt to be used with this material. The mesh will be rendered with DefaultMaterial."), 
					*Material->GetPathName(), 
					*Component->SkeletalMesh->GetPathName() )
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
			}

			bool bSectionCastsShadow = !bSectionHidden && bCastShadow &&
				(Component->SkeletalMesh->Materials.IsValidIndex(UseMaterialIndex) == false || Component->SkeletalMesh->Materials[UseMaterialIndex].bEnableShadowCasting);

			bAnySectionCastsShadow |= bSectionCastsShadow;
			LODSection.SectionElements.Add(
				FSectionElementInfo(
					Material,
					bSectionCastsShadow,
					UseMaterialIndex
					));
			MaterialsInUse_GameThread.Add(Material);
		}
	}

	bCastDynamicShadow = bCastDynamicShadow && bAnySectionCastsShadow;

	// Try to find a color for level coloration.
	if( Owner )
	{
		ULevel* Level = Owner->GetLevel();
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		if ( LevelStreaming )
		{
			LevelColor = LevelStreaming->LevelColor;
		}
	}

	// Get a color for property coloration
	FColor NewPropertyColor;
	GEngine->GetPropertyColorationColor( (UObject*)Component, NewPropertyColor );
	PropertyColor = NewPropertyColor;

	// Copy out shadow physics asset data
	if(SkeletalMeshComponent)
	{
		UPhysicsAsset* ShadowPhysicsAsset = SkeletalMeshComponent->SkeletalMesh->ShadowPhysicsAsset;

		if (ShadowPhysicsAsset
			&& SkeletalMeshComponent->CastShadow
			&& (SkeletalMeshComponent->bCastCapsuleDirectShadow || SkeletalMeshComponent->bCastCapsuleIndirectShadow))
		{
			for (int32 BodyIndex = 0; BodyIndex < ShadowPhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				UBodySetup* BodySetup = ShadowPhysicsAsset->SkeletalBodySetups[BodyIndex];
				int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BodySetup->BoneName);

				if (BoneIndex != INDEX_NONE)
				{
					const FMatrix& RefBoneMatrix = SkeletalMeshComponent->SkeletalMesh->GetComposedRefPoseMatrix(BoneIndex);

					const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
					for (int32 SphereIndex = 0; SphereIndex < NumSpheres; SphereIndex++)
					{
						const FKSphereElem& SphereShape = BodySetup->AggGeom.SphereElems[SphereIndex];
						ShadowCapsuleData.Add(TPairInitializer<int32, FCapsuleShape>(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphereShape.Center), SphereShape.Radius, FVector(0.0f, 0.0f, 1.0f), 0.0f)));
					}

					const int32 NumCapsules = BodySetup->AggGeom.SphylElems.Num();
					for (int32 CapsuleIndex = 0; CapsuleIndex < NumCapsules; CapsuleIndex++)
					{
						const FKSphylElem& SphylShape = BodySetup->AggGeom.SphylElems[CapsuleIndex];
						ShadowCapsuleData.Add(TPairInitializer<int32, FCapsuleShape>(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphylShape.Center), SphylShape.Radius, RefBoneMatrix.TransformVector((SphylShape.Orientation * SphylBasis).Vector()), SphylShape.Length)));
					}

					if (NumSpheres > 0 || NumCapsules > 0)
					{
						ShadowCapsuleBoneIndices.AddUnique(BoneIndex);
					}
				}
			}
		}
	}

	// Sort to allow merging with other bone hierarchies
	if (ShadowCapsuleBoneIndices.Num())
	{
		ShadowCapsuleBoneIndices.Sort();
	}
}


// FPrimitiveSceneProxy interface.

/** 
 * Iterates over sections,chunks,elements based on current instance weight usage 
 */
class FSkeletalMeshSectionIter
{
public:
	FSkeletalMeshSectionIter(const int32 InLODIdx, const FSkeletalMeshObject& InMeshObject, const FStaticLODModel& InLODModel, const FSkeletalMeshSceneProxy::FLODSectionElements& InLODSectionElements)
		: SectionIndex(0)
		, MeshObject(InMeshObject)
		, LODSectionElements(InLODSectionElements)
		, Sections(InLODModel.Sections)
#if WITH_EDITORONLY_DATA
		, SectionIndexPreview(InMeshObject.SectionIndexPreview)
#endif
	{
		while (NotValidPreviewSection())
		{
			SectionIndex++;
		}
	}
	FORCEINLINE FSkeletalMeshSectionIter& operator++()
	{
		do 
		{
		SectionIndex++;
		} while (NotValidPreviewSection());
		return *this;
	}
	FORCEINLINE operator bool() const
	{
		return ((SectionIndex < Sections.Num()) && LODSectionElements.SectionElements.IsValidIndex(GetSectionElementIndex()));
	}
	FORCEINLINE const FSkelMeshSection& GetSection() const
	{
		return Sections[SectionIndex];
	}
	FORCEINLINE const FTwoVectors& GetCustomLeftRightVectors() const
	{
		return MeshObject.GetCustomLeftRightVectors(SectionIndex);
	}
	FORCEINLINE const int32 GetSectionElementIndex() const
	{
		return SectionIndex;
	}
	FORCEINLINE const FSkeletalMeshSceneProxy::FSectionElementInfo& GetSectionElementInfo() const
	{
		int32 SectionElementInfoIndex = GetSectionElementIndex();
		return LODSectionElements.SectionElements[SectionElementInfoIndex];
	}
	FORCEINLINE bool NotValidPreviewSection()
	{
#if WITH_EDITORONLY_DATA

		int32 ActualPreviewSectionIdx = SectionIndexPreview;
		if(ActualPreviewSectionIdx != INDEX_NONE && Sections.IsValidIndex(ActualPreviewSectionIdx))
		{
			const FSkelMeshSection& PreviewSection = Sections[ActualPreviewSectionIdx];
			if(PreviewSection.CorrespondClothSectionIndex != INDEX_NONE)
			{
				ActualPreviewSectionIdx = PreviewSection.CorrespondClothSectionIndex;
			}
		}

		return	(SectionIndex < Sections.Num()) && 
				((ActualPreviewSectionIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
#else
		return false;
#endif
	}
private:
	int32 SectionIndex;
	const FSkeletalMeshObject& MeshObject;
	const FSkeletalMeshSceneProxy::FLODSectionElements& LODSectionElements;
	const TArray<FSkelMeshSection>& Sections;
#if WITH_EDITORONLY_DATA
	const int32 SectionIndexPreview;
#endif
};

#if WITH_EDITOR
HHitProxy* FSkeletalMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if ( Component->GetOwner() )
	{
		if ( LODSections.Num() > 0 )
		{
			for ( int32 LODIndex = 0; LODIndex < SkelMeshResource->LODModels.Num(); LODIndex++ )
			{
				const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

				FLODSectionElements& LODSection = LODSections[LODIndex];

				check(LODSection.SectionElements.Num() == LODModel.Sections.Num());

				for ( int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++ )
				{
					HHitProxy* ActorHitProxy;

					if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex);
					}
					else
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, SectionIndex);
					}

					// Set the hitproxy.
					check(LODSection.SectionElements[SectionIndex].HitProxy == NULL);
					LODSection.SectionElements[SectionIndex].HitProxy = ActorHitProxy;
					OutHitProxies.Add(ActorHitProxy);
				}
			}
		}
		else
		{
			return FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		}
	}

	return NULL;
}
#endif

void FSkeletalMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshSceneProxy_GetMeshElements);
	GetMeshElementsConditionallySelectable(Views, ViewFamily, true, VisibilityMap, Collector);
}

void FSkeletalMeshSceneProxy::GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if( !MeshObject )
	{
		return;
	}	
	MeshObject->PreGDMECallback(ViewFamily.FrameNumber);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			MeshObject->UpdateMinDesiredLODLevel(View, GetBounds(), ViewFamily.FrameNumber);
		}
	}

	const int32 LODIndex = MeshObject->GetLOD();
	check(LODIndex < SkelMeshResource->LODModels.Num());
	const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

	if( LODSections.Num() > 0 )
	{
		const FLODSectionElements& LODSection = LODSections[LODIndex];

		check(LODSection.SectionElements.Num() == LODModel.Sections.Num());

		for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODModel, LODSection); Iter; ++Iter)
		{
			const FSkelMeshSection& Section = Iter.GetSection();
			const int32 SectionIndex = Iter.GetSectionElementIndex();
			const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();
			const FTwoVectors& CustomLeftRightVectors = Iter.GetCustomLeftRightVectors();

			bool bSectionSelected = false;

#if WITH_EDITORONLY_DATA
			// TODO: This is not threadsafe! A render command should be used to propagate SelectedEditorSection to the scene proxy.
			bSectionSelected = (SkeletalMeshForDebug->SelectedEditorSection == Iter.GetSectionElementIndex());
#endif
			// If hidden skip the draw
			if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex))
			{
				continue;
			}

			// If disabled, then skip the draw
			if(Section.bDisabled)
			{
				continue;
			}

			GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODModel, LODIndex, SectionIndex, bSectionSelected, SectionElementInfo, CustomLeftRightVectors, bInSelectable, Collector);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if( PhysicsAssetForDebug )
			{
				DebugDrawPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

			if (ViewFamily.EngineShowFlags.SkeletalMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
	}
#endif
}

void FSkeletalMeshSceneProxy::GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, 
	const FStaticLODModel& LODModel, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
	const FSectionElementInfo& SectionElementInfo, const FTwoVectors& CustomLeftRightVectors, bool bInSelectable, FMeshElementCollector& Collector ) const
{
	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	// If hidden skip the draw
	if (Section.bDisabled || MeshObject->IsMaterialHidden(LODIndex,SectionElementInfo.UseMaterialIndex))
	{
		return;
	}

#if !WITH_EDITOR
	const bool bIsSelected = false;
#else // #if !WITH_EDITOR
	bool bIsSelected = IsSelected();

	// if the mesh isn't selected but the mesh section is selected in the AnimSetViewer, find the mesh component and make sure that it can be highlighted (ie. are we rendering for the AnimSetViewer or not?)
	if( !bIsSelected && bSectionSelected && bCanHighlightSelectedSections )
	{
		bIsSelected = true;
	}
#endif // #if WITH_EDITOR

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	const ERHIFeatureLevel::Type FeatureLevel = ViewFamily.GetFeatureLevel();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			Mesh.DynamicVertexData = NULL;
			Mesh.UseDynamicData = false;
			Mesh.LCI = NULL;
			Mesh.bWireframe |= bForceWireframe;
			Mesh.Type = PT_TriangleList;
			Mesh.VertexFactory = MeshObject->GetSkinVertexFactory(View, LODIndex, SectionIndex);
			
			if(!Mesh.VertexFactory)
			{
				// hide this part
				continue;
			}

			Mesh.bSelectable = bInSelectable;
			BatchElement.FirstIndex = Section.BaseIndex;

			BatchElement.IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
			BatchElement.MaxVertexIndex = LODModel.NumVertices - 1;
	
			if(SectionIndex < FSkeletalMeshObject::MAX_GPUSKINCACHE_CHUNKS_PER_LOD)
			{
				BatchElement.UserIndex = MeshObject->GPUSkinCacheKeys[SectionIndex];
			}
			else
			{
				BatchElement.UserIndex = -1;
			}

			const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation( SectionElementInfo.Material, Mesh.VertexFactory->GetType(), ViewFamily.GetFeatureLevel() );
			if ( bRequiresAdjacencyInformation )
			{
				check( LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() );
				BatchElement.IndexBuffer = LODModel.AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
				Mesh.Type = PT_12_ControlPointPatchList;
				BatchElement.FirstIndex *= 4;
			}

		#if WITH_EDITOR

			Mesh.BatchHitProxyId = SectionElementInfo.HitProxy ? SectionElementInfo.HitProxy->Id : FHitProxyId();

			if (bSectionSelected)
			{
				auto SelectionOverrideProxy = new FOverrideSelectionColorMaterialRenderProxy(
					SectionElementInfo.Material->GetRenderProxy(bIsSelected, IsHovered()),
					GetSelectionColor(GEngine->GetSelectedMaterialColor(), bIsSelected, IsHovered())
					);

				Collector.RegisterOneFrameMaterialProxy(SelectionOverrideProxy);

				Mesh.MaterialRenderProxy = SelectionOverrideProxy;
			}
			else
			{
				Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy(bIsSelected, IsHovered());
			}
		#else
			Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy(bIsSelected, IsHovered());
		#endif

			BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();

			// Select which indices to use if TRISORT_CustomLeftRight
			if( Section.TriangleSorting == TRISORT_CustomLeftRight )
			{
				switch( MeshObject->CustomSortAlternateIndexMode )
				{
				case CSAIM_Left:
					// Left view - use second set of indices.
					BatchElement.FirstIndex += Section.NumTriangles * 3;
					break;
				case  CSAIM_Right:
					// Right view - use first set of indices.
					break;
				default:
					// Calculate the viewing direction
					FVector SortWorldOrigin = GetLocalToWorld().TransformPosition(CustomLeftRightVectors.v1);
					FVector SortWorldDirection = GetLocalToWorld().TransformVector(CustomLeftRightVectors.v2);

					if( (SortWorldDirection | (SortWorldOrigin - View->ViewMatrices.ViewOrigin)) < 0.f )
					{
						BatchElement.FirstIndex += Section.NumTriangles * 3;
					}
					break;
				}
			}

			BatchElement.NumPrimitives = Section.NumTriangles;

#if WITH_EDITORONLY_DATA
			if( GIsEditor && MeshObject->ProgressiveDrawingFraction != 1.f )
			{
				if (Mesh.MaterialRenderProxy && Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->GetBlendMode() == BLEND_Translucent)
				{
					BatchElement.NumPrimitives = FMath::RoundToInt(((float)Section.NumTriangles)*FMath::Clamp<float>(MeshObject->ProgressiveDrawingFraction,0.f,1.f));
					if( BatchElement.NumPrimitives == 0 )
					{
						continue;
					}
				}
			}
#endif // WITH_EDITORONLY_DATA

			BatchElement.MinVertexIndex = Section.BaseVertexIndex;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.CastShadow = SectionElementInfo.bEnableShadowCasting;

			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = bIsSelected;

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			Mesh.VisualizeLODIndex = LODIndex;
		#endif

			if ( ensureMsgf(Mesh.MaterialRenderProxy, TEXT("GetDynamicElementsSection with invalid MaterialRenderProxy. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex) &&
				 ensureMsgf(Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel), TEXT("GetDynamicElementsSection with invalid FMaterial. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex) )
			{
				Collector.AddMesh(ViewIndex, Mesh);
			}

			const int32 NumVertices = Section.GetNumVertices();
			INC_DWORD_STAT_BY(STAT_GPUSkinVertices,(uint32)(bIsCPUSkinned ? 0 : NumVertices));
			INC_DWORD_STAT_BY(STAT_SkelMeshTriangles,Mesh.GetNumPrimitives());
			INC_DWORD_STAT(STAT_SkelMeshDrawCalls);
		}
	}
}

bool FSkeletalMeshSceneProxy::HasDistanceFieldRepresentation() const
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
	// Self shadowing detection reuses the distance field GBuffer bit, so disable when distance field features are in use
	return CastsDynamicShadow() && CastsCapsuleIndirectShadow() && CVar->GetValueOnRenderThread() == 0;
}

void FSkeletalMeshSceneProxy::GetShadowShapes(TArray<FCapsuleShape>& CapsuleShapes) const 
{
	SCOPE_CYCLE_COUNTER(STAT_GetShadowShapes);

	const TArray<FMatrix>& ReferenceToLocalMatrices = MeshObject->GetReferenceToLocalMatrices();
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	int32 CapsuleIndex = CapsuleShapes.Num();
	CapsuleShapes.SetNum(CapsuleShapes.Num() + ShadowCapsuleData.Num(), false);

	for(const TPair<int32, FCapsuleShape>& CapsuleData : ShadowCapsuleData)
	{
		FMatrix ReferenceToWorld = ReferenceToLocalMatrices[CapsuleData.Key] * ProxyLocalToWorld;
		const float MaxScale = ReferenceToWorld.GetScaleVector().GetMax();

		FCapsuleShape& NewCapsule = CapsuleShapes[CapsuleIndex++];

		NewCapsule.Center = ReferenceToWorld.TransformPosition(CapsuleData.Value.Center);
		NewCapsule.Radius = CapsuleData.Value.Radius * MaxScale;
		NewCapsule.Orientation = ReferenceToWorld.TransformVector(CapsuleData.Value.Orientation).GetSafeNormal();
		NewCapsule.Length = CapsuleData.Value.Length * MaxScale;
	}
}

/**
 * Returns the world transform to use for drawing.
 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
 */
bool FSkeletalMeshSceneProxy::GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const
{
	OutLocalToWorld = GetLocalToWorld();
	if (OutLocalToWorld.GetScaledAxis(EAxis::X).IsNearlyZero(SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Y).IsNearlyZero(SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Z).IsNearlyZero(SMALL_NUMBER))
	{
		return false;
	}
	OutWorldToLocal = GetLocalToWorld().InverseFast();
	return true;
}

/**
 * Relevance is always dynamic for skel meshes unless they are disabled
 */
FPrimitiveViewRelevance FSkeletalMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.SkeletalMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

#if !UE_BUILD_SHIPPING
	Result.bSeparateTranslucencyRelevance |= View->Family->EngineShowFlags.Constraints;
#endif
	return Result;
}

bool FSkeletalMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !ShouldRenderCustomDepth();
}

/** Util for getting LOD index currently used by this SceneProxy. */
int32 FSkeletalMeshSceneProxy::GetCurrentLODIndex()
{
	if(MeshObject)
	{
		return MeshObject->GetLOD();
	}
	else
	{
		return 0;
	}
}


/** 
 * Render physics asset for debug display
 */
void FSkeletalMeshSceneProxy::DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FMatrix ProxyLocalToWorld, WorldToLocal;
	if (!GetWorldMatrices(ProxyLocalToWorld, WorldToLocal))
	{
		return; // Cannot draw this, world matrix not valid
	}

	FMatrix ScalingMatrix = ProxyLocalToWorld;
	FVector TotalScale = ScalingMatrix.ExtractScaling();

	// Only if valid
	if( !TotalScale.IsNearlyZero() )
	{
		FTransform LocalToWorldTransform(ProxyLocalToWorld);

		TArray<FTransform>* BoneSpaceBases = MeshObject->GetComponentSpaceTransforms();
		if(BoneSpaceBases)
		{
			//TODO: These data structures are not double buffered. This is not thread safe!
			check(PhysicsAssetForDebug);
			if (EngineShowFlags.Collision && IsCollisionEnabled())
			{
				PhysicsAssetForDebug->GetCollisionMesh(ViewIndex, Collector, SkeletalMeshForDebug, *BoneSpaceBases, LocalToWorldTransform, TotalScale);
			}
			if (EngineShowFlags.Constraints)
			{
				PhysicsAssetForDebug->DrawConstraints(ViewIndex, Collector, SkeletalMeshForDebug, *BoneSpaceBases, LocalToWorldTransform, TotalScale.X);
			}
		}
	}
}

/**
* Updates morph material usage for materials referenced by each LOD entry
*
* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
*/
void FSkeletalMeshSceneProxy::UpdateMorphMaterialUsage_GameThread(bool bNeedsMorphUsage)
{
	if( bNeedsMorphUsage != bMaterialsNeedMorphUsage_GameThread )
	{
		// keep track of current morph material usage for the proxy
		bMaterialsNeedMorphUsage_GameThread = bNeedsMorphUsage;

		TSet<UMaterialInterface*> MaterialsToSwap;
		for (auto It = MaterialsInUse_GameThread.CreateConstIterator(); It; ++It)
		{
			UMaterialInterface* Material = *It;
			if (Material)
			{
				const bool bCheckMorphUsage = !bMaterialsNeedMorphUsage_GameThread || (bMaterialsNeedMorphUsage_GameThread && Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets));
				const bool bCheckSkelUsage = Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
				// make sure morph material usage and default skeletal usage are both valid
				if( !bCheckMorphUsage || !bCheckSkelUsage  )
				{
					MaterialsToSwap.Add(Material);
				}
			}
		}

		// update the new LODSections on the render thread proxy
		if (MaterialsToSwap.Num())
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
			UpdateSkelProxyLODSectionElementsCmd,
				TSet<UMaterialInterface*>,MaterialsToSwap,MaterialsToSwap,
				UMaterialInterface*,DefaultMaterial,UMaterial::GetDefaultMaterial(MD_Surface),
				ERHIFeatureLevel::Type, FeatureLevel, GetScene().GetFeatureLevel(),
			FSkeletalMeshSceneProxy*,SkelMeshSceneProxy,this,
			{
					for( int32 LodIdx=0; LodIdx < SkelMeshSceneProxy->LODSections.Num(); LodIdx++ )
					{
						FLODSectionElements& LODSection = SkelMeshSceneProxy->LODSections[LodIdx];
						for( int32 SectIdx=0; SectIdx < LODSection.SectionElements.Num(); SectIdx++ )
						{
							FSectionElementInfo& SectionElement = LODSection.SectionElements[SectIdx];
							if( MaterialsToSwap.Contains(SectionElement.Material) )
							{
								// fallback to default material if needed
								SectionElement.Material = DefaultMaterial;
							}
						}
					}
					SkelMeshSceneProxy->MaterialRelevance |= DefaultMaterial->GetRelevance(FeatureLevel);
			});
		}
	}
}

#if WITH_EDITORONLY_DATA
const FStreamingSectionBuildInfo* FSkeletalMeshSceneProxy::GetStreamingSectionData(float& OutComponentExtraScale, float& OutMeshExtraScale, int32 LODIndex, int32 ElementIndex) const
{
	static FStreamingSectionBuildInfo Data;
	
	OutMeshExtraScale = 1.f; // No extra scale as this scale is already taken into account in StreamingTexelFactor
	OutComponentExtraScale = StreamingDistanceMultiplier;

	Data.BoxOrigin = GetBounds().Origin;
	Data.BoxExtent = GetBounds().BoxExtent;
	for (int32 TexCoordIndex = 0; TexCoordIndex < FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD; ++TexCoordIndex)
	{
		Data.TexelFactors[TexCoordIndex] = StreamingTexelFactor;
	}
	return &Data;
}
#endif

USkinnedMeshComponent::USkinnedMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AnimUpdateRateParams(NULL)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;	
	WireframeColor = FColor(221, 221, 28, 255);

	MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	StreamingDistanceMultiplier = 1.0f;
	bCanHighlightSelectedSections = false;
	CanCharacterStepUpOn = ECB_Owner;
#if WITH_EDITORONLY_DATA
	ProgressiveDrawingFraction = 1.0f;
	ChunkIndexPreview = -1;
	SectionIndexPreview = -1;
#endif // WITH_EDITORONLY_DATA
	bPerBoneMotionBlur = true;
	bCastCapsuleDirectShadow = false;
	bCastCapsuleIndirectShadow = false;

	bDoubleBufferedComponentSpaceTransforms = true;
	CurrentEditableComponentTransforms = 0;
	CurrentReadComponentTransforms = 1;
	bNeedToFlipSpaceBaseBuffers = false;

	bCanEverAffectNavigation = false;
	MasterBoneMapCacheCount = 0;
}


void USkinnedMeshComponent::UpdateMorphMaterialUsageOnProxy()
{
	// update morph material usage
	if( SceneProxy )
	{
		const bool bHasMorphs = ActiveMorphTargets.Num() > 0;
		((FSkeletalMeshSceneProxy*)SceneProxy)->UpdateMorphMaterialUsage_GameThread(bHasMorphs);
	}
}

// UObject interface
// Override to have counting working better
void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsCountingMemory())
	{
		// add all native variables - mostly bigger chunks 
		ComponentSpaceTransformsArray[0].CountBytes(Ar);
		ComponentSpaceTransformsArray[1].CountBytes(Ar);
		MasterBoneMap.CountBytes(Ar);
	}
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
	{
		if ((NULL != AnimationBlueprint_DEPRECATED) && (NULL == AnimBlueprintGeneratedClass))
		{
			AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimationBlueprint_DEPRECATED->GeneratedClass);
		}
	}
#endif

	Super::Serialize(Ar);
			
	// to count memory : TODO: REMOVE?
	if(Ar.IsCountingMemory())
	{
		BoneSpaceTransforms.CountBytes(Ar);
		RequiredBones.CountBytes(Ar);
	}

	if (Ar.UE4Ver() < VER_UE4_REMOVE_SKELETALMESH_COMPONENT_BODYSETUP_SERIALIZATION)
	{
		//we used to serialize bodysetup of skeletal mesh component. We no longer do this, but need to not break existing content
		if (bEnablePerPolyCollision)
		{
			Ar << BodySetup;
		}
	}

	// Since we separated simulation vs blending
	// if simulation is on when loaded, just set blendphysics to be true
	if (BodyInstance.bSimulatePhysics)
	{
		bBlendPhysics = true;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_EDITORONLY_BLUEPRINTS))
	{
		if ((NULL != AnimationBlueprint_DEPRECATED))
		{
			// Migrate the class from the animation blueprint once, and null the value so we never get in again
			AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimationBlueprint_DEPRECATED->GeneratedClass);
			AnimationBlueprint_DEPRECATED = NULL;
		}
	}
#endif

	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_NO_ANIM_BP_CLASS_IN_GAMEPLAY_CODE))
	{
		if (nullptr != AnimBlueprintGeneratedClass)
		{
			AnimClass = AnimBlueprintGeneratedClass;
		}
	}

	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_AUTO_WELDING))
	{
		BodyInstance.bAutoWeld = false;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


SIZE_T USkinnedMeshComponent::GetResourceSize(EResourceSizeMode::Type Mode)
{
	int32 ResourceSize = 0;
	// Get Mesh Object's memory
	if(MeshObject)
	{
		ResourceSize += MeshObject->GetResourceSize();
	}

	return ResourceSize;
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	ERHIFeatureLevel::Type SceneFeatureLevel = GetWorld()->FeatureLevel;
	FSkeletalMeshSceneProxy* Result = NULL;
	FSkeletalMeshResource* SkelMeshResource = GetSkeletalMeshResource();

	// Only create a scene proxy for rendering if properly initialized
	if( SkelMeshResource && 
		SkelMeshResource->LODModels.IsValidIndex(PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject)
	{
		// Only create a scene proxy if the bone count being used is supported, or if we don't have a skeleton (this is the case with destructibles)
		int32 MaxBonesPerChunk = SkelMeshResource->GetMaxBonesPerSection();
		if (MaxBonesPerChunk <= GetFeatureLevelMaxNumberOfBones(SceneFeatureLevel))
		{
			Result = ::new FSkeletalMeshSceneProxy(this,SkelMeshResource);
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

/** Returns SkeletalMeshComponent subobject **/
USkeletalMeshComponent* ASkeletalMeshActor::GetSkeletalMeshComponent() { return SkeletalMeshComponent; }



