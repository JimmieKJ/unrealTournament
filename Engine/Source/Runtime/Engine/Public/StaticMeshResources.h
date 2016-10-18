// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMesh.h: Static mesh class definition.
=============================================================================*/

#pragma once

#include "Engine/StaticMesh.h"
#include "RawIndexBuffer.h"
#include "TextureLayout3d.h"
#include "LocalVertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "Materials/MaterialInterface.h"

/**
 * The LOD settings to use for a group of static meshes.
 */
class FStaticMeshLODGroup
{
public:
	/** Default values. */
	FStaticMeshLODGroup()
		: DefaultNumLODs(1)
		, DefaultLightMapResolution(64)
		, BasePercentTrianglesMult(1.0f)
		, DisplayName( NSLOCTEXT( "UnrealEd", "None", "None" ) )
	{
		FMemory::Memzero(SettingsBias);
		SettingsBias.PercentTriangles = 1.0f;
	}

	/** Returns the default number of LODs to build. */
	int32 GetDefaultNumLODs() const
	{
		return DefaultNumLODs;
	}

	/** Returns the default lightmap resolution. */
	int32 GetDefaultLightMapResolution() const
	{
		return DefaultLightMapResolution;
	}

	/** Returns default reduction settings for the specified LOD. */
	FMeshReductionSettings GetDefaultSettings(int32 LODIndex) const
	{
		check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
		return DefaultSettings[LODIndex];
	}

	/** Applies global settings tweaks for the specified LOD. */
	ENGINE_API FMeshReductionSettings GetSettings(const FMeshReductionSettings& InSettings, int32 LODIndex) const;

private:
	/** FStaticMeshLODSettings initializes group entries. */
	friend class FStaticMeshLODSettings;
	/** The default number of LODs to build. */
	int32 DefaultNumLODs;
	/** Default lightmap resolution. */
	int32 DefaultLightMapResolution;
	/** An additional reduction of base meshes in this group. */
	float BasePercentTrianglesMult;
	/** Display name. */
	FText DisplayName;
	/** Default reduction settings for meshes in this group. */
	FMeshReductionSettings DefaultSettings[MAX_STATIC_MESH_LODS];
	/** Biases applied to reduction settings. */
	FMeshReductionSettings SettingsBias;
};

/**
 * Per-group LOD settings for static meshes.
 */
class FStaticMeshLODSettings
{
public:

	/**
	 * Initializes LOD settings by reading them from the passed in config file section.
	 * @param IniFile Preloaded ini file object to load from
	 */
	ENGINE_API void Initialize(const FConfigFile& IniFile);

	/** Retrieve the settings for the specified LOD group. */
	const FStaticMeshLODGroup& GetLODGroup(FName LODGroup) const
	{
		const FStaticMeshLODGroup* Group = Groups.Find(LODGroup);
		if (Group == NULL)
		{
			Group = Groups.Find(NAME_None);
		}
		check(Group);
		return *Group;
	}

	/** Retrieve the names of all defined LOD groups. */
	void GetLODGroupNames(TArray<FName>& OutNames) const;

	/** Retrieves the localized display names of all LOD groups. */
	void GetLODGroupDisplayNames(TArray<FText>& OutDisplayNames) const;

private:
	/** Reads an entry from the INI to initialize settings for an LOD group. */
	void ReadEntry(FStaticMeshLODGroup& Group, FString Entry);
	/** Per-group settings. */
	TMap<FName,FStaticMeshLODGroup> Groups;
};

template<typename TangentTypeT>
struct TStaticMeshVertexTangentDatum
{
	TangentTypeT TangentX;
	TangentTypeT TangentZ;

	FORCEINLINE FVector GetTangentX() const
	{
		return TangentX;
	}

	FORCEINLINE FVector4 GetTangentZ() const
	{
		return TangentZ;
	}

	FORCEINLINE FVector GetTangentY() const
	{
		FVector  TanX = GetTangentX();
		FVector4 TanZ = GetTangentZ();

		return (FVector(TanZ) ^ TanX) * TanZ.W;
	}

	FORCEINLINE void SetTangents(FVector X, FVector Y, FVector Z)
	{
		TangentX = X;
		TangentZ = FVector4(Z, GetBasisDeterminantSign(X, Y, Z));
	}
};

template<typename UVTypeT, uint32 NumTexCoords>
struct TStaticMeshVertexUVsDatum
{
	UVTypeT UVs[NumTexCoords];

	FORCEINLINE FVector2D GetUV(uint32 TexCoordIndex) const
	{
		check(TexCoordIndex < NumTexCoords);

		return UVs[TexCoordIndex];
	}

	FORCEINLINE void SetUV(uint32 TexCoordIndex, FVector2D UV)
	{
		check(TexCoordIndex < NumTexCoords);

		UVs[TexCoordIndex] = UV;
	}
};

enum class EStaticMeshVertexTangentBasisType
{
	Default,
	HighPrecision,
};

enum class EStaticMeshVertexUVType
{
	Default,
	HighPrecision,
};

template<EStaticMeshVertexTangentBasisType TangentBasisType>
struct TStaticMeshVertexTangentTypeSelector
{
};

template<>
struct TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>
{
	typedef FPackedNormal TangentTypeT;
	static const EVertexElementType VertexElementType = VET_PackedNormal;
};

template<>
struct TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>
{
	typedef FPackedRGBA16N TangentTypeT;
	static const EVertexElementType VertexElementType = VET_UShort4N;
};

template<EStaticMeshVertexUVType UVType>
struct TStaticMeshVertexUVsTypeSelector
{
};

template<>
struct TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>
{
	typedef FVector2DHalf UVsTypeT;
};

template<>
struct TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>
{
	typedef FVector2D UVsTypeT;
};

#define ConstExprStaticMeshVertexTypeID(bUseHighPrecisionTangentBasis, bUseFullPrecisionUVs, TexCoordsCount) \
	(((((bUseHighPrecisionTangentBasis) ? 1 : 0) * 2) + ((bUseFullPrecisionUVs) ? 1 : 0)) * MAX_STATIC_TEXCOORDS) + ((TexCoordsCount) - 1)

FORCEINLINE uint32 ComputeStaticMeshVertexTypeID(bool bUseHighPrecisionTangentBasis, bool bUseFullPrecisionUVs, uint32 TexCoordsCount)
{
	return ConstExprStaticMeshVertexTypeID(bUseHighPrecisionTangentBasis, bUseFullPrecisionUVs, TexCoordsCount);
}

/**
* All information about a static-mesh vertex with a variable number of texture coordinates.
* Position information is stored separately to reduce vertex fetch bandwidth in passes that only need position. (z prepass)
*/
template<EStaticMeshVertexTangentBasisType TangentBasisTypeT, EStaticMeshVertexUVType UVTypeT, uint32 NumTexCoordsT>
struct TStaticMeshFullVertex : 
	public TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<TangentBasisTypeT>::TangentTypeT>,
	public TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<UVTypeT>::UVsTypeT, NumTexCoordsT>
{
	static_assert(NumTexCoordsT > 0, "Must have at least 1 texcoord.");

	static const EStaticMeshVertexTangentBasisType TangentBasisType = TangentBasisTypeT;
	static const EStaticMeshVertexUVType UVType = UVTypeT;
	static const uint32 NumTexCoords = NumTexCoordsT;

	static const uint32 VertexTypeID = ConstExprStaticMeshVertexTypeID(
		TangentBasisTypeT == EStaticMeshVertexTangentBasisType::HighPrecision, 
		UVTypeT == EStaticMeshVertexUVType::HighPrecision,
		NumTexCoordsT
		);

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar, TStaticMeshFullVertex& Vertex)
	{
		Ar << Vertex.TangentX;
		Ar << Vertex.TangentZ;

		for (uint32 UVIndex = 0; UVIndex < NumTexCoordsT; UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}

		return Ar;
	}
};

#define APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, UVCount, ...) \
	{ \
		typedef TStaticMeshFullVertex<TangentBasisType, UVType, UVCount> VertexType; \
		case VertexType::VertexTypeID: { __VA_ARGS__ } break; \
	}

#define SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(TangentBasisType, UVType, ...) \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 1, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 2, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 3, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 4, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 5, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 6, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 7, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 8, __VA_ARGS__);

#define SELECT_STATIC_MESH_VERTEX_TYPE(bIsHighPrecisionTangentBais, bIsHigPrecisionUVs, NumTexCoords, ...) \
	{ \
		uint32 VertexTypeID = ComputeStaticMeshVertexTypeID(bIsHighPrecisionTangentBais, bIsHigPrecisionUVs, NumTexCoords); \
		switch (VertexTypeID) \
		{ \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::Default,		  EStaticMeshVertexUVType::Default,		  __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::Default,		  EStaticMeshVertexUVType::HighPrecision, __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::HighPrecision,  EStaticMeshVertexUVType::Default,		  __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::HighPrecision,  EStaticMeshVertexUVType::HighPrecision, __VA_ARGS__); \
		}; \
	}

/**
 * A set of static mesh triangles which are rendered with the same material.
 */
struct FStaticMeshSection
{
	/** The index of the material with which to render this section. */
	int32 MaterialIndex;

	/** Range of vertices and indices used when rendering this section. */
	uint32 FirstIndex;
	uint32 NumTriangles;
	uint32 MinVertexIndex;
	uint32 MaxVertexIndex;

	/** If true, collision is enabled for this section. */
	bool bEnableCollision;
	/** If true, this section will cast a shadow. */
	bool bCastShadow;

	/** Constructor. */
	FStaticMeshSection()
		: MaterialIndex(0)
		, FirstIndex(0)
		, NumTriangles(0)
		, MinVertexIndex(0)
		, MaxVertexIndex(0)
		, bEnableCollision(false)
		, bCastShadow(true)
	{}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FStaticMeshSection& Section);
};

/** An interface to the static-mesh vertex data storage type. */
class FStaticMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FStaticMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual uint32 GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual uint8* GetDataPointer() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};

/** A vertex that stores just position. */
struct FPositionVertex
{
	FVector	Position;

	friend FArchive& operator<<(FArchive& Ar,FPositionVertex& V)
	{
		Ar << V.Position;
		return Ar;
	}
};

/** A vertex buffer of positions. */
class FPositionVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	ENGINE_API FPositionVertexBuffer();

	/** Destructor. */
	ENGINE_API ~FPositionVertexBuffer();

	/** Delete existing resources */
	ENGINE_API void CleanUp();

	/**
	* Initializes the buffer with the given vertices, used to convert legacy layouts.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	ENGINE_API void Init(const TArray<FStaticMeshBuildVertex>& InVertices);

	/**
	 * Initializes this vertex buffer with the contents of the given vertex buffer.
	 * @param InVertexBuffer - The vertex buffer to initialize from.
	 */
	void Init(const FPositionVertexBuffer& InVertexBuffer);

	ENGINE_API void Init(const TArray<FVector>& InPositions);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(uint32 InNumVertices);

	/**
	* Serializer
	*
	* @param	Ar				Archive to serialize with
	* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, bool bNeedsCPUAccess );

	/**
	* Specialized assignment operator, only used when importing LOD's. 
	*/
	ENGINE_API void operator=(const FPositionVertexBuffer &Other);

	// Vertex data accessors.
	FORCEINLINE FVector& VertexPosition(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	FORCEINLINE const FVector& VertexPosition(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("PositionOnly Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	class FPositionVertexData* VertexData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData( bool bNeedsCPUAccess = true );
};

/** Vertex buffer for a static mesh LOD */
class FStaticMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FStaticMeshVertexBuffer();

	/** Destructor. */
	ENGINE_API ~FStaticMeshVertexBuffer();

	/** Delete existing resources */
	ENGINE_API void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 * @param InNumTexCoords - The number of texture coordinate to store in the buffer.
	 */
	ENGINE_API void Init(const TArray<FStaticMeshBuildVertex>& InVertices,uint32 InNumTexCoords);

	/**
	 * Initializes this vertex buffer with the contents of the given vertex buffer.
	 * @param InVertexBuffer - The vertex buffer to initialize from.
	 */
	void Init(const FStaticMeshVertexBuffer& InVertexBuffer);

	/**
	 * Removes the cloned vertices used for extruding shadow volumes.
	 * @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	 */
	void RemoveLegacyShadowVolumeVertices(uint32 NumVertices);

	/**
	* Serializer
	*
	* @param	Ar				Archive to serialize with
	* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, bool bNeedsCPUAccess );

	/**
	* Specialized assignment operator, only used when importing LOD's. 
	*/
	ENGINE_API void operator=(const FStaticMeshVertexBuffer &Other);

	FORCEINLINE FVector4 VertexTangentX(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentX();
		}
		else
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentX();
		}
	}

	FORCEINLINE FVector VertexTangentZ(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentZ();
		}
		else
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentZ();
		}
	}

	/**
	* Calculate the binormal (TangentY) vector using the normal,tangent vectors
	*
	* @param VertexIndex - index into the vertex buffer
	* @return binormal (TangentY) vector
	*/
	FORCEINLINE FVector VertexTangentY(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentY();
		}
		else
		{
			return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentY();
		}
	}

	FORCEINLINE void SetVertexTangents(uint32 VertexIndex, FVector X, FVector Y, FVector Z)
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->SetTangents(X, Y, Z);
		}
		else
		{
			return reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->SetTangents(X, Y, Z);
		}
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param Vec2D - UV values to set
	*/
	FORCEINLINE void SetVertexUV(uint32 VertexIndex, uint32 UVIndex, const FVector2D& Vec2D)
	{
		checkSlow(VertexIndex < GetNumVertices());
		checkSlow(UVIndex < GetNumTexCoords());
		
		if (GetUseHighPrecisionTangentBasis())
		{
			if (GetUseFullPrecisionUVs())
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
			else
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
		}
		else
		{
			if (GetUseFullPrecisionUVs())
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
			else
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
		}
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(uint32 VertexIndex,uint32 UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		checkSlow(UVIndex < GetNumTexCoords());

		if (GetUseHighPrecisionTangentBasis())
		{
			if (GetUseFullPrecisionUVs())
			{
				return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->GetUV(UVIndex);
			}
			else
			{
				return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->GetUV(UVIndex);
			}
		}
		else
		{
			if (GetUseFullPrecisionUVs())
			{
				return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->GetUV(UVIndex);
			}
			else
			{
				return reinterpret_cast<const TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->GetUV(UVIndex);
			}
		}
	}

	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}

	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	FORCEINLINE uint32 GetNumTexCoords() const
	{
		return NumTexCoords;
	}

	FORCEINLINE bool GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}

	FORCEINLINE void SetUseFullPrecisionUVs(bool UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}

	FORCEINLINE bool GetUseHighPrecisionTangentBasis() const
	{
		return bUseHighPrecisionTangentBasis;
	}

	FORCEINLINE void SetUseHighPrecisionTangentBasis(bool bUseHighPrecision)
	{
		bUseHighPrecisionTangentBasis = bUseHighPrecision;
	}

	const uint8* GetRawVertexData() const
	{
		check( Data != NULL );
		return Data;
	}

	/**
	* Convert the existing data in this mesh without rebuilding the mesh (loss of precision)
	*/
	template<typename SrcVertexTypeT, typename DstVertexTypeT>
	void ConvertVertexFormat();

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* VertexData;

	/** The number of texcoords/vertex in the buffer. */
	uint32 NumTexCoords;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Corresponds to UStaticMesh::UseFullPrecisionUVs. if true then 32 bit UVs are used */
	bool bUseFullPrecisionUVs;

	/** If true then RGB10A2 is used to store tangent else RGBA8 */
	bool bUseHighPrecisionTangentBasis;

	/** Allocates the vertex data storage type. */
	void AllocateData( bool bNeedsCPUAccess = true );
};

/** Rendering resources needed to render an individual static mesh LOD. */
struct FStaticMeshLODResources
{
	/** The buffer containing vertex data. */
	FStaticMeshVertexBuffer VertexBuffer;
	/** The buffer containing the position vertex data. */
	FPositionVertexBuffer PositionVertexBuffer;
	/** The buffer containing the vertex color data. */
	FColorVertexBuffer ColorVertexBuffer;

	/** Index buffer resource for rendering. */
	FRawStaticIndexBuffer IndexBuffer;
	/** Reversed index buffer, used to prevent changing culling state between drawcalls. */
	FRawStaticIndexBuffer ReversedIndexBuffer;
	/** Index buffer resource for rendering in depth only passes. */
	FRawStaticIndexBuffer DepthOnlyIndexBuffer;
	/** Reversed depth only index buffer, used to prevent changing culling state between drawcalls. */
	FRawStaticIndexBuffer ReversedDepthOnlyIndexBuffer;
	/** Index buffer resource for rendering wireframe mode. */
	FRawStaticIndexBuffer WireframeIndexBuffer;
	/** Index buffer containing adjacency information required by tessellation. */
	FRawStaticIndexBuffer AdjacencyIndexBuffer;

	/** The vertex factory used when rendering this mesh. */
	FLocalVertexFactory VertexFactory;

	/** The vertex factory used when rendering this mesh with vertex colors. This is lazy init.*/
	FLocalVertexFactory VertexFactoryOverrideColorVertexBuffer;

	/** Sections for this LOD. */
	TArray<FStaticMeshSection> Sections;

	/** Distance field data associated with this mesh, null if not present.  */
	class FDistanceFieldVolumeData* DistanceFieldData; 

	/** The maximum distance by which this LOD deviates from the base from which it was generated. */
	float MaxDeviation;

	/** True if the adjacency index buffer contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasAdjacencyInfo : 1;

	/** True if the depth only index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasDepthOnlyIndices : 1;

	/** True if the reversed index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasReversedIndices : 1;

	/** True if the reversed index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasReversedDepthOnlyIndices: 1;


	uint32 DepthOnlyNumTriangles;

	struct FSplineMeshVertexFactory* SplineVertexFactory;
	struct FSplineMeshVertexFactory* SplineVertexFactoryOverrideColorVertexBuffer;

	/** Default constructor. */
	FStaticMeshLODResources();

	~FStaticMeshLODResources();

	/** Initializes all rendering resources. */
	void InitResources(UStaticMesh* Parent);

	/** Releases all rendering resources. */
	void ReleaseResources();

	/** Serialize. */
	void Serialize(FArchive& Ar, UObject* Owner, int32 Idx);

	/** Return the triangle count of this LOD. */
	ENGINE_API int32 GetNumTriangles() const;

	/** Return the number of vertices in this LOD. */
	ENGINE_API int32 GetNumVertices() const;

	ENGINE_API int32 GetNumTexCoords() const;

	/**
	 * Initializes a vertex factory for rendering this static mesh
	 *
	 * @param	InOutVertexFactory				The vertex factory to configure
	 * @param	InParentMesh					Parent static mesh
	 * @param	bInOverrideColorVertexBuffer	If true, make a vertex factory ready for per-instance colors
	 */
	void InitVertexFactory(FLocalVertexFactory& InOutVertexFactory, UStaticMesh* InParentMesh, bool bInOverrideColorVertexBuffer);
};

/**
 * FStaticMeshRenderData - All data needed to render a static mesh.
 */
class FStaticMeshRenderData
{
public:
	/** Default constructor. */
	FStaticMeshRenderData();

	/** Per-LOD resources. */
	TIndirectArray<FStaticMeshLODResources> LODResources;

	/** Screen size to switch LODs */
	float ScreenSize[MAX_STATIC_MESH_LODS];

	/** Streaming texture factors. */
	float StreamingTextureFactors[MAX_STATIC_TEXCOORDS];

	/** Maximum value in StreamingTextureFactors. */
	float MaxStreamingTextureFactor;

	/** Bounds of the renderable mesh. */
	FBoxSphereBounds Bounds;

	/** True if LODs share static lighting data. */
	bool bLODsShareStaticLighting;

	/** True if the mesh or LODs were reduced using Simplygon. */
	bool bReducedBySimplygon;

#if WITH_EDITORONLY_DATA
	/** The derived data key associated with this render data. */
	FString DerivedDataKey;

	/** Map of wedge index to vertex index. */
	TArray<int32> WedgeMap;

	/** Map of material index -> original material index at import time. */
	TArray<int32> MaterialIndexToImportIndex;

	/** The next cached derived data in the list. */
	TScopedPointer<class FStaticMeshRenderData> NextCachedRenderData;

	/**
	 * Cache derived renderable data for the static mesh with the provided
	 * level of detail settings.
	 */
	void Cache(UStaticMesh* Owner, const FStaticMeshLODSettings& LODSettings);
#endif // #if WITH_EDITORONLY_DATA

	/** Serialization. */
	void Serialize(FArchive& Ar, UStaticMesh* Owner, bool bCooked);

	/** Initialize the render resources. */
	void InitResources(UStaticMesh* Owner);

	/** Releases the render resources. */
	ENGINE_API void ReleaseResources();

	/** Compute the size of this resource. */
	SIZE_T GetResourceSize() const;

	/** Allocate LOD resources. */
	ENGINE_API void AllocateLODResources(int32 NumLODs);

private:
#if WITH_EDITORONLY_DATA
	/** Allow the editor to explicitly update section information. */
	friend class FLevelOfDetailSettingsLayout;
#endif // #if WITH_EDITORONLY_DATA
#if WITH_EDITOR
	/** Resolve all per-section settings. */
	ENGINE_API void ResolveSectionInfo(UStaticMesh* Owner);
#endif // #if WITH_EDITORONLY_DATA
};

/**
 * FStaticMeshComponentRecreateRenderStateContext - Destroys render state for all StaticMeshComponents using a given StaticMesh and 
 * recreates them when it goes out of scope. Used to ensure stale rendering data isn't kept around in the components when importing
 * over or rebuilding an existing static mesh.
 */
class FStaticMeshComponentRecreateRenderStateContext
{
public:

	/** Initialization constructor. */
	FStaticMeshComponentRecreateRenderStateContext( UStaticMesh* InStaticMesh, bool InUnbuildLighting = true, bool InRefreshBounds = false )
		: bUnbuildLighting( InUnbuildLighting ),
		  bRefreshBounds( InRefreshBounds )
	{
		for ( TObjectIterator<UStaticMeshComponent> It;It;++It )
		{
			if ( It->StaticMesh == InStaticMesh )
			{
				checkf( !It->IsUnreachable(), TEXT("%s"), *It->GetFullName() );

				if ( It->bRenderStateCreated )
				{
					check( It->IsRegistered() );
					It->DestroyRenderState_Concurrent();
					StaticMeshComponents.Add(*It);
				}
			}
		}

		// Flush the rendering commands generated by the detachments.
		// The static mesh scene proxies reference the UStaticMesh, and this ensures that they are cleaned up before the UStaticMesh changes.
		FlushRenderingCommands();
	}

	/** Destructor: recreates render state for all components that had their render states destroyed in the constructor. */
	~FStaticMeshComponentRecreateRenderStateContext()
	{
		const int32 ComponentCount = StaticMeshComponents.Num();
		for( int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex )
		{
			UStaticMeshComponent* Component = StaticMeshComponents[ ComponentIndex ];
			if ( bUnbuildLighting )
			{
				// Invalidate the component's static lighting.
				// This unregisters and reregisters so must not be in the constructor
				Component->InvalidateLightingCache();
			}

			if( bRefreshBounds )
			{
				Component->UpdateBounds();
			}

			if ( Component->IsRegistered() && !Component->bRenderStateCreated )
			{
				Component->CreateRenderState_Concurrent();
			}
		}
	}

private:

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	bool bUnbuildLighting;
	bool bRefreshBounds;
};

/**
 * A static mesh component scene proxy.
 */
class ENGINE_API FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	FStaticMeshSceneProxy(UStaticMeshComponent* Component);

	virtual ~FStaticMeshSceneProxy() {}

	/** Gets the number of mesh batches required to represent the proxy, aside from section needs. */
	virtual int32 GetNumMeshBatches() const
	{
		return 1;
	}

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(
		int32 LODIndex, 
		int32 BatchIndex, 
		int32 ElementIndex, 
		uint8 InDepthPriorityGroup, 
		bool bUseSelectedMaterial, 
		bool bUseHoveredMaterial, 
		bool bAllowPreCulledIndices,
		FMeshBatch& OutMeshBatch) const;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const;

protected:
	/**
	 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
	 */
	virtual void SetIndexSource(int32 LODIndex, int32 ElementIndex, FMeshBatch& OutMeshElement, bool bWireframe, bool bRequiresAdjacencyInformation, bool bUseInversedIndices, bool bAllowPreCulledIndices) const;
	bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;

public:
	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void OnTransformChanged() override;
	virtual int32 GetLOD(const FSceneView* View) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	virtual void GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, TArray<FMatrix>& ObjectLocalToWorldTransforms) const override;
	virtual void GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const override;
	virtual bool HasDistanceFieldRepresentation() const override;
	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODs.GetAllocatedSize() ); }

	virtual void GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual void GetLCIs(FLCIArray& LCIs) override;

#if WITH_EDITORONLY_DATA
	virtual const FStreamingSectionBuildInfo* GetStreamingSectionData(float& OutComponentExtraScale, float& OutMeshExtraScale, int32 LODIndex, int32 ElementIndex) const override;
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual int32 GetLightMapResolution() const override { return LightMapResolution; }
#endif

protected:

	/** Information used by the proxy about a single LOD of the mesh. */
	class FLODInfo : public FLightCacheInterface
	{
	public:

		/** Information about an element of a LOD. */
		struct FSectionInfo
		{
			/** Default constructor. */
			FSectionInfo()
				: Material(NULL)
				, bSelected(false)
#if WITH_EDITOR
				, HitProxy(NULL)
#endif
				, FirstPreCulledIndex(0)
				, NumPreCulledTriangles(-1)
			{}

			/** The material with which to render this section. */
			UMaterialInterface* Material;

			/** True if this section should be rendered as selected (editor only). */
			bool bSelected;

#if WITH_EDITOR
			/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
			HHitProxy* HitProxy;
#endif

			int32 FirstPreCulledIndex;
			int32 NumPreCulledTriangles;
		};

		/** Per-section information. */
		TArray<FSectionInfo> Sections;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handle the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		const FRawStaticIndexBuffer* PreCulledIndexBuffer;

		/** Initialization constructor. */
		FLODInfo(const UStaticMeshComponent* InComponent,int32 InLODIndex);

		bool UsesMeshModifyingMaterials() const { return bUsesMeshModifyingMaterials; }

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const override;

	private:

		TArray<FGuid> IrrelevantLights;

		/** True if any elements in this LOD use mesh-modifying materials **/
		bool bUsesMeshModifyingMaterials;
	};

	AActor* Owner;
	const UStaticMesh* StaticMesh;
	UBodySetup* BodySetup;
	FStaticMeshRenderData* RenderData;

	TIndirectArray<FLODInfo> LODs;

	const FDistanceFieldVolumeData* DistanceFieldData;	

	/** Hierarchical LOD Index used for rendering */
	uint8 HierarchicalLODIndex;

	/**
	 * The forcedLOD set in the static mesh editor, copied from the mesh component
	 */
	int32 ForcedLodModel;

	/** Minimum LOD index to use.  Clamped to valid range [0, NumLODs - 1]. */
	int32 ClampedMinLOD;

	FVector TotalScale3D;

	uint32 bCastShadow : 1;
	ECollisionTraceFlag		CollisionTraceFlag;

	/** The view relevance for all the static mesh's materials. */
	FMaterialRelevance MaterialRelevance;

	/** Collision Response of this component**/
	FCollisionResponseContainer CollisionResponse;

#if WITH_EDITORONLY_DATA
	/** Data shared with the component */
	TSharedPtr<TArray<FStreamingSectionBuildInfo>, ESPMode::NotThreadSafe> StreamingSectionData;
	/** The component streaming distance multiplier */
	float StreamingDistanceMultiplier;
	/** The mesh streaming texel factor (fallback) */
	float StreamingTexelFactor;

	/** Index of the section to preview. If set to INDEX_NONE, all section will be rendered */
	int32 SectionIndexPreview;
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** LightMap resolution used for VMI_LightmapDensity */
	int32 LightMapResolution;
#endif

#if !(UE_BUILD_SHIPPING)
	/** LOD used for collision */
	int32 LODForCollision;
	/** If we want to draw the mesh collision for debugging */
	uint32 bDrawMeshCollisionWireframe : 1;
#endif

	/**
	 * Returns the display factor for the given LOD level
	 *
	 * @Param LODIndex - The LOD to get the display factor for
	 */
	float GetScreenSize(int32 LODIndex) const;

	/**
	 * Returns the LOD mask for a view, this is like the ordinary LOD but can return two values for dither fading
	 */
	FLODMask GetLODMask(const FSceneView* View) const;
};

/*-----------------------------------------------------------------------------
	FStaticMeshInstanceData
-----------------------------------------------------------------------------*/

template <class FloatType>
struct FInstanceStream
{
	FVector4 InstanceOrigin;  // per-instance random in w 
	FloatType InstanceTransform1[4];  // hitproxy.r + 256 * selected in .w
	FloatType InstanceTransform2[4]; // hitproxy.g in .w
	FloatType InstanceTransform3[4]; // hitproxy.b in .w
	int16 InstanceLightmapAndShadowMapUVBias[4]; 

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[1][0];
		Me->InstanceTransform1[2] = Transform.M[2][0];
		Me->InstanceTransform1[3] = FloatType();

		Me->InstanceTransform2[0] = Transform.M[0][1];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[2][1];
		Me->InstanceTransform2[3] = FloatType();

		Me->InstanceTransform3[0] = Transform.M[0][2];
		Me->InstanceTransform3[1] = Transform.M[1][2];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FloatType();

		Me->InstanceLightmapAndShadowMapUVBias[0] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[1] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[2] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[3] = 0;
	}

	FORCEINLINE void GetInstanceTransform(FMatrix& Transform) const
	{
		Transform.M[3][0] = InstanceOrigin.X;
		Transform.M[3][1] = InstanceOrigin.Y;
		Transform.M[3][2] = InstanceOrigin.Z;

		Transform.M[0][0] = InstanceTransform1[0];
		Transform.M[1][0] = InstanceTransform1[1];
		Transform.M[2][0] = InstanceTransform1[2];

		Transform.M[0][1] = InstanceTransform2[0];
		Transform.M[1][1] = InstanceTransform2[1];
		Transform.M[2][1] = InstanceTransform2[2];

		Transform.M[0][2] = InstanceTransform3[0];
		Transform.M[1][2] = InstanceTransform3[1];
		Transform.M[2][2] = InstanceTransform3[2];

		Transform.M[0][3] = 0.f;
		Transform.M[1][3] = 0.f;
		Transform.M[2][3] = 0.f;
		Transform.M[3][3] = 0.f;
	}

	FORCEINLINE void GetInstanceShaderValues(FVector4 OutInstanceTransform[3], FVector4& OutInstanceLightmapAndShadowMapUVBias, FVector4& OutInstanceOrigin) const
	{
		OutInstanceLightmapAndShadowMapUVBias = FVector4(
			float(InstanceLightmapAndShadowMapUVBias[0]),
			float(InstanceLightmapAndShadowMapUVBias[1]),
			float(InstanceLightmapAndShadowMapUVBias[2]),
			float(InstanceLightmapAndShadowMapUVBias[3]));
			
		OutInstanceTransform[0] = FVector4(
			(float)InstanceTransform1[0],
			(float)InstanceTransform1[1],
			(float)InstanceTransform1[2],
			(float)InstanceTransform1[3]);

		OutInstanceTransform[1] = FVector4(
			(float)InstanceTransform2[0],
			(float)InstanceTransform2[1],
			(float)InstanceTransform2[2],
			(float)InstanceTransform2[3]);
		
		OutInstanceTransform[2] = FVector4(
			(float)InstanceTransform3[0],
			(float)InstanceTransform3[1],
			(float)InstanceTransform3[2],
			(float)InstanceTransform3[3]);

		OutInstanceOrigin = InstanceOrigin;
	}

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias, FColor HitProxyColor, bool bSelected)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		const FVector2D* RESTRICT rLightmapUVBias = (const FVector2D* RESTRICT)&LightmapUVBias;
		const FVector2D* RESTRICT rShadowmapUVBias = (const FVector2D* RESTRICT)&ShadowmapUVBias;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[1][0];
		Me->InstanceTransform1[2] = Transform.M[2][0];
		Me->InstanceTransform1[3] = ((float)HitProxyColor.R) + (bSelected ? 256.f : 0.0f);

		Me->InstanceTransform2[0] = Transform.M[0][1];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[2][1];
		Me->InstanceTransform2[3] = (float)HitProxyColor.G;

		Me->InstanceTransform3[0] = Transform.M[0][2];
		Me->InstanceTransform3[1] = Transform.M[1][2];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = (float)HitProxyColor.B;

		Me->InstanceLightmapAndShadowMapUVBias[0] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->X  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[1] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->Y  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[2] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->X * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[3] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->Y * 32767.0f), MIN_int16, MAX_int16);
	}

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		const FVector2D* RESTRICT rLightmapUVBias = (const FVector2D* RESTRICT)&LightmapUVBias;
		const FVector2D* RESTRICT rShadowmapUVBias = (const FVector2D* RESTRICT)&ShadowmapUVBias;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[1][0];
		Me->InstanceTransform1[2] = Transform.M[2][0];
		Me->InstanceTransform1[3] = FloatType();

		Me->InstanceTransform2[0] = Transform.M[0][1];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[2][1];
		Me->InstanceTransform2[3] = FloatType();

		Me->InstanceTransform3[0] = Transform.M[0][2];
		Me->InstanceTransform3[1] = Transform.M[1][2];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FloatType();

		Me->InstanceLightmapAndShadowMapUVBias[0] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->X  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[1] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->Y  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[2] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->X * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[3] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->Y * 32767.0f), MIN_int16, MAX_int16);
	}

	FORCEINLINE void NullifyInstance()
	{
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		Me->InstanceTransform1[0] = FloatType();
		Me->InstanceTransform1[1] = FloatType();
		Me->InstanceTransform1[2] = FloatType();

		Me->InstanceTransform2[0] = FloatType();
		Me->InstanceTransform2[1] = FloatType();
		Me->InstanceTransform2[2] = FloatType();

		Me->InstanceTransform3[0] = FloatType();
		Me->InstanceTransform3[1] = FloatType();
		Me->InstanceTransform3[2] = FloatType();
	}

	FORCEINLINE void SetInstanceEditorData(FColor HitProxyColor, bool bSelected)
	{
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		
		Me->InstanceTransform1[3] = ((float)HitProxyColor.R) + (bSelected ? 256.f : 0.0f);
		Me->InstanceTransform2[3] = (float)HitProxyColor.G;
		Me->InstanceTransform3[3] = (float)HitProxyColor.B;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FInstanceStream& V )
	{
		return Ar 
			<< V.InstanceOrigin.X 
			<< V.InstanceOrigin.Y
			<< V.InstanceOrigin.Z
			<< V.InstanceOrigin.W

			<< V.InstanceTransform1[0]
			<< V.InstanceTransform1[1]
			<< V.InstanceTransform1[2]
			<< V.InstanceTransform1[3]

			<< V.InstanceTransform2[0]
			<< V.InstanceTransform2[1]
			<< V.InstanceTransform2[2]
			<< V.InstanceTransform2[3]

			<< V.InstanceTransform3[0]
			<< V.InstanceTransform3[1]
			<< V.InstanceTransform3[2]
			<< V.InstanceTransform3[3]

			<< V.InstanceLightmapAndShadowMapUVBias[0]
			<< V.InstanceLightmapAndShadowMapUVBias[1]
			<< V.InstanceLightmapAndShadowMapUVBias[2]
			<< V.InstanceLightmapAndShadowMapUVBias[3];
	}
};

typedef FInstanceStream<FFloat16>	FInstanceStream16;
typedef FInstanceStream<float>		FInstanceStream32;

/** The implementation of the static mesh instance data storage type. */
class FStaticMeshInstanceData :
	public FStaticMeshVertexDataInterface
{
public:
	FStaticMeshInstanceData()
		: InstanceStream16(false)
		, InstanceStream32(false)
		, bUseHalfFloat(PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || GVertexElementTypeSupport.IsSupported(VET_Half2))
	{
	}
	/**
	 * Constructor
	 * @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	 * @param bSupportsVertexHalfFloat - true if device has support for half float in vertex arrays
	 */
	FStaticMeshInstanceData(bool InNeedsCPUAccess, bool bInUseHalfFloat)
		: InstanceStream16(InNeedsCPUAccess)
		, InstanceStream32(InNeedsCPUAccess)
		, bUseHalfFloat(bInUseHalfFloat)
	{
	}

	SIZE_T GetResourceSize() const
	{
		return SIZE_T(NumInstances()) * SIZE_T(GetStride());
	}

	static SIZE_T GetResourceSize(int32 InNumInstances, bool bInUseHalfFloat)
	{
		return SIZE_T(InNumInstances) * SIZE_T(bInUseHalfFloat ? sizeof(FInstanceStream16) : sizeof(FInstanceStream32));
	}

	/**
	 * Resizes the vertex data buffer, discarding any data which no longer fits.
	 * @param NumVertices - The number of vertices to allocate the buffer for.
	 */
	virtual void ResizeBuffer(uint32 NumInstances) override
	{
		checkf(0, TEXT("ArrayType::Add is not supported on all platforms"));
	}

	virtual uint32 GetStride() const override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return sizeof(FInstanceStream16);
		}
		else
		{
			return sizeof(FInstanceStream32);
		}
	}
	virtual uint8* GetDataPointer() override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return (uint8*)&(InstanceStream16)[0];
		}
		else
		{
			return (uint8*)&(InstanceStream32)[0];
		}
	}
	virtual FResourceArrayInterface* GetResourceArray() override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return &InstanceStream16;
		}
		else
		{
			return &InstanceStream32;
		}
	}
	virtual void Serialize(FArchive& Ar) override
	{
		InstanceStream16.BulkSerialize(Ar);
		InstanceStream32.BulkSerialize(Ar);
	}

	void AllocateInstances(int32 NumInstances)
	{
		// We cannot write directly to the data on all platforms,
		// so we make a TArray of the right type, then assign it
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16.Empty(NumInstances);
			InstanceStream16.AddUninitialized(NumInstances);
		}
		else
		{
			InstanceStream32.Empty(NumInstances);
			InstanceStream32.AddUninitialized(NumInstances);
		}
	}

	FORCEINLINE void GetInstanceTransform(int32 InstanceIndex, FMatrix& Transform) const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].GetInstanceTransform(Transform);
		}
		else
		{
			InstanceStream32[InstanceIndex].GetInstanceTransform(Transform);
		}
	}

	FORCEINLINE void GetInstanceShaderValues(int32 InstanceIndex, FVector4 InstanceTransform[3], FVector4& InstanceLightmapAndShadowMapUVBias, FVector4& InstanceOrigin) const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].GetInstanceShaderValues(InstanceTransform, InstanceLightmapAndShadowMapUVBias, InstanceOrigin);
		}
		else
		{
			InstanceStream32[InstanceIndex].GetInstanceShaderValues(InstanceTransform, InstanceLightmapAndShadowMapUVBias, InstanceOrigin);
		}
	}

	FORCEINLINE void SetInstance(int32 InstanceIndex, const FMatrix& Transform, float RandomInstanceID)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstance(Transform, RandomInstanceID);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstance(Transform, RandomInstanceID);
		}
	}
	
	FORCEINLINE void SetInstance(int32 InstanceIndex, const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstance(Transform, RandomInstanceID, LightmapUVBias, ShadowmapUVBias);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstance(Transform, RandomInstanceID, LightmapUVBias, ShadowmapUVBias);
		}
	}
	
	FORCEINLINE void NullifyInstance(int32 InstanceIndex)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].NullifyInstance();
		}
		else
		{
			InstanceStream32[InstanceIndex].NullifyInstance();
		}
	}

	FORCEINLINE void SetInstanceEditorData(int32 InstanceIndex, FColor HitProxyColor, bool bSelected)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstanceEditorData(HitProxyColor, bSelected);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstanceEditorData(HitProxyColor, bSelected);
		}
	}

	FORCEINLINE uint8* GetInstanceWriteAddress(int32 InstanceIndex)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return (uint8*)(InstanceStream16.GetData() + InstanceIndex);
		}
		else
		{
			return (uint8*)(InstanceStream32.GetData() + InstanceIndex);
		}
	}

	FORCEINLINE int32 NumInstances() const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.Num();
		}
		else
		{
			return InstanceStream32.Num();
		}
	}

	FORCEINLINE bool GetAllowCPUAccess() const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.GetAllowCPUAccess();
		}
		else
		{
			return InstanceStream32.GetAllowCPUAccess();
		}
	}

	FORCEINLINE void SetAllowCPUAccess(bool bInNeedsCPUAccess)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.SetAllowCPUAccess(bInNeedsCPUAccess);
		}
		else
		{
			return InstanceStream32.SetAllowCPUAccess(bInNeedsCPUAccess);
		}
	}

private:
	TResourceArray<FInstanceStream16, VERTEXBUFFER_ALIGNMENT> InstanceStream16;
	TResourceArray<FInstanceStream32, VERTEXBUFFER_ALIGNMENT> InstanceStream32;
	const bool bUseHalfFloat;
};
	
#if WITH_EDITOR
/**
 * Remaps painted vertex colors when the renderable mesh has changed.
 * @param InPaintedVertices - The original position and normal for each painted vertex.
 * @param InOverrideColors - The painted vertex colors.
 * @param NewPositions - Positions of the new renderable mesh on which colors are to be mapped.
 * @param OptionalVertexBuffer - [optional] Vertex buffer containing vertex normals for the new mesh.
 * @param OutOverrideColors - Will contain vertex colors for the new mesh.
 */
ENGINE_API void RemapPaintedVertexColors(
	const TArray<FPaintedVertex>& InPaintedVertices,
	const FColorVertexBuffer& InOverrideColors,
	const FPositionVertexBuffer& NewPositions,
	const FStaticMeshVertexBuffer* OptionalVertexBuffer,
	TArray<FColor>& OutOverrideColors
	);
#endif // #if WITH_EDITOR
