// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMesh.h: Static mesh class definition.
=============================================================================*/

#pragma once

#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "RawIndexBuffer.h"
#include "TextureLayout3d.h"
#include "LocalVertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

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

/** 
 * All information about a static-mesh vertex with a variable number of texture coordinates.
 * Position information is stored separately to reduce vertex fetch bandwidth in passes that only need position. (z prepass)
 */
struct FStaticMeshFullVertex
{
	FPackedNormal TangentX;
	FPackedNormal TangentZ;

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	*/
	void Serialize(FArchive& Ar)
	{
		Ar << TangentX;
		Ar << TangentZ;
	}
};

/** 
* 16 bit UV version of static mesh vertex
*/
template<uint32 NumTexCoords>
struct TStaticMeshFullVertexFloat16UVs : public FStaticMeshFullVertex
{
	FVector2DHalf UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TStaticMeshFullVertexFloat16UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of static mesh vertex
*/
template<uint32 NumTexCoords>
struct TStaticMeshFullVertexFloat32UVs : public FStaticMeshFullVertex
{
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TStaticMeshFullVertexFloat32UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

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

	FORCEINLINE FPackedNormal& VertexTangentX(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentX;
	}
	FORCEINLINE const FPackedNormal& VertexTangentX(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentX;
	}

	/**
	* Calculate the binormal (TangentY) vector using the normal,tangent vectors
	*
	* @param VertexIndex - index into the vertex buffer
	* @return binormal (TangentY) vector
	*/
	FORCEINLINE FVector VertexTangentY(uint32 VertexIndex) const
	{
		const FPackedNormal& TangentX = VertexTangentX(VertexIndex);
		const FPackedNormal& TangentZ = VertexTangentZ(VertexIndex);
		return (FVector(TangentZ) ^ FVector(TangentX)) * ((float)TangentZ.Vector.W  / 127.5f - 1.0f);
	}

	FORCEINLINE FPackedNormal& VertexTangentZ(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentZ;
	}
	FORCEINLINE const FPackedNormal& VertexTangentZ(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentZ;
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param Vec2D - UV values to set
	*/
	FORCEINLINE void SetVertexUV(uint32 VertexIndex,uint32 UVIndex,const FVector2D& Vec2D)
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			((TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex] = Vec2D;
		}
		else
		{
			((TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex] = Vec2D;
		}		
	}

	/**
	* Fet the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(uint32 VertexIndex,uint32 UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			return ((TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}
		else
		{
			return ((TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
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
	const uint8* GetRawVertexData() const
	{
		check( Data != NULL );
		return Data;
	}

	/**
	* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
	* Without rebuilding the mesh (loss of precision)
	*/
	template<int32 NumTexCoords>
	void ConvertToFullPrecisionUVs();

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
	/** Index buffer resource for rendering in depth only passes. */
	FRawStaticIndexBuffer DepthOnlyIndexBuffer;
	/** Index buffer resource for rendering wireframe mode. */
	FRawStaticIndexBuffer WireframeIndexBuffer;
	/** Index buffer containing adjacency information required by tessellation. */
	FRawStaticIndexBuffer AdjacencyIndexBuffer;

	/** The vertex factory used when rendering this mesh. */
	FLocalVertexFactory VertexFactory;

	/** Sections for this LOD. */
	TArray<FStaticMeshSection> Sections;

	/** Distance field data associated with this mesh, null if not present.  */
	class FDistanceFieldVolumeData* DistanceFieldData; 

	/** The maximum distance by which this LOD deviates from the base from which it was generated. */
	float MaxDeviation;

	/** True if the adjacency index buffer contained data at init. Needed as it will not be available to the CPU afterwards. */
	bool bHasAdjacencyInfo;

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
	 * @param	InOverrideColorVertexBuffer		Optional color vertex buffer to use *instead* of the color vertex stream associated with this static mesh
	 */
	void InitVertexFactory(FLocalVertexFactory& InOutVertexFactory, UStaticMesh* InParentMesh, FColorVertexBuffer* InOverrideColorVertexBuffer);
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
	FStaticMeshComponentRecreateRenderStateContext( UStaticMesh* InStaticMesh, bool InUnbuildLighting = true )
	: bUnbuildLighting( InUnbuildLighting )
	{
		for ( TObjectIterator<UStaticMeshComponent> It;It;++It )
		{
			if ( It->StaticMesh == InStaticMesh )
			{
				checkf( !It->HasAnyFlags(RF_Unreachable), TEXT("%s"), *It->GetFullName() );

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

			if ( Component->IsRegistered() && !Component->bRenderStateCreated )
			{
				Component->CreateRenderState_Concurrent();
			}
		}
	}

private:

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	bool bUnbuildLighting;
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
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, const bool bUseSelectedMaterial, const bool bUseHoveredMaterial, FMeshBatch& OutMeshBatch) const;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const;


protected:
	/**
	 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
	 */
	virtual void SetIndexSource(int32 LODIndex, int32 ElementIndex, FMeshBatch& OutMeshElement, bool bWireframe, bool bRequiresAdjacencyInformation ) const;
	bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;

public:
	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void OnTransformChanged() override;
	virtual int32 GetLOD(const FSceneView* View) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override;
	virtual bool CanBeOccluded() const override;
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	virtual void GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, TArray<FMatrix>& ObjectLocalToWorldTransforms) const override;
	virtual void GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const override;
	virtual bool HasDistanceFieldRepresentation() const override;
	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODs.GetAllocatedSize() ); }

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
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
			{}

			/** The material with which to render this section. */
			UMaterialInterface* Material;

			/** True if this section should be rendered as selected (editor only). */
			bool bSelected;

#if WITH_EDITOR
			/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
			HHitProxy* HitProxy;
#endif
		};

		/** Per-section information. */
		TArray<FSectionInfo> Sections;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handle the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		/** When the mesh component has overridden the LOD's vertex colors, this vertex factory will be created
		    and passed along to the renderer instead of the mesh's stock vertex factory */
		TScopedPointer< FLocalVertexFactory > OverrideColorVertexFactory;


		/** Initialization constructor. */
		FLODInfo(const UStaticMeshComponent* InComponent,int32 InLODIndex);

		/** Destructor */
		virtual ~FLODInfo();

		// Accessors.
		const FLightMap* GetLightMap() const
		{
			return LightMap;
		}

		bool UsesMeshModifyingMaterials() const { return bUsesMeshModifyingMaterials; }

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const;

		virtual FLightMapInteraction GetLightMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const;

		virtual FShadowMapInteraction GetShadowMapInteraction() const;


	private:

		/** The lightmap used by this LOD. */
		FLightMap* LightMap;

		/** The shadowmap used by this LOD. */
		FShadowMap* ShadowMap;

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

	/**
	 * Returns the display factor for the given LOD level
	 *
	 * @Param LODIndex - The LOD to get the display factor for
	 */
	float GetScreenSize(int32 LODIndex) const;
};

/*-----------------------------------------------------------------------------
	FStaticMeshInstanceData
-----------------------------------------------------------------------------*/

struct FInstanceStream
{
	FVector4 InstanceOrigin;  // per-instance random in w 
	FFloat16 InstanceTransform1[4];  // hitproxy.r + 256 * selected in .w
	FFloat16 InstanceTransform2[4]; // hitproxy.g in .w
	FFloat16 InstanceTransform3[4]; // hitproxy.b in .w
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
		Me->InstanceTransform1[3] = FFloat16();

		Me->InstanceTransform2[0] = Transform.M[0][1];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[2][1];
		Me->InstanceTransform2[3] = FFloat16();

		Me->InstanceTransform3[0] = Transform.M[0][2];
		Me->InstanceTransform3[1] = Transform.M[1][2];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FFloat16();

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
		Me->InstanceTransform1[3] = FFloat16();

		Me->InstanceTransform2[0] = Transform.M[0][1];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[2][1];
		Me->InstanceTransform2[3] = FFloat16();

		Me->InstanceTransform3[0] = Transform.M[0][2];
		Me->InstanceTransform3[1] = Transform.M[1][2];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FFloat16();

		Me->InstanceLightmapAndShadowMapUVBias[0] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->X  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[1] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->Y  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[2] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->X * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[3] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->Y * 32767.0f), MIN_int16, MAX_int16);
	}

	FORCEINLINE void NullifyInstance()
	{
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		Me->InstanceTransform1[0] = FFloat16();
		Me->InstanceTransform1[1] = FFloat16();
		Me->InstanceTransform1[2] = FFloat16();

		Me->InstanceTransform2[0] = FFloat16();
		Me->InstanceTransform2[1] = FFloat16();
		Me->InstanceTransform2[2] = FFloat16();

		Me->InstanceTransform3[0] = FFloat16();
		Me->InstanceTransform3[1] = FFloat16();
		Me->InstanceTransform3[2] = FFloat16();
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


/** The implementation of the static mesh instance data storage type. */
class FStaticMeshInstanceData :
	public FStaticMeshVertexDataInterface,
	public TResourceArray<FInstanceStream, VERTEXBUFFER_ALIGNMENT>
{
public:

	typedef TResourceArray<FInstanceStream, VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	 * Constructor
	 * @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	 */
	FStaticMeshInstanceData(bool InNeedsCPUAccess=false)
		:	TResourceArray<FInstanceStream, VERTEXBUFFER_ALIGNMENT>(InNeedsCPUAccess)
	{
	}

	static uint32 StaticGetStride()
	{
		return sizeof(FInstanceStream);
	}

	static SIZE_T GetResourceSize(uint32 NumInstances)
	{
		return SIZE_T(NumInstances) * SIZE_T(StaticGetStride());
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
		return StaticGetStride();
	}
	virtual uint8* GetDataPointer() override
	{
		return (uint8*)&(*this)[0];
	}
	virtual FResourceArrayInterface* GetResourceArray() override
	{
		return this;
	}
	virtual void Serialize(FArchive& Ar) override
	{
		TResourceArray<FInstanceStream,VERTEXBUFFER_ALIGNMENT>::BulkSerialize(Ar);
	}

#if 0
	void Set(const TArray<FInstanceStream>& RawData)
	{
		*((ArrayType*)this) = TArray<FInstanceStream, TAlignedHeapAllocator<VERTEXBUFFER_ALIGNMENT> >(RawData);
	}
#endif

	void AllocateInstances(int32 NumInstances)
	{
		// We cannot write directly to the data on all platforms,
		// so we make a TArray of the right type, then assign it
		Empty(NumInstances);
		AddUninitialized(NumInstances);
	}
	FORCEINLINE FInstanceStream* GetInstanceWriteAddress(int32 InstanceIndex)
	{
		return GetData() + InstanceIndex;
	}
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
