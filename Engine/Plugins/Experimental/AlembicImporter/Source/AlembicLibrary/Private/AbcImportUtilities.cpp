// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbcImportUtilities.h"
#include "Stats/StatsMisc.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>
THIRD_PARTY_INCLUDES_END

#include "AbcImportData.h"

#define LOCTEXT_NAMESPACE "AbcImporterUtilities"

FMatrix AbcImporterUtilities::ConvertAlembicMatrix(const Alembic::Abc::M44d& AbcMatrix)
{
	FMatrix Matrix;
	for (uint32 i = 0; i < 16; ++i)
	{
		Matrix.M[i >> 2][i % 4] = (float)AbcMatrix.getValue()[i];
	}

	return Matrix;
}

uint32 AbcImporterUtilities::GenerateMaterialIndicesFromFaceSets(Alembic::AbcGeom::IPolyMeshSchema &Schema, const Alembic::Abc::ISampleSelector FrameSelector, TArray<int32> &MaterialIndicesOut)
{
	// Retrieve face set names to determine if we will have to process face sets (used for face-material indices)
	std::vector<std::string> FaceSetNames;
	Schema.getFaceSetNames(FaceSetNames);

	// Number of unique face sets found in the Alembic Object
	uint32 NumUniqueFaceSets = 0;
	if (FaceSetNames.size() != 0)
	{
		// Loop over the face-set names
		for (uint32 FaceSetIndex = 0; FaceSetIndex < FaceSetNames.size(); ++FaceSetIndex)
		{
			Alembic::AbcGeom::IFaceSet FaceSet = Schema.getFaceSet(FaceSetNames[FaceSetIndex]);
			Alembic::AbcGeom::IFaceSetSchema FaceSetSchema = FaceSet.getSchema();
			Alembic::AbcGeom::IFaceSetSchema::Sample FaceSetSample;
			FaceSetSchema.get(FaceSetSample, FrameSelector);

			// Retrieve face indices that are part of this face set
			Alembic::Abc::Int32ArraySamplePtr Faces = FaceSetSample.getFaces();
			const bool bFacesAvailable = (Faces != nullptr);
			const int NumFaces = Faces->size();

			// Set the shared Material index for all the contained faces
			for (int32 i = 0; i < NumFaces && NumFaces < MaterialIndicesOut.Num(); ++i)
			{
				const int32 FaceIndex = Faces->get()[i];
				MaterialIndicesOut[FaceIndex] = FaceSetIndex;
			}

			// Found a new unique faceset
			NumUniqueFaceSets++;
		}
	}

	return NumUniqueFaceSets;
}

void AbcImporterUtilities::RetrieveFaceSetNames(Alembic::AbcGeom::IPolyMeshSchema &Schema, TArray<FString>& NamesOut)
{
	// Retrieve face set names to determine if we will have to process face sets (used for face-material indices)
	std::vector<std::string> FaceSetNames;
	Schema.getFaceSetNames(FaceSetNames);

	for (const std::string& Name : FaceSetNames)
	{
		NamesOut.Add(FString(Name.c_str()));
	}
}

void AbcImporterUtilities::TriangulateIndexBuffer(const TArray<uint32>& InFaceCounts, TArray<uint32>& InOutIndices)
{
	check(InFaceCounts.Num() > 0);
	check(InOutIndices.Num() > 0);

	TArray<uint32> NewIndices;
	NewIndices.Reserve(InFaceCounts.Num() * 4);

	uint32 Index = 0;
	for (const uint32 NumIndicesForFace : InFaceCounts)
	{			
		if (NumIndicesForFace > 3)
		{
			// Triangle 0
			NewIndices.Add(InOutIndices[Index]);
			NewIndices.Add(InOutIndices[Index + 1]);
			NewIndices.Add(InOutIndices[Index + 3]);


			// Triangle 1
			NewIndices.Add(InOutIndices[Index + 3]);
			NewIndices.Add(InOutIndices[Index + 1]);
			NewIndices.Add(InOutIndices[Index + 2]);
		}
		else
		{
			NewIndices.Add(InOutIndices[Index]);
			NewIndices.Add(InOutIndices[Index + 1]);
			NewIndices.Add(InOutIndices[Index + 2]);
		}

		Index += NumIndicesForFace;
	}

	// Set new data
	InOutIndices = NewIndices;
}

void AbcImporterUtilities::TriangulateMaterialIndices(const TArray<uint32>& InFaceCounts, TArray<int32>& InOutData)
{
	check(InFaceCounts.Num() > 0);
	check(InOutData.Num() > 0);

	TArray<int32> NewData;		
	NewData.Reserve(InFaceCounts.Num() * 2);

	for (int32 Index = 0; Index < InFaceCounts.Num(); ++Index)
	{
		const uint32 NumIndicesForFace = InFaceCounts[Index];
		if (NumIndicesForFace == 4)
		{
			NewData.Add(InOutData[Index]);
			NewData.Add(InOutData[Index]);
		}
		else
		{
			NewData.Add(InOutData[Index]);
		}
	}

	// Set new data
	InOutData = NewData;
}

FAbcMeshSample* AbcImporterUtilities::GenerateAbcMeshSampleForFrame(Alembic::AbcGeom::IPolyMeshSchema& Schema, const Alembic::Abc::ISampleSelector FrameSelector, const bool bFirstFrame)
{
	SCOPE_LOG_TIME("STAT_ALEMBIC_GenerateAbcMesh", nullptr);

	FAbcMeshSample* Sample = new FAbcMeshSample();

	// Get single (vertex-data) sample from Alembic file
	Alembic::AbcGeom::IPolyMeshSchema::Sample MeshSample;
	Schema.get(MeshSample, FrameSelector);

	bool bRetrievalResult = true;

	// Retrieve all available mesh data
	Alembic::Abc::P3fArraySamplePtr PositionsSample = MeshSample.getPositions();
	bRetrievalResult &= RetrieveTypedAbcData<Alembic::Abc::P3fArraySamplePtr, FVector>(PositionsSample, Sample->Vertices);	

	Alembic::Abc::Int32ArraySamplePtr FaceCountsSample = MeshSample.getFaceCounts();
	TArray<uint32> FaceCounts;
	bRetrievalResult &= RetrieveTypedAbcData<Alembic::Abc::Int32ArraySamplePtr, uint32>(FaceCountsSample, FaceCounts);
	const bool bNeedsTriangulation = FaceCounts.Contains(4);

	const uint32* Result = FaceCounts.FindByPredicate([](uint32 FaceCount) { return FaceCount < 3 || FaceCount > 4; });
	if (Result)
	{
		// We found an Ngon which we can't triangulate atm
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FoundNGon", "Unable to import mesh due to a face consisting of {0} vertices, expecting triangles (3) or quads (4)."), FText::FromString(FString::FromInt((*Result)))));
		FAbcImportLogger::AddImportMessage(Message);
		delete Sample;
		return nullptr;
	}

	Alembic::Abc::Int32ArraySamplePtr IndicesSample = MeshSample.getFaceIndices();
	bRetrievalResult &= RetrieveTypedAbcData<Alembic::Abc::Int32ArraySamplePtr, uint32>(IndicesSample, Sample->Indices);
	if (bNeedsTriangulation)
	{
		TriangulateIndexBuffer(FaceCounts, Sample->Indices);
	}

	Alembic::AbcGeom::IV2fGeomParam UVCoordinateParameter = Schema.getUVsParam();
	if (UVCoordinateParameter.valid())
	{
		Alembic::Abc::V2fArraySamplePtr UVSample = UVCoordinateParameter.getValueProperty().getValue(FrameSelector);
		RetrieveTypedAbcData<Alembic::Abc::V2fArraySamplePtr, FVector2D>(UVSample, Sample->UVs);

		// Can only retrieve UV indices when the UVs array is indexed
		const bool bIndexedUVs = UVCoordinateParameter.getIndexProperty().valid();
		if (bIndexedUVs)
		{
			Alembic::Abc::UInt32ArraySamplePtr UVIndiceSample = UVCoordinateParameter.getIndexProperty().getValue(FrameSelector);
			TArray<uint32> UVIndices;
			RetrieveTypedAbcData<Alembic::Abc::UInt32ArraySamplePtr, uint32>(UVIndiceSample, UVIndices);

			if (bNeedsTriangulation)
			{
				TriangulateIndexBuffer(FaceCounts, UVIndices);
			}

			// Expand UV array
			ExpandVertexAttributeArray<FVector2D>(UVIndices, Sample->UVs);
		}
		else if (Sample->Normals.Num())
		{
			// For vertex only normals (and no normal indices available), expand using the regular indices
			if (Sample->UVs.Num() != Sample->Indices.Num())
			{
				ExpandVertexAttributeArray<FVector2D>(Sample->Indices, Sample->UVs);
			}
			else if (bNeedsTriangulation)
			{
				TriangulateVertexAttributeBuffer(FaceCounts, Sample->UVs);
			}
		}
	}
	else
	{
		Sample->UVs.AddZeroed(Sample->Indices.Num());
	}

	Alembic::AbcGeom::IN3fGeomParam NormalParameter = Schema.getNormalsParam();
	// Check if Normals are available anyhow
	const bool bNormalsAvailable = NormalParameter.valid();

	// Check if the Normals are 'constant' which means there won't be any normal data available after frame 0
	bool bConstantNormals = bNormalsAvailable && Schema.getNormalsParam().isConstant();
	if (bNormalsAvailable && (!bConstantNormals || (bConstantNormals && bFirstFrame)))
	{
		Alembic::Abc::N3fArraySamplePtr NormalsSample = NormalParameter.getValueProperty().getValue(FrameSelector);
		RetrieveTypedAbcData<Alembic::Abc::N3fArraySamplePtr, FVector>(NormalsSample, Sample->Normals);

		// Can only retrieve normal indices when the Normals array is indexed
		bool bIndexedNormals = NormalParameter.getIndexProperty().valid();
		if (bIndexedNormals)
		{
			Alembic::Abc::UInt32ArraySamplePtr NormalIndiceSample = NormalParameter.getIndexProperty().getValue(FrameSelector);
			TArray<uint32> NormalIndices;
			RetrieveTypedAbcData<Alembic::Abc::UInt32ArraySamplePtr, uint32>(NormalIndiceSample, NormalIndices);

			if (bNeedsTriangulation)
			{
				TriangulateIndexBuffer(FaceCounts, NormalIndices);
			}

			// Expand Normal array
			ExpandVertexAttributeArray<FVector>(NormalIndices, Sample->Normals);
		}
		else
		{
			// For vertex only normals (and no normal indices available), expand using the regular indices
			if (Sample->Normals.Num() != Sample->Indices.Num())
			{
				ExpandVertexAttributeArray<FVector>(Sample->Indices, Sample->Normals);
			}
			else if (bNeedsTriangulation)
			{
				TriangulateVertexAttributeBuffer(FaceCounts, Sample->Normals);
			}
		}
	}

	// Pre initialize face-material indices
	Sample->MaterialIndices.AddZeroed(Sample->Indices.Num() / 3);
	Sample->NumMaterials = GenerateMaterialIndicesFromFaceSets(Schema, FrameSelector, Sample->MaterialIndices);

	// Triangulate material face indices if needed
	if (bNeedsTriangulation)
	{
		TriangulateMaterialIndices(FaceCounts, Sample->MaterialIndices);
	}

	if (!bRetrievalResult)
	{
		delete Sample;
		Sample = nullptr;
	}

	return Sample;
}

/** Generated smoothing groups based on the given face normals, will compare angle between adjacent normals to determine whether or not an edge is hard/soft
	and calculates the smoothing group information with the edge data */
void AbcImporterUtilities::GenerateSmoothingGroups(TMultiMap<uint32, uint32> &TouchingFaces, const TArray<FVector>& FaceNormals,
	TArray<uint32>& FaceSmoothingGroups, uint32& HighestSmoothingGroup, const float HardAngleDotThreshold)
{
	// Cache whether or not the hard angle thresshold is set to 0.0 by the user
	const bool bZeroThreshold = FMath::IsNearlyZero(HardAngleDotThreshold);

	// MultiMap holding connected face indices of which is determined they belong to the same smoothing group (angle between face normals tested)
	TMultiMap<uint32, uint32> SmoothingGroupConnectedFaces;
	// Loop over all the faces
	const int32 NumFaces = FaceNormals.Num();
	SmoothingGroupConnectedFaces.Reserve(NumFaces * 3);
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Retrieve all the indices to faces that are connect to the current face
		TArray<uint32> ConnectedFaceIndices;
		TouchingFaces.MultiFind(FaceIndex, ConnectedFaceIndices);

		// Get the vertex-averaged face normal
		FVector FaceNormal = FaceNormals[FaceIndex];

		for (int32 i = 0; i < ConnectedFaceIndices.Num(); ++i)
		{
			const uint32 ConnectedFaceIndex = ConnectedFaceIndices[i];
			FVector ConnectedFaceNormal = FaceNormals[ConnectedFaceIndex];

			// Calculate the Angle between the two connected face normals and clamp from 0-1
			const float DotProduct = FMath::Clamp(FaceNormal | ConnectedFaceNormal, 0.0f, 1.0f);

			// Compare DotProduct against threshold and handle 0.0 case correctly
			if (DotProduct > HardAngleDotThreshold || (bZeroThreshold && FMath::IsNearlyZero(DotProduct)))
			{
				// If the faces have a "similar" normal we can determine that they should belong to the same smoothing group so mark them as SmoothingGroupConnectedFaces
				SmoothingGroupConnectedFaces.Add(FaceIndex, ConnectedFaceIndex);
			}
		}
	}

	// Map holding FaceIndices and their SmoothingGroupIndex
	TMap<uint32, uint32> SmoothingGroupFaces;
	uint32 NumSmoothingGroups = 0;

	SmoothingGroupFaces.Reserve(NumFaces);

	HighestSmoothingGroup = 0;
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Find faces which are smoothing-group connected to the current face
		TArray<uint32> ConnectedFaceIndices;
		SmoothingGroupConnectedFaces.MultiFind(FaceIndex, ConnectedFaceIndices);

		// Check if we haven't already stored a SmoothingGroupIndex for this face
		const uint32* StoredGroupIndex = SmoothingGroupFaces.Find(FaceIndex);

		// Determine which SmoothingGroupIndex to use and whether or not to increment NumSmoothingGroups 
		uint32 SmoothingGroupIndex = (StoredGroupIndex != nullptr) ? (*StoredGroupIndex) : NumSmoothingGroups;
		NumSmoothingGroups = (StoredGroupIndex != nullptr) ? NumSmoothingGroups : NumSmoothingGroups + 1;

		// For each connected face add an entry to the SmoothingGroupFaces map using the previously determined SmoothingGroupIndex
		for (int32 ConnectedFaceIndex = 0; ConnectedFaceIndex < ConnectedFaceIndices.Num(); ++ConnectedFaceIndex)
		{
			SmoothingGroupFaces.Add(FaceIndex, SmoothingGroupIndex);
			SmoothingGroupFaces.Add(ConnectedFaceIndices[ConnectedFaceIndex], SmoothingGroupIndex);

			// Store the SmoothingGroupIndex in the RawMesh data structure (used for tangent calculation)				
			FaceSmoothingGroups[ConnectedFaceIndices[ConnectedFaceIndex]] = SmoothingGroupIndex;
		}

		// Store the smoothing group index for the face we are currently handling
		FaceSmoothingGroups[FaceIndex] = SmoothingGroupIndex;

		HighestSmoothingGroup = FMath::Max(HighestSmoothingGroup, SmoothingGroupIndex);
	}
}

void AbcImporterUtilities::GenerateSmoothingGroupsIndices(FAbcMeshSample* MeshSample, const UAbcImportSettings* ImportSettings)
{
	// Vertex lookup map
	TMultiMap<uint32, uint32> VertexLookupMap;

	// Stores face indices that touch (at either one of their vertices)
	TMultiMap<uint32, uint32> TouchingFaces;

	// Stores the individual face normals (vertex averaged)
	TArray<FVector> FaceNormals;

	// Pre-initialize RawMesh arrays
	const int32 NumFaces = MeshSample->Indices.Num() / 3;
	MeshSample->SmoothingGroupIndices.Empty(NumFaces);
	MeshSample->SmoothingGroupIndices.AddZeroed(NumFaces);

	// Loop over faces
	uint32 Offset = 0;

	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Will hold the averaged face normal
		FVector FaceNormal(0, 0, 0);

		// Determine number of vertices for this face (we only support triangle-based meshes for now)
		const int32 NumVertsForFace = 3;
			
		// Triangle index winding
		const int32 TriangleIndices[3] = { 2, 1, 0 };

		// Loop over verts for current face (only support triangulated)
		for (int32 CornerIndex = 0; CornerIndex < NumVertsForFace; ++CornerIndex)
		{
			// Sample and face corner offset
			const uint32 TriSampleIndex = Offset + TriangleIndices[CornerIndex];
			const uint32 CornerOffset = Offset + CornerIndex;

			// Vertex, uv and normal indices
			const int32 VertexIndex = MeshSample->Indices[TriSampleIndex];

			// Check if there is already information stored for this VertexIndex
			TArray<const uint32*> VertexInformations;
			VertexLookupMap.MultiFindPointer(VertexIndex, VertexInformations);

			// If it doesn't add a new entry with storing the current FaceIndex
			if (VertexInformations.Num() == 0)
			{
				VertexLookupMap.Add(VertexIndex, FaceIndex);
			}
			else
			{
				// If there is an entry found (can be multiple)
				bool bFound = false;
				for (int32 VertexInfoIndex = 0; VertexInfoIndex < VertexInformations.Num(); ++VertexInfoIndex)
				{
					// Check if they belong to the face index, if so we don't have to add another entry
					const uint32* StoredFaceIndex = VertexInformations[VertexInfoIndex];
					if (*StoredFaceIndex == FaceIndex)
					{
						bFound = true;
					}
					else
					{
						// If the VertexIndices are the same but the FaceIndex differs we found two faces that share at least one vertex, thus add them to the TouchFaces map
						TouchingFaces.AddUnique(*StoredFaceIndex, FaceIndex);
					}
				}

				// If we didn't find an entry with the same FaceIndex add a new entry for it
				if (!bFound)
				{
					VertexLookupMap.Add(VertexIndex, FaceIndex);
				}
			}

			// Retrieve normal to calculate the face normal
			FVector Normal = MeshSample->Normals[TriSampleIndex];

			// Averaged face normal addition
			FaceNormal += Normal;
		}

		// Moving along the vertex reading position by the amount of vertices for this face
		Offset += NumVertsForFace;

		// Store the averaged face normal
		FaceNormals.Add(FaceNormal.GetSafeNormal());
	}
		
	MeshSample->NumSmoothingGroups = 0;
	GenerateSmoothingGroups(TouchingFaces, FaceNormals, MeshSample->SmoothingGroupIndices, MeshSample->NumSmoothingGroups, ImportSettings->NormalGenerationSettings.HardEdgeAngleThreshold);
	MeshSample->NumSmoothingGroups += 1;
}

void AbcImporterUtilities::CalculateNormals(FAbcMeshSample* Sample)
{
	Sample->Normals.Empty(Sample->Indices.Num());
	Sample->Normals.AddZeroed(Sample->Indices.Num());

	const uint32 NumFaces = Sample->Indices.Num() / 3;
	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Triangle index winding
		const int32 TriangleIndices[3] = { 2, 1, 0 };
		const int32 FaceOffset = FaceIndex * 3;

		FVector VertexPositions[3];
		int32 VertexIndices[3];

		// Retrieve vertex indices and positions
		VertexIndices[0] = Sample->Indices[FaceOffset + TriangleIndices[0]];
		VertexPositions[0] = Sample->Vertices[VertexIndices[0]];

		VertexIndices[1] = Sample->Indices[FaceOffset + TriangleIndices[1]];
		VertexPositions[1] = Sample->Vertices[VertexIndices[1]];

		VertexIndices[2] = Sample->Indices[FaceOffset + TriangleIndices[2]];
		VertexPositions[2] = Sample->Vertices[VertexIndices[2]];


		// Calculate normal for triangle face			
		FVector N = FVector::CrossProduct((VertexPositions[0] - VertexPositions[1]), (VertexPositions[0] - VertexPositions[2]));
		N.Normalize();

		// Unrolled loop
		Sample->Normals[FaceOffset + 0] += N;
		Sample->Normals[FaceOffset + 1] += N;
		Sample->Normals[FaceOffset + 2] += N;
	}

	for (FVector& Normal : Sample->Normals)
	{
		Normal.Normalize();
	}
}

void AbcImporterUtilities::CalculateSmoothNormals(FAbcMeshSample* Sample)
{
	TArray<FVector> PerVertexNormals;		
	PerVertexNormals.AddZeroed(Sample->Vertices.Num());

	// Loop over each face
	const uint32 NumFaces = Sample->Indices.Num() / 3;
	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const int32 TriangleIndices[3] = { 2, 1, 0 };
		const int32 FaceOffset = FaceIndex * 3;
			
		// Retrieve vertex indices and positions
		int32 VertexIndices[3];
		FVector VertexPositions[3];
		
		// Retrieve vertex indices and positions
		VertexIndices[0] = Sample->Indices[FaceOffset + TriangleIndices[0]];
		VertexPositions[0] = Sample->Vertices[VertexIndices[0]];

		VertexIndices[1] = Sample->Indices[FaceOffset + TriangleIndices[1]];
		VertexPositions[1] = Sample->Vertices[VertexIndices[1]];

		VertexIndices[2] = Sample->Indices[FaceOffset + TriangleIndices[2]];
		VertexPositions[2] = Sample->Vertices[VertexIndices[2]];
			
		// Calculate normal for triangle face			
		FVector N = FVector::CrossProduct((VertexPositions[0] - VertexPositions[1]), (VertexPositions[0] - VertexPositions[2]));
		N.Normalize();

		// Unrolled loop
		PerVertexNormals[VertexIndices[0]] += N;
		PerVertexNormals[VertexIndices[1]] += N;
		PerVertexNormals[VertexIndices[2]] += N;
	}

	Sample->Normals.Empty(Sample->Indices.Num());
	Sample->Normals.AddZeroed(Sample->Indices.Num());

	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const int32 FaceOffset = FaceIndex * 3;

		// Unrolled loop for calculating final normals
		Sample->Normals[FaceOffset + 0] = PerVertexNormals[Sample->Indices[FaceOffset + 0]];
		Sample->Normals[FaceOffset + 0].Normalize();

		Sample->Normals[FaceOffset + 1] = PerVertexNormals[Sample->Indices[FaceOffset + 1]];
		Sample->Normals[FaceOffset + 1].Normalize();

		Sample->Normals[FaceOffset + 2] = PerVertexNormals[Sample->Indices[FaceOffset + 2]];
		Sample->Normals[FaceOffset + 2].Normalize();
	}
}

void AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(FAbcMeshSample* Sample, const TArray<uint32>& SmoothingMasks, const uint32 NumSmoothingGroups)
{
	if (NumSmoothingGroups == 1)
	{
		CalculateSmoothNormals(Sample);
		return;
	}

	TArray<FVector> PerVertexNormals;
	TArray<TArray<FVector>> SmoothingGroupVertexNormals;

	SmoothingGroupVertexNormals.AddDefaulted(NumSmoothingGroups);

	for (uint32 SmoothingGroupIndex = 0; SmoothingGroupIndex < NumSmoothingGroups; ++SmoothingGroupIndex)
	{
		SmoothingGroupVertexNormals[SmoothingGroupIndex].AddZeroed(Sample->Vertices.Num());
	}

	PerVertexNormals.AddZeroed(Sample->Vertices.Num());

	// Loop over each face
	const uint32 NumFaces = Sample->Indices.Num() / 3;
	const int32 TriangleIndices[3] = { 2, 1, 0 };
	int32 VertexIndices[3];
	FVector VertexPositions[3];

	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Retrieve smoothing group for this face
		const int32 SmoothingGroup = SmoothingMasks[FaceIndex];
		const int32 FaceOffset = FaceIndex * 3;

		// Retrieve vertex indices and positions
		VertexIndices[0] = Sample->Indices[FaceOffset + TriangleIndices[0]];
		VertexPositions[0] = Sample->Vertices[VertexIndices[0]];

		VertexIndices[1] = Sample->Indices[FaceOffset + TriangleIndices[1]];
		VertexPositions[1] = Sample->Vertices[VertexIndices[1]];

		VertexIndices[2] = Sample->Indices[FaceOffset + TriangleIndices[2]];
		VertexPositions[2] = Sample->Vertices[VertexIndices[2]];

		// Calculate normal for triangle face			
		FVector N = FVector::CrossProduct((VertexPositions[0] - VertexPositions[1]), (VertexPositions[0] - VertexPositions[2]));
		N.Normalize();
				
		// Unrolled loop
		SmoothingGroupVertexNormals[SmoothingGroup][VertexIndices[0]] += N;
		SmoothingGroupVertexNormals[SmoothingGroup][VertexIndices[1]] += N;
		SmoothingGroupVertexNormals[SmoothingGroup][VertexIndices[2]] += N;			
	}

	Sample->Normals.Empty(Sample->Indices.Num());
	Sample->Normals.AddZeroed(Sample->Indices.Num());

	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		// Retrieve smoothing group for this face
		const int32 SmoothingGroup = SmoothingMasks[FaceIndex];
		const int32 FaceOffset = FaceIndex * 3;
			
		// Unrolled loop for calculating final normals
		Sample->Normals[FaceOffset + 0] = SmoothingGroupVertexNormals[SmoothingGroup][Sample->Indices[FaceOffset + 0]];
		Sample->Normals[FaceOffset + 0].Normalize();												  
																										  
		Sample->Normals[FaceOffset + 1] = SmoothingGroupVertexNormals[SmoothingGroup][Sample->Indices[FaceOffset + 1]];
		Sample->Normals[FaceOffset + 1].Normalize();												  
																										  
		Sample->Normals[FaceOffset + 2] = SmoothingGroupVertexNormals[SmoothingGroup][Sample->Indices[FaceOffset + 2]];
		Sample->Normals[FaceOffset + 2].Normalize();
	}
}

void AbcImporterUtilities::ComputeTangents(FAbcMeshSample* Sample, UAbcImportSettings* ImportSettings, IMeshUtilities& MeshUtilities)
{
	uint32 TangentOptions = 0x0;
	TangentOptions |= ImportSettings->NormalGenerationSettings.bIgnoreDegenerateTriangles ? ETangentOptions::IgnoreDegenerateTriangles : 0;

	MeshUtilities.CalculateTangents(Sample->Vertices, Sample->Indices, Sample->UVs, Sample->SmoothingGroupIndices, TangentOptions, Sample->TangentX, Sample->TangentY, Sample->Normals);
}

FAbcMeshSample* AbcImporterUtilities::MergeMeshSamples(const TArray<FAbcMeshSample*>& Samples)
{
	FAbcMeshSample* MergedSample = new FAbcMeshSample();
	FMemory::Memzero(MergedSample, sizeof(FAbcMeshSample));

	for (const FAbcMeshSample* Sample : Samples)
	{
		const uint32 VertexOffset = MergedSample->Vertices.Num();
		MergedSample->Vertices.Append(Sample->Vertices);
			
		const uint32 IndicesOffset = MergedSample->Indices.Num();
		MergedSample->Indices.Append(Sample->Indices);
			
		// Remap indices
		const uint32 NumIndices = MergedSample->Indices.Num();
		for (uint32 IndiceIndex = IndicesOffset; IndiceIndex < NumIndices; ++IndiceIndex)
		{
			MergedSample->Indices[IndiceIndex] += VertexOffset;
		}

		// Vertex attributes (per index based)
		MergedSample->Normals.Append(Sample->Normals);
		MergedSample->TangentX.Append(Sample->TangentX);
		MergedSample->TangentY.Append(Sample->TangentY);
		MergedSample->UVs.Append(Sample->UVs);

		// Currently not used but will still merge
		/*MergedSample->Colours.Append(Sample->Colours);
		MergedSample->ColourIndices.Append(Sample->ColourIndices);
		MergedSample->Visibility.Append(Sample->Visibility);
		MergedSample->VisibilityIndices.Append(Sample->VisibilityIndices);*/

		const uint32 MaterialIndicesOffset = MergedSample->MaterialIndices.Num();
		const uint32 SmoothingGroupIndicesOffset = MergedSample->SmoothingGroupIndices.Num();

		ensureMsgf(MaterialIndicesOffset == SmoothingGroupIndicesOffset, TEXT("Material and smoothing group indice count should match"));

		// Per Face material and smoothing group index
		MergedSample->MaterialIndices.Append(Sample->MaterialIndices);
		MergedSample->SmoothingGroupIndices.Append(Sample->SmoothingGroupIndices);

		// Remap material and smoothing group indices
		const uint32 NumMaterialIndices = MergedSample->MaterialIndices.Num();
		for (uint32 IndiceIndex = MaterialIndicesOffset; IndiceIndex < NumMaterialIndices; ++IndiceIndex)
		{
			MergedSample->MaterialIndices[IndiceIndex] += MergedSample->NumMaterials;
			MergedSample->SmoothingGroupIndices[IndiceIndex] += MergedSample->NumSmoothingGroups;
		}

		MergedSample->NumSmoothingGroups += (Sample->NumSmoothingGroups != 0 ) ? Sample->NumSmoothingGroups : 1;
		MergedSample->NumMaterials += (Sample->NumMaterials != 0) ? Sample->NumMaterials : 1;			
	}
		
	return MergedSample;
}

FAbcMeshSample* AbcImporterUtilities::MergeMeshSamples(FAbcMeshSample* MeshSampleOne, FAbcMeshSample* MeshSampleTwo)
{
	TArray<FAbcMeshSample*> Samples;
	Samples.Add(MeshSampleOne);
	Samples.Add(MeshSampleTwo);		
	return MergeMeshSamples(Samples);
}

void AbcImporterUtilities::AppendMeshSample(FAbcMeshSample* MeshSampleOne, FAbcMeshSample* MeshSampleTwo)
{
	const uint32 VertexOffset = MeshSampleOne->Vertices.Num();
	MeshSampleOne->Vertices.Append(MeshSampleTwo->Vertices);

	const uint32 IndicesOffset = MeshSampleOne->Indices.Num();
	MeshSampleOne->Indices.Append(MeshSampleTwo->Indices);

	// Remap indices
	const uint32 NumIndices = MeshSampleOne->Indices.Num();
	for (uint32 IndiceIndex = IndicesOffset; IndiceIndex < NumIndices; ++IndiceIndex)
	{
		MeshSampleOne->Indices[IndiceIndex] += VertexOffset;
	}

	// Vertex attributes (per index based)
	MeshSampleOne->Normals.Append(MeshSampleTwo->Normals);
	MeshSampleOne->TangentX.Append(MeshSampleTwo->TangentX);
	MeshSampleOne->TangentY.Append(MeshSampleTwo->TangentY);
	MeshSampleOne->UVs.Append(MeshSampleTwo->UVs);

	// Currently not used but will still merge
	/*MeshSampleOne->Colours.Append(MeshSampleTwo->Colours);
	MeshSampleOne->ColourIndices.Append(MeshSampleTwo->ColourIndices);
	MeshSampleOne->Visibility.Append(MeshSampleTwo->Visibility);
	MeshSampleOne->VisibilityIndices.Append(MeshSampleTwo->VisibilityIndices);*/

	const uint32 MaterialIndicesOffset = MeshSampleOne->MaterialIndices.Num();
	const uint32 SmoothingGroupIndicesOffset = MeshSampleOne->SmoothingGroupIndices.Num();

	ensureMsgf(MaterialIndicesOffset == SmoothingGroupIndicesOffset, TEXT("Material and smoothing group indice count should match"));

	// Per Face material and smoothing group index
	MeshSampleOne->MaterialIndices.Append(MeshSampleTwo->MaterialIndices);
	MeshSampleOne->SmoothingGroupIndices.Append(MeshSampleTwo->SmoothingGroupIndices);

	// Remap material and smoothing group indices
	const uint32 NumMaterialIndices = MeshSampleOne->MaterialIndices.Num();
	for (uint32 IndiceIndex = MaterialIndicesOffset; IndiceIndex < NumMaterialIndices; ++IndiceIndex)
	{
		MeshSampleOne->MaterialIndices[IndiceIndex] += MeshSampleOne->NumMaterials;
		MeshSampleOne->SmoothingGroupIndices[IndiceIndex] += MeshSampleOne->NumSmoothingGroups;
	}

	MeshSampleOne->NumSmoothingGroups += (MeshSampleTwo->NumSmoothingGroups != 0) ? MeshSampleTwo->NumSmoothingGroups : 1;
	MeshSampleOne->NumMaterials += (MeshSampleTwo->NumMaterials != 0) ? MeshSampleTwo->NumMaterials : 1;
}

void AbcImporterUtilities::GetHierarchyForObject(const Alembic::Abc::IObject& Object, TDoubleLinkedList<Alembic::AbcGeom::IXform>& Hierarchy)
{
	Alembic::Abc::IObject Parent;
	Parent = Object.getParent();

	// Traverse through parents until we reach RootNode
	while (Parent.valid())
	{
		// Only if the Object is of type IXform we need to store it in the hierarchy (since we only need them for matrix animation right now)
		if (AbcImporterUtilities::IsType<Alembic::AbcGeom::IXform>(Parent.getMetaData()))
		{
			Hierarchy.AddHead(Alembic::AbcGeom::IXform(Parent, Alembic::Abc::kWrapExisting));
		}
		Parent = Parent.getParent();
	}
}

void AbcImporterUtilities::PropogateMatrixTransformationToSample(FAbcMeshSample* Sample, const FMatrix& Matrix)
{		
	for (FVector& Position : Sample->Vertices)
	{
		Position = Matrix.TransformPosition(Position);
	}

	// TODO could make this a for loop and combine the transforms
	for (FVector& Normal : Sample->Normals)
	{
		Normal = Matrix.TransformVector(Normal);
	}

	for (FVector& TangentX : Sample->TangentX)
	{
		TangentX = Matrix.TransformVector(TangentX);
	}

	for (FVector& TangentY : Sample->TangentY)
	{
		TangentY = Matrix.TransformVector(TangentY);
	}
}

// TODO restructure this to be retrieve cached transformations (computed on import) instead
FMatrix AbcImporterUtilities::GetTransformationForFrame(const FAbcPolyMeshObject& Object, const Alembic::Abc::ISampleSelector FrameSelector)
{
	check(Object.Mesh.valid());
	TDoubleLinkedList<Alembic::AbcGeom::IXform> Hierarchy;
	GetHierarchyForObject(Object.Mesh, Hierarchy);

	// This is in here for safety, normally Alembic writes out same sample count for every node
	uint32 HighestNumSamples = 0;
	TDoubleLinkedList<Alembic::AbcGeom::IXform>::TConstIterator It(Hierarchy.GetHead());
	while (It)
	{			
		const uint32 NumSamples = (*It).getSchema().getNumSamples();
		if (NumSamples > HighestNumSamples)
		{
			HighestNumSamples = NumSamples;
		}
		It++;
	}

	// If there are no samples available we push out one identity matrix
	FMatrix SampleMatrix = FMatrix::Identity;

	if (HighestNumSamples != 0)
	{
		Alembic::Abc::M44d WorldMatrix;

		// Traverse DLinkedList back to front		
		It = TDoubleLinkedList<Alembic::AbcGeom::IXform>::TConstIterator(Hierarchy.GetTail());

		while (It)
		{
			// Get schema from parent object
			Alembic::AbcGeom::XformSample sample;
			Alembic::AbcGeom::IXformSchema Schema = (*It).getSchema();
			Schema.get(sample, FrameSelector);

			// Get matrix and concatenate
			Alembic::Abc::M44d Matrix = sample.getMatrix();
			WorldMatrix *= Matrix;

			It--;
		}

		// Store Sample data and time
		SampleMatrix = AbcImporterUtilities::ConvertAlembicMatrix(WorldMatrix);
	}

	return SampleMatrix;
}

void AbcImporterUtilities::CalculateAverageFrameData(const TSharedPtr<FAbcPolyMeshObject>& MeshObject, TArray<FVector>& AverageVertexData, TArray<FVector>& AverageNormalData, float& OutMinSampleTime, float& OutMaxSampleTime)
{
	const uint32 FrameZeroIndex = 0;
	const uint32 NumVertices = MeshObject->MeshSamples[FrameZeroIndex]->Vertices.Num();
	const uint32 NumIndices = MeshObject->MeshSamples[FrameZeroIndex]->Indices.Num();

	// Determine offset for vertices and normals
	const int32 VertexOffset = AverageVertexData.Num();
	const int32 NormalsOffset = AverageNormalData.Num();

	// Add new data for this mesh object
	AverageVertexData.AddZeroed(NumVertices);
	AverageNormalData.AddZeroed(NumIndices);

	for (const FAbcMeshSample* MeshSample : MeshObject->MeshSamples)
	{
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			AverageVertexData[VertexOffset + VertexIndex] += MeshSample->Vertices[VertexIndex];
		}

		for (uint32 NormalIndex = 0; NormalIndex < NumIndices; ++NormalIndex)
		{
			AverageNormalData[NormalsOffset + NormalIndex] += MeshSample->Normals[NormalIndex];
		}

		OutMinSampleTime = FMath::Min(OutMinSampleTime, MeshSample->SampleTime);
		OutMaxSampleTime = FMath::Max(OutMaxSampleTime, MeshSample->SampleTime);
	}

	const float OneOverNumSamples = 1.0f / (float)MeshObject->MeshSamples.Num();

	for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		AverageVertexData[VertexOffset + VertexIndex] *= OneOverNumSamples;
	}

	for (uint32 NormalIndex = 0; NormalIndex < NumIndices; ++NormalIndex)
	{
		//AverageNormalData[NormalsOffset + NormalIndex] *= OneOverNumSamples;
		AverageNormalData[NormalsOffset + NormalIndex].Normalize();
	}
}

void AbcImporterUtilities::GenerateDeltaFrameDataMatrix(const TSharedPtr<FAbcPolyMeshObject>& MeshObject, TArray<FVector>& AverageVertexData, TArray<float>& OutGeneratedMatrix)
{
	checkf(MeshObject->MeshSamples[0]->Vertices.Num() == AverageVertexData.Num(), TEXT("Incorrect mesh object with average vertex data array length"));
	const uint32 NumVertices = AverageVertexData.Num();
	// Expanding to number of matrix rows (one for each vector component)
	const uint32 NumMatrixRows = NumVertices * 3;
	const int32 NumSamples = MeshObject->MeshSamples.Num();

	OutGeneratedMatrix.AddZeroed(NumMatrixRows * NumSamples);

	ParallelFor(NumSamples, [&](int32 SampleIndex)
	{
		const FAbcMeshSample* MeshSample = MeshObject->MeshSamples[SampleIndex];
		const int32 SampleOffset = (SampleIndex * NumMatrixRows);
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			const int32 ComponentIndexOffset = VertexIndex * 3;
			const FVector AverageDifference = AverageVertexData[VertexIndex] - MeshSample->Vertices[VertexIndex];
			OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 0] = AverageDifference.X;
			OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 1] = AverageDifference.Y;
			OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 2] = AverageDifference.Z;
		}
	});
}

void AbcImporterUtilities::GenerateDeltaFrameDataMatrix(const TArray<FVector>& FrameVertexData, TArray<FVector>& AverageVertexData, const int32 SampleOffset, const int32 AverageVertexOffset, TArray<float>& OutGeneratedMatrix)
{
	const uint32 NumVertices = FrameVertexData.Num();
	for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const int32 ComponentIndexOffset = (VertexIndex + AverageVertexOffset) * 3;
		const FVector AverageDifference = AverageVertexData[VertexIndex + AverageVertexOffset] - FrameVertexData[VertexIndex];
		OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 0] = AverageDifference.X;
		OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 1] = AverageDifference.Y;
		OutGeneratedMatrix[SampleOffset + ComponentIndexOffset + 2] = AverageDifference.Z;
	}
}

void AbcImporterUtilities::GenerateCompressedMeshData(FCompressedAbcData& CompressedData, const uint32 NumUsedSingularValues, const uint32 NumSamples, const TArray<float>& BasesMatrix, const TArray<float>& BasesWeights, const float SampleTimeStep, const float StartTime)
{
	// Allocate base sample data	
	CompressedData.BaseSamples.AddZeroed(NumUsedSingularValues);
	CompressedData.CurveValues.AddZeroed(NumUsedSingularValues);
	CompressedData.TimeValues.AddZeroed(NumUsedSingularValues);

	// Generate the bases data and weights
	for (uint32 BaseIndex = 0; BaseIndex < NumUsedSingularValues; ++BaseIndex)
	{
		FAbcMeshSample* Base = new FAbcMeshSample(*CompressedData.AverageSample);

		const uint32 NumVertices = Base->Vertices.Num();
		const uint32 NumMatrixRows = NumVertices * 3;
		const int32 BaseOffset = BaseIndex * NumMatrixRows;
		for (uint32 Index = 0; Index < NumVertices; ++Index)
		{
			const int32 IndexOffset = BaseOffset + (Index * 3);
			FVector& BaseVertex = Base->Vertices[Index];

			BaseVertex.X -= BasesMatrix[IndexOffset + 0];
			BaseVertex.Y -= BasesMatrix[IndexOffset + 1];
			BaseVertex.Z -= BasesMatrix[IndexOffset + 2];
		}
		CompressedData.BaseSamples[BaseIndex] = Base;

		TArray<float>& CurveValues = CompressedData.CurveValues[BaseIndex];
		TArray<float>& TimeValues = CompressedData.TimeValues[BaseIndex];

		CurveValues.Reserve(NumSamples);
		TimeValues.Reserve(NumSamples);

		// Use original number of singular values to index into the array (otherwise we would be reading incorrect data if NumUsedSingularValues != the original number
		const uint32 OriginalNumberOfSingularValues = BasesWeights.Num() / NumSamples;
		// Should be possible to rearrange the data so this can become a memcpy
		for (uint32 CurveSampleIndex = 0; CurveSampleIndex < NumSamples; ++CurveSampleIndex)
		{
			CurveValues.Add(BasesWeights[BaseIndex + (OriginalNumberOfSingularValues* CurveSampleIndex)]);
			TimeValues.Add(StartTime + (SampleTimeStep * CurveSampleIndex));
		}
	}
}

void AbcImporterUtilities::AppendMaterialNames(const TSharedPtr<FAbcPolyMeshObject>& MeshObject, FCompressedAbcData& CompressedData)
{
	// Add material names from this mesh object
	if (MeshObject->FaceSetNames.Num() > 0)
	{
		CompressedData.MaterialNames.Append(MeshObject->FaceSetNames);
	}
	else
	{
		static const FString DefaultName("NoFaceSetName");
		CompressedData.MaterialNames.Add(DefaultName);
	}
}

void AbcImporterUtilities::CalculateNewStartAndEndFrameIndices(const float FrameStepRatio, uint32& InOutStartFrameIndex, uint32& InOutEndFrameIndex )
{
	// Using the calculated ratio we recompute the start/end frame indices
	InOutStartFrameIndex = FMath::Max(FMath::FloorToInt(InOutStartFrameIndex * FrameStepRatio), 0);
	InOutEndFrameIndex = FMath::CeilToInt(InOutEndFrameIndex * FrameStepRatio);
}

bool AbcImporterUtilities::AreVerticesEqual(const FSoftSkinVertex& V1, const FSoftSkinVertex& V2)
{
	if (FMath::Abs(V1.Position.X - V2.Position.X) > THRESH_POINTS_ARE_SAME
		|| FMath::Abs(V1.Position.Y - V2.Position.Y) > THRESH_POINTS_ARE_SAME
		|| FMath::Abs(V1.Position.Z - V2.Position.Z) > THRESH_POINTS_ARE_SAME)
	{
		return false;
	}

	for (int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx)
	{
		if (FMath::Abs(V1.UVs[UVIdx].X - V2.UVs[UVIdx].X) >(1.0f / 1024.0f))
			return false;

		if (FMath::Abs(V1.UVs[UVIdx].Y - V2.UVs[UVIdx].Y) > (1.0f / 1024.0f))
			return false;
	}

	FVector N1, N2;
	N1 = V1.TangentX;
	N2 = V2.TangentX;

	if (FMath::Abs(N1.X - N2.X) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Y - N2.Y) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Z - N2.Z) > THRESH_NORMALS_ARE_SAME)
	{
		return false;
	}

	N1 = V1.TangentY;
	N2 = V2.TangentY;

	if (FMath::Abs(N1.X - N2.X) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Y - N2.Y) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Z - N2.Z) > THRESH_NORMALS_ARE_SAME)
	{
		return false;
	}

	N1 = V1.TangentZ;
	N2 = V2.TangentZ;

	if (FMath::Abs(N1.X - N2.X) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Y - N2.Y) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Z - N2.Z) > THRESH_NORMALS_ARE_SAME)
	{
		return false;
	}

	return true;
}

void AbcImporterUtilities::ApplyConversion(FAbcMeshSample* InOutSample, const FAbcConversionSettings& InConversionSettings)
{
	if ( InConversionSettings.bFlipV || InConversionSettings.bFlipU )
	{
		// Apply UV matrix to flip channels
		FMatrix2x2 UVMatrix( FScale2D(InConversionSettings.bFlipU ? -1.0f : 1.0f, InConversionSettings.bFlipV ? -1.0f : 1.0f) );
		FVector2D UVOffset(InConversionSettings.bFlipU ? 1.0f : 0.0f, InConversionSettings.bFlipV ? 1.0f : 0.0f);
		for (FVector2D& UV : InOutSample->UVs)
		{
			UV = UVOffset + UVMatrix.TransformPoint(UV);
		}
	}
	
	// Calculate conversion matrix	
	const FMatrix Matrix = FScaleMatrix::Make(InConversionSettings.Scale) * FRotationMatrix::Make(FQuat::MakeFromEuler(InConversionSettings.Rotation));
	if (!Matrix.Equals(FMatrix::Identity))
	{
		// In case of negative determinant (e.g. negative scaling), invert the indice data
		if (Matrix.Determinant() < 0.0f)
		{
			Algo::Reverse(InOutSample->Indices);
			Algo::Reverse(InOutSample->Normals);
			Algo::Reverse(InOutSample->TangentX);
			Algo::Reverse(InOutSample->TangentY);
			Algo::Reverse(InOutSample->UVs);
			Algo::Reverse(InOutSample->MaterialIndices);
			Algo::Reverse(InOutSample->SmoothingGroupIndices);
		}
	}
}

void AbcImporterUtilities::ApplyConversion(TArray<FMatrix>& InOutMatrices, const FAbcConversionSettings& InConversionSettings)
{
	// Calculate conversion matrix	
	const FMatrix ConversionMatrix = FScaleMatrix::Make(InConversionSettings.Scale) * FRotationMatrix::Make(FQuat::MakeFromEuler(InConversionSettings.Rotation));
	if (!ConversionMatrix.Equals(FMatrix::Identity))
	{
		for (FMatrix& SampleMatrix : InOutMatrices)
		{			
			SampleMatrix = SampleMatrix * ConversionMatrix;
		}
	}
}

#undef LOCTEXT_NAMESPACE // "AbcImporterUtilities"
