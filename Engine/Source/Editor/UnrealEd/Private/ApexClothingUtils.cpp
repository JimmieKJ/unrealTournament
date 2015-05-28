// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexClothingUtils, Log, All);

#include "PhysicsPublic.h"
#include "ApexClothingUtils.h"

#if WITH_APEX

#include "EditorPhysXSupport.h"
// Utilities
#include "NxParamUtils.h"

#if WITH_APEX_CLOTHING
	#include "NxClothingAssetAuthoring.h"
#endif // #if WITH_APEX_CLOTHING

#include "Runtime/Engine/Private/PhysicsEngine/PhysXSupport.h"

#endif // #if WITH_APEX

#define LOCTEXT_NAMESPACE "ApexClothingUtils"

namespace ApexClothingUtils
{
#if WITH_APEX_CLOTHING

	//for re-import of skeletal mesh
	FClothingBackup GClothingBackup;


struct FBoneIndices
{
	// Whenever Apex supports 8 bone influences per vertex, use MAX_TOTAL_INFLUENCES instead; also update FApexBoneIndices::Data and FApexClothPhysToRenderVertData::SimulMeshVertIndices
	uint8 Data[4];

	void Set(uint8 InData0, uint8 InData1, uint8 InData2, uint8 InData3)
	{
		Data[0] = InData0; 
		Data[1] = InData1; 
		Data[2] = InData2; 
		Data[3] = InData3;

		static_assert(MAX_INFLUENCES_PER_STREAM == sizeof(Data), "Mismatched bone influences per vertex.");
	}
};

struct FVertexImportData
{
	TArray <FVector>		Positions;
	TArray <FVector>		Normals;
	TArray <FVector>		Tangents;
	TArray <FVector>		Binormals;
	TArray <FColor>			Colors;
	TArray <FVector2D>		UVs;
	TArray <FBoneIndices>	BoneIndices;
	TArray <FVector4>		BoneWeights;
	uint32					NumTexCoords;		// The number of texture coordinate sets
	bool					bHasTangents;		// If true there are tangents (TangentX) in the imported file
	bool 					bHasVertexColors;	// If true there are vertex colors in the imported file

	FVertexImportData()
		: NumTexCoords(0)
		, bHasTangents(false)
		, bHasVertexColors(false)
	{
	}

	void Empty(int32 Slack=0)
	{
		Positions.Empty(Slack);
		Normals.Empty(Slack);
		Tangents.Empty(Slack);
		Binormals.Empty(Slack);
		Colors.Empty(Slack);
		UVs.Empty(Slack);
		BoneIndices.Empty(Slack);
		BoneWeights.Empty(Slack);
	}

	void AddUninitialized( int32 Count=1 )
	{
		Positions.AddUninitialized(Count);
		Normals.AddUninitialized(Count);
		Tangents.AddUninitialized(Count);
		Binormals.AddUninitialized(Count);
		Colors.AddUninitialized(Count);
		UVs.AddUninitialized(Count);
		BoneIndices.AddUninitialized(Count);
		BoneWeights.AddUninitialized(Count);
	}

};

//enforces a call of "OnRegister" to update vertex factories
void ReregisterSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == SkelMesh )
		{
			MeshComponent->ReregisterComponent();
		}
	}
}

void RefreshSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == SkelMesh )
		{
			MeshComponent->RecreateRenderState_Concurrent();
		}
	}
}

int32 FindSectionByMaterialIndex( USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 MaterialIndex )
{
	FSkeletalMeshResource* ImportedResource = SkelMesh->GetImportedResource();
	check(LODIndex < (uint32)ImportedResource->LODModels.Num());

	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];
	int32 NumNonClothSections = LODModel.NumNonClothingSections();

	for(int32 SecIdx=0; SecIdx < NumNonClothSections; SecIdx++)
	{
		if(LODModel.Sections[SecIdx].MaterialIndex == MaterialIndex)
		{
			return SecIdx;
		}
	}

	return -1;
}

uint32 GetMaxClothSimulVertices(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel >= ERHIFeatureLevel::SM4)
	{					
		return MAX_APEXCLOTH_VERTICES_FOR_VB;
	}
	else
	{
		return MAX_APEXCLOTH_VERTICES_FOR_UB;
	}
}

#if WITH_APEX_CLOTHING
NxClothingAsset* CreateApexClothingAssetFromPxStream(physx::PxFileBuf& Stream)
{
	// Peek into the buffer to see what kind of data it is (binary or xml)
	NxParameterized::Serializer::SerializeType SerializeType = GApexSDK->getSerializeType(Stream);
	// Create an NxParameterized serializer for the correct data type
	NxParameterized::Serializer* Serializer = GApexSDK->createSerializer(SerializeType);

	if(!Serializer)
	{
		return NULL;
	}
	// Deserialize into a DeserializedData buffer
	NxParameterized::Serializer::DeserializedData DeserializedData;
	NxParameterized::Serializer::ErrorType Error = Serializer->deserialize(Stream, DeserializedData);

	NxApexAsset* ApexAsset = NULL;
	if( DeserializedData.size() > 0 )
	{
		// The DeserializedData has something in it, so create an APEX asset from it
		ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL);
	}
	// Release the serializer
	Serializer->release();

	return (NxClothingAsset*)ApexAsset;
}

NxClothingAsset* CreateApexClothingAssetFromFile(FString& Filename)
{
	NxClothingAsset* ApexClothingAsset = NULL;

	TArray<uint8> FileBuffer;
	if(FFileHelper::LoadFileToArray(FileBuffer, *Filename, FILEREAD_Silent))
	{
		physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream((void*)FileBuffer.GetData(), FileBuffer.Num());

		if(Stream != NULL)
		{
			ApexClothingAsset = CreateApexClothingAssetFromPxStream(*Stream);
			// Release our stream
			Stream->release();
		}
	}
	else
	{
		UE_LOG(LogApexClothingUtils, Warning, TEXT("Could not open APEX clothing file: %s"), *Filename);
	}

	return ApexClothingAsset;
}

NxClothingAsset* CreateApexClothingAssetFromBuffer(const uint8* Buffer, int32 BufferSize)
{
	NxClothingAsset* ApexClothingAsset = NULL;

	// Wrap Buffer with the APEX read stream class
	physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream(Buffer, BufferSize);

	if (Stream != NULL)
	{
		ApexClothingAsset = ApexClothingUtils::CreateApexClothingAssetFromPxStream(*Stream);
		// Release our stream
		GApexSDK->releaseMemoryReadStream(*Stream);
	}

	return ApexClothingAsset;
}

#endif// #if WITH_APEX_CLOTHING

void LoadBonesFromClothingAsset(NxClothingAsset& ApexClothingAsset, TArray<FName>& BoneNames)
{
	const uint32 NumBones = ApexClothingAsset.getNumUsedBones();

	if(NumBones == 0)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s has no bones"), ANSI_TO_TCHAR(ApexClothingAsset.getName()) );
		return;
	}

	BoneNames.Empty(NumBones);

	for (uint32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
	{
		FString BoneName = FString(ApexClothingAsset.getBoneName(BoneIndex)).Replace(TEXT(" "),TEXT("-"));

		BoneNames.Add(*BoneName);
	}
}

FVector4 P2U4BaryCoord(const PxVec3& PVec)
{
	return FVector4(PVec.x, PVec.y, 1.f - PVec.x -PVec.y, PVec.z);

}

bool LoadPhysicalMeshFromClothingAsset(NxClothingAsset& ApexClothingAsset,
										TArray<FApexClothPhysToRenderVertData>& RenderToPhysicalMapping, 
										TArray<FVector>& PhysicalMeshVertices, 
										TArray<FVector>& PhysicalMeshNormals,
										int32 LODIndex, 
										uint32 StartSimulIndex, 
										uint32 NumRealSimulVertices,
										uint32 NumTotalVertices)
{

	TArray<ClothingMeshSkinningMap> SkinningMap;

	int32 NumLOD = ApexClothingAsset.getNumGraphicalLodLevels();

	check(LODIndex < NumLOD);

	uint32 MapSize = ApexClothingAsset.getMeshSkinningMapSize(LODIndex);

	SkinningMap.AddUninitialized(MapSize);
	ApexClothingAsset.getMeshSkinningMap(LODIndex,SkinningMap.GetData());

	// Mapping
	uint32 EndTotalIndex = StartSimulIndex + NumTotalVertices;

	RenderToPhysicalMapping.Empty(NumTotalVertices);

	uint16 MaxSimulVertIndex = 0;

	for(uint32 i=StartSimulIndex; i < EndTotalIndex; i++)
	{
		FApexClothPhysToRenderVertData& Mapping = RenderToPhysicalMapping[RenderToPhysicalMapping.AddZeroed()];
		if(i < NumRealSimulVertices)
		{
			Mapping.PositionBaryCoordsAndDist = P2U4BaryCoord(SkinningMap[i].positionBary);
			Mapping.NormalBaryCoordsAndDist = P2U4BaryCoord(SkinningMap[i].normalBary);
			Mapping.TangentBaryCoordsAndDist = P2U4BaryCoord(SkinningMap[i].tangentBary);
			Mapping.SimulMeshVertIndices[0] = (uint16)SkinningMap[i].vertexIndex0;
			Mapping.SimulMeshVertIndices[1] = (uint16)SkinningMap[i].vertexIndex1;
			Mapping.SimulMeshVertIndices[2] = (uint16)SkinningMap[i].vertexIndex2;
			Mapping.SimulMeshVertIndices[3] = (uint16)0; // store valid index to distinguish from fixed vertex

			// to extract only clothing section's simulation index, because apex file includes non-clothing sections info as well.
			for(int ArrayIdx = 0; ArrayIdx < 3; ArrayIdx++)
			{
				MaxSimulVertIndex = FMath::Max(MaxSimulVertIndex, Mapping.SimulMeshVertIndices[ArrayIdx]);
			}
		}
	}

	// processing fixed vertices
	for(uint32 i=NumRealSimulVertices; i < NumTotalVertices; i++)
	{
		FApexClothPhysToRenderVertData& Mapping = RenderToPhysicalMapping[i];
		Mapping.SimulMeshVertIndices[3] = (uint16)0xFFFF; // fixed vertices
	}

	// Load Parameters for PhysicalMesh via NxParameterized Interface

	const NxParameterized::Interface* AssetParams = ApexClothingAsset.getAssetNxParameterized();

	physx::PxI32 PhysicalMeshCount;
	NxParameterized::getParamArraySize(*AssetParams, "physicalMeshes", PhysicalMeshCount);

	check( PhysicalMeshCount > LODIndex );

	char ParameterName[MAX_SPRINTF];
	FCStringAnsi::Sprintf(ParameterName, "physicalMeshes[%d]", LODIndex);

	NxParameterized::Interface* PhysicalMeshParams;
	PxU32 NumVertices = 0;
	PxU32 NumIndices = 0;
	int32 MaxSimulVertexCount = MaxSimulVertIndex + 1;

	if (NxParameterized::getParamRef(*AssetParams, ParameterName, PhysicalMeshParams))
	{
		if(PhysicalMeshParams != NULL)
		{
			verify(NxParameterized::getParamU32(*PhysicalMeshParams, "physicalMesh.numVertices", NumVertices));
			verify(NxParameterized::getParamU32(*PhysicalMeshParams, "physicalMesh.numIndices", NumIndices));

			physx::PxI32 VertexCount = 0;
			if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.vertices", VertexCount))
			{
				check(VertexCount == NumVertices);

				check(MaxSimulVertexCount <= VertexCount);
				
				PhysicalMeshVertices.Empty(MaxSimulVertexCount);

				for (physx::PxI32 VertexIndex = 0; VertexIndex < MaxSimulVertexCount; ++VertexIndex)
				{
					FCStringAnsi::Sprintf( ParameterName, "physicalMesh.vertices[%d]", VertexIndex );
					NxParameterized::Handle MeshVertexHandle(*PhysicalMeshParams);
					if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshVertexHandle) != NULL)
					{
						physx::PxVec3  Vertex;
						FVector			Position;
						MeshVertexHandle.getParamVec3( Vertex );

						Position = P2UVector(Vertex);

						PhysicalMeshVertices.Add(Position);
					}
				}
			}

			physx::PxI32 NormalCount = 0;
			if (NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.normals", NormalCount))
			{
				check(NormalCount == NumVertices);
				check(MaxSimulVertexCount <= NormalCount);

				PhysicalMeshNormals.Empty(MaxSimulVertexCount);

				for (physx::PxI32 NormalIndex = 0; NormalIndex < MaxSimulVertexCount; ++NormalIndex)
				{
					FCStringAnsi::Sprintf( ParameterName, "physicalMesh.normals[%d]", NormalIndex );
					NxParameterized::Handle MeshNormalHandle(*PhysicalMeshParams);
					if (NxParameterized::findParam(*PhysicalMeshParams, ParameterName, MeshNormalHandle) != NULL)
					{
						physx::PxVec3  PxNormal;
						FVector			UNormal;
						MeshNormalHandle.getParamVec3( PxNormal );

						UNormal = P2UVector(PxNormal);

						PhysicalMeshNormals.Add(UNormal);

					}
				}
			}
		}
	}

	return true;
}

bool LoadGraphicalMeshFromClothingAsset(NxClothingAsset& ApexClothingAsset, 
	const NxRenderMeshAsset& ApexRenderMesh, 
	FVertexImportData& OutImportData, 
	TArray<uint32>& OutIndexBuffer, 
	uint32 LODIndex, 
	uint32 SelectedSubmeshIndex,
	uint32& OutVertexCount, 
	uint32& OutTriangleCount,
	uint32& OutStartSimulIndex,
	uint32& OutNumSimulVertices
)
{
	uint32 SubmeshCount = ApexRenderMesh.getSubmeshCount();

	if(SubmeshCount == 0)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s dosen't have submeshes"), ANSI_TO_TCHAR(ApexClothingAsset.getName()) );	
		return false;
	}

	if(SelectedSubmeshIndex >= SubmeshCount)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s submesh index is bigger"), ANSI_TO_TCHAR(ApexClothingAsset.getName()) );	
		return false;
	}

	const NxParameterized::Interface* AssetParams = ApexClothingAsset.getAssetNxParameterized();
	char ParameterName[MAX_SPRINTF];
	FCStringAnsi::Sprintf(ParameterName, "graphicalLods[%d]", LODIndex);

	NxParameterized::Interface* GraphicalMeshParams;
	int32 PhysicalSubmeshIndex = -1;

	if (NxParameterized::getParamRef(*AssetParams, ParameterName, GraphicalMeshParams))
	{
		if(GraphicalMeshParams != NULL)
		{
			FCStringAnsi::Sprintf(ParameterName, "physicsSubmeshPartitioning[%d].numSimulatedVertices", SelectedSubmeshIndex);
			uint32 SimulVert = 0;
			NxParameterized::getParamU32(*GraphicalMeshParams, ParameterName, SimulVert);

			OutNumSimulVertices = SimulVert;
		}
	}

	// Count vertices and triangles
	OutVertexCount = 0;
	OutTriangleCount = 0;

	uint32 TotalVertexCount = 0;
	uint32 TotalIndexCount = 0;
	uint32 TotalTriangleCount = 0;
	OutStartSimulIndex = 0;

	uint32 StartIBIndex = 0;
	uint32 IndexCount = 0;

	for(uint32 SubmeshIndex=0; SubmeshIndex < SubmeshCount; SubmeshIndex++)
	{
		const NxRenderSubmesh& Submesh = ApexRenderMesh.getSubmesh(SubmeshIndex);

		const NxVertexBuffer& VB = Submesh.getVertexBuffer();

		// Count vertices
		uint32 VBCount = VB.getVertexCount();

		// Count triangles
		uint32 IndexCountTemp = 0;
		for (uint32 PartIndex = 0; PartIndex < ApexRenderMesh.getPartCount(); ++PartIndex)
		{
			IndexCountTemp += Submesh.getIndexCount(PartIndex);
		}

		if(SubmeshIndex == SelectedSubmeshIndex)
		{
			OutStartSimulIndex = TotalVertexCount;
			OutVertexCount = VBCount;
			StartIBIndex = TotalIndexCount;
			IndexCount = IndexCountTemp;
			OutTriangleCount = IndexCount/3;
		}

		check(TotalIndexCount%3 == 0);
		TotalIndexCount += IndexCountTemp;
		TotalVertexCount += VBCount;
	}

	TotalTriangleCount = TotalIndexCount/3;
	OutIndexBuffer.Empty(IndexCount);

	OutImportData.AddUninitialized(OutVertexCount);

	// @TODO : is it OK to remove this variable "VertexIndexBase"?
	uint32 VertexIndexBase = 0;

	// Submesh data
	const NxRenderSubmesh& Submesh = ApexRenderMesh.getSubmesh(SelectedSubmeshIndex);
	const NxVertexBuffer& VB = Submesh.getVertexBuffer();
	const NxVertexFormat& VBFormat = VB.getFormat();
	const physx::PxU32 SubmeshVertexCount = VB.getVertexCount();

	check(OutVertexCount > 0);
	check(OutVertexCount == SubmeshVertexCount);

	for (uint32 PartIndex = 0; PartIndex < ApexRenderMesh.getPartCount(); ++PartIndex)
	{
		uint32 SubmeshPartIndiceCount = Submesh.getIndexCount(PartIndex);
		const physx::PxU32* PartIndexBuffer = Submesh.getIndexBuffer(PartIndex);

		for(uint32 i = 0; i < SubmeshPartIndiceCount; i++)
		{
			OutIndexBuffer.Add(PartIndexBuffer[i]);
		}
	}

	check(OutIndexBuffer.Num() % 3 == 0);

	//change the order of indices to flip polygons for matching handedness between UE4 and APEX 
	for(int32 Index=0; Index < OutIndexBuffer.Num(); Index += 3)
	{
		Swap(OutIndexBuffer[Index+1], OutIndexBuffer[Index+2]);
	}

	// Get VB data semantic indices:

	// Positions
	const PxI32 PositionBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::POSITION));
	if (!VB.getBufferData(&OutImportData.Positions[VertexIndexBase], physx::NxRenderDataFormat::FLOAT3, sizeof(FVector), PositionBufferIndex, 0, SubmeshVertexCount))
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s Need a position buffer"), ANSI_TO_TCHAR(ApexClothingAsset.getName()));	
		return false;	// Need a position buffer!
	}

	// Normals
	const PxI32 NormalBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::NORMAL));
	const bool bHaveNormals = VB.getBufferData(&OutImportData.Normals[VertexIndexBase], physx::NxRenderDataFormat::FLOAT3, sizeof(FVector), NormalBufferIndex, 0, SubmeshVertexCount);
	if (!bHaveNormals)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s APEX clothing asset doesn't have Normals data! This can introduce rendering artifact!"), ANSI_TO_TCHAR(ApexClothingAsset.getName()));	
		FMemory::Memset(&OutImportData.Normals[VertexIndexBase], 0, SubmeshVertexCount*sizeof(FVector));	// Fill with zeros
	}

	// Tangents
	const PxI32 TangentBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::TANGENT));
	physx::apex::NxRenderDataFormat::Enum TangentFormatEnum = VBFormat.getBufferFormat(TangentBufferIndex);
	bool bHaveTangents = false;

	if(TangentFormatEnum == NxRenderDataFormat::FLOAT4)
	{
		FVector4 *TangentsArray = (FVector4*)VB.getBufferAndFormat(TangentFormatEnum, TangentBufferIndex);

		if(TangentsArray)
		{
			for(uint32 TangentIdx=0; TangentIdx < SubmeshVertexCount; TangentIdx++)
			{
				OutImportData.Tangents[VertexIndexBase+TangentIdx] = TangentsArray[TangentIdx];
			}

			bHaveTangents = true;
		}
	}
	else
	if(TangentFormatEnum == NxRenderDataFormat::FLOAT3)
	{
		bHaveTangents = VB.getBufferData(&OutImportData.Tangents[VertexIndexBase], physx::NxRenderDataFormat::FLOAT3, sizeof(FVector), TangentBufferIndex, 0, SubmeshVertexCount);
	}

	OutImportData.bHasTangents = bHaveTangents;
	if (!bHaveTangents)
	{
		const FText Text = LOCTEXT("Warning_NoTangents", "This APEX clothing asset doesn't have Tangents data! It could be old format. This can introduce rendering artifact.\n");
		FMessageDialog::Open(EAppMsgType::Ok, Text);

		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s APEX clothing asset doesn't have Tangents data! This can introduce rendering artifact!"), ANSI_TO_TCHAR(ApexClothingAsset.getName()));
		// makes arbitrary vectors perpendicular to normals
		if(bHaveNormals)
		{
			for(uint32 i=0; i<SubmeshVertexCount; i++)
			{
				FVector Perpendicular, Dummy;
				OutImportData.Normals[VertexIndexBase+i].FindBestAxisVectors(Perpendicular, Dummy);
				OutImportData.Tangents[VertexIndexBase+i] = Perpendicular;
			}
		}
		else
		{
			FMemory::Memset(&OutImportData.Tangents[VertexIndexBase], 0, SubmeshVertexCount*sizeof(FVector));	// Fill with zeros
		}
	}

	// Binormals
	const PxI32 BinormalBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::BINORMAL));
	bool bHaveBinormals = VB.getBufferData(&OutImportData.Binormals[VertexIndexBase], physx::NxRenderDataFormat::FLOAT3, sizeof(FVector), BinormalBufferIndex, 0, SubmeshVertexCount);
	if (!bHaveBinormals)
	{
		bHaveBinormals = bHaveNormals && bHaveTangents;
		for (uint32 i = 0; i < SubmeshVertexCount; ++i)
		{
			uint32 Index = VertexIndexBase+i;
			OutImportData.Binormals[Index] 
			= OutImportData.Normals[Index]^OutImportData.Tangents[Index];	// Build from normals and tangents.  If one of these doesn't exist we'll get (0,0,0)'s
		}
	}

	// Colors
	const PxI32 ColorBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::COLOR));
	const bool bHaveColors = VB.getBufferData(&OutImportData.Colors[VertexIndexBase], physx::NxRenderDataFormat::B8G8R8A8, sizeof(FColor), ColorBufferIndex, 0, SubmeshVertexCount);
	OutImportData.bHasVertexColors = bHaveColors;

	if (!bHaveColors)
	{
		FMemory::Memset(&OutImportData.Colors[VertexIndexBase], 0xFF, SubmeshVertexCount*sizeof(FColor));	// Fill with 0xFF
	}

	// UVs
	// Need to consider multiple UV ? 
	// Assuming that this asset has one UV coordinate
	const PxI32 UVBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::TEXCOORD0));
	OutImportData.NumTexCoords = 1;
	if (!VB.getBufferData(&OutImportData.UVs[VertexIndexBase], physx::NxRenderDataFormat::FLOAT2, sizeof(FVector2D), UVBufferIndex, 0, SubmeshVertexCount))
	{
		OutImportData.NumTexCoords = 0;
		FMemory::Memset(&OutImportData.UVs[VertexIndexBase], 0, SubmeshVertexCount*sizeof(FVector2D));	// Fill with zeros
	}

	// Set Bone Indices

	const PxI32 BoneIndexBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::BONE_INDEX));
	physx::apex::NxRenderDataFormat::Enum DataFormatEnum = VBFormat.getBufferFormat(BoneIndexBufferIndex);
	bool bHaveBoneIndices = false;

	if(DataFormatEnum == NxRenderDataFormat::USHORT4)
	{
		// USkeletalMesh is using uint8 for Bone Index but APEX clothing is using uint16 for it
		struct FApexBoneIndices
		{
			uint16 Data[MAX_INFLUENCES_PER_STREAM];
		};

		TArray<FApexBoneIndices> BoneIndices;
		BoneIndices.AddUninitialized(SubmeshVertexCount);

		bHaveBoneIndices = VB.getBufferData(BoneIndices.GetData(), physx::NxRenderDataFormat::USHORT4, sizeof(FApexBoneIndices), BoneIndexBufferIndex, 0, SubmeshVertexCount);
		// Converting uint16 to uint8 because APEX Clothing is using uint16 for Bone Index 
		if (bHaveBoneIndices)
		{
			for(uint32 i=0; i<SubmeshVertexCount; i++)
			{
				for(uint32 j=0; j<MAX_INFLUENCES_PER_STREAM; j++)
				{
					OutImportData.BoneIndices[VertexIndexBase+i].Data[j] = (uint8)BoneIndices[i].Data[j];
				}
			}
		}
	}
	else
	if(DataFormatEnum == NxRenderDataFormat::USHORT1)
	{
		TArray<uint16> BoneIndices;
		BoneIndices.AddUninitialized(SubmeshVertexCount);

		bHaveBoneIndices = VB.getBufferData(BoneIndices.GetData(), physx::NxRenderDataFormat::USHORT1, sizeof(uint16), BoneIndexBufferIndex, 0, SubmeshVertexCount);

		if(bHaveBoneIndices)
		{
			for(uint32 i=0; i<SubmeshVertexCount; i++)
			{
				OutImportData.BoneIndices[VertexIndexBase+i].Set((uint8)BoneIndices[i], 0, 0, 0);
			}
		}
	}

	if (!bHaveBoneIndices)
	{
		FMemory::Memset(&OutImportData.BoneIndices[VertexIndexBase], 0, SubmeshVertexCount*sizeof(FBoneIndices));	// Fill with zeros
	}

	// Bone Weights
	const PxI32 BoneWeightBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(NxRenderVertexSemantic::BONE_WEIGHT));
	DataFormatEnum = VBFormat.getBufferFormat(BoneWeightBufferIndex);
	bool bHaveBoneWeights = false;

	if(DataFormatEnum == NxRenderDataFormat::FLOAT4)
	{
		bHaveBoneWeights = VB.getBufferData(&OutImportData.BoneWeights[VertexIndexBase], physx::NxRenderDataFormat::FLOAT4, sizeof(FVector4), BoneWeightBufferIndex, 0, SubmeshVertexCount);
	}
	else
	if(DataFormatEnum == NxRenderDataFormat::FLOAT1)
	{
		TArray<float> BoneWeights;
		BoneWeights.AddUninitialized(SubmeshVertexCount);

		bHaveBoneWeights = VB.getBufferData(BoneWeights.GetData(), physx::NxRenderDataFormat::FLOAT1, sizeof(float), BoneWeightBufferIndex, 0, SubmeshVertexCount);

		if(bHaveBoneWeights)
		{
			for(uint32 i=0; i<SubmeshVertexCount; i++)
			{
				OutImportData.BoneWeights[VertexIndexBase+i].Set(BoneWeights[i], 0, 0, 0);
			}
		}
	}

	if (!bHaveBoneWeights)
	{
		FMemory::Memset(&OutImportData.BoneWeights[VertexIndexBase], 0, SubmeshVertexCount*sizeof(FVector4));	// Fill with zeros
	}

	return true;
}

void SetBoneIndex(FSoftSkinVertex& OutVertex, FBoneIndices& BoneIndices, FVector4& BoneWeights, TArray<FName>& BoneNames, TArray<FName>& OutUsedBones, TMap<FName,int32>& OutUsedBonesMap)
{
	uint32	TotalInfluenceWeight = 0;

	for( uint32 InfIdx=0;InfIdx<MAX_INFLUENCES_PER_STREAM;InfIdx++ )
	{
		uint8 BoneIndex = BoneIndices.Data[InfIdx];

		if( BoneIndex < 255 && BoneWeights[InfIdx] > 0.f )
		{
			check( BoneIndex < BoneNames.Num() );
			FName BoneName = BoneNames[BoneIndex];
			int32* BoneIdxPtr = OutUsedBonesMap.Find(BoneName);
			int32 BoneIdx;
			if( BoneIdxPtr )
			{
				BoneIdx = *BoneIdxPtr;
			}
			else
			{
				BoneIdx = OutUsedBones.Add(BoneName);
				OutUsedBonesMap.Add(BoneName, BoneIdx);
			}
			OutVertex.InfluenceBones[InfIdx] = BoneIdx;
			OutVertex.InfluenceWeights[InfIdx] = FMath::Clamp<int32>(255.f * BoneWeights[InfIdx], 0, 255);
			TotalInfluenceWeight += OutVertex.InfluenceWeights[InfIdx];
		}
		else
		{
			OutVertex.InfluenceBones[InfIdx] = 0;
			OutVertex.InfluenceWeights[InfIdx] = 0;
		}
	}

	// make the total weight to 1.0 
	if(OutVertex.InfluenceWeights[0] > 0)
	{
		OutVertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
	}

	for( uint32 InfIdx=MAX_INFLUENCES_PER_STREAM;InfIdx<MAX_TOTAL_INFLUENCES;InfIdx++ )
	{
		OutVertex.InfluenceBones[InfIdx] = 0;
		OutVertex.InfluenceWeights[InfIdx] = 0;
	}
}

bool AssociateClothingAssetWithSkeletalMesh(USkeletalMesh* SkelMesh, int32 LODIndex, uint32 SectionIndex, int32 AssetIndex, int32 AssetSubmeshIndex, FVertexImportData& ImportData, TArray<FApexClothPhysToRenderVertData>& RenderToPhysicalMapping, TArray<FVector>& PhysicalMeshVertices,TArray<FVector>& PhysicalMeshNormals, TArray<FName>& BoneNames, TArray<uint32>& IndexBuffer, uint32 TriangleCount)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	if( LODIndex >= ImportedResource->LODModels.Num())
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("Specified LOD Index(%d) is bigger than %d"), LODIndex, ImportedResource->LODModels.Num() );
		return false;
	}

	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	if( (int32)SectionIndex >= LODModel.Sections.Num())
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("Specified Section Index(%d) is bigger than %d"), SectionIndex, LODModel.Sections.Num() );
		return false;
	}

	TArray<FName>		UsedBones;
	TMap<FName,int32>	UsedBonesMap;

	//TempChunk makes it easy to go back to previous status when error occurred
	//to check errors about bone-mapping first
	FSkelMeshChunk		TempChunk; 

	uint32 NumImportedVertices = ImportData.Positions.Num();

	TempChunk.NumRigidVertices = 0;
	TempChunk.RigidVertices.Empty();
	TempChunk.NumSoftVertices = NumImportedVertices;
	TempChunk.SoftVertices.Empty(NumImportedVertices);

	for( uint32 VertexIndex=0;VertexIndex<NumImportedVertices;VertexIndex++ )
	{
		FSoftSkinVertex& OutVertex = TempChunk.SoftVertices[TempChunk.SoftVertices.AddZeroed()];

		OutVertex.Position	= ImportData.Positions[VertexIndex];
		OutVertex.TangentX	= ImportData.Tangents[VertexIndex];
		OutVertex.TangentY	= ImportData.Binormals[VertexIndex];
		OutVertex.TangentZ	= ImportData.Normals[VertexIndex];
		OutVertex.UVs[0]	= ImportData.UVs[VertexIndex];
		OutVertex.Color		= ImportData.Colors[VertexIndex];

		//refresh used bones & map
		SetBoneIndex(OutVertex, 
			ImportData.BoneIndices[VertexIndex],
			ImportData.BoneWeights[VertexIndex],
			BoneNames, 
			UsedBones, 
			UsedBonesMap);
	}

	// Set the bone map
	int32 NumUsedBones = UsedBones.Num();
	TempChunk.BoneMap.Empty(NumUsedBones);

	for( int32 UsedBoneIndex=0; UsedBoneIndex<NumUsedBones; UsedBoneIndex++ )
	{
		const int32 BoneIdx = SkelMesh->RefSkeleton.FindBoneIndex(UsedBones[UsedBoneIndex]);
		if(BoneIdx != INDEX_NONE)
		{
			TempChunk.BoneMap.Add(BoneIdx);
		}
	}

	//if bone count is 0, then crash will occur while drawing
	if(TempChunk.BoneMap.Num() == 0)
	{
		FString FailedBones = "\nMapping failed bones : \n";

		for(int32 i=0; i < UsedBones.Num(); i++)
		{
			FailedBones += FString::Printf(TEXT("\t\t%s\n"),*UsedBones[i].ToString());
		}

		const FText Text = FText::Format(LOCTEXT("Error_NoCorrespondBones", "There are no corresponding bones. At least one bone should exist. This Could be Improper APEX file.\n{0}"), FText::FromString(FailedBones));
		FMessageDialog::Open(EAppMsgType::Ok, Text);

		return false;
	}

	int32 RestoreSectionIndex = -1;
	int32 OriginSectionIndex = SectionIndex;
	// check if this section is clothing section 
	if(LODModel.HasApexClothData(SectionIndex))
	{// reimporting
		RestoreSectionIndex = LODModel.Sections[SectionIndex].CorrespondClothSectionIndex;
		OriginSectionIndex = RestoreSectionIndex;
	}
	else // check if this section is disabled
	if(LODModel.Sections[SectionIndex].bDisabled)
	{// reimporting
		RestoreSectionIndex = SectionIndex;
	}

	//add new one after restoring to original section
	if(RestoreSectionIndex >= 0)
	{
		RestoreOriginalClothingSection(SkelMesh, LODIndex, RestoreSectionIndex);
	}

	SkelMesh->PreEditChange(NULL);

	//add new section & chunk for clothing
	int32 NewSectionIndex = LODModel.Sections.AddZeroed();
	int16 NewChunkIndex = LODModel.Chunks.Add(TempChunk);

	FSkelMeshSection& ClothSection = LODModel.Sections[NewSectionIndex];
	FSkelMeshSection& OriginMeshSection = LODModel.Sections[OriginSectionIndex];

	// copy default info from original section
	ClothSection = OriginMeshSection;

	FSkelMeshChunk& ClothChunk = LODModel.Chunks[NewChunkIndex];
	FSkelMeshChunk& OriginChunk = LODModel.Chunks[OriginMeshSection.ChunkIndex];

	// copy data from original chunk
	ClothChunk.BaseVertexIndex = OriginChunk.BaseVertexIndex;
	ClothChunk.MaxBoneInfluences = OriginChunk.MaxBoneInfluences;

	OriginMeshSection.bDisabled = true;
	// assign newly added chunk
	ClothSection.ChunkIndex = NewChunkIndex;
	ClothSection.bDisabled = false;

	ClothSection.CorrespondClothSectionIndex = OriginSectionIndex;
	OriginMeshSection.CorrespondClothSectionIndex = NewSectionIndex;

	ClothChunk.ApexClothMappingData = RenderToPhysicalMapping;
	ClothChunk.PhysicalMeshVertices = PhysicalMeshVertices;
	ClothChunk.PhysicalMeshNormals = PhysicalMeshNormals;

	//adjust index buffer
	TArray<uint32> OutIndexBuffer;
	LODModel.MultiSizeIndexContainer.GetIndexBuffer(OutIndexBuffer);

	ClothSection.BaseIndex = OutIndexBuffer.Num();
	ClothSection.NumTriangles = TriangleCount;

	uint32 NumVertices = 0;
	for(int32 SectionIdx = 0; SectionIdx < NewSectionIndex; SectionIdx++)
	{
		uint16 ChunkIdx = LODModel.Sections[SectionIdx].ChunkIndex;
		FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIdx]; 	
		NumVertices += Chunk.GetNumVertices();
	}

	ClothChunk.BaseVertexIndex = NumVertices;

	// if Section Index is greater than 0, Vertex Index has to be adjusted
	if(NewSectionIndex > 0)
	{
		for(int32 Index=0; Index<IndexBuffer.Num(); Index++)
			IndexBuffer[Index] += ClothChunk.BaseVertexIndex;
	}

	OutIndexBuffer += IndexBuffer;

	LODModel.MultiSizeIndexContainer.CopyIndexBuffer(OutIndexBuffer);

	LODModel.NumVertices += ClothChunk.GetNumVertices();

	// set asset submesh info to new chunk 
	ClothChunk.SetClothSubmeshIndex(AssetIndex, AssetSubmeshIndex);

	// New Chunk is already added so needs to update VertexFactories before Render codes are called
	ReregisterSkelMeshComponents(SkelMesh);

	// PostEditChange includes RefreshSkelMeshComponents, RenderState will be recreated 
	SkelMesh->PostEditChange();

	return true;
}

void FlipUV(::NxParameterized::Interface *ApexRenderMeshAssetAuthoring,bool bFlipU,bool bFlipV)
{
	if ( !bFlipU && !bFlipV ) 
	{
		return;
	}

	::NxParameterized::Handle Handle(*ApexRenderMeshAssetAuthoring);
	::NxParameterized::Interface *Param = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,"submeshes",Handle);
	if ( Param )
	{
		int NumSubmeshes=0;
		if ( Handle.getArraySize(NumSubmeshes,0) == ::NxParameterized::ERROR_NONE )
		{
			// iterate each submesh
			for (int32 i = 0; i < NumSubmeshes; i++)
			{
				char Scratch[MAX_SPRINTF];

				FCStringAnsi::Sprintf( Scratch, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats", i );
				Param = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,Scratch,Handle);
				physx::PxI32 BufArraySize = 0;
				::NxParameterized::ErrorType BufArrayErrorType = Handle.getArraySize(BufArraySize, 0);
				PX_ASSERT(BufArrayErrorType ==::NxParameterized::ERROR_NONE);

				// iterate each vertex buffer of this submesh
				for (physx::PxI32 j = 0; j < BufArraySize; j++)
				{
					FCStringAnsi::Sprintf( Scratch, "submeshes[%d].vertexBuffer.vertexFormat.bufferFormats[%d].semantic", i, j );
					Param = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring, Scratch, Handle);

					if (Param)
					{
						physx::PxI32 bufSemantic;
						::NxParameterized::ErrorType BufSemanticErrorType = Handle.getParamI32(bufSemantic);
						PX_ASSERT(BufSemanticErrorType ==::NxParameterized::ERROR_NONE);

						// this vertex buffer is a texCoords one
						if ( bufSemantic <= physx::NxRenderVertexSemantic::TEXCOORD3
							&& bufSemantic >= physx::NxRenderVertexSemantic::TEXCOORD0 )
						{
							// retrieve the data handle, It is not an F32x2 array it is an struct array
							FCStringAnsi::Sprintf( Scratch, "submeshes[%d].vertexBuffer.buffers[%d].data", i, j );
							Param = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring, Scratch, Handle);
							physx::PxI32 DataElementSize = 0;

							if (Handle.getArraySize(DataElementSize, 0) ==::NxParameterized::ERROR_NONE)
							{
								float MaxTexelU = -1e9;
								float MaxTexelV = -1e9;

								for (int32 k = 0; k < DataElementSize; k++)
								{
									Handle.set(k); // push handle to select array element k

									float Texel[2];
									Handle.set(0); // read element 0
									Handle.getParamF32(Texel[0]);
									Handle.popIndex();
									Handle.set(1); // read element 1
									Handle.getParamF32(Texel[1]);
									Handle.popIndex();

									MaxTexelU = FMath::Max(MaxTexelU, Texel[0] - 0.0001f);
									MaxTexelV = FMath::Max(MaxTexelV, Texel[1] - 0.0001f);
									Handle.popIndex(); // pop handle back to array
								}

								MaxTexelU = ::floor(MaxTexelU) + 1.0f;
								MaxTexelV = ::floor(MaxTexelV) + 1.0f;

								for (int32 k = 0; k < DataElementSize; k++)
								{
									Handle.set(k); // push handle to select array element k

									float Texel;
									Handle.set(1); // read element 1
									Handle.getParamF32(Texel);
									Handle.popIndex();

									if (bFlipU)
									{
										Handle.set(0); // read element 0
										Handle.getParamF32(Texel);
										Handle.setParamF32(MaxTexelU - Texel);
										Handle.popIndex();
									}

									if (bFlipV)
									{
										Handle.set(1); // read element 1
										Handle.getParamF32(Texel);
										Handle.setParamF32(MaxTexelV - Texel);
										Handle.popIndex();
									}

									Handle.popIndex(); // pop handle back to array
								}
							}
							else
							{
								PX_ASSERT(0);
							}
						}
					}
				}
			}
		}
	}
}

physx::apex::NxApexAssetAuthoring* ApexAuthoringFromAsset(NxApexAsset* Asset, NxApexSDK* ApexSDK)
{
	const ::NxParameterized::Interface* OldInterface = Asset->getAssetNxParameterized();

	// make the copy
	::NxParameterized::Interface* NewInterface = ApexSDK->getParameterizedTraits()->createNxParameterized(OldInterface->className());
	NewInterface->copy(*OldInterface);

	// pass NULL to use an auto-generated name
	return ApexSDK->createAssetAuthoring(NewInterface, NULL);
}

NxClothingAsset* ApplyTransform(NxClothingAsset* ApexClothingAsset)
{	
	// create an NxClothingAssetAuthoring instance of the asset
	NxClothingAssetAuthoring *ApexClothingAssetAuthoring 
		= static_cast<NxClothingAssetAuthoring*>(ApexAuthoringFromAsset(ApexClothingAsset, GApexSDK));

	if(ensure(ApexClothingAssetAuthoring))
	{
		const NxParameterized::Interface* AssetParams = ApexClothingAsset->getAssetNxParameterized();

		// load bone actors size and bone sphere size
		physx::PxI32 NumBoneActors, NumBoneSpheres;
		verify(NxParameterized::getParamArraySize(*AssetParams, "boneActors", NumBoneActors));
		verify(NxParameterized::getParamArraySize(*AssetParams, "boneSpheres", NumBoneSpheres));

		// clear bone spheres and connections if this asset have both bone actors and bone spheres because APEX Clothing doesn't support both
		if (NumBoneActors > 0 && NumBoneSpheres > 0)
		{
			ApexClothingAssetAuthoring->clearCollision();
		}

		// Invert Y-axis
		physx::PxMat44 InvertYTransform = physx::PxMat44::createIdentity();
		InvertYTransform.column1.y = -1;
		physx::PxMat44 MeshTransform;

		// refer to gravity direction to find up-axis
		physx::PxVec3 GravityDirection;
		verify(NxParameterized::getParamVec3(*AssetParams, "simulation.gravityDirection", GravityDirection));

		// check whether up-axis is Y-up or not
		// if Y-up, then change up-axis into Z-up
		if(GravityDirection.z == 0.0f && FMath::Abs(GravityDirection.y) > 0.0f )
		{
			physx::PxVec3 NewGravityDirection(GravityDirection.x, GravityDirection.z, GravityDirection.y);
			ApexClothingAssetAuthoring->setSimulationGravityDirection(NewGravityDirection);
			
			// inverse Y and rotate 90 degrees with respect to x-axis
			// InvertYTransform * Rotated 90 Degrees With X-Axis Matrix
			MeshTransform = physx::PxMat44::createIdentity();
			MeshTransform.column1.y = 0.0f;
			MeshTransform.column1.z = 1.0f;
			MeshTransform.column2.y = 1.0f;
			MeshTransform.column2.z = 0.0f;
		}
		else
		{
			MeshTransform = InvertYTransform;
		}

		ApexClothingAssetAuthoring->applyTransformation(MeshTransform, 1.0f, true, true);

		TArray<PxMat44> NewBindPoses;
		
		for (uint32 i=0; i<ApexClothingAsset->getNumUsedBones(); i++)
		{
			physx::PxMat44 BindPose;
			ApexClothingAssetAuthoring->getBoneBindPose(i,BindPose);
			physx::PxMat44 NewBindPose = BindPose * InvertYTransform;
			NewBindPoses.Add(NewBindPose);
		}

		ApexClothingAssetAuthoring->updateBindPoses(NewBindPoses.GetData(), NewBindPoses.Num(), true, true);

		int32 NumLODs = ApexClothingAssetAuthoring->getNumLods();
		// destroy and create a new asset based off the authoring version	
		for (int32 i = 0; i<NumLODs; i++)
		{
			NxParameterized::Interface *ApexRenderMeshAssetAuthoring = ApexClothingAssetAuthoring->getRenderMeshAssetAuthoring(i);
			if ( ApexRenderMeshAssetAuthoring )
			{
				NxParameterized::Handle AuthorHandle(ApexRenderMeshAssetAuthoring);
				bool bFlipU = false;
				bool bFlipV = false;
				NxParameterized::Interface *AuthorParam = NxParameterized::findParam(*ApexRenderMeshAssetAuthoring,"textureUVOrigin",AuthorHandle);
				if ( AuthorParam )
				{
					uint32 TextureUVOrigin;
					AuthorHandle.getParamU32(TextureUVOrigin);
					switch ( TextureUVOrigin )
					{
					case NxTextureUVOrigin::ORIGIN_TOP_LEFT:
						bFlipU = false;
						bFlipV = false;
						break;
					case NxTextureUVOrigin::ORIGIN_TOP_RIGHT:	
						bFlipU = true;
						bFlipV = false;
						break;
					case NxTextureUVOrigin::ORIGIN_BOTTOM_LEFT:
						bFlipU = false;
						bFlipV = true;
						break;
					case NxTextureUVOrigin::ORIGIN_BOTTOM_RIGHT:
						bFlipU = false;
						bFlipV = false;
						break;
					}
					AuthorHandle.setParamU32(NxTextureUVOrigin::ORIGIN_TOP_LEFT);
				}
				FlipUV(ApexRenderMeshAssetAuthoring,bFlipU,bFlipV);
			}
			else
			{
				break;
			}
		}

		// cache off asset name
		char ApexAssetName[1024] = {0};

		FCStringAnsi::Strncpy(ApexAssetName, ApexClothingAsset->getName(), sizeof(ApexAssetName) );

		// destroy old asset
		ApexClothingAsset->release();
		ApexClothingAsset = NULL;

		// create new asset from the authoring
		NxClothingAsset* NewAsset = (NxClothingAsset*)GApexSDK->createAsset( *ApexClothingAssetAuthoring, ApexAssetName );
		
		// ApexSDK returns NULL when asset name collides with another name
		if(!NewAsset)
		{
			// if name is NULL, ApexSDK will auto-generate one that won't collide with other names
			NewAsset = (NxClothingAsset*)GApexSDK->createAsset( *ApexClothingAssetAuthoring, NULL );
		}

		check(NewAsset);

		// release authoring
		ApexClothingAssetAuthoring->release();

		return NewAsset;
	}

	return ApexClothingAsset;
}

void RestoreAllClothingSections(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 AssetIndex)
{
	int32 NumAssets = SkelMesh->ClothingAssets.Num();

	check((int32)AssetIndex < NumAssets);

	TArray<uint32> SectionIndices;
	SkelMesh->GetOriginSectionIndicesWithCloth(LODIndex, AssetIndex, SectionIndices);

	for(int32 i=0; i < SectionIndices.Num(); i++)
	{
		RestoreOriginalClothingSection(SkelMesh, LODIndex, SectionIndices[i]);
	}
}

void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bRecreateSkelMeshComponent)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	int32 NumLODs = ImportedResource->LODModels.Num();

	for(int32 LODIdx=0; LODIdx < NumLODs; LODIdx++)
	{
		RestoreAllClothingSections(SkelMesh, LODIdx, AssetIndex);

		FStaticLODModel& LODModel = ImportedResource->LODModels[LODIdx];

		int32 NumChunks = LODModel.Chunks.Num();

		//decrease asset index because one asset is removed
		for(int32 ChunkIdx=0; ChunkIdx < NumChunks; ChunkIdx++)
		{
			if(LODModel.Chunks[ChunkIdx].CorrespondClothAssetIndex > (int16)AssetIndex)
				LODModel.Chunks[ChunkIdx].CorrespondClothAssetIndex--;
		}
	}

	// release and make it invalid
	GPhysCommandHandler->DeferredRelease(SkelMesh->ClothingAssets[AssetIndex].ApexClothingAsset);
	SkelMesh->ClothingAssets[AssetIndex].ApexClothingAsset = NULL;

	//this requires to refresh UI layout
	SkelMesh->ClothingAssets.RemoveAt(AssetIndex);

	if(bRecreateSkelMeshComponent)
	{
		// Refresh skeletal mesh components
		RefreshSkelMeshComponents(SkelMesh);
	}
}

void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	uint16 ChunkIndex = Section.ChunkIndex;

	int32 OriginSectionIndex = -1, ClothSectionIndex = -1;

	// if this section is original mesh section ( without clothing data )
	if(Section.CorrespondClothSectionIndex >= 0
		&& !LODModel.Chunks[ChunkIndex].HasApexClothData())
	{
		ClothSectionIndex = Section.CorrespondClothSectionIndex;
		OriginSectionIndex = SectionIndex;
	}

	// if this section is clothing section
	if(Section.CorrespondClothSectionIndex >= 0
		&&LODModel.Chunks[ChunkIndex].HasApexClothData())
	{
		ClothSectionIndex = SectionIndex;
		OriginSectionIndex = Section.CorrespondClothSectionIndex;
	}

	if(OriginSectionIndex >= 0 && ClothSectionIndex >= 0)
	{
		// Apply to skeletal mesh
		SkelMesh->PreEditChange(NULL);

		FSkelMeshSection& ClothSection = LODModel.Sections[ClothSectionIndex];
		FSkelMeshSection& OriginSection = LODModel.Sections[OriginSectionIndex];

		// remove cloth section & chunk
		TArray<uint32> OutIndexBuffer;
		LODModel.MultiSizeIndexContainer.GetIndexBuffer(OutIndexBuffer);

		int16 RemovedChunkIndex = ClothSection.ChunkIndex;
		uint32 RemovedBaseIndex = ClothSection.BaseIndex;
		uint32 RemovedNumIndices = ClothSection.NumTriangles * 3;

		FSkelMeshChunk& ClothChunk = LODModel.Chunks[ClothSection.ChunkIndex];

		uint32 RemovedBaseVertexIndex = ClothChunk.BaseVertexIndex;

		int32 NumIndexBuffer = OutIndexBuffer.Num();

		int32 NumRemovedVertices = ClothChunk.GetNumVertices();

		// Remove old indices and update any indices for other sections
		OutIndexBuffer.RemoveAt(RemovedBaseIndex, RemovedNumIndices);

		NumIndexBuffer = OutIndexBuffer.Num();

		for(int32 i=0; i < NumIndexBuffer; i++)
		{
			if(OutIndexBuffer[i] >= ClothChunk.BaseVertexIndex)
				OutIndexBuffer[i] -= NumRemovedVertices;
		}

		LODModel.MultiSizeIndexContainer.CopyIndexBuffer(OutIndexBuffer);

		FClothingAssetData& CorrespondAssetData = SkelMesh->ClothingAssets[ClothChunk.CorrespondClothAssetIndex];

		LODModel.Chunks.RemoveAt(ClothSection.ChunkIndex);
		LODModel.Sections.RemoveAt(OriginSection.CorrespondClothSectionIndex);

		LODModel.NumVertices -= NumRemovedVertices;

		int32 NumSections = LODModel.Sections.Num();
		//decrease indices
		for(int32 i=0; i < NumSections; i++)
		{
			if(LODModel.Sections[i].CorrespondClothSectionIndex > OriginSection.CorrespondClothSectionIndex)
				LODModel.Sections[i].CorrespondClothSectionIndex -= 1;

			if(LODModel.Sections[i].ChunkIndex > RemovedChunkIndex)
				LODModel.Sections[i].ChunkIndex -= 1;

			if(LODModel.Sections[i].BaseIndex > RemovedBaseIndex)
				LODModel.Sections[i].BaseIndex -= RemovedNumIndices;

			int16 NewChunkIndex = LODModel.Sections[i].ChunkIndex;

			if(LODModel.Chunks[NewChunkIndex].BaseVertexIndex > RemovedBaseVertexIndex)
				LODModel.Chunks[NewChunkIndex].BaseVertexIndex -= NumRemovedVertices;

		}

		OriginSection.bDisabled = false;
		OriginSection.CorrespondClothSectionIndex = -1;

		// Cloth chunk is removed so updates Vertex Factories
		ReregisterSkelMeshComponents(SkelMesh);
		// Reinitialize the render data
		SkelMesh->PostEditChange();
	}
	else
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("No exists proper section : %d "), SectionIndex );	
	}
}

void RestoreOriginalClothingSectionAllLODs(USkeletalMesh* SkelMesh, uint32 LOD0_SectionIndex)
{
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	int32 NumLOD = SkelMesh->LODInfo.Num();
	check(NumLOD == ImportedResource->LODModels.Num());

	for(int32 LODIndex=0; LODIndex < NumLOD; LODIndex++)
	{
		uint32 NumNonClothSection = ImportedResource->LODModels[LODIndex].NumNonClothingSections();
		if(LOD0_SectionIndex < NumNonClothSection)
		{
			int32 SecIdx = LOD0_SectionIndex;
	#if USE_MATERIAL_MAP
			if(SkelMesh->LODInfo[LODIndex].LODMaterialMap.Num() > 0)
			{
				int32 MaterialIndex = ImportedResource->LODModels[0].Sections[LOD0_SectionIndex].MaterialIndex;
				int32 NewMatIndex = SkelMesh->LODInfo[LODIndex].LODMaterialMap[MaterialIndex];
				SecIdx = FindSectionByMaterialIndex(SkelMesh, LODIndex, NewMatIndex);
				if(SecIdx < 0)
				{ 
					continue;
				}
			}
	#endif// #if USE_MATERIAL_MAP
			RestoreOriginalClothingSection(SkelMesh, LODIndex, SecIdx);
		}
	}
}

bool ImportClothingSectionFromClothingAsset( USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, int32 AssetIndex, int32 AssetSubmeshIndex)
{
	FSkeletalMeshResource* ImportedResource = SkelMesh->GetImportedResource();
	check(LODIndex < (uint32)ImportedResource->LODModels.Num());
	check(SectionIndex < (uint32)ImportedResource->LODModels[LODIndex].NumNonClothingSections());
	check( AssetIndex < SkelMesh->ClothingAssets.Num() );

	FClothingAssetData& AssetData = SkelMesh->ClothingAssets[AssetIndex];

	NxClothingAsset* ApexClothingAsset = AssetData.ApexClothingAsset;

	// The APEX Clothing Asset contains an APEX Render Mesh Asset, get a pointer to this
	physx::PxU32 NumLODLevels = ApexClothingAsset->getNumGraphicalLodLevels();

	FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

	if(LODIndex >= (int32)NumLODLevels)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("LOD index %d is bigger than LOD Levels %d "), LODIndex, NumLODLevels );			
		return false;
	}

	const NxRenderMeshAsset* ApexRenderMesh = ApexClothingAsset->getRenderMeshAsset(LODIndex);

	if (ApexRenderMesh == NULL)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("getRenderMeshAsset from %s is failed "), ApexClothingAsset->getName());			
		return false;
	}

	// Number of submeshes (aka "elements" in Unreal)
	const physx::PxU32 SubmeshCount = ApexRenderMesh->getSubmeshCount();
	if (AssetSubmeshIndex >= (int32)SubmeshCount)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("requested submesh %d is greater than num (%d)"), AssetSubmeshIndex, SubmeshCount );			
		return false;
	}

	TArray<uint32> IndexBuffer;

	TArray<FApexClothPhysToRenderVertData> RenderToPhysicalMapping;
	TArray<FVector> PhysicalMeshVertices;
	TArray<FVector> PhysicalMeshNormals;
	FVertexImportData ImportData;

	// get values from LoadGraphicalMeshFromClothingAsset
	uint32 SubmeshVertexCount = 0;
	uint32 SubmeshTriangleCount = 0;
	uint32 StartSimulIndex = 0; 
	uint32 NumSimulVertices = 0;

	if(!LoadGraphicalMeshFromClothingAsset(*ApexClothingAsset, *ApexRenderMesh, ImportData, IndexBuffer, LODIndex, AssetSubmeshIndex, SubmeshVertexCount, SubmeshTriangleCount, StartSimulIndex, NumSimulVertices))
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("LoadGraphicalMeshFromClothingAsset is failed "));			
		return false;
	}

	LoadPhysicalMeshFromClothingAsset(*ApexClothingAsset, RenderToPhysicalMapping, PhysicalMeshVertices, PhysicalMeshNormals, LODIndex, StartSimulIndex, NumSimulVertices, SubmeshVertexCount);

	// Load bone names
	TArray<FName> BoneNames;
	LoadBonesFromClothingAsset(*ApexClothingAsset, BoneNames);

	return AssociateClothingAssetWithSkeletalMesh(SkelMesh, LODIndex, SectionIndex, 
											AssetIndex, AssetSubmeshIndex, ImportData, 		
											RenderToPhysicalMapping, PhysicalMeshVertices, PhysicalMeshNormals, 
											BoneNames, IndexBuffer, SubmeshTriangleCount);
}

void ReImportClothingSectionsFromClothingAsset(USkeletalMesh* SkelMesh)
{
	check(SkelMesh);

	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();

	int32 NumLODs = ImportedResource->LODModels.Num();

	for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
	{
		int32 NumSections = ImportedResource->LODModels[LODIndex].NumNonClothingSections();

		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			ReImportClothingSectionFromClothingAsset(SkelMesh, LODIndex, SectionIndex);
		}
	}
}

void ReImportClothingSectionFromClothingAsset(USkeletalMesh* SkelMesh, int32 LODIndex, uint32 SectionIndex)
{
	check(SkelMesh);

	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	FStaticLODModel& Model = ImportedResource->LODModels[LODIndex];

	check(Model.Sections.IsValidIndex(SectionIndex));

	int16 ClothSecIdx = Model.Sections[SectionIndex].CorrespondClothSectionIndex;
	if(ClothSecIdx < 0)
	{
		return;
	}

	int16 ClothChunkIdx = Model.Sections[ClothSecIdx].ChunkIndex;
	int16 AssetIndex = Model.Chunks[ClothChunkIdx].CorrespondClothAssetIndex;
	int16 SubmeshIdx = Model.Chunks[ClothChunkIdx].ClothAssetSubmeshIndex;

	ImportClothingSectionFromClothingAsset(SkelMesh, LODIndex, SectionIndex, AssetIndex, SubmeshIdx);
}

static bool IsProperApexFile(NxClothingAsset& ApexClothingAsset, USkeletalMesh* SkelMesh)
{
	TArray<FName> BoneNames;
	LoadBonesFromClothingAsset(ApexClothingAsset, BoneNames);

	FString FailedBonesStr = "\n";
	int32 FaildBoneCount = 0;

	for(int32 BoneIndex=0; BoneIndex < BoneNames.Num(); ++BoneIndex)
	{
		const int32 BoneIdx = SkelMesh->RefSkeleton.FindBoneIndex(BoneNames[BoneIndex]);
		if( BoneIdx == INDEX_NONE )
		{
			FaildBoneCount++;
			FailedBonesStr += FString::Printf(TEXT("  %s\n"),*BoneNames[BoneIndex].ToString() );
		}
	}

	if(FaildBoneCount > 0)
	{
		//warning 
		const FText Text = FText::Format(LOCTEXT("Warning_NotMatchBones", "This file references bones which do not exist in the Skeletal Mesh: \n{0}\n Proceed?"), FText::FromString(FailedBonesStr));
		if( EAppReturnType::Ok != FMessageDialog::Open(EAppMsgType::OkCancel, Text) )
		{
			return false;
		}
	}

	return true;
}

EClothUtilRetType ImportApexAssetFromApexFile(FString& ApexFile, USkeletalMesh* SkelMesh, int32 AssetIndex)
{
	NxClothingAsset* ApexClothingAsset = CreateApexClothingAssetFromFile(ApexFile);

	if (ApexClothingAsset == NULL)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("Creating a cloth from %s is Failed"), *ApexFile );			
		return CURT_Fail;
	}

	bool bReimport = (AssetIndex >= 0);

	//when re-importing, just skip this warning dialog
	if(!bReimport && !IsProperApexFile(*ApexClothingAsset, SkelMesh))
	{
		ApexClothingAsset->release();
		return CURT_Cancel;
	}

	ApexClothingAsset = ApplyTransform(ApexClothingAsset);

	FName AssetName = FName(*FPaths::GetCleanFilename(ApexFile));
	int32 NumAssets = SkelMesh->ClothingAssets.Num();

	if(!bReimport)
	{
		// new asset
		AssetIndex = SkelMesh->ClothingAssets.Num();
		FClothingAssetData* Data = new(SkelMesh->ClothingAssets) FClothingAssetData;
		Data->ApexFileName = ApexFile;
		Data->AssetName = AssetName;
		Data->bClothPropertiesChanged = false;
		Data->ApexClothingAsset = ApexClothingAsset;
	}
	else
	{
		// re-import
		FClothingAssetData& AssetData = SkelMesh->ClothingAssets[AssetIndex];
		if (AssetData.ApexClothingAsset)
		{
			GPhysCommandHandler->DeferredRelease(AssetData.ApexClothingAsset);
			AssetData.ApexClothingAsset = NULL;
		}
		AssetData.ApexClothingAsset = ApexClothingAsset;
		AssetData.ApexFileName = ApexFile;

		FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
		int32 NumLODs = ImportedResource->LODModels.Num();

		for(int32 LODIndex=0; LODIndex < NumLODs; LODIndex++)
		{
			TArray<uint32> SectionIndices;
			SkelMesh->GetOriginSectionIndicesWithCloth(LODIndex, AssetIndex, SectionIndices);

			FStaticLODModel& LODModel = ImportedResource->LODModels[LODIndex];

			for(int32 i=0; i < SectionIndices.Num(); i++)
			{
				FSkelMeshSection& Section = LODModel.Sections[SectionIndices[i]];

				int16 ClothSecIdx = Section.CorrespondClothSectionIndex;
				int16 ClothChunkIdx = LODModel.Sections[ClothSecIdx].ChunkIndex;
				int16 SubmeshIdx = LODModel.Chunks[ClothChunkIdx].ClothAssetSubmeshIndex;

				int32 SecIdx = SectionIndices[i];

				int32 NumNonClothSection = LODModel.NumNonClothingSections();
				if(SecIdx < NumNonClothSection)
				{
					if(!ImportClothingSectionFromClothingAsset(SkelMesh, LODIndex, SecIdx, AssetIndex, SubmeshIdx))
					{
						// if import is failed, restores original section because sub-mesh might be removed
						RestoreOriginalClothingSection(SkelMesh, LODIndex, SecIdx);
					}
				}
			}
		}
	}

	SkelMesh->LoadClothCollisionVolumes(AssetIndex, ApexClothingAsset);

	return CURT_Ok;
}

bool GetSubmeshInfoFromApexAsset(NxClothingAsset *ApexClothingAsset, uint32 LODIndex, TArray<FSubmeshInfo>& OutSubmeshInfos)
{
	if( ApexClothingAsset == NULL)
	{
		return false;
	}

	// The APEX Clothing Asset contains an APEX Render Mesh Asset, get a pointer to this
	physx::PxU32 NumLODLevels = ApexClothingAsset->getNumGraphicalLodLevels();

	if(LODIndex >= (int32)NumLODLevels)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("LOD index %d is bigger than LOD Levels %d "), LODIndex, NumLODLevels );			
		return false;
	}

	const NxRenderMeshAsset* ApexRenderMesh = ApexClothingAsset->getRenderMeshAsset(LODIndex);

	if (ApexRenderMesh == NULL)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("getRenderMeshAsset from %s is failed "), ApexClothingAsset->getName());			
		return false;
	}

	uint32 SubmeshCount = ApexRenderMesh->getSubmeshCount();

	if(SubmeshCount == 0)
	{
		UE_LOG(LogApexClothingUtils, Warning,TEXT("%s dosen't have submeshes"), ANSI_TO_TCHAR(ApexClothingAsset->getName()) );	
		return false;
	}

	OutSubmeshInfos.Empty();

	const NxParameterized::Interface* AssetParams = ApexClothingAsset->getAssetNxParameterized();
	char ParameterName[MAX_SPRINTF];

	// Extracts simulation mesh info
	int32 PhysicalMeshCount;
	NxParameterized::getParamArraySize(*AssetParams, "physicalMeshes", PhysicalMeshCount);

	check( PhysicalMeshCount > (int32)LODIndex );

	FCStringAnsi::Sprintf(ParameterName, "physicalMeshes[%d]", LODIndex);

	NxParameterized::Interface* PhysicalMeshParams;
	uint32 NumVertices = 0;
	// num of physical mesh vertices but including not only dynamically simulated vertices but also fixed vertices
	int32 NumPhysMeshVerts = 0;
	// num of actual simulation vertices dynamically moving
	int32 NumRealDynamicSimulVerts = 0;

	if (NxParameterized::getParamRef(*AssetParams, ParameterName, PhysicalMeshParams))
	{
		if(PhysicalMeshParams != NULL)
		{
			verify(NxParameterized::getParamU32(*PhysicalMeshParams, "physicalMesh.numVertices", NumVertices));
			verify(NxParameterized::getParamArraySize(*PhysicalMeshParams, "physicalMesh.vertices", NumPhysMeshVerts));

			// Extracts num of real dynamically simulated vertices because NumPhysMeshVerts includes fixed verts as well.
			// "submeshes" in PhysicalMesh have dynmaically moving simulation vertices
			int32 NumPhysMeshSubmeshes;
			verify(NxParameterized::getParamArraySize(*PhysicalMeshParams, "submeshes", NumPhysMeshSubmeshes));

			for(int32 Submesh=0; Submesh<NumPhysMeshSubmeshes; Submesh++)
			{
				FCStringAnsi::Sprintf(ParameterName, "submeshes[%d].numVertices", Submesh);
				uint32 NumSubmeshVerts;
				verify(NxParameterized::getParamU32(*PhysicalMeshParams, ParameterName, NumSubmeshVerts));
				NumRealDynamicSimulVerts += NumSubmeshVerts;
			}
		}
	}

	// Extracts rendering mesh info
	FCStringAnsi::Sprintf(ParameterName, "graphicalLods[%d]", LODIndex);

	NxParameterized::Interface* GraphicalMeshParams;
	int32 PhysicalSubmeshIndex = -1;

	if (NxParameterized::getParamRef(*AssetParams, ParameterName, GraphicalMeshParams))
	{
		if(GraphicalMeshParams != NULL)
		{
			for(uint32 SubmeshIndex=0; SubmeshIndex < SubmeshCount; SubmeshIndex++)
			{
				uint32 NumRenderVertsAffectedBySimul = 0, NumSimulVertAdditional = 0;

				FCStringAnsi::Sprintf(ParameterName, "physicsSubmeshPartitioning[%d].numSimulatedVertices", SubmeshIndex);
				NxParameterized::getParamU32(*GraphicalMeshParams, ParameterName, NumRenderVertsAffectedBySimul);

				if(NumRenderVertsAffectedBySimul == 0)
				{
					continue;
				}

				const NxRenderSubmesh& Submesh = ApexRenderMesh->getSubmesh(SubmeshIndex);

				const NxVertexBuffer& VB = Submesh.getVertexBuffer();

				// Count vertices
				uint32 VBCount = VB.getVertexCount();

				// Count triangles
				uint32 IndexCount = 0;
				for (uint32 PartIndex = 0; PartIndex < ApexRenderMesh->getPartCount(); ++PartIndex)
				{
					IndexCount += Submesh.getIndexCount(PartIndex);
				}

				FSubmeshInfo* SubmeshInfo = new(OutSubmeshInfos) FSubmeshInfo;
				SubmeshInfo->VertexCount = VBCount;
				SubmeshInfo->TriangleCount = IndexCount/3;
				SubmeshInfo->SubmeshIndex = SubmeshIndex;
				SubmeshInfo->SimulVertexCount = NumRealDynamicSimulVerts;
				SubmeshInfo->FixedVertexCount = (VBCount - NumRenderVertsAffectedBySimul); 
			}
		}
	}

	return true;	
}

void BackupClothingDataFromSkeletalMesh(USkeletalMesh* SkelMesh)
{
	BackupClothingDataFromSkeletalMesh(SkelMesh, GClothingBackup);
}

void ReapplyClothingDataToSkeletalMesh(USkeletalMesh* SkelMesh)
{
	ReapplyClothingDataToSkeletalMesh(SkelMesh, GClothingBackup);

	GClothingBackup.Clear();
}

void BackupClothingDataFromSkeletalMesh(USkeletalMesh* SkelMesh, FClothingBackup& ClothingBackup)
{
	check(SkelMesh);

	// back up clothing assets 
	int32 NumAssets = SkelMesh->ClothingAssets.Num();

	if(NumAssets == 0)
	{
		return;
	}

	ClothingBackup.ClothingAssets = SkelMesh->ClothingAssets;
	SkelMesh->ClothingAssets.Empty();

	// back up sections & chunks
	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	FStaticLODModel& LODModel = ImportedResource->LODModels[0];

	int32 NumSections = LODModel.Sections.Num();
	int32 NumChunks = LODModel.Chunks.Num();

	int32 NumClothingSections = 0;
	int32 NumIndexBuffer = 0;
	int32 NumBoneIndices = 0;
	// count clothing sections
	for(int32 i=0; i < NumSections; i++)
	{
		if(LODModel.HasApexClothData(i))
		{
			FSkelMeshSection& Section = LODModel.Sections[i];
			NumClothingSections++;
			NumIndexBuffer += (Section.NumTriangles * 3);
			NumBoneIndices += LODModel.Chunks[Section.ChunkIndex].BoneMap.Num();
		}
	}

	if(NumClothingSections > 0)
	{
		ClothingBackup.Sections.Empty(NumClothingSections);
		ClothingBackup.Chunks.Empty(NumClothingSections);
		ClothingBackup.IndexBuffer.Empty(NumIndexBuffer);

		TArray<uint32> OutIndexBuffer;
		LODModel.MultiSizeIndexContainer.GetIndexBuffer(OutIndexBuffer);

		for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
		{
			if(LODModel.HasApexClothData(SecIdx))
			{
				FSkelMeshSection& Section = LODModel.Sections[SecIdx];
				int32 BackupSectionIndex = ClothingBackup.Sections.Add(Section);
				FSkelMeshChunk& Chunk = LODModel.Chunks[Section.ChunkIndex];
				int32 BackupChunkIndex = ClothingBackup.Chunks.Add(Chunk);
				// back up index buffer
				FSkelMeshSection& NewSection = ClothingBackup.Sections[BackupSectionIndex];
				FSkelMeshChunk& NewChunk = ClothingBackup.Chunks[BackupChunkIndex];

				NewSection.ChunkIndex = BackupChunkIndex;
				int32 BaseIndex = NewSection.BaseIndex;
				NewSection.BaseIndex = ClothingBackup.IndexBuffer.Num();
				int32 NumIndices = NewSection.NumTriangles * 3;
				int32 EndIndex = BaseIndex + NumIndices;

				int32 BaseVertexIndex = NewChunk.BaseVertexIndex;

				for(int32 Idx=BaseIndex; Idx < EndIndex; Idx++)
				{
					ClothingBackup.IndexBuffer.Add(OutIndexBuffer[Idx] - BaseVertexIndex);
				}	
			}
		}
	}

	ClothingBackup.bBackedUp = true;

}

void ReapplyClothingDataToSkeletalMesh( USkeletalMesh* SkelMesh, FClothingBackup& ClothingBackup)
{
	if(!SkelMesh
	|| ClothingBackup.ClothingAssets.Num() == 0
	|| !ClothingBackup.bBackedUp)
	{
		return;
	}

	SkelMesh->PreEditChange(NULL);

	SkelMesh->ClothingAssets = ClothingBackup.ClothingAssets;

	FSkeletalMeshResource* ImportedResource= SkelMesh->GetImportedResource();
	FStaticLODModel& LODModel = ImportedResource->LODModels[0];

	int32 NumSections = ClothingBackup.Sections.Num();
	int32 NumOriginSections = LODModel.Sections.Num();

	// Reload from assets
	for(int32 SecIdx=0; SecIdx < NumSections; SecIdx++)
	{
		FSkelMeshSection& Section = ClothingBackup.Sections[SecIdx];

		// corresponding section could be removed
		if(Section.CorrespondClothSectionIndex >= NumOriginSections)
		{
			continue;
		}

		int16 ChunkIndex = Section.ChunkIndex;

		FSkelMeshChunk& Chunk = ClothingBackup.Chunks[ChunkIndex];

		ImportClothingSectionFromClothingAsset(
			SkelMesh, 
			0,
			Section.CorrespondClothSectionIndex,
			Chunk.CorrespondClothAssetIndex,
			Chunk.ClothAssetSubmeshIndex
		);

	}
}

int32 GetNumLODs(NxClothingAsset *InAsset)
{
	return InAsset->getNumGraphicalLodLevels();
}

int32 GetNumRenderSubmeshes(NxClothingAsset *InAsset, int32 LODIndex)
{
	// The APEX Clothing Asset contains an APEX Render Mesh Asset, get a pointer to this
	int32 NumLODLevels = InAsset->getNumGraphicalLodLevels();

	if(LODIndex >= NumLODLevels)
	{
		return -1;
	}

	const NxRenderMeshAsset* ApexRenderMesh = InAsset->getRenderMeshAsset(LODIndex);

	check(ApexRenderMesh);

	int32 SubmeshCount = ApexRenderMesh->getSubmeshCount();

	return SubmeshCount;
}

void GetPhysicsPropertiesFromApexAsset(NxClothingAsset *InAsset, FClothPhysicsProperties& PropertyInfo)
{
	const NxParameterized::Interface* AssetParams = InAsset->getAssetNxParameterized();

	uint32 MaterialIndex;
	// uses default material index
	verify(NxParameterized::getParamU32(*AssetParams, "materialIndex", MaterialIndex));
	
	// ClothingMaterialLibraryParameters 
	NxParameterized::Interface* MaterialLibraryParams;
	NxParameterized::getParamRef(*AssetParams, "materialLibrary", MaterialLibraryParams);

	if (MaterialLibraryParams != NULL)
	{
		int32 NumMaterials;
		verify(NxParameterized::getParamArraySize(*MaterialLibraryParams, "materials", NumMaterials));

		check(MaterialIndex < (uint32)NumMaterials);
 
		char ParameterName[MAX_SPRINTF]; 

		// stiffness properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].bendingStiffness", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.BendResistance));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].shearingStiffness", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.ShearResistance));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].hardStretchLimitation", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.StretchLimit));

		// resistance properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].friction", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Friction));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].damping", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Damping));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].drag", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Drag));

		// scale properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].gravityScale", MaterialIndex);
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.GravityScale));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].inertiaScale", MaterialIndex);
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.InertiaBlend));

		// self-collision properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].selfcollisionThickness", MaterialIndex); 
		verify(NxParameterized::getParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.SelfCollisionThickness));

	}
}

void SetPhysicsPropertiesToApexAsset(NxClothingAsset *InAsset, FClothPhysicsProperties& PropertyInfo)
{
	const NxParameterized::Interface* AssetParams = InAsset->getAssetNxParameterized();

	uint32 MaterialIndex;
	verify(NxParameterized::getParamU32(*AssetParams, "materialIndex", MaterialIndex));

	// ClothingMaterialLibraryParameters 
	NxParameterized::Interface* MaterialLibraryParams;
	NxParameterized::getParamRef(*AssetParams, "materialLibrary", MaterialLibraryParams);

	if (MaterialLibraryParams != NULL)
	{
		char ParameterName[MAX_SPRINTF];

		// stiffness properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].bendingStiffness", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.BendResistance));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].shearingStiffness", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.ShearResistance));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].hardStretchLimitation", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.StretchLimit));

		// resistance properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].friction", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Friction));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].damping", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Damping));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].drag", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.Drag));

		// scale properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].gravityScale", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.GravityScale));
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].inertiaScale", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.InertiaBlend));

		// self-collision properties
		FCStringAnsi::Sprintf(ParameterName, "materials[%d].selfcollisionThickness", MaterialIndex);
		verify(NxParameterized::setParamF32(*MaterialLibraryParams, ParameterName, PropertyInfo.SelfCollisionThickness));
	}
}

#else

void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, bool bRecreateSkelMeshComponent)
{
	// print error about not supporting APEX
	UE_LOG(LogApexClothingUtils, Warning,TEXT("APEX Clothing is not supported") );	
}

void BackupClothingDataFromSkeletalMesh(USkeletalMesh* SkelMesh)
{

}

void ReapplyClothingDataToSkeletalMesh(USkeletalMesh* SkelMesh)
{

}

#endif // WITH_APEX_CLOTHING

} // namespace ApexClothingUtils

#undef LOCTEXT_NAMESPACE

