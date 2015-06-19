// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. 

#include "CableComponentPluginPrivatePCH.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"


/** Vertex Buffer */
class FCableVertexBuffer : public FVertexBuffer 
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(NumVerts * sizeof(FDynamicMeshVertex), BUF_Dynamic, CreateInfo);
	}

	int32 NumVerts;
};

/** Index Buffer */
class FCableIndexBuffer : public FIndexBuffer 
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};

/** Vertex Factory */
class FCableVertexFactory : public FLocalVertexFactory
{
public:

	FCableVertexFactory()
	{}


	/** Initialization */
	void Init(const FCableVertexBuffer* VertexBuffer)
	{
		if(IsInRenderingThread())
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			NewData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			SetData(NewData);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitCableVertexFactory,
				FCableVertexFactory*,VertexFactory,this,
				const FCableVertexBuffer*,VertexBuffer,VertexBuffer,
			{
				// Initialize the vertex factory's stream components.
				DataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					);
				NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
				VertexFactory->SetData(NewData);
			});
		}
	}
};

/** Dynamic data sent to render thread */
struct FCableDynamicData
{
	/** Array of points */
	TArray<FVector> CablePoints;
};

//////////////////////////////////////////////////////////////////////////
// FCableSceneProxy

class FCableSceneProxy : public FPrimitiveSceneProxy
{
public:

	FCableSceneProxy(UCableComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, Material(NULL)
		, DynamicData(NULL)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, NumSegments(Component->NumSegments)
		, CableWidth(Component->CableWidth)
		, NumSides(Component->NumSides)
		, TileMaterial(Component->TileMaterial)
	{
		VertexBuffer.NumVerts = GetRequiredVertexCount();
		IndexBuffer.NumIndices = GetRequiredIndexCount();

		// Init vertex factory
		VertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FCableSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

		if(DynamicData != NULL)
		{
			delete DynamicData;
		}
	}

	int32 GetRequiredVertexCount() const
	{
		return (NumSegments + 1) * (NumSides + 1);
	}

	int32 GetRequiredIndexCount() const
	{
		return (NumSegments * NumSides * 2) * 3;
	}

	int32 GetVertIndex(int32 AlongIdx, int32 AroundIdx) const
	{
		return (AlongIdx * (NumSides+1)) + AroundIdx;
	}

	void BuildCableMesh(const TArray<FVector>& InPoints, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices)
	{
		const FColor VertexColor(255,255,255);
		const int32 NumPoints = InPoints.Num();
		const int32 SegmentCount = NumPoints-1;

		// Build vertices

		// We double up the first and last vert of the ring, because the UVs are different
		int32 NumRingVerts = NumSides+1;

		// For each point along spline..
		for(int32 PointIdx=0; PointIdx<NumPoints; PointIdx++)
		{
			const float AlongFrac = (float)PointIdx/(float)SegmentCount; // Distance along cable

			// Find direction of cable at this point, by averaging previous and next points
			const int32 PrevIndex = FMath::Max(0, PointIdx-1);
			const int32 NextIndex = FMath::Min(PointIdx+1, NumPoints-1);
			const FVector ForwardDir = (InPoints[NextIndex] - InPoints[PrevIndex]).GetSafeNormal();

			// Find a side vector at this point
			const FVector WorldUp(0,0,1);
			const FVector RightDir = (WorldUp ^ ForwardDir).GetSafeNormal();

			// Find an up vector
			const FVector UpDir = (ForwardDir ^ RightDir).GetSafeNormal();

			// Generate a ring of verts
			for(int32 VertIdx = 0; VertIdx<NumRingVerts; VertIdx++)
			{
				const float AroundFrac = float(VertIdx)/float(NumSides);
				// Find angle around the ring
				const float RadAngle = 2.f * PI * AroundFrac;
				// Find direction from center of cable to this vertex
				const FVector OutDir = (FMath::Cos(RadAngle) * UpDir) + (FMath::Sin(RadAngle) * RightDir);

				FDynamicMeshVertex Vert;
				Vert.Position = InPoints[PointIdx] + (OutDir * 0.5f * CableWidth);
				Vert.TextureCoordinate = FVector2D(AlongFrac * TileMaterial, AroundFrac);
				Vert.Color = VertexColor;
				Vert.SetTangents(ForwardDir, OutDir ^ ForwardDir, OutDir);
				OutVertices.Add(Vert);
			}
		}

		// Build triangles
		for(int32 SegIdx=0; SegIdx<SegmentCount; SegIdx++)
		{
			for(int32 SideIdx=0; SideIdx<NumSides; SideIdx++)
			{
				int32 TL = GetVertIndex(SegIdx, SideIdx);
				int32 BL = GetVertIndex(SegIdx, SideIdx+1);
				int32 TR = GetVertIndex(SegIdx+1, SideIdx);
				int32 BR = GetVertIndex(SegIdx+1, SideIdx+1);

				OutIndices.Add(TL);
				OutIndices.Add(BL);
				OutIndices.Add(TR);

				OutIndices.Add(TR);
				OutIndices.Add(BL);
				OutIndices.Add(BR);
			}
		}
	}

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(FCableDynamicData* NewDynamicData)
	{
		check(IsInRenderingThread());

		// Free existing data if present
		if(DynamicData)
		{
			delete DynamicData;
			DynamicData = NULL;
		}
		DynamicData = NewDynamicData;

		// Build mesh from cable points
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		BuildCableMesh(NewDynamicData->CablePoints, Vertices, Indices);

		check(Vertices.Num() == GetRequiredVertexCount());
		check(Indices.Num() == GetRequiredIndexCount());

		void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, &Vertices[0], Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);

		void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
		FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_CableSceneProxy_GetDynamicMeshElements );

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy(IsSelected());
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = GetRequiredIndexCount()/3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = GetRequiredVertexCount();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	UMaterialInterface* Material;
	FCableVertexBuffer VertexBuffer;
	FCableIndexBuffer IndexBuffer;
	FCableVertexFactory VertexFactory;

	FCableDynamicData* DynamicData;

	FMaterialRelevance MaterialRelevance;

	int32 NumSegments;

	float CableWidth;

	int32 NumSides;

	float TileMaterial;
};



//////////////////////////////////////////////////////////////////////////

UCableComponent::UCableComponent( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;

	CableWidth = 10.f;
	NumSegments = 10;
	NumSides = 4;
	EndLocation = FVector(0,0,0);
	CableLength = 100.f;
	SubstepTime = 0.02f;
	SolverIterations = 1;
	TileMaterial = 1.f;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FPrimitiveSceneProxy* UCableComponent::CreateSceneProxy()
{
	return new FCableSceneProxy(this);
}

int32 UCableComponent::GetNumMaterials() const
{
	return 1;
}

void UCableComponent::OnRegister()
{
	Super::OnRegister();

	const int32 NumParticles = NumSegments+1;

	Particles.Reset();
	Particles.AddUninitialized(NumParticles);

	FVector CableStart, CableEnd;
	GetEndPositions(CableStart, CableEnd);

	const FVector Delta = CableEnd - CableStart;

	for(int32 ParticleIdx=0; ParticleIdx<NumParticles; ParticleIdx++)
	{
		FCableParticle& Particle = Particles[ParticleIdx];

		const float Alpha = (float)ParticleIdx/(float)NumSegments;
		const FVector InitialPosition = CableStart + (Alpha * Delta);

		Particle.Position = InitialPosition;
		Particle.OldPosition = InitialPosition;

		// fix particles at ends
		if(ParticleIdx == 0 || ParticleIdx == NumParticles-1)
		{
			Particle.bFree = false;
		}
		else
		{
			Particle.bFree = true;
		}
	}
}

void UCableComponent::VerletIntegrate(float InSubstepTime, const FVector& Gravity)
{
	const int32 NumParticles = NumSegments+1;
	const float SubstepTimeSqr = InSubstepTime * InSubstepTime;

	for(int32 ParticleIdx=0; ParticleIdx<NumParticles; ParticleIdx++)
	{
		FCableParticle& Particle = Particles[ParticleIdx];
		if(Particle.bFree)
		{
			const FVector Vel = Particle.Position - Particle.OldPosition;
			const FVector NewPosition = Particle.Position + Vel + (SubstepTimeSqr * Gravity);

			Particle.OldPosition = Particle.Position;
			Particle.Position = NewPosition;
		}
	}
}

/** Solve a single distance constraint between a pair of particles */
static void SolveDistanceConstraint(FCableParticle& ParticleA, FCableParticle& ParticleB, float DesiredDistance)
{
	// Find current vector between particles
	FVector Delta = ParticleB.Position - ParticleA.Position;
	// 
	float CurrentDistance = Delta.Size();
	float ErrorFactor = (CurrentDistance - DesiredDistance)/CurrentDistance;

	// Only move free particles to satisfy constraints
	if(ParticleA.bFree && ParticleB.bFree)
	{
		ParticleA.Position += ErrorFactor * 0.5f * Delta;
		ParticleB.Position -= ErrorFactor * 0.5f * Delta;
	}
	else if(ParticleA.bFree)
	{
		ParticleA.Position += ErrorFactor * Delta;
	}
	else if(ParticleB.bFree)
	{
		ParticleB.Position -= ErrorFactor * Delta;
	}
}

void UCableComponent::SolveConstraints()
{
	const float SegmentLength = CableLength/(float)NumSegments;

	// For each iteration..
	for(int32 IterationIdx=0; IterationIdx<SolverIterations; IterationIdx++)
	{
		// For each segment..
		for(int32 SegIdx=0; SegIdx<NumSegments; SegIdx++)
		{
			FCableParticle& ParticleA = Particles[SegIdx];
			FCableParticle& ParticleB = Particles[SegIdx+1];
			// Solve for this pair of particles
			SolveDistanceConstraint(ParticleA, ParticleB, SegmentLength);
		}
	}
}

void UCableComponent::PerformSubstep(float InSubstepTime, const FVector& Gravity)
{
	VerletIntegrate(InSubstepTime, Gravity);
	SolveConstraints();
}

void UCableComponent::SetAttachEndTo(AActor* Actor, FName ComponentProperty)
{
	AttachEndTo.OtherActor = Actor;
	AttachEndTo.ComponentProperty = ComponentProperty;
}

AActor* UCableComponent::GetAttachedActor() const
{
	return AttachEndTo.OtherActor;
}

USceneComponent* UCableComponent::GetAttachedComponent() const
{
	return AttachEndTo.GetComponent(GetOwner());
}

void UCableComponent::GetEndPositions(FVector& OutStartPosition, FVector& OutEndPosition)
{
	// Start position is just component position
	OutStartPosition = GetComponentLocation();

	// See if we want to attach the other end to some other component
	USceneComponent* EndComponent = AttachEndTo.GetComponent(GetOwner());
	if(EndComponent == NULL)
	{
		EndComponent = this;
	}

	OutEndPosition = EndComponent->ComponentToWorld.TransformPosition(EndLocation);
}

void UCableComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FVector Gravity = FVector(0, 0, GetWorld()->GetGravityZ());

	// Update end points
	FVector CableStart, CableEnd;
	GetEndPositions(CableStart, CableEnd);

	FCableParticle& StartParticle = Particles[0];
	StartParticle.Position = StartParticle.OldPosition = CableStart;

	FCableParticle& EndParticle = Particles[NumSegments];
	EndParticle.Position = EndParticle.OldPosition = CableEnd;

	// Ensure a non-zero substep
	float UseSubstep = FMath::Max(SubstepTime, 0.005f);

	// Perform simulation substeps
	TimeRemainder += DeltaTime;
	while(TimeRemainder > UseSubstep)
	{
		PerformSubstep(UseSubstep, Gravity);
		TimeRemainder -= UseSubstep;
	}

	// Need to send new data to render thread
	MarkRenderDynamicDataDirty();

	// Call this because bounds have changed
	UpdateComponentToWorld();
};

void UCableComponent::SendRenderDynamicData_Concurrent()
{
	if(SceneProxy)
	{
		// Allocate cable dynamic data
		FCableDynamicData* DynamicData = new FCableDynamicData;

		// Transform current positions from particles into component-space array
		int32 NumPoints = NumSegments+1;
		DynamicData->CablePoints.AddUninitialized(NumPoints);
		for(int32 PointIdx=0; PointIdx<NumPoints; PointIdx++)
		{
			DynamicData->CablePoints[PointIdx] = ComponentToWorld.InverseTransformPosition(Particles[PointIdx].Position);
		}

		// Enqueue command to send to render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FSendCableDynamicData,
			FCableSceneProxy*,CableSceneProxy,(FCableSceneProxy*)SceneProxy,
			FCableDynamicData*,DynamicData,DynamicData,
		{
			CableSceneProxy->SetDynamicData_RenderThread(DynamicData);
		});
	}
}

FBoxSphereBounds UCableComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Calculate bounding box of cable points
	FBox CableBox(0);
	for(int32 ParticleIdx=0; ParticleIdx<Particles.Num(); ParticleIdx++)
	{
		const FCableParticle& Particle = Particles[ParticleIdx];
		CableBox += Particle.Position;
	}

	// Expand by cable width
	return FBoxSphereBounds(CableBox.ExpandBy(CableWidth));
}

