// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2009 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Static mesh creation from FBX data.
	Largely based on StaticMeshEdit.cpp
=============================================================================*/

#include "UnrealEd.h"
#include "RawMesh.h"
#include "MeshUtilities.h"

#include "Engine.h"
#include "StaticMeshResources.h"
#include "TextureLayout.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "FbxImporter.h"
#include "../Private/GeomFitUtils.h"
#include "TargetPlatform.h"
#include "FbxErrors.h"

#define LOCTEXT_NAMESPACE "FbxStaticMeshImport"

using namespace UnFbx;

struct ExistingStaticMeshData;
extern ExistingStaticMeshData* SaveExistingStaticMeshData(UStaticMesh* ExistingMesh);
extern void RestoreExistingMeshData(struct ExistingStaticMeshData* ExistingMeshDataPtr, UStaticMesh* NewMesh);

static FbxString GetNodeNameWithoutNamespace( FbxNode* Node )
{
	FbxString NodeName = Node->GetName();

	// Namespaces are marked with colons so find the last colon which will mark the start of the actual name
	int32 LastNamespceIndex = NodeName.ReverseFind(':');

	if( LastNamespceIndex == -1 )
	{
		// No namespace
		return NodeName;
	}
	else
	{
		// chop off the namespace
		return NodeName.Right( NodeName.GetLen() - (LastNamespceIndex + 1) );
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
UStaticMesh* UnFbx::FFbxImporter::ImportStaticMesh(UObject* InParent, FbxNode* Node, const FName& Name, EObjectFlags Flags, UFbxStaticMeshImportData* ImportData, UStaticMesh* InStaticMesh, int LODIndex)
{
	TArray<FbxNode*> MeshNodeArray;
	
	if ( !Node->GetMesh())
	{
		return NULL;
	}
	
	MeshNodeArray.Add(Node);
	return ImportStaticMeshAsSingle(InParent, MeshNodeArray, Name, Flags, ImportData, InStaticMesh, LODIndex);
}

bool UnFbx::FFbxImporter::BuildStaticMeshFromGeometry(FbxMesh* Mesh, UStaticMesh* StaticMesh, TArray<FFbxMaterial>& MeshMaterials, int LODIndex,
													  EVertexColorImportOption::Type VertexColorImportOption, const TMap<FVector, FColor>& ExistingVertexColorData, const FColor& VertexOverrideColor)
{
	check(StaticMesh->SourceModels.IsValidIndex(LODIndex));

	FbxNode* Node = Mesh->GetNode();
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];

	//remove the bad polygons before getting any data from mesh
	Mesh->RemoveBadPolygons();

	//Get the base layer of the mesh
	FbxLayer* BaseLayer = Mesh->GetLayer(0);
	if (BaseLayer == NULL)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoGeometryInMesh", "There is no geometry information in mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_NoGeometry);
		return false;
	}

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	// mapping from UVSets to Fbx LayerElementUV
	// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
	int32 LayerCount = Mesh->GetLayerCount();
	TArray<FString> UVSets;
	UVSets.Empty();
	if (LayerCount > 0)
	{
		int32 UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);

						UVSets.AddUnique(LocalUVSetName);
					}
				}
			}
		}
	}


	// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
	// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
	// will be slotted into available spaces.
	if( UVSets.Num() > 0 )
	{
		for(int32 ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
		{
			FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
			int32 SetIdx = UVSets.Find( ChannelName );

			// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
			// we will swap it into the correct spot.
			if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
			{
				// If we are going to swap to a position that is outside the bounds of the
				// array, then we pad out to that spot with empty data.
				for(int32 ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
				{
					UVSets.Add ( FString(TEXT("")) );
				}
				//Swap the entry into the appropriate spot.
				UVSets.Swap( SetIdx, ChannelNumIdx );
			}
		}
	}


	// See if any of our UV set entry names match LightMapUV.
	for(int UVSetIdx = 0; UVSetIdx < UVSets.Num(); UVSetIdx++)
	{
		if( UVSets[UVSetIdx] == TEXT("LightMapUV"))
		{
			StaticMesh->LightMapCoordinateIndex = UVSetIdx;
		}
	}

	//
	// create materials
	//
	int32 MaterialCount = 0;
	TArray<UMaterialInterface*> Materials;
	if (ImportOptions->bImportMaterials)
	{
		CreateNodeMaterials(Node, Materials, UVSets);
	}
	else if (ImportOptions->bImportTextures)
	{
		ImportTexturesFromNode(Node);
	}

	MaterialCount = Node->GetMaterialCount();
	check(!ImportOptions->bImportMaterials || Materials.Num() == MaterialCount);
	
	// Used later to offset the material indices on the raw triangle data
	int32 MaterialIndexOffset = MeshMaterials.Num();

	for (int32 MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++)
	{
		FFbxMaterial* NewMaterial = new(MeshMaterials) FFbxMaterial;
		FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);
		NewMaterial->FbxMaterial = FbxMaterial;
		if (ImportOptions->bImportMaterials)
		{
			NewMaterial->Material = Materials[MaterialIndex];
		}
		else
		{
			FString MaterialFullName = GetMaterialFullName(*FbxMaterial);
			FString BasePackageName = PackageTools::SanitizePackageName(FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName()) / MaterialFullName);
			UMaterialInterface* UnrealMaterialInterface = FindObject<UMaterialInterface>(NULL, *(BasePackageName + TEXT(".") + MaterialFullName));
			if (UnrealMaterialInterface == NULL)
			{
				UnrealMaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			NewMaterial->Material = UnrealMaterialInterface;
		}
	}

	if ( MaterialCount == 0 )
	{
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);
		FFbxMaterial* NewMaterial = new(MeshMaterials) FFbxMaterial;
		NewMaterial->Material = DefaultMaterial;
		NewMaterial->FbxMaterial = NULL;
		MaterialCount = 1;
	}

	//
	// Convert data format to unreal-compatible
	//

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	int32 LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSmoothing);
	for(int32 i = 0; i < LayerSmoothingCount; i++)
	{
		GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
	}

	if (!Mesh->IsTriangleMesh())
	{
		UE_LOG(LogFbx, Warning, TEXT("Triangulating static mesh %s"), ANSI_TO_TCHAR(Node->GetName()));

		const bool bReplace = true;
		FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(Mesh, bReplace);

		if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			Mesh = ConvertedNode->GetNode()->GetMesh();
		}
		else
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_FailedToTriangulate", "Unable to triangulate mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_TriangulationFailed);
			return false; // not clean, missing some dealloc
		}
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);

	//
	//	get the "material index" layer.  Do this AFTER the triangulation step as that may reorder material indices
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eByPolygon;

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	int32 UniqueUVCount = UVSets.Num();
	TArray<FbxLayerElementUV const*> LayerElementUV;
	TArray<FbxLayerElement::EReferenceMode> UVReferenceMode;
	TArray<FbxLayerElement::EMappingMode> UVMappingMode;
	if (UniqueUVCount > 0)
	{
		LayerElementUV.AddZeroed(UniqueUVCount);
		UVReferenceMode.AddZeroed(UniqueUVCount);
		UVMappingMode.AddZeroed(UniqueUVCount);
	}
	LayerCount = Mesh->GetLayerCount();
	for (int32 UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
	{
		bool bFoundUV = false;
		LayerElementUV[UVIndex] = NULL;
		for (int32 UVLayerIndex = 0; !bFoundUV &&UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int32 FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);
						if (LocalUVSetName == UVSets[UVIndex])
						{
							LayerElementUV[UVIndex] = ElementUV;
							UVReferenceMode[UVIndex] = ElementUV->GetReferenceMode();
							UVMappingMode[UVIndex] = ElementUV->GetMappingMode();
							break;
						}
					}
				}
			}
		}
	}
	UniqueUVCount = FMath::Min<int32>(UniqueUVCount, MAX_MESH_TEXTURE_COORDS);


	//
	// get the smoothing group layer
	//
	bool bSmoothingAvailable = false;

	FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
	FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eByEdge);
	if (SmoothingInfo)
	{
		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByEdge )
		{
			if (!GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing(Mesh))
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_FailedtoConvertSmoothingGroup", "Unable to fully convert the smoothing groups for mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_ConvertSmoothingGroupFailed);
				bSmoothingAvailable = false;
			}
		}

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByPolygon )
		{
			bSmoothingAvailable = true;
		}


		SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
		SmoothingMappingMode = SmoothingInfo->GetMappingMode();
	}

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
	FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
	FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

	//whether there is normal, tangent and binormal data in this mesh
	bool bHasNTBInformation = LayerElementNormal && LayerElementTangent && LayerElementBinormal;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementNormal)
	{
		NormalReferenceMode = LayerElementNormal->GetReferenceMode();
		NormalMappingMode = LayerElementNormal->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementTangent)
	{
		TangentReferenceMode = LayerElementTangent->GetReferenceMode();
		TangentMappingMode = LayerElementTangent->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode BinormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode BinormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementBinormal)
	{
		BinormalReferenceMode = LayerElementBinormal->GetReferenceMode();
		BinormalMappingMode = LayerElementBinormal->GetMappingMode();
	}

	//
	// build collision
	//
	bool bImportedCollision = ImportCollisionModels(StaticMesh, GetNodeNameWithoutNamespace(Node));

	if (false && !bImportedCollision && StaticMesh)	//if didn't import collision automatically generate one
	{
		StaticMesh->CreateBodySetup();

		const int32 NumDirs = 18;
		TArray<FVector> Dirs;
		Dirs.AddUninitialized(NumDirs);
		for (int32 DirIdx = 0; DirIdx < NumDirs; ++DirIdx) { Dirs[DirIdx] = KDopDir18[DirIdx]; }
		GenerateKDopAsSimpleCollision(StaticMesh, Dirs);
	}

	bool bEnableCollision = bImportedCollision || (GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates);
	for(int32 SectionIndex=MaterialIndexOffset; SectionIndex<MaterialIndexOffset+MaterialCount; SectionIndex++)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
		Info.bEnableCollision = bEnableCollision;
		StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	}

	//
	// build un-mesh triangles
	//

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix;
	FbxAMatrix TotalMatrixForNormal;
	TotalMatrix = ComputeTotalMatrix(Node);
	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.Transpose();
	int32 TriangleCount = Mesh->GetPolygonCount();

	if(TriangleCount == 0)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoTrianglesFoundInMesh", "No triangles were found on mesh  '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::StaticMesh_NoTriangles);
		return false;
	}

	int32 VertexCount = Mesh->GetControlPointsCount();
	int32 WedgeCount = TriangleCount * 3;
	bool OddNegativeScale = IsOddNegativeScale(TotalMatrix);

	// Load the existing raw mesh.
	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);
	int32 VertexOffset = RawMesh.VertexPositions.Num();
	int32 WedgeOffset = RawMesh.WedgeIndices.Num();
	int32 TriangleOffset = RawMesh.FaceMaterialIndices.Num();

	int32 MaxMaterialIndex = 0;

	// Reserve space for attributes.
	RawMesh.FaceMaterialIndices.AddZeroed(TriangleCount);
	RawMesh.FaceSmoothingMasks.AddZeroed(TriangleCount);
	RawMesh.WedgeIndices.AddZeroed(WedgeCount);

	if (bHasNTBInformation || RawMesh.WedgeTangentX.Num() > 0 || RawMesh.WedgeTangentY.Num() > 0)
	{
		RawMesh.WedgeTangentX.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentX.Num());
		RawMesh.WedgeTangentY.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentY.Num());
	}

	if (LayerElementNormal || RawMesh.WedgeTangentZ.Num() > 0 )
	{
		RawMesh.WedgeTangentZ.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentZ.Num());
	}

	if (LayerElementVertexColor || VertexColorImportOption != EVertexColorImportOption::Replace || RawMesh.WedgeColors.Num() )
	{
		int32 NumNewColors = WedgeOffset + WedgeCount - RawMesh.WedgeColors.Num();
		int32 FirstNewColor = RawMesh.WedgeColors.Num();
		RawMesh.WedgeColors.AddUninitialized(NumNewColors);
		for (int32 WedgeIndex = FirstNewColor; WedgeIndex < FirstNewColor + NumNewColors; ++WedgeIndex)
		{
			RawMesh.WedgeColors[WedgeIndex] = FColor::White;
		}
	}

	// When importing multiple mesh pieces to the same static mesh.  Ensure each mesh piece has the same number of Uv's
	int32 ExistingUVCount = 0;
	for( int32 ExistingUVIndex = 0; ExistingUVIndex < MAX_MESH_TEXTURE_COORDS; ++ExistingUVIndex )
	{
		if( RawMesh.WedgeTexCoords[ExistingUVIndex].Num() > 0 )
		{
			// Mesh already has UVs at this index
			++ExistingUVCount;
		}
		else
		{
			// No more UVs
			break;
		}
	}

	int32 UVCount = FMath::Max( UniqueUVCount, ExistingUVCount );

	// At least one UV set must exist.  
	UVCount = FMath::Max( 1, UVCount );

	for (int32 UVLayerIndex = 0; UVLayerIndex<UVCount; UVLayerIndex++)
	{
		RawMesh.WedgeTexCoords[UVLayerIndex].AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTexCoords[UVLayerIndex].Num());
	}

	int32 TriangleIndex;
	TMap<int32,int32> IndexMap;
	for( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		int32 DestTriangleIndex = TriangleOffset + TriangleIndex;

		for ( int32 CornerIndex=0; CornerIndex<3; CornerIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 WedgeIndex = WedgeOffset + TriangleIndex * 3 + (OddNegativeScale ? 2 - CornerIndex : CornerIndex);

			// Store vertex index and position.
			int32 ControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, CornerIndex);
			int32* ExistingIndex = IndexMap.Find(ControlPointIndex);
			if (ExistingIndex)
			{
				RawMesh.WedgeIndices[WedgeIndex] = *ExistingIndex;
			}
			else
			{
				FbxVector4 FbxPosition = Mesh->GetControlPoints()[ControlPointIndex];
				FbxVector4 FinalPosition = TotalMatrix.MultT(FbxPosition);
				int32 VertexIndex = RawMesh.VertexPositions.Add(Converter.ConvertPos(FinalPosition));
				RawMesh.WedgeIndices[WedgeIndex] = VertexIndex;
				IndexMap.Add(ControlPointIndex,VertexIndex);
			}

			//
			// normals, tangents and binormals
			//
			if (LayerElementNormal)
			{
				int TriangleCornerIndex = TriangleIndex*3 + CornerIndex;
				//normals may have different reference and mapping mode than tangents and binormals
				int NormalMapIndex = (NormalMappingMode == FbxLayerElement::eByControlPoint) ? 
					ControlPointIndex : TriangleCornerIndex;
				int NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDirect) ? 
					NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				//tangents and binormals share the same reference, mapping mode and index array
				if (bHasNTBInformation)
				{
					int TangentMapIndex = (TangentMappingMode == FbxLayerElement::eByControlPoint) ? 
						ControlPointIndex : TriangleCornerIndex;
					int TangentValueIndex = (TangentReferenceMode == FbxLayerElement::eDirect) ? 
						TangentMapIndex : LayerElementTangent->GetIndexArray().GetAt(TangentMapIndex);

					FbxVector4 TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentValueIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					FVector TangentX = Converter.ConvertDir(TempValue);
					RawMesh.WedgeTangentX[WedgeIndex] = TangentX.GetSafeNormal();

					int BinormalMapIndex = (BinormalMappingMode == FbxLayerElement::eByControlPoint) ? 
						ControlPointIndex : TriangleCornerIndex;
					int BinormalValueIndex = (BinormalReferenceMode == FbxLayerElement::eDirect) ? 
						BinormalMapIndex : LayerElementBinormal->GetIndexArray().GetAt(BinormalMapIndex);

					TempValue = LayerElementBinormal->GetDirectArray().GetAt(BinormalValueIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					FVector TangentY = -Converter.ConvertDir(TempValue);
					RawMesh.WedgeTangentY[WedgeIndex] = TangentY.GetSafeNormal();
				}

				FbxVector4 TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
				TempValue = TotalMatrixForNormal.MultT(TempValue);
				FVector TangentZ = Converter.ConvertDir(TempValue);
				RawMesh.WedgeTangentZ[WedgeIndex] = TangentZ.GetSafeNormal();
			}

			//
			// vertex colors
			//
			if (VertexColorImportOption == EVertexColorImportOption::Replace)
			{
				if (LayerElementVertexColor)
				{
					int32 VertexColorMappingIndex = (VertexColorMappingMode == FbxLayerElement::eByControlPoint) ?
						Mesh->GetPolygonVertex(TriangleIndex, CornerIndex) : (TriangleIndex * 3 + CornerIndex);

					int32 VectorColorIndex = (VertexColorReferenceMode == FbxLayerElement::eDirect) ?
					VertexColorMappingIndex : LayerElementVertexColor->GetIndexArray().GetAt(VertexColorMappingIndex);

					FbxColor VertexColor = LayerElementVertexColor->GetDirectArray().GetAt(VectorColorIndex);

					RawMesh.WedgeColors[WedgeIndex] = FColor(
						uint8(255.f*VertexColor.mRed),
						uint8(255.f*VertexColor.mGreen),
						uint8(255.f*VertexColor.mBlue),
						uint8(255.f*VertexColor.mAlpha)
						);
				}
			}
			else if (VertexColorImportOption == EVertexColorImportOption::Ignore)
			{
				// try to match this triangles current vertex with one that existed in the previous mesh.
				// This is a find in a tmap which uses a fast hash table lookup.
				FVector Position = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]];
				const FColor* PaintedColor = ExistingVertexColorData.Find(Position);
				if (PaintedColor)
				{
					// A matching color for this vertex was found
					RawMesh.WedgeColors[WedgeIndex] = *PaintedColor;
				}
			}
			else
			{
				// set the triangle's vertex color to a constant override
				check(VertexColorImportOption == EVertexColorImportOption::Override);
				RawMesh.WedgeColors[WedgeIndex] = VertexOverrideColor;
			}
		}

		//
		// smoothing mask
		//
		if (bSmoothingAvailable && SmoothingInfo)
		{
			if (SmoothingMappingMode == FbxLayerElement::eByPolygon)
			{
				int lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDirect) ? TriangleIndex : SmoothingInfo->GetIndexArray().GetAt(TriangleIndex);
				RawMesh.FaceSmoothingMasks[DestTriangleIndex] = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_UnsupportedSmoothingGroup", "Unsupported Smoothing group mapping mode on mesh  '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_UnsupportingSmoothingGroup);
			}
		}

		//
		// uvs
		//
		// In FBX file, the same UV may be saved multiple times, i.e., there may be same UV in LayerElementUV
		// So we don't import the duplicate UVs
		int32 UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<UniqueUVCount; UVLayerIndex++)
		{
			if (LayerElementUV[UVLayerIndex] != NULL) 
			{
				for (int32 CornerIndex=0;CornerIndex<3;CornerIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					int32 WedgeIndex = WedgeOffset + TriangleIndex * 3 + (OddNegativeScale ? 2 - CornerIndex : CornerIndex);

					int lControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, CornerIndex);
					int UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eByControlPoint) ? lControlPointIndex : TriangleIndex*3+CornerIndex;
					int32 UVIndex = (UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDirect) ? 
						UVMapIndex : LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);
					FbxVector2	UVVector = LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					RawMesh.WedgeTexCoords[UVLayerIndex][WedgeIndex].X = static_cast<float>(UVVector[0]);
					RawMesh.WedgeTexCoords[UVLayerIndex][WedgeIndex].Y = 1.f-static_cast<float>(UVVector[1]);   //flip the Y of UVs for DirectX
				}
			}
		}

		//
		// material index
		//
		int32 MaterialIndex = 0;
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
					// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eAllSame:
					{	
						MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(0);
					}
					break;
				case FbxLayerElement::eByPolygon:
					{	
						MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(TriangleIndex);
					}
					break;
				}
			}
		}
		MaterialIndex += MaterialIndexOffset;

		if (MaterialIndex >= MaterialCount + MaterialIndexOffset || MaterialIndex < 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Error_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
			MaterialIndex = 0;
		}
	
		RawMesh.FaceMaterialIndices[DestTriangleIndex] = MaterialIndex;
	}


	// Store the new raw mesh.
	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
	
	//
	// clean up.  This needs to happen before the mesh is destroyed
	//
	LayerElementUV.Empty();
	UVReferenceMode.Empty();
	UVMappingMode.Empty();

	return true;
}

UStaticMesh* UnFbx::FFbxImporter::ReimportStaticMesh(UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData)
{
	char MeshName[1024];
	FCStringAnsi::Strcpy(MeshName,1024,TCHAR_TO_ANSI(*Mesh->GetName()));
	TArray<FbxNode*> FbxMeshArray;
	FbxNode* Node = NULL;
	UStaticMesh* NewMesh = NULL;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	FillFbxMeshArray(Scene->GetRootNode(), FbxMeshArray, this);
	
	// if there is only one mesh, use it without name checking 
	// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
	if (FbxMeshArray.Num() == 1)
	{
		Node = FbxMeshArray[0];
	}
	else
	{
		// find the Fbx mesh node that the Unreal Mesh matches according to name
		int32 MeshIndex;
		for ( MeshIndex = 0; MeshIndex < FbxMeshArray.Num(); MeshIndex++ )
		{
			const char* FbxMeshName = FbxMeshArray[MeshIndex]->GetName();
			// The name of Unreal mesh may have a prefix, so we match from end
			int32 i = 0;
			char* MeshPtr = MeshName + FCStringAnsi::Strlen(MeshName) - 1;
			if (FCStringAnsi::Strlen(FbxMeshName) <= FCStringAnsi::Strlen(MeshName))
			{
				const char* FbxMeshPtr = FbxMeshName + FCStringAnsi::Strlen(FbxMeshName) - 1;
				while (i < FCStringAnsi::Strlen(FbxMeshName))
				{
					if (*MeshPtr != *FbxMeshPtr)
					{
						break;
					}
					else
					{
						i++;
						MeshPtr--;
						FbxMeshPtr--;
					}
				}
			}

			if (i == FCStringAnsi::Strlen(FbxMeshName)) // matched
			{
				// check further
				if ( FCStringAnsi::Strlen(FbxMeshName) == FCStringAnsi::Strlen(MeshName) || // the name of Unreal mesh is full match
					*MeshPtr == '_')														// or the name of Unreal mesh has a prefix
				{
					Node = FbxMeshArray[MeshIndex];
					break;
				}
			}
		}
	}

	// If there is no match it may be because an LOD group was imported where
	// the mesh name does not match the file name. This is actually the common case.
	if (!Node && FbxMeshArray.IsValidIndex(0))
	{
		FbxNode* BaseLODNode = FbxMeshArray[0];
		FbxNode* NodeParent = BaseLODNode ? BaseLODNode->GetParent() : NULL;
		if (NodeParent && NodeParent->GetNodeAttribute() && NodeParent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			// Reimport the entire LOD chain.
			Node = BaseLODNode;
		}
	}
	
	if (Node)
	{
		FbxNode* NodeParent = Node->GetParent();
		// Don't import materials and textures during a reimport
		ImportOptions->bImportMaterials = false;
		ImportOptions->bImportTextures = false;
		
		// if the Fbx mesh is a part of LODGroup, update LOD
		if (NodeParent && NodeParent->GetNodeAttribute() && NodeParent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			NewMesh = ImportStaticMesh(Mesh->GetOuter(), NodeParent->GetChild(0), *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, 0);
			if (NewMesh)
			{
				// import LOD meshes
				for (int32 LODIndex = 1; LODIndex < NodeParent->GetChildCount(); LODIndex++)
				{
					ImportStaticMesh(Mesh->GetOuter(), NodeParent->GetChild(LODIndex), *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, LODIndex);
				}
			}
		}
		else
		{
			NewMesh = ImportStaticMesh(Mesh->GetOuter(), Node, *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, 0);
		}
	}
	else
	{
		// no FBX mesh match, maybe the Unreal mesh is imported from multiple FBX mesh (enable option "Import As Single")
		if (FbxMeshArray.Num() > 0)
		{
			NewMesh = ImportStaticMeshAsSingle(Mesh->GetOuter(), FbxMeshArray, *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, 0);
		}
		else // no mesh found in the FBX file
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_NoFBXMeshFound", "No FBX mesh found when reimport Unreal mesh '{0}'. The FBX file is crashed."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		}
	}
	return NewMesh;
}

void UnFbx::FFbxImporter::VerifyGeometry(UStaticMesh* StaticMesh)
{
	// Calculate bounding box to check if too small
	{
		FVector Center, Extents;
		ComputeBoundingBox(StaticMesh, Center, Extents);

		if (Extents.GetAbsMax() < 5.f)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Prompt_MeshVerySmall", "Warning: The imported mesh is very small. This is most likely an issue with the units used when exporting to FBX.")), FFbxErrors::Generic_Mesh_SmallGeometry);
		}
	}
}

UStaticMesh* UnFbx::FFbxImporter::ImportStaticMeshAsSingle(UObject* InParent, TArray<FbxNode*>& MeshNodeArray, const FName InName, EObjectFlags Flags, UFbxStaticMeshImportData* TemplateImportData, UStaticMesh* InStaticMesh, int LODIndex)
{
	bool bBuildStatus = true;
	struct ExistingStaticMeshData* ExistMeshDataPtr = NULL;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (MeshNodeArray.Num() == 0)
	{
		return NULL;
	}
	
	// Count the number of verts
	int32 NumVerts = 0;
	int32 MeshIndex;
	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray[MeshIndex];
		FbxMesh* FbxMesh = Node->GetMesh();

		if (FbxMesh)
		{
			NumVerts += FbxMesh->GetControlPointsCount();

			// If not combining meshes, reset the vert count between meshes
			if (!ImportOptions->bCombineToSingle)
			{
				NumVerts = 0;
			}
		}
	}

	Parent = InParent;
	
	FString MeshName = ObjectTools::SanitizeObjectName(InName.ToString());

	// warning for missing smoothing group info
	CheckSmoothingInfo(MeshNodeArray[0]->GetMesh());

	// Parent package to place new meshes
	UPackage* Package = NULL;

	// create empty mesh
	UStaticMesh*	StaticMesh = NULL;

	UStaticMesh* ExistingMesh = NULL;
	UObject* ExistingObject = NULL;

	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;

	EVertexColorImportOption::Type VertexColorImportOption = ImportOptions->VertexColorImportOption;
	FString NewPackageName;

	if( InStaticMesh == NULL || LODIndex == 0 )
	{
		// Create a package for each mesh
		NewPackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) + TEXT("/") + MeshName;
		NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
		Package = CreatePackage(NULL, *NewPackageName);

		ExistingMesh = FindObject<UStaticMesh>( Package, *MeshName );
		ExistingObject = FindObject<UObject>( Package, *MeshName );		
	}

	if (ExistingMesh)
	{
		ExistingMesh->GetVertexColorData(ExistingVertexColorData);

		if (0 == ExistingVertexColorData.Num())
		{
			// If there were no vertex colors and we specified to ignore FBX vertex colors, automatically take vertex colors from the file anyway.
			if (VertexColorImportOption == EVertexColorImportOption::Ignore)
			{
				VertexColorImportOption = EVertexColorImportOption::Replace;
			}
		}

		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingMesh->PreEditChange(NULL);
		ExistMeshDataPtr = SaveExistingStaticMeshData(ExistingMesh);
	}
	else if (ExistingObject)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		bool bDeleteSucceeded = ObjectTools::DeleteSingleObject( ExistingObject );

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Create a package for each mesh
			Package = CreatePackage(NULL, *NewPackageName);
		}
		else
		{
			// failed to delete
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("ContentBrowser_CannotDeleteReferenced", "{0} wasn't created.\n\nThe asset is referenced by other content."), FText::FromString(MeshName))), FFbxErrors::Generic_CannotDeleteReferenced);
			return NULL;
		}

		// Vertex colors should be copied always if there is no existing static mesh.
		if (VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			VertexColorImportOption = EVertexColorImportOption::Replace;
		}
	}
	else
	{ 
		// Vertex colors should be copied always if there is no existing static mesh.
		if (VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			VertexColorImportOption = EVertexColorImportOption::Replace;
		}
	}
	
	if( InStaticMesh != NULL && LODIndex > 0 )
	{
		StaticMesh = InStaticMesh;
	}
	else
	{
		StaticMesh = new(Package,FName(*MeshName),Flags|RF_Public) UStaticMesh(FObjectInitializer());
	}

	if (StaticMesh->SourceModels.Num() < LODIndex+1)
	{
		// Add one LOD 
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		
		if (StaticMesh->SourceModels.Num() < LODIndex+1)
		{
			LODIndex = StaticMesh->SourceModels.Num() - 1;
		}
	}
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
	if( InStaticMesh != NULL && LODIndex > 0 && !SrcModel.RawMeshBulkData->IsEmpty() )
	{
		// clear out the old mesh data
		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->SaveRawMesh( RawMesh );
	}
	
	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 64;
	StaticMesh->LightMapCoordinateIndex = 1;

	TArray<FFbxMaterial> MeshMaterials;
	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray[MeshIndex];

		if (Node->GetMesh())
		{
			if (!BuildStaticMeshFromGeometry(Node->GetMesh(), StaticMesh, MeshMaterials, LODIndex,
											 VertexColorImportOption, ExistingVertexColorData, ImportOptions->VertexOverrideColor))
			{
				bBuildStatus = false;
				break;
			}
		}
	}

	if (bBuildStatus)
	{
		UE_LOG(LogFbx,Log,TEXT("== Initial material list:"));
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			UE_LOG(LogFbx,Log,TEXT("%d: %s"),MaterialIndex,*MeshMaterials[MaterialIndex].GetName() );
		}

		// Compress the materials array by removing any duplicates.
		bool bDoRemap = false;
		TArray<int32> MaterialMap;
		TArray<FFbxMaterial> UniqueMaterials;
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			bool bUnique = true;
			for (int32 OtherMaterialIndex = MaterialIndex - 1; OtherMaterialIndex >= 0; --OtherMaterialIndex)
			{
				if (MeshMaterials[MaterialIndex].FbxMaterial == MeshMaterials[OtherMaterialIndex].FbxMaterial)
				{
					int32 UniqueIndex = MaterialMap[OtherMaterialIndex];

					if( UniqueIndex > MAX_MESH_MATERIAL_INDEX )
					{
						UniqueIndex = MAX_MESH_MATERIAL_INDEX;
					}

					MaterialMap.Add(UniqueIndex);
					bDoRemap = true;
					bUnique = false;
					break;
				}
			}
			if (bUnique)
			{
				int32 UniqueIndex = UniqueMaterials.Add(MeshMaterials[MaterialIndex]);

				if (UniqueIndex > MAX_MESH_MATERIAL_INDEX)
				{
					UniqueIndex = MAX_MESH_MATERIAL_INDEX;
				}

				MaterialMap.Add( UniqueIndex );
			}
			else
			{
				UE_LOG(LogFbx,Log,TEXT("  remap %d -> %d"), MaterialIndex, MaterialMap[MaterialIndex]);
			}
		}

		if (UniqueMaterials.Num() > MAX_MESH_MATERIAL_INDEX)
		{
			AddTokenizedErrorMessage(
				FTokenizedMessage::Create(
				EMessageSeverity::Warning,
				FText::Format(LOCTEXT("Error_TooManyMaterials", "StaticMesh has too many({1}) materials. Clamping materials to {0} which may produce unexpected results. Break apart your mesh into multiple pieces to fix this."),
				FText::AsNumber(MAX_MESH_MATERIAL_INDEX),
				FText::AsNumber(UniqueMaterials.Num())
				)), 
				FFbxErrors::StaticMesh_TooManyMaterials);
		}

		// Sort materials based on _SkinXX in the name.
		TArray<uint32> SortedMaterialIndex;
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			int32 SkinIndex = 0xffff;
			int32 RemappedIndex = MaterialMap[MaterialIndex];
			if (!SortedMaterialIndex.IsValidIndex(RemappedIndex))
			{
				FString FbxMatName = MeshMaterials[RemappedIndex].GetName();

				int32 Offset = FbxMatName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (Offset != INDEX_NONE)
				{
					// Chop off the material name so we are left with the number in _SKINXX
					FString SkinXXNumber = FbxMatName.Right(FbxMatName.Len() - (Offset + 1)).RightChop(4);

					if (SkinXXNumber.IsNumeric())
					{
						SkinIndex = FPlatformString::Atoi( *SkinXXNumber );
						bDoRemap = true;
					}
				}

				SortedMaterialIndex.Add(((uint32)SkinIndex << 16) | ((uint32)RemappedIndex & 0xffff));
			}
		}
		SortedMaterialIndex.Sort();

		UE_LOG(LogFbx,Log,TEXT("== After sorting:"));
		TArray<UMaterialInterface*> SortedMaterials;
		for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
		{
			int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
			SortedMaterials.Add(UniqueMaterials[RemappedIndex].Material);
			UE_LOG(LogFbx,Log,TEXT("%d: %s"),SortedIndex,*UniqueMaterials[RemappedIndex].GetName());
		}
		UE_LOG(LogFbx,Log,TEXT("== Mapping table:"));
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialMap.Num(); ++MaterialIndex)
		{
			for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
			{
				int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
				if (MaterialMap[MaterialIndex] == RemappedIndex)
				{
					UE_LOG(LogFbx,Log,TEXT("  sort %d -> %d"), MaterialIndex, SortedIndex);
					MaterialMap[MaterialIndex] = SortedIndex;
					break;
				}
			}
		}
		
		// Remap material indices.
		int32 MaxMaterialIndex = 0;
		int32 FirstOpenUVChannel = 1;
		{
			FRawMesh RawMesh;
			SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

			if (bDoRemap)
			{
				for (int32 TriIndex = 0; TriIndex < RawMesh.FaceMaterialIndices.Num(); ++TriIndex)
				{
					RawMesh.FaceMaterialIndices[TriIndex] = MaterialMap[RawMesh.FaceMaterialIndices[TriIndex]];
				}
			}

			// Compact material indices so that we won't have any sections with zero triangles.
			RawMesh.CompactMaterialIndices();

			// Also compact the sorted materials array.
			if (RawMesh.MaterialIndexToImportIndex.Num() > 0)
			{
				TArray<UMaterialInterface*> OldSortedMaterials;

				Exchange(OldSortedMaterials,SortedMaterials);
				SortedMaterials.Empty(RawMesh.MaterialIndexToImportIndex.Num());
				for (int32 MaterialIndex = 0; MaterialIndex < RawMesh.MaterialIndexToImportIndex.Num(); ++MaterialIndex)
				{
					UMaterialInterface* Material = NULL;
					int32 ImportIndex = RawMesh.MaterialIndexToImportIndex[MaterialIndex];
					if (OldSortedMaterials.IsValidIndex(ImportIndex))
					{
						Material = OldSortedMaterials[ImportIndex];
					}
					SortedMaterials.Add(Material);
				}
			}

			for (int32 TriIndex = 0; TriIndex < RawMesh.FaceMaterialIndices.Num(); ++TriIndex)
			{
				MaxMaterialIndex = FMath::Max<int32>(MaxMaterialIndex,RawMesh.FaceMaterialIndices[TriIndex]);
			}

			for( int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; i++ )
			{
				if( RawMesh.WedgeTexCoords[i].Num() == 0 )
				{
					FirstOpenUVChannel = i;
					break;
				}
			}

			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
		}

		// Setup per-section info and the materials array.
		if (LODIndex == 0)
		{
			StaticMesh->Materials.Empty();
		}
		
		int32 NumMaterials = FMath::Min(SortedMaterials.Num(),MaxMaterialIndex+1);
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, MaterialIndex);
			Info.MaterialIndex = StaticMesh->Materials.Num();
			StaticMesh->SectionInfoMap.Set(LODIndex, MaterialIndex, Info);
			StaticMesh->Materials.Add(SortedMaterials[MaterialIndex]);
		}

	
		// Setup default LOD settings based on the selected LOD group.
		if (ExistingMesh == NULL && LODIndex == 0)
		{
			ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(CurrentPlatform);
			const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(ImportOptions->StaticMeshLODGroup);
			int32 NumLODs = LODGroup.GetDefaultNumLODs();
			while (StaticMesh->SourceModels.Num() < NumLODs)
			{
				new (StaticMesh->SourceModels) FStaticMeshSourceModel();
			}
			for (int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
			{
				StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
			}
			StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
		}

		if (ExistMeshDataPtr)
		{
			RestoreExistingMeshData(ExistMeshDataPtr, StaticMesh);
		}

		UFbxStaticMeshImportData* ImportData = UFbxStaticMeshImportData::GetImportDataForStaticMesh(StaticMesh, TemplateImportData);
		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(UFactory::CurrentFilename, StaticMesh);
		ImportData->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*UFactory::CurrentFilename).ToString();
		ImportData->bDirty = false;

		// @todo This overrides restored values currently but we need to be able to import over the existing settings if the user chose to do so.
		SrcModel.BuildSettings.bRemoveDegenerates = ImportOptions->bRemoveDegenerates;
		SrcModel.BuildSettings.bRecomputeNormals = ImportOptions->NormalImportMethod == FBXNIM_ComputeNormals;
		SrcModel.BuildSettings.bRecomputeTangents = ImportOptions->NormalImportMethod != FBXNIM_ImportNormalsAndTangents;

		if( ImportOptions->bGenerateLightmapUVs )
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.DstLightmapIndex = FirstOpenUVChannel;
			StaticMesh->LightMapCoordinateIndex = FirstOpenUVChannel;
		}
		else
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
		}

		StaticMesh->LODGroup = ImportOptions->StaticMeshLODGroup;
		StaticMesh->Build(false);
		
		// The code to check for bad lightmap UVs doesn't scale well with number of triangles.
		// Skip it here because Lightmass will warn about it during a light build anyway.
		bool bWarnOnBadLightmapUVs = false;
		if (bWarnOnBadLightmapUVs)
		{
			TArray< FString > MissingUVSets;
			TArray< FString > BadUVSets;
			TArray< FString > ValidUVSets;
			UStaticMesh::CheckLightMapUVs( StaticMesh, MissingUVSets, BadUVSets, ValidUVSets );

			// NOTE: We don't care about missing UV sets here, just bad ones!
			if( BadUVSets.Num() > 0 )
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_UVSetLayoutProblem", "Warning: The light map UV set for static mesh '{0}' appears to have layout problems.  Either the triangle UVs are overlapping one another or the UV are out of bounds (0.0 - 1.0 range.)"), FText::FromString(MeshName))), FFbxErrors::StaticMesh_UVSetLayoutProblem);
			}
		}
	}
	else
	{
		StaticMesh = NULL;
	}

	if (StaticMesh)
	{

		//collision generation
		if (StaticMesh->bCustomizedCollision == false && ImportOptions->bAutoGenerateCollision)
		{
			FKAggregateGeom & AggGeom = StaticMesh->BodySetup->AggGeom;
			AggGeom.ConvexElems.Empty(1);	//if no custom collision is setup we just regenerate collision when reimport

			const int32 NumDirs = 18;
			TArray<FVector> Dirs;
			Dirs.AddUninitialized(NumDirs);
			for (int32 DirIdx = 0; DirIdx < NumDirs; ++DirIdx) { Dirs[DirIdx] = KDopDir18[DirIdx]; }
			GenerateKDopAsSimpleCollision(StaticMesh, Dirs);
		}
		
		//warnings based on geometry
		VerifyGeometry(StaticMesh);
	}

	return StaticMesh;
}

struct FbxSocketNode
{
	FName SocketName;
	FbxNode* Node;
};

static void FindMeshSockets( FbxNode* StartNode, TArray<FbxSocketNode>& OutFbxSocketNodes )
{
	static const FString SocketPrefix( TEXT("SOCKET_") );
	if( StartNode && StartNode->GetNodeAttributeCount() > 0 )
	{
		// Find null attributes, they cold be sockets
		FbxNodeAttribute* Attribute = StartNode->GetNodeAttribute();

		if( Attribute != NULL && Attribute->GetAttributeType() == FbxNodeAttribute::eNull )
		{
			// Is this prefixed correctly? If so it is a socket
			FString SocketName = ANSI_TO_TCHAR( StartNode->GetName() );
			if( SocketName.StartsWith( SocketPrefix ) )
			{
				// Remove the prefix from the name
				SocketName = SocketName.RightChop( SocketPrefix.Len() );

				FbxSocketNode NewNode;
				NewNode.Node = StartNode;
				NewNode.SocketName = *SocketName;
				OutFbxSocketNodes.Add( NewNode );
			}
		}
	}

	// Recursively examine all children
	for ( int32 ChildIndex=0; ChildIndex < StartNode->GetChildCount(); ++ChildIndex )
	{
		FindMeshSockets( StartNode->GetChild(ChildIndex), OutFbxSocketNodes );
	}
}

void UnFbx::FFbxImporter::ImportStaticMeshSockets( UStaticMesh* StaticMesh )
{
	FbxNode* RootNode = Scene->GetRootNode();

	// Find all nodes that are sockets
	TArray<FbxSocketNode> SocketNodes;
	FindMeshSockets( RootNode, SocketNodes );

	// Create a UStaticMeshSocket for each fbx socket
	for( int32 SocketIndex = 0; SocketIndex < SocketNodes.Num(); ++SocketIndex )
	{
		FbxSocketNode& SocketNode = SocketNodes[ SocketIndex ];

		UStaticMeshSocket* Socket = StaticMesh->FindSocket( SocketNode.SocketName );
		if( !Socket )
		{
			// If the socket didn't exist create a new one now
			Socket = ConstructObject<UStaticMeshSocket>( UStaticMeshSocket::StaticClass(), StaticMesh );
			check(Socket);

			Socket->SocketName = SocketNode.SocketName;
			StaticMesh->Sockets.Add(Socket);
		}

		if( Socket )
		{
			FVector Translation = Converter.ConvertPos( SocketNode.Node->LclTranslation.Get() );
			FRotator Rotation = Converter.ConvertEuler( SocketNode.Node->LclRotation.Get() );
			FVector Scale = Converter.ConvertScale( SocketNode.Node->LclScaling.Get() );

			Socket->RelativeLocation = Translation;
			Socket->RelativeRotation = Rotation;
			Socket->RelativeScale = Scale;
		}
	}
}


bool UnFbx::FFbxImporter::FillCollisionModelList(FbxNode* Node)
{
	FbxString NodeName = GetNodeNameWithoutNamespace( Node );

	if ( NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
		 NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1 )
	{
		// Get name of static mesh that the collision model connect to
		uint32 StartIndex = NodeName.Find('_') + 1;
		int32 TmpEndIndex = NodeName.Find('_', StartIndex);
		int32 EndIndex = TmpEndIndex;
		// Find the last '_' (underscore)
		while (TmpEndIndex >= 0)
		{
			EndIndex = TmpEndIndex;
			TmpEndIndex = NodeName.Find('_', EndIndex+1);
		}
		
		const int32 NumMeshNames = 2;
		FbxString MeshName[NumMeshNames];
		if ( EndIndex >= 0 )
		{
			// all characters between the first '_' and the last '_' are the FBX mesh name
			// convert the name to upper because we are case insensitive
			MeshName[0] = NodeName.Mid(StartIndex, EndIndex - StartIndex).Upper();
			
			// also add a version of the mesh name that includes what follows the last '_'
			// in case that's not a suffix but, instead, is part of the mesh name
			if (StartIndex < (int32)NodeName.GetLen())
			{            
				MeshName[1] = NodeName.Mid(StartIndex).Upper();
			}
		}
		else if (StartIndex < (int32)NodeName.GetLen())
		{            
			MeshName[0] = NodeName.Mid(StartIndex).Upper();
		}

		for (int32 NameIdx = 0; NameIdx < NumMeshNames; ++NameIdx)
		{
			if ((int32)MeshName[NameIdx].GetLen() > 0)
			{
				FbxMap<FbxString, TSharedPtr<FbxArray<FbxNode* > > >::RecordType const *Models = CollisionModels.Find(MeshName[NameIdx]);
				TSharedPtr< FbxArray<FbxNode* > > Record;
				if ( !Models )
				{
					Record = MakeShareable( new FbxArray<FbxNode*>() );
					CollisionModels.Insert(MeshName[NameIdx], Record);
				}
				else
				{
					Record = Models->GetValue();
				}
				Record->Add(Node);
			}
		}

		return true;
	}

	return false;
}

extern void AddConvexGeomFromVertices( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void AddSphereGeomFromVerts( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void AddBoxGeomFromTris( const TArray<FPoly>& Tris, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void DecomposeUCXMesh( const TArray<FVector>& CollisionVertices, const TArray<int32>& CollisionFaceIdx, UBodySetup* BodySetup );

bool UnFbx::FFbxImporter::ImportCollisionModels(UStaticMesh* StaticMesh, const FbxString& InNodeName)
{
	// find collision models
	bool bRemoveEmptyKey = false;
	FbxString EmptyKey;

	// convert the name to upper because we are case insensitive
	FbxMap<FbxString, TSharedPtr< FbxArray<FbxNode* > > >::RecordType const *Record = CollisionModels.Find(InNodeName.Upper());
	if ( !Record )
	{
		// compatible with old collision name format
		// if CollisionModels has only one entry and the key is ""
		if ( CollisionModels.GetSize() == 1 )
		{
			Record = CollisionModels.Find( EmptyKey );
		}
		if ( !Record ) 
		{
			return false;
		}
		else
		{
			bRemoveEmptyKey = true;
		}
	}

	TSharedPtr< FbxArray<FbxNode*> > Models = Record->GetValue();

	StaticMesh->bCustomizedCollision = true;

	StaticMesh->CreateBodySetup();

	TArray<FVector>	CollisionVertices;
	TArray<int32>		CollisionFaceIdx;

	// construct collision model
	for (int32 ModelIndex=0; ModelIndex<Models->GetCount(); ModelIndex++)
	{
		FbxNode* Node = Models->GetAt(ModelIndex);
		FbxMesh* FbxMesh = Node->GetMesh();

		FbxMesh->RemoveBadPolygons();

		// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
		int32 LayerSmoothingCount = FbxMesh->GetLayerCount(FbxLayerElement::eSmoothing);
		for(int32 LayerIndex = 0; LayerIndex < LayerSmoothingCount; LayerIndex++)
		{
			GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (FbxMesh, LayerIndex);
		}

		if (!FbxMesh->IsTriangleMesh())
		{
			FString NodeName = ANSI_TO_TCHAR(MakeName(Node->GetName()));
			UE_LOG(LogFbx, Warning, TEXT("Triangulating mesh %s for collision model"), *NodeName);

			const bool bReplace = true;
			FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(FbxMesh, bReplace); // not in place ! the old mesh is still there

			if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
			{
				FbxMesh = ConvertedNode->GetNode()->GetMesh();
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_FailedToTriangulate", "Unable to triangulate mesh '{0}'"), FText::FromString(NodeName))), FFbxErrors::Generic_Mesh_TriangulationFailed);
				return false;
			}
		}

		int32 ControlPointsIndex;
		int32 ControlPointsCount = FbxMesh->GetControlPointsCount();
		FbxVector4* ControlPoints = FbxMesh->GetControlPoints();
		FbxAMatrix Matrix = ComputeTotalMatrix(Node);

		for ( ControlPointsIndex = 0; ControlPointsIndex < ControlPointsCount; ControlPointsIndex++ )
		{
			new(CollisionVertices)FVector( Converter.ConvertPos(Matrix.MultT(ControlPoints[ControlPointsIndex])) );
		}

		int32 TriangleCount = FbxMesh->GetPolygonCount();
		int32 TriangleIndex;
		for ( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
		{
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,0));
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,1));
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,2));
		}

		TArray<FPoly> CollisionTriangles;

		// Make triangles
		for(int32 x = 0;x < CollisionFaceIdx.Num();x += 3)
		{
			FPoly*	Poly = new( CollisionTriangles ) FPoly();

			Poly->Init();

			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 2]] );
			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 1]] );
			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 0]] );
			Poly->iLink = x / 3;

			Poly->CalcNormal(1);
		}

		// Construct geometry object
		FbxString ModelName(Node->GetName());
		if ( ModelName.Find("UCX") != -1 || ModelName.Find("MCDCX") != -1 )
		{
			if( !ImportOptions->bOneConvexHullPerUCX )
			{
				DecomposeUCXMesh( CollisionVertices, CollisionFaceIdx, StaticMesh->BodySetup );
			}
			else
			{
				FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

				// This function cooks the given data, so we cannot test for duplicates based on the position data
				// before we call it
				AddConvexGeomFromVertices( CollisionVertices, &AggGeo, (const TCHAR*)Node->GetName() );

				// Now test the late element in the AggGeo list and remove it if its a duplicate
				if(AggGeo.ConvexElems.Num() > 1)
				{
					FKConvexElem& NewElem = AggGeo.ConvexElems.Last();

					for(int32 ElementIndex = 0; ElementIndex < AggGeo.ConvexElems.Num()-1; ++ElementIndex)
					{
						FKConvexElem& CurrentElem = AggGeo.ConvexElems[ElementIndex];
					
						if(CurrentElem.VertexData.Num() == NewElem.VertexData.Num())
						{
							bool bFoundDifference = false;
							for(int32 VertexIndex = 0; VertexIndex < NewElem.VertexData.Num(); ++VertexIndex)
							{
								if(CurrentElem.VertexData[VertexIndex] != NewElem.VertexData[VertexIndex])
								{
									bFoundDifference = true;
									break;
								}
							}

							if(!bFoundDifference)
							{
								// The new collision geo is a duplicate, delete it
								AggGeo.ConvexElems.RemoveAt(AggGeo.ConvexElems.Num()-1);
								break;
							}
						}
					}
				}
			}
		}
		else if ( ModelName.Find("UBX") != -1 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddBoxGeomFromTris( CollisionTriangles, &AggGeo, (const TCHAR*)Node->GetName() );

			// Now test the last element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.BoxElems.Num() > 1)
			{
				FKBoxElem& NewElem = AggGeo.BoxElems.Last();

				for(int32 ElementIndex = 0; ElementIndex < AggGeo.BoxElems.Num()-1; ++ElementIndex)
				{
					FKBoxElem& CurrentElem = AggGeo.BoxElems[ElementIndex];

					if(	CurrentElem == NewElem )
					{
						// The new element is a duplicate, remove it
						AggGeo.BoxElems.RemoveAt(AggGeo.BoxElems.Num()-1);
						break;
					}
				}
			}
		}
		else if ( ModelName.Find("USP") != -1 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddSphereGeomFromVerts( CollisionVertices, &AggGeo, (const TCHAR*)Node->GetName() );

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.SphereElems.Num() > 1)
			{
				FKSphereElem& NewElem = AggGeo.SphereElems.Last();

				for(int32 ElementIndex = 0; ElementIndex < AggGeo.SphereElems.Num()-1; ++ElementIndex)
				{
					FKSphereElem& CurrentElem = AggGeo.SphereElems[ElementIndex];

					if(	CurrentElem == NewElem )
					{
						// The new element is a duplicate, remove it
						AggGeo.SphereElems.RemoveAt(AggGeo.SphereElems.Num()-1);
						break;
					}
				}
			}
		}

		// Clear any cached rigid-body collision shapes for this body setup.
		StaticMesh->BodySetup->ClearPhysicsMeshes();

		// Remove the empty key because we only use the model once for the first mesh
		if (bRemoveEmptyKey)
		{
			CollisionModels.Remove(EmptyKey);
		}

		CollisionVertices.Empty();
		CollisionFaceIdx.Empty();
	}

	// Create new GUID
	StaticMesh->BodySetup->InvalidatePhysicsData();

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(StaticMesh);
		
	return true;
}
#undef LOCTEXT_NAMESPACE