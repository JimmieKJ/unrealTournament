// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "RawMesh.h"
#include "MeshUtilities.h"

#include "MeshSimplify.h"

class FQuadricSimplifierMeshReductionModule : public IMeshReductionModule
{
public:
	virtual ~FQuadricSimplifierMeshReductionModule() {}

	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshReductionModule interface.
	virtual class IMeshReduction* GetMeshReductionInterface() override;
	virtual class IMeshMerging* GetMeshMergingInterface() override;
};


DEFINE_LOG_CATEGORY_STATIC(LogQuadricSimplifier, Log, All);
IMPLEMENT_MODULE(FQuadricSimplifierMeshReductionModule, MeshSimplify);

template< uint32 NumTexCoords >
class TVertSimp
{
	typedef TVertSimp< NumTexCoords > VertType;
public:
	FVector			Position;
	FVector			Normal;
	FVector			Tangents[2];
	FVector2D		TexCoords[ NumTexCoords ];

	FVector&		GetPos()				{ return Position; }
	const FVector&	GetPos() const			{ return Position; }
	float*			GetAttributes()			{ return (float*)&Normal; }
	const float*	GetAttributes() const	{ return (const float*)&Normal; }

	void			Correct()
	{
		Normal.Normalize();
		Tangents[0] = Tangents[0] - ( Tangents[0] * Normal ) * Normal;
		Tangents[1] = Tangents[1] - ( Tangents[1] * Normal ) * Normal;
		Tangents[0].Normalize();
		Tangents[1].Normalize();
	}

	bool		operator==(	const VertType& a ) const
	{
		return	Position == a.Position &&
				Normal == a.Normal &&
				Tangents[0] == a.Tangents[0] &&
				Tangents[1] == a.Tangents[1] &&
				TexCoords == a.TexCoords;
	}

	VertType	operator+( const VertType& a ) const
	{
		VertType v;
		v.Position		= Position + a.Position;
		v.Normal		= Normal + a.Normal;
		v.Tangents[0]	= Tangents[0] + a.Tangents[0];
		v.Tangents[1]	= Tangents[1] + a.Tangents[1];

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] + a.TexCoords[i];
		}
		return v;
	}

	VertType	operator-( const VertType& a ) const
	{
		VertType v;
		v.Position		= Position - a.Position;
		v.Normal		= Normal - a.Normal;
		v.Tangents[0]	= Tangents[0] - a.Tangents[0];
		v.Tangents[1]	= Tangents[1] - a.Tangents[1];
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] - a.TexCoords[i];
		}
		return v;
	}

	VertType	operator*( const float a ) const
	{
		VertType v;
		v.Position		= Position * a;
		v.Normal		= Normal * a;
		v.Tangents[0]	= Tangents[0] * a;
		v.Tangents[1]	= Tangents[1] * a;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] * a;
		}
		return v;
	}

	VertType	operator/( const float a ) const
	{
		float ia = 1.0f / a;
		return (*this) * ia;
	}
};

class FQuadricSimplifierMeshReduction : public IMeshReduction
{
public:
	virtual const FString& GetVersionString() const override
	{
		static FString Version = TEXT("1.0");
		return Version;
	}

	virtual void Reduce(
		FRawMesh& OutReducedMesh,
		float& OutMaxDeviation,
		const FRawMesh& InMesh,
		const FMeshReductionSettings& InSettings
		) override
	{
#if 0
		TMap<int32,int32> FinalVerts;
		TArray<int32> DupVerts;
		int32 NumFaces = InMesh.WedgeIndices.Num() / 3;

		// Process each face, build vertex buffer and per-section index buffers.
		for(int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			int32 VertexIndices[3];
			FVector CornerPositions[3];

			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerPositions[CornerIndex] = InMesh.VertexPositions[ InMesh.WedgeIndices[ FaceIndex * 3 + CornerIndex ] ];
			}

			// Don't process degenerate triangles.
			if( PointsEqual(CornerPositions[0],CornerPositions[1], ComparisonThreshold) ||
				PointsEqual(CornerPositions[1],CornerPositions[2], ComparisonThreshold) ||
				PointsEqual(CornerPositions[2],CornerPositions[0], ComparisonThreshold) )
			{
				continue;
			}

			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				int32 WedgeIndex = FaceIndex * 3 + CornerIndex;
				FStaticMeshBuildVertex ThisVertex = BuildStaticMeshVertex(InMesh, WedgeIndex);

				int32 Index = INDEX_NONE;
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					if (DupVerts[k] >= WedgeIndex)
					{
						// the verts beyond me haven't been placed yet, so these duplicates are not relevant
						break;
					}

					int32 *Location = FinalVerts.Find(DupVerts[k]);
					if (Location != NULL
						&& AreVerticesEqual(ThisVertex, OutVertices[*Location], ComparisonThreshold))
					{
						Index = *Location;
						break;
					}
				}
				if (Index == INDEX_NONE)
				{
					Index = OutVertices.Add(ThisVertex);
					FinalVerts.Add(WedgeIndex,Index);
				}
				VertexIndices[CornerIndex] = Index;
			}

			// Reject degenerate triangles.
			if( VertexIndices[0] == VertexIndices[1] ||
				VertexIndices[1] == VertexIndices[2] ||
				VertexIndices[2] == VertexIndices[0] )
			{
				continue;
			}

			// Put the indices in the material index buffer.
			int32 SectionIndex = FMath::Clamp(RawMesh.FaceMaterialIndices[FaceIndex], 0, OutPerSectionIndices.Num());
			TArray<uint32>& SectionIndices = OutPerSectionIndices[SectionIndex];
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				SectionIndices.Add(VertexIndices[CornerIndex]);
			}
		}
#endif




		uint32 NumVerts = 0;
		uint32 NumIndexes = 0;

		TVertSimp<1>* Verts = new TVertSimp<1>[ NumVerts ];


#if 0
		int32 NumVertices = InMesh.VertexPositions.Num();
		int32 NumWedges = InMesh.WedgeIndices.Num();
		int32 NumTris = NumWedges / 3;

		if (NumWedges == 0)
		{
			return NULL;
		}

		for(int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
		{
			if (RawMesh.WedgeTexCoords[TexCoordIndex].Num() == NumWedges)
			{
				GeometryData->AddTexCoords(TexCoordIndex);
				SimplygonSDK::spRealArray TexCoords = GeometryData->GetTexCoords(TexCoordIndex);
				check(TexCoords->GetTupleSize() == 2);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					TexCoords->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex]);
				}
			}
		}

		if (RawMesh.WedgeColors.Num() == NumWedges)
		{
			GeometryData->AddColors(0);
			SimplygonSDK::spRealArray LinearColors = GeometryData->GetColors(0);
			check(LinearColors);
			check(LinearColors->GetTupleSize() == 4);
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				FLinearColor LinearColor(RawMesh.WedgeColors[WedgeIndex]);
				LinearColors->SetTuple(WedgeIndex, (float*)&LinearColor);
			}
		}

		if (InMesh.WedgeTangentZ.Num() == NumWedges)
		{
			if (InMesh.WedgeTangentX.Num() == NumWedges && InMesh.WedgeTangentY.Num() == NumWedges)
			{
				GeometryData->AddTangents(0);
				SimplygonSDK::spRealArray Tangents = GeometryData->GetTangents(0);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Tangents->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentX[WedgeIndex]);
				}

				GeometryData->AddBitangents(0);
				SimplygonSDK::spRealArray Bitangents = GeometryData->GetBitangents(0);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Bitangents->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentY[WedgeIndex]);
				}
			}
			GeometryData->AddNormals();
			SimplygonSDK::spRealArray Normals = GeometryData->GetNormals();
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				Normals->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentZ[WedgeIndex]);
			}
		}

		// Per-triangle data.
		/*for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		{
			MaterialIndices->SetItem(TriIndex, RawMesh.FaceMaterialIndices[TriIndex]);
			GroupIds->SetItem(TriIndex, RawMesh.FaceSmoothingMasks[TriIndex]);
		}*/

		for( uint32 i = 0; i < NumVerts; i++ )
		{
			Verts[i].Position		= InMesh.VertexPositions[i];
			Verts[i].TexCoords[0]	= tri->verts[i].st;
			Verts[i].Normal			= tri->verts[i].normal;
			Verts[i].Tangents[0]	= tri->verts[i].tangents[0];
			Verts[i].Tangents[1]	= tri->verts[i].tangents[1];
		}

		for( uint32 i = 0; i < NumVerts; i++ )
		{
			Verts[i].Position.x = 0.0625f * FMath::RoundToFloat( 16.0f * Verts[i].Position.X );
			Verts[i].Position.y = 0.0625f * FMath::RoundToFloat( 16.0f * Verts[i].Position.Y );
			Verts[i].Position.z = 0.0625f * FMath::RoundToFloat( 16.0f * Verts[i].Position.Z );
		}

		for( uint32 i = 0; i < NumVerts; i++ )
		{
			Verts[i].Normal			= InMesh.WedgeTangentZ[i];
			Verts[i].Tangents[0]	= tri->verts[i].tangents[0];
			Verts[i].Tangents[1]	= tri->verts[i].tangents[1];
		}
#endif

		uint32* Indexes = new uint32[ NumIndexes ];

#if 0
		for (uint32 i = 0; i < NumIndexes; i++)
		{
			 Indexes[i] = InMesh.WedgeIndices[i];
		}
#endif

		const uint32 NumAttributes = ( sizeof( TVertSimp<1> ) - sizeof( FVector ) ) / sizeof(float);
		TMeshSimplifier< TVertSimp<1>, NumAttributes >* MeshSimp = new TMeshSimplifier< TVertSimp<1>, NumAttributes >( Verts, NumVerts, Indexes, NumIndexes );

		const float AttributeWeights[] =
		{
			16.0f, 16.0f, 16.0f,	// Normal
			0.1f, 0.1f, 0.1f,		// Tangent[0]
			0.1f, 0.1f, 0.1f,		// Tangent[1]
			0.5f, 0.5f,				// TexCoord[0]
			0.5f, 0.5f,				// TexCoord[1]
			0.5f, 0.5f,				// TexCoord[2]
			0.5f, 0.5f				// TexCoord[3]
		};

		MeshSimp->SetAttributeWeights( AttributeWeights );
		MeshSimp->SetBoundaryLocked();
		MeshSimp->InitCosts();

		int32 NumTris = NumIndexes / 3;

		MeshSimp->SimplifyMesh( 200000.0f, NumTris * InSettings.PercentTriangles );

		NumVerts = MeshSimp->GetNumVerts();
		NumIndexes = MeshSimp->GetNumTris() * 3;

		MeshSimp->OutputMesh( Verts, Indexes );
		delete MeshSimp;

		delete[] Verts;
		delete[] Indexes;

	//	CreateRawMeshFromGeometry(OutReducedMesh, GeometryData, WINDING_Keep);
	//	OutMaxDeviation = ReductionProcessor->GetMaxDeviation();
	}

	virtual bool ReduceSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		int32 LODIndex,
		const FSkeletalMeshOptimizationSettings& Settings,
		bool bCalcLODDistance
		) override
	{
		return false;
	}

	virtual bool IsSupported() const override
	{
		return true;
	}

	virtual ~FQuadricSimplifierMeshReduction() {}

	static FQuadricSimplifierMeshReduction* Create()
	{
		return new FQuadricSimplifierMeshReduction;
	}

private:
#if 0
	SimplygonSDK::spGeometryData CreateGeometryFromRawMesh(const FRawMesh& RawMesh)
	{
		int32 NumVertices = RawMesh.VertexPositions.Num();
		int32 NumWedges = RawMesh.WedgeIndices.Num();
		int32 NumTris = NumWedges / 3;

		if (NumWedges == 0)
		{
			return NULL;
		}

		SimplygonSDK::spGeometryData GeometryData = SDK->CreateGeometryData();
		GeometryData->SetVertexCount(NumVertices);
		GeometryData->SetTriangleCount(NumTris);

		SimplygonSDK::spRealArray Positions = GeometryData->GetCoords();
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			Positions->SetTuple(VertexIndex, (float*)&RawMesh.VertexPositions[VertexIndex]);
		}

		SimplygonSDK::spRidArray Indices = GeometryData->GetVertexIds();
		for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
		{
			Indices->SetItem(WedgeIndex, RawMesh.WedgeIndices[WedgeIndex]);
		}

		for(int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
		{
			if (RawMesh.WedgeTexCoords[TexCoordIndex].Num() == NumWedges)
			{
				GeometryData->AddTexCoords(TexCoordIndex);
				SimplygonSDK::spRealArray TexCoords = GeometryData->GetTexCoords(TexCoordIndex);
				check(TexCoords->GetTupleSize() == 2);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					TexCoords->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex]);
				}
			}
		}

		if (RawMesh.WedgeColors.Num() == NumWedges)
		{
			GeometryData->AddColors(0);
			SimplygonSDK::spRealArray LinearColors = GeometryData->GetColors(0);
			check(LinearColors);
			check(LinearColors->GetTupleSize() == 4);
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				FLinearColor LinearColor(RawMesh.WedgeColors[WedgeIndex]);
				LinearColors->SetTuple(WedgeIndex, (float*)&LinearColor);
			}
		}

		if (RawMesh.WedgeTangentZ.Num() == NumWedges)
		{
			if (RawMesh.WedgeTangentX.Num() == NumWedges && RawMesh.WedgeTangentY.Num() == NumWedges)
			{
				GeometryData->AddTangents(0);
				SimplygonSDK::spRealArray Tangents = GeometryData->GetTangents(0);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Tangents->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentX[WedgeIndex]);
				}

				GeometryData->AddBitangents(0);
				SimplygonSDK::spRealArray Bitangents = GeometryData->GetBitangents(0);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Bitangents->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentY[WedgeIndex]);
				}
			}
			GeometryData->AddNormals();
			SimplygonSDK::spRealArray Normals = GeometryData->GetNormals();
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				Normals->SetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentZ[WedgeIndex]);
			}
		}

		// Per-triangle data.
		GeometryData->AddMaterialIds();
		SimplygonSDK::spRidArray MaterialIndices = GeometryData->GetMaterialIds();
		for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		{
			MaterialIndices->SetItem(TriIndex, RawMesh.FaceMaterialIndices[TriIndex]);
		}

		GeometryData->AddGroupIds();
		SimplygonSDK::spRidArray GroupIds = GeometryData->GetGroupIds();
		for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		{
			GroupIds->SetItem(TriIndex, RawMesh.FaceSmoothingMasks[TriIndex]);
		}
		
		return GeometryData;
	}

	void CreateRawMeshFromGeometry(FRawMesh& OutRawMesh, const SimplygonSDK::spGeometryData& GeometryData, EWindingMode WindingMode)
	{
		check(GeometryData);

		SimplygonSDK::spRealArray Positions = GeometryData->GetCoords();
		SimplygonSDK::spRidArray Indices = GeometryData->GetVertexIds();
		SimplygonSDK::spRidArray MaterialIndices = GeometryData->GetMaterialIds();
		SimplygonSDK::spRidArray GroupIds = GeometryData->GetGroupIds();
		SimplygonSDK::spRealArray LinearColors = GeometryData->GetColors(0);
		SimplygonSDK::spRealArray Normals = GeometryData->GetNormals();
		SimplygonSDK::spRealArray Tangents = GeometryData->GetTangents(0);
		SimplygonSDK::spRealArray Bitangents = GeometryData->GetBitangents(0);

		check(Positions);
		check(Indices);

		FRawMesh& RawMesh = OutRawMesh;
		const bool bReverseWinding = (WindingMode == WINDING_Reverse);
		int32 NumTris = GeometryData->GetTriangleCount();
		int32 NumWedges = NumTris * 3;
		int32 NumVertices = GeometryData->GetVertexCount();

		RawMesh.VertexPositions.Empty(NumVertices);
		RawMesh.VertexPositions.AddUninitialized(NumVertices);
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			Positions->GetTuple(VertexIndex, (float*)&RawMesh.VertexPositions[VertexIndex]);
		}

		RawMesh.WedgeIndices.Empty(NumWedges);
		RawMesh.WedgeIndices.AddUninitialized(NumWedges);
		for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const uint32 DestIndex = bReverseWinding ? (2 - CornerIndex) : CornerIndex;
				RawMesh.WedgeIndices[TriIndex * 3 + DestIndex] = Indices->GetItem(TriIndex * 3 + CornerIndex);
			}
		}

		for(int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
		{
			SimplygonSDK::spRealArray TexCoords = GeometryData->GetTexCoords(TexCoordIndex);
			if (TexCoords)
			{
				RawMesh.WedgeTexCoords[TexCoordIndex].Empty(NumWedges);
				RawMesh.WedgeTexCoords[TexCoordIndex].AddUninitialized(NumWedges);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					TexCoords->GetTuple(WedgeIndex, (float*)&RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex]);
				}
			}
		}

		if (LinearColors)
		{
			RawMesh.WedgeColors.Empty(NumWedges);
			RawMesh.WedgeColors.AddUninitialized(NumWedges);
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				FLinearColor LinearColor;
				LinearColors->GetTuple(WedgeIndex, (float*)&LinearColor);
				RawMesh.WedgeColors[WedgeIndex] = LinearColor.ToFColor(true);
			}
		}

		if (Normals)
		{
			if (Tangents && Bitangents)
			{
				RawMesh.WedgeTangentX.Empty(NumWedges);
				RawMesh.WedgeTangentX.AddUninitialized(NumWedges);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Tangents->GetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentX[WedgeIndex]);
				}

				RawMesh.WedgeTangentY.Empty(NumWedges);
				RawMesh.WedgeTangentY.AddUninitialized(NumWedges);
				for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				{
					Bitangents->GetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentY[WedgeIndex]);
				}
			}

			RawMesh.WedgeTangentZ.Empty(NumWedges);
			RawMesh.WedgeTangentZ.AddUninitialized(NumWedges);
			for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			{
				Normals->GetTuple(WedgeIndex, (float*)&RawMesh.WedgeTangentZ[WedgeIndex]);
				if (!bReverseWinding)
				{
					RawMesh.WedgeTangentZ[WedgeIndex] = (-RawMesh.WedgeTangentZ[WedgeIndex]).GetSafeNormal();
				}
			}
		}

		RawMesh.FaceMaterialIndices.Empty(NumTris);
		RawMesh.FaceMaterialIndices.AddZeroed(NumTris);
		if (MaterialIndices)
		{
			for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
			{
				RawMesh.FaceMaterialIndices[TriIndex] = MaterialIndices->GetItem(TriIndex);
			}
		}

		RawMesh.FaceSmoothingMasks.Empty(NumTris);
		RawMesh.FaceSmoothingMasks.AddZeroed(NumTris);
		if (GroupIds)
		{
			for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
			{
				RawMesh.FaceSmoothingMasks[TriIndex] = GroupIds->GetItem(TriIndex);
			}
		}
	}

	void SetReductionSettings(SimplygonSDK::spReductionSettings ReductionSettings, const FMeshReductionSettings& Settings, int32 NumTris)
	{
		float MaxDeviation = Settings.MaxDeviation > 0.0f ? Settings.MaxDeviation : FLT_MAX;
		float MinReductionRatio = FMath::Max<float>(1.0f / NumTris, 0.05f);
		float MaxReductionRatio = (Settings.MaxDeviation > 0.0f && Settings.PercentTriangles == 1.0f) ? MinReductionRatio : 1.0f;
		float ReductionRatio = FMath::Clamp(Settings.PercentTriangles, MinReductionRatio, MaxReductionRatio);

		unsigned int FeatureFlagsMask = SimplygonSDK::SG_FEATUREFLAGS_GROUP;
		if (Settings.TextureImportance != EMeshFeatureImportance::Off)
		{
			FeatureFlagsMask |= (SimplygonSDK::SG_FEATUREFLAGS_TEXTURE0 | SimplygonSDK::SG_FEATUREFLAGS_MATERIAL);
		}
		if (Settings.ShadingImportance != EMeshFeatureImportance::Off)
		{
			FeatureFlagsMask |= SimplygonSDK::SG_FEATUREFLAGS_SHADING;
		}

		const float ImportanceTable[] =
		{
			0.0f,	// OFF
			0.125f,	// Lowest
			0.35f,	// Low,
			1.0f,	// Normal
			2.8f,	// High
			8.0f,	// Highest
		};

		static_assert(ARRAY_COUNT(ImportanceTable) == (EMeshFeatureImportance::Highest + 1), "Importance table size mismatch."); // -1 because of TEMP_BROKEN(?)
		check(Settings.SilhouetteImportance < EMeshFeatureImportance::Highest+1); // -1 because of TEMP_BROKEN(?)
		check(Settings.TextureImportance < EMeshFeatureImportance::Highest+1); // -1 because of TEMP_BROKEN(?)
		check(Settings.ShadingImportance < EMeshFeatureImportance::Highest+1); // -1 because of TEMP_BROKEN(?)

		ReductionSettings->SetFeatureFlags(FeatureFlagsMask);
		ReductionSettings->SetMaxDeviation(MaxDeviation);
		ReductionSettings->SetReductionRatio(ReductionRatio);
		ReductionSettings->SetGeometryImportance(ImportanceTable[Settings.SilhouetteImportance]);
		ReductionSettings->SetTextureImportance(ImportanceTable[Settings.TextureImportance]);
		ReductionSettings->SetMaterialImportance(ImportanceTable[Settings.TextureImportance]);
		ReductionSettings->SetShadingImportance( ImportanceTable[Settings.ShadingImportance]);
	}
#endif

	/**
	 * Calculates the view distance that a mesh should be displayed at.
	 * @param MaxDeviation - The maximum surface-deviation between the reduced geometry and the original. This value should be acquired from Simplygon
	 * @returns The calculated view distance	 
	 */
	float CalculateViewDistance( float MaxDeviation )
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

		const float ViewDistance = (MaxDeviation * 960.0f);
		return ViewDistance;
	}
};
TScopedPointer<FQuadricSimplifierMeshReduction> GQuadricSimplifierMeshReduction;

void FQuadricSimplifierMeshReductionModule::StartupModule()
{
	GQuadricSimplifierMeshReduction = FQuadricSimplifierMeshReduction::Create();
}

void FQuadricSimplifierMeshReductionModule::ShutdownModule()
{
	GQuadricSimplifierMeshReduction = NULL;
}

IMeshReduction* FQuadricSimplifierMeshReductionModule::GetMeshReductionInterface()
{
	return GQuadricSimplifierMeshReduction;
}

IMeshMerging* FQuadricSimplifierMeshReductionModule::GetMeshMergingInterface()
{
	return NULL;
}
