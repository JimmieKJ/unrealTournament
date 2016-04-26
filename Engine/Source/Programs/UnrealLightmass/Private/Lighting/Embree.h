// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_EMBREE

namespace Lightmass
{	
	class FStaticLightingMesh;

	void EmbreeFilterFunc(void* UserPtr, RTCRay& InRay);

	// Accumulates transmission color, and store the distance.
	// This is required since Embree test collision in any order, 
	// possibly accumulating transmission that are being collision
	struct FEmbreeTransmissionAccumulator
	{
		TArray<FLinearColor, TInlineAllocator<64> > Colors;
		FORCEINLINE void Push(const FLinearColor& Color, float tFar)
		{
			new (Colors) FLinearColor(Color.R, Color.G, Color.B, tFar);
		}

		void Resolve(FLinearColor& FinalColor, float tCollide);
		void Resolve(FLinearColor& FinalColor);
	};

	struct FEmbreeRay : public RTCRay
	{
		FEmbreeRay(
			const FStaticLightingMesh* InShadowMesh, 
			const FStaticLightingMesh* InMappingMesh, 
			uint32 InTraceFlags,
			bool InFindClosestIntersection, 
			bool InCalculateTransmission, 
			bool InDirectShadowingRay
			);

		// Inputs required by filter functions
		const FStaticLightingMesh* ShadowMesh; // Controls self shadow behavior, such as only self shadow and no self shadow.
		const FStaticLightingMesh* MappingMesh; // Controls LOD behavior to control which mesh are allowed to cast shadows
		uint32 TraceFlags;
		bool bFindClosestIntersection;
		bool bCalculateTransmission;
		bool bDirectShadowingRay;
		bool bStaticAndOpaqueOnly;
		bool bTwoSidedCollision;
		bool bFlipSidedness;

		// Additional Outputs.
		int32 ElementIndex; // Material Index
		int32 RelativeVertexIndex; // Which index is closer.
		FVector2D TextureCoordinates; // Material Coordinates
		FVector2D LightmapCoordinates;
		FEmbreeTransmissionAccumulator TransmissionAcc;
	};


	struct FEmbreeTriangleDesc
	{
		int16 ElementIndex;
		uint16 CastShadow : 1;
		uint16 StaticAndOpaqueMask : 1;
		uint16 TwoSidedMask: 1;
		uint16 Translucent : 1;
		uint16 IndirectlyShadowedOnly : 1;
		uint16 Masked : 1;
		uint16 CastShadowAsMasked : 1;
	};

	// Mapping between Embree Geometry Id and Lightmass Mesh/LOD Id
	struct FEmbreeGeometry
	{
		FEmbreeGeometry(
			RTCDevice EmbreeDevice, 
			RTCScene EmbreeScene, 
			const FBoxSphereBounds& ImportanceBounds,
			const FStaticLightingMesh* InMesh,
			const FStaticLightingMapping* InMapping
			);

		const FStaticLightingMesh* Mesh;
		const FStaticLightingMapping* Mapping;

		TArray<FEmbreeTriangleDesc> TriangleDescs; // The material ID of each triangle.
		TArray<FVector2D> UVs;
		TArray<FVector2D> LightmapUVs;

		uint32 GeomID; // Embree ID for this mesh.

		float SurfaceArea;
		float SurfaceAreaWithinImportanceVolume;
		bool bHasShadowCastingPrimitives;
	};

	class FEmbreeAggregateMesh final : public FStaticLightingAggregateMesh
	{
	public:

		/** Initialization constructor. */
		FEmbreeAggregateMesh(const FScene& InScene);

		virtual ~FEmbreeAggregateMesh();

		virtual void AddMesh(const FStaticLightingMesh* Mesh, const FStaticLightingMapping* Mapping) override;

		virtual void ReserveMemory( int32 NumMeshes, int32 NumVertices, int32 NumTriangles ) override {}

		/** Prepares the mesh for raytracing. */
		virtual void PrepareForRaytracing() override;

		virtual void DumpStats() const override;

		virtual bool IntersectLightRay(
			const FLightRay& LightRay,
			bool bFindClosestIntersection,
			bool bCalculateTransmission,
			bool bDirectShadowingRay,
			class FCoherentRayCache& CoherentRayCache,
			FLightRayIntersection& Intersection) const override;

	private:

		/** Information about the meshes used in the kDOP tree. */
		TArray<const FEmbreeGeometry*> MeshInfos;

		/** Embree scene */
		RTCDevice EmbreeDevice;
		RTCScene EmbreeScene;
		
		/** Total number of triangles in the shadow mesh */
		int32 TotalNumTriangles;

	};

	class FEmbreeVerifyAggregateMesh final : public FStaticLightingAggregateMesh
	{
		typedef FStaticLightingAggregateMesh Super;

	public:

		FEmbreeVerifyAggregateMesh(const FScene& InScene);

		virtual void AddMesh(const FStaticLightingMesh* Mesh, const FStaticLightingMapping* Mapping) override;

		virtual void ReserveMemory( int32 NumMeshes, int32 NumVertices, int32 NumTriangles ) override;

		virtual void PrepareForRaytracing() override;

		virtual void DumpStats() const override;
		virtual void DumpCheckStats() const override;

		virtual bool IntersectLightRay(
			const FLightRay& LightRay,
			bool bFindClosestIntersection,
			bool bCalculateTransmission,
			bool bDirectShadowingRay,
			class FCoherentRayCache& CoherentRayCache,
			FLightRayIntersection& Intersection) const override;

	private:

		static bool VerifyTransmissions(const FLightRayIntersection& EmbreeIntersection, const FLightRayIntersection& ClosestIntersection);
		static bool VerifyChecks(const FLightRayIntersection& EmbreeIntersection, const FLightRayIntersection& ClosestIntersection, bool bFindClosestIntersection);

		FDefaultAggregateMesh DefaultAggregate;
		FEmbreeAggregateMesh EmbreeAggregate;

		mutable volatile int64 TransmissionMismatchCount;
		mutable volatile int64 TransmissionEqualCount;

		mutable volatile int64 CheckEqualCount;
		mutable volatile int64 CheckMismatchCount;
	};

} // namespace
#endif // USE_EMBREE
