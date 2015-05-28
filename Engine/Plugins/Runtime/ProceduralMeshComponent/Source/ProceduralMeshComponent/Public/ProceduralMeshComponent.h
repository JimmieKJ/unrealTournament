// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.generated.h"

/** 
 *	Struct used to specify a tangent vector for a vertex 
 *	The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member.
 */
USTRUCT(BlueprintType)
struct FProcMeshTangent
{
	GENERATED_USTRUCT_BODY()

	/** Direction of X tangent for this vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	FVector TangentX;

	/** Bool that indicates whether we should flip the Y tangent when we compute it using cross product */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	bool bFlipTangentY;

	FProcMeshTangent()
	: TangentX(1.f, 0.f, 0.f)
	, bFlipTangentY(false)
	{}

	FProcMeshTangent(float X, float Y, float Z)
	: TangentX(X, Y, Z)
	, bFlipTangentY(false)
	{}

	FProcMeshTangent(FVector InTangentX, bool bInFlipTangentY)
	: TangentX(InTangentX)
	, bFlipTangentY(bInFlipTangentY)
	{}
};

/** One vertex for the procedural mesh, used for storing data internally */
USTRUCT()
struct FProcMeshVertex
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FProcMeshTangent Tangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector2D UV0;


	FProcMeshVertex()
	: Position(0.f, 0.f, 0.f)
	, Normal(0.f, 0.f, 1.f)
	, Tangent(FVector(1.f, 0.f, 0.f), false)
	, Color(255, 255, 255)
	, UV0(0.f, 0.f)
	{}
};

/** One section of the procedural mesh. Each material has its own section. */
USTRUCT()
struct FProcMeshSection
{
	GENERATED_USTRUCT_BODY()

	/** Vertex buffer for this section */
	UPROPERTY()
	TArray<FProcMeshVertex> ProcVertexBuffer;
	/** Index buffer for this section */
	UPROPERTY()
	TArray<int32> ProcIndexBuffer;
	/** Local bounding box of section */
	UPROPERTY()
	FBox SectionLocalBox;
	/** Should we build collision data for triangles in this section */
	UPROPERTY()
	bool bEnableCollision;

	FProcMeshSection()
	: SectionLocalBox(0)
	, bEnableCollision(false)
	{}

	/** Reset this section, clear all mesh info. */
	void Reset()
	{
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
		bEnableCollision = false;
	}
};

/** 
 *	Component that allows you to specify custom triangle mesh geometry 
 *	Beware! This feature is experimental and may be substantially changed in future releases.
 */
UCLASS(hidecategories=(Object,LOD), meta=(BlueprintSpawnableComponent), Experimental, ClassGroup=Experimental)
class PROCEDURALMESHCOMPONENT_API UProceduralMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	Create/replace a section for this procedural mesh component.
	 *	@param	SectionIndex		Index of the section to create or replace.
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Triangles			Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	bCreateCollision	Indicates whether collision should be created for this section. This adds significant cost.
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh", meta=(AutoCreateRefTerm = "Normals,UV0,VertexColors,Tangents" ))
	void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	/** Clear a section of the procedural mesh. Other sections do not change index. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void ClearMeshSection(int32 SectionIndex);

	/** Clear all mesh sections and reset to empty state */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void ClearAllMeshSections();


	// Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override{ return false; }
	// End Interface_CollisionDataProvider Interface

	/** Collision data */
	UPROPERTY(transient, duplicatetransient)
	class UBodySetup* ProcMeshBodySetup;

private:
	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual class UBodySetup* GetBodySetup() override;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const override;
	// End UMeshComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// Begin USceneComponent interface.

	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();
	/** Ensure ProcMeshBodySetup is allocated and configured */
	void CreateProcMeshBodySetup();
	/** Mark collision data as dirty, and re-create on instance if necessary */
	void UpdateCollision();

	/** Array of sections of mesh */
	UPROPERTY()
	TArray<FProcMeshSection> ProcMeshSections;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;


	friend class FProceduralMeshSceneProxy;
};


