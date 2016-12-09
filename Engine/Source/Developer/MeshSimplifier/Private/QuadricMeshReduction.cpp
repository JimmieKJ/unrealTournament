// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/ScopedPointer.h"
#include "MeshUtilities.h"
#include "MeshBuild.h"
#include "RawMesh.h"
#include "MeshSimplify.h"
#include "UniquePtr.h"

class FQuadricSimplifierMeshReductionModule : public IMeshReductionModule
{
public:
	virtual ~FQuadricSimplifierMeshReductionModule() {}

	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshReductionModule interface.
	virtual class IMeshReduction* GetStaticMeshReductionInterface() override;
	virtual class IMeshReduction* GetSkeletalMeshReductionInterface() override;
	virtual class IMeshMerging* GetMeshMergingInterface() override;
};


DEFINE_LOG_CATEGORY_STATIC(LogQuadricSimplifier, Log, All);
IMPLEMENT_MODULE(FQuadricSimplifierMeshReductionModule, QuadricMeshReduction);

template< uint32 NumTexCoords >
class TVertSimp
{
	typedef TVertSimp< NumTexCoords > VertType;
public:
	uint32			MaterialIndex;
	FVector			Position;
	FVector			Normal;
	FVector			Tangents[2];
	FLinearColor	Color;
	FVector2D		TexCoords[ NumTexCoords ];

	uint32			GetMaterialIndex() const	{ return MaterialIndex; }
	FVector&		GetPos()					{ return Position; }
	const FVector&	GetPos() const				{ return Position; }
	float*			GetAttributes()				{ return (float*)&Normal; }
	const float*	GetAttributes() const		{ return (const float*)&Normal; }

	void			Correct()
	{
		Normal.Normalize();
		Tangents[0] = Tangents[0] - ( Tangents[0] * Normal ) * Normal;
		Tangents[1] = Tangents[1] - ( Tangents[1] * Normal ) * Normal;
		Tangents[0].Normalize();
		Tangents[1].Normalize();
		Color = Color.GetClamped();
	}

	bool		operator==(	const VertType& a ) const
	{
		if( MaterialIndex	!= a.MaterialIndex ||
			Position		!= a.Position ||
			Normal			!= a.Normal ||
			Tangents[0]		!= a.Tangents[0] ||
			Tangents[1]		!= a.Tangents[1] ||
			Color			!= a.Color )
		{
			return false;
		}

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			if( TexCoords[i] != a.TexCoords[i] )
			{
				return false;
			}
		}
		return true;
	}

	VertType	operator+( const VertType& a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position + a.Position;
		v.Normal		= Normal + a.Normal;
		v.Tangents[0]	= Tangents[0] + a.Tangents[0];
		v.Tangents[1]	= Tangents[1] + a.Tangents[1];
		v.Color			= Color + a.Color;

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] + a.TexCoords[i];
		}
		return v;
	}

	VertType	operator-( const VertType& a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position - a.Position;
		v.Normal		= Normal - a.Normal;
		v.Tangents[0]	= Tangents[0] - a.Tangents[0];
		v.Tangents[1]	= Tangents[1] - a.Tangents[1];
		v.Color			= Color - a.Color;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] - a.TexCoords[i];
		}
		return v;
	}

	VertType	operator*( const float a ) const
	{
		VertType v;
		v.MaterialIndex	= MaterialIndex;
		v.Position		= Position * a;
		v.Normal		= Normal * a;
		v.Tangents[0]	= Tangents[0] * a;
		v.Tangents[1]	= Tangents[1] * a;
		v.Color			= Color * a;
		
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
		const TMultiMap<int32, int32>& InOverlappingCorners,
		const FMeshReductionSettings& InSettings
		) override
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		const uint32 NumTexCoords = MAX_STATIC_TEXCOORDS;

		TArray< TVertSimp< NumTexCoords > >	Verts;
		TArray< uint32 >					Indexes;

		TMap< int32, int32 > VertsMap;
		TArray<int32> DupVerts;

		int32 NumWedges = InMesh.WedgeIndices.Num();
		int32 NumFaces = NumWedges / 3;

		// Process each face, build vertex buffer and index buffer
		for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			FVector Positions[3];
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				Positions[CornerIndex] = InMesh.VertexPositions[ InMesh.WedgeIndices[ FaceIndex * 3 + CornerIndex ] ];
			}

			// Don't process degenerate triangles.
			if( PointsEqual( Positions[0], Positions[1] ) ||
				PointsEqual( Positions[0], Positions[2] ) ||
				PointsEqual( Positions[1], Positions[2] ) )
			{
				continue;
			}

			int32 VertexIndices[3];
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				int32 WedgeIndex = FaceIndex * 3 + CornerIndex;

				TVertSimp< NumTexCoords > NewVert;
				NewVert.MaterialIndex	= InMesh.FaceMaterialIndices[ FaceIndex ];
				NewVert.Position		= Positions[ CornerIndex ];
				NewVert.Tangents[0]		= InMesh.WedgeTangentX[ WedgeIndex ];
				NewVert.Tangents[1]		= InMesh.WedgeTangentY[ WedgeIndex ];
				NewVert.Normal			= InMesh.WedgeTangentZ[ WedgeIndex ];

				// Fix bad tangents
				NewVert.Tangents[0] = NewVert.Tangents[0].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[0];
				NewVert.Tangents[1] = NewVert.Tangents[1].ContainsNaN() ? FVector::ZeroVector : NewVert.Tangents[1];
				NewVert.Normal		= NewVert.Normal.ContainsNaN()		? FVector::ZeroVector : NewVert.Normal;

				if( InMesh.WedgeColors.Num() == NumWedges )
				{
					NewVert.Color = FLinearColor::FromSRGBColor( InMesh.WedgeColors[ WedgeIndex ] );
				}
				else
				{
					NewVert.Color = FLinearColor::Transparent;
				}

				for( int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
				{
					if( InMesh.WedgeTexCoords[ UVIndex ].Num() == NumWedges )
					{
						NewVert.TexCoords[ UVIndex ] = InMesh.WedgeTexCoords[ UVIndex ][ WedgeIndex ];
					}
					else
					{
						NewVert.TexCoords[ UVIndex ] = FVector2D::ZeroVector;
					}
				}


				DupVerts.Reset();
				InOverlappingCorners.MultiFind( WedgeIndex, DupVerts );
				DupVerts.Sort();

				int32 Index = INDEX_NONE;
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					if( DupVerts[k] >= WedgeIndex )
					{
						// the verts beyond me haven't been placed yet, so these duplicates are not relevant
						break;
					}

					int32* Location = VertsMap.Find( DupVerts[k] );
					if( Location )
					{
						TVertSimp< NumTexCoords >& FoundVert = Verts[ *Location ];

						if( NewVert.MaterialIndex != FoundVert.MaterialIndex ||
							!PointsEqual(  NewVert.Position,	FoundVert.Position ) ||
							!NormalsEqual( NewVert.Tangents[0],	FoundVert.Tangents[0] ) ||
							!NormalsEqual( NewVert.Tangents[1],	FoundVert.Tangents[1] ) ||
							!NormalsEqual( NewVert.Normal,		FoundVert.Normal ) ||
							NewVert.Color != FoundVert.Color )
						{
							continue;
						}

						// UVs
						bool bMatch = true;
						for( int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
						{
							if( !UVsEqual( NewVert.TexCoords[ UVIndex ], FoundVert.TexCoords[ UVIndex ] ) )
							{
								bMatch = false;
							}
						}

						if( bMatch )
						{
							Index = *Location;
							break;
						}
					}
				}
				if( Index == INDEX_NONE )
				{
					Index = Verts.Add( NewVert );
					VertsMap.Add( WedgeIndex, Index );
				}
				VertexIndices[ CornerIndex ] = Index;
			}

			// Reject degenerate triangles.
			if( VertexIndices[0] == VertexIndices[1] ||
				VertexIndices[1] == VertexIndices[2] ||
				VertexIndices[0] == VertexIndices[2] )
			{
				continue;
			}

			Indexes.Add( VertexIndices[0] );
			Indexes.Add( VertexIndices[1] );
			Indexes.Add( VertexIndices[2] );
		}

		uint32 NumVerts = Verts.Num();
		uint32 NumIndexes = Indexes.Num();
		uint32 NumTris = NumIndexes / 3;

		static_assert( NumTexCoords == 8, "NumTexCoords changed, fix AttributeWeights" );
		const uint32 NumAttributes = ( sizeof( TVertSimp< NumTexCoords > ) - sizeof( uint32 ) - sizeof( FVector ) ) / sizeof(float);
		float AttributeWeights[] =
		{
			16.0f, 16.0f, 16.0f,	// Normal
			0.1f, 0.1f, 0.1f,		// Tangent[0]
			0.1f, 0.1f, 0.1f,		// Tangent[1]
			0.1f, 0.1f, 0.1f, 0.1f,	// Color
			0.5f, 0.5f,				// TexCoord[0]
			0.5f, 0.5f,				// TexCoord[1]
			0.5f, 0.5f,				// TexCoord[2]
			0.5f, 0.5f,				// TexCoord[3]
			0.5f, 0.5f,				// TexCoord[4]
			0.5f, 0.5f,				// TexCoord[5]
			0.5f, 0.5f,				// TexCoord[6]
			0.5f, 0.5f,				// TexCoord[7]
		};
		float* ColorWeights = AttributeWeights + 3 + 3 + 3;
		float* TexCoordWeights = ColorWeights + 4;

		// Zero out weights that aren't used
		{
			if( InMesh.WedgeColors.Num() != NumWedges )
			{
				ColorWeights[0] = 0.0f;
				ColorWeights[1] = 0.0f;
				ColorWeights[2] = 0.0f;
				ColorWeights[3] = 0.0f;
			}

			for( int32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++ )
			{
				if( InMesh.WedgeTexCoords[ TexCoordIndex ].Num() != NumWedges )
				{
					TexCoordWeights[ 2 * TexCoordIndex + 0 ] = 0.0f;
					TexCoordWeights[ 2 * TexCoordIndex + 1 ] = 0.0f;
				}
			}
		}
		
		TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >* MeshSimp = new TMeshSimplifier< TVertSimp< NumTexCoords >, NumAttributes >( Verts.GetData(), NumVerts, Indexes.GetData(), NumIndexes );

		MeshSimp->SetAttributeWeights( AttributeWeights );
		//MeshSimp->SetBoundaryLocked();
		MeshSimp->InitCosts();

		float MaxErrorSqr = MeshSimp->SimplifyMesh( MAX_FLT, NumTris * InSettings.PercentTriangles );

		NumVerts = MeshSimp->GetNumVerts();
		NumTris = MeshSimp->GetNumTris();
		NumIndexes = NumTris * 3;

		MeshSimp->OutputMesh( Verts.GetData(), Indexes.GetData() );
		delete MeshSimp;

		OutMaxDeviation = FMath::Sqrt( MaxErrorSqr ) / 8.0f;

		{
			// Output FRawMesh
			OutReducedMesh.VertexPositions.Empty( NumVerts );
			OutReducedMesh.VertexPositions.AddUninitialized( NumVerts );
			for( uint32 i= 0; i < NumVerts; i++ )
			{
				OutReducedMesh.VertexPositions[i] = Verts[i].Position;
			}

			OutReducedMesh.WedgeIndices.Empty( NumIndexes );
			OutReducedMesh.WedgeIndices.AddUninitialized( NumIndexes );

			for( uint32 i = 0; i < NumIndexes; i++ )
			{
				OutReducedMesh.WedgeIndices[i] = Indexes[i];
			}

			OutReducedMesh.WedgeTangentX.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentY.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentZ.Empty( NumIndexes );
			OutReducedMesh.WedgeTangentX.AddUninitialized( NumIndexes );
			OutReducedMesh.WedgeTangentY.AddUninitialized( NumIndexes );
			OutReducedMesh.WedgeTangentZ.AddUninitialized( NumIndexes );
			for( uint32 i= 0; i < NumIndexes; i++ )
			{
				OutReducedMesh.WedgeTangentX[i] = Verts[ Indexes[i] ].Tangents[0];
				OutReducedMesh.WedgeTangentY[i] = Verts[ Indexes[i] ].Tangents[1];
				OutReducedMesh.WedgeTangentZ[i] = Verts[ Indexes[i] ].Normal;
			}

			if( InMesh.WedgeColors.Num() == NumWedges )
			{
				OutReducedMesh.WedgeColors.Empty( NumIndexes );
				OutReducedMesh.WedgeColors.AddUninitialized( NumIndexes );
				for( uint32 i= 0; i < NumIndexes; i++ )
				{
					OutReducedMesh.WedgeColors[i] = Verts[ Indexes[i] ].Color.ToFColor(true);
				}
			}
			else
			{
				OutReducedMesh.WedgeColors.Empty();
			}

			for( int32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++ )
			{
				if( InMesh.WedgeTexCoords[ TexCoordIndex ].Num() == NumWedges )
				{
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].Empty( NumIndexes );
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].AddUninitialized( NumIndexes );
					for( uint32 i= 0; i < NumIndexes; i++ )
					{
						OutReducedMesh.WedgeTexCoords[ TexCoordIndex ][i] = Verts[ Indexes[i] ].TexCoords[ TexCoordIndex ];
					}
				}
				else
				{
					OutReducedMesh.WedgeTexCoords[ TexCoordIndex ].Empty();
				}
			}

			OutReducedMesh.FaceMaterialIndices.Empty( NumTris );
			OutReducedMesh.FaceMaterialIndices.AddUninitialized( NumTris );
			for( uint32 i= 0; i < NumTris; i++ )
			{
				OutReducedMesh.FaceMaterialIndices[i] = Verts[ Indexes[3*i] ].MaterialIndex;
			}

			OutReducedMesh.FaceSmoothingMasks.Empty( NumTris );
			OutReducedMesh.FaceSmoothingMasks.AddZeroed( NumTris );

			Verts.Empty();
			Indexes.Empty();
		}
	}

	virtual bool ReduceSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		int32 LODIndex,
		const FSkeletalMeshOptimizationSettings& Settings,
		bool bCalcLODDistance,
		bool bReregisterComponent = true
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
};
TUniquePtr<FQuadricSimplifierMeshReduction> GQuadricSimplifierMeshReduction;

void FQuadricSimplifierMeshReductionModule::StartupModule()
{
	GQuadricSimplifierMeshReduction.Reset(FQuadricSimplifierMeshReduction::Create());
}

void FQuadricSimplifierMeshReductionModule::ShutdownModule()
{
	GQuadricSimplifierMeshReduction = nullptr;
}

IMeshReduction* FQuadricSimplifierMeshReductionModule::GetStaticMeshReductionInterface()
{
	return GQuadricSimplifierMeshReduction.Get();
}

IMeshReduction* FQuadricSimplifierMeshReductionModule::GetSkeletalMeshReductionInterface()
{
	return nullptr;
}

IMeshMerging* FQuadricSimplifierMeshReductionModule::GetMeshMergingInterface()
{
	return nullptr;
}
