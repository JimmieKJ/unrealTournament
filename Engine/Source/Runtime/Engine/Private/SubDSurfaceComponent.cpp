// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/SubDSurfaceComponent.h"
#include "DynamicMeshBuilder.h"
#include "Engine/SubDSurface.h"
#include "Engine/Font.h"
#include "Engine/SubDSurfaceActor.h"
#include "LocalVertexFactory.h"

#define VS2013OR2015 (_MSC_VER == 1800 || _MSC_VER == 1900)

// 0/1 - we have libs pre compiled for VS2013 and V2015 in 64bit - later we can add more or remove the dependency to OpenSubDiv
// RawMesh is only available when using editor builds - later we want to remove this dependency (useful at the moment to compare to vertex cache optimized meshes)
#define USE_OPENSUBDIV (VS2013OR2015 && _WIN64 && WITH_EDITORONLY_DATA)

#if USE_OPENSUBDIV

#include "RawMesh.h"	//  fatal error C1083: Cannot open include file: 'RawMesh.h': No such file or directory  (when RawMesh is not part of PrivateDependencyModuleNames in Engine.Build.cs)

// Disable warning C4191: 'type cast' : unsafe conversion
#pragma warning(push)
#pragma warning(disable:4191)

#define M_PI PI
#define or ||
#define and &&
#define not !

//	#include "osd/cpuComputeContext.h"
//	#include "osd/cpuComputeController.h"
	#include "osd/cpuVertexBuffer.h"
	#include <far/topologyRefinerFactory.h>
	#include <far/topologyDescriptor.h>
	#include <far/primvarRefiner.h>

// from RefineAdaptive?
	#include <far/patchTableFactory.h>
	#include <far/patchMap.h>
	#include <far/ptexIndices.h>

	#include <far/stencilTableFactory.h>

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable : 6011)  // Dereferencing NULL pointer 'X'
	#pragma warning(disable : 6308)  // 'realloc' might return null pointer: assigning null pointer to 'X', which is passed as an argument to 'realloc', will cause the original memory block to be leaked.
	#pragma warning(disable : 6385)  // Reading invalid data from 'X':  the readable size is 'Y' bytes, but 'Z' bytes may be read.
	#pragma warning(disable : 6386)  // Buffer overrun while writing to 'X':  the writable size is 'Y' bytes, but 'Z' bytes might be written.
	#pragma warning(disable : 28182) // Dereferencing NULL pointer. 'X' contains the same NULL value as 'Y' did.
#endif

// for non manifold
	#include <hbr/mesh.h>
	#include <hbr/catmark.h>

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif

#undef and
#undef not
#undef M_PI

// Restore warning C4191.
#pragma warning(pop)

using namespace OpenSubdiv;

#endif // USE_OPENSUBDIV

uint32 USubDSurface::GetVertexCount() const
{
	if(!VertexAttributeStreams.Num())
	{
		return 0;
	}

	// assumes are all the same size
	return VertexAttributeStreams[0]->Num();
}

UVertexAttributeStream* USubDSurface::CreateVertexAttributeStream(FName InUsage)
{
	// ensure the stream wasn't there before
	check(!FindStreamByUsage(InUsage));

	uint32 Num = VertexAttributeStreams.Num();

//	VertexAttributeStreams* Ret = new UVertexAttributeStream();
//	VertexAttributeStreams.Add(Ret);
	
	UVertexAttributeStream* VertexAttributeStream = NewObject<UVertexAttributeStream>(this); 

	VertexAttributeStream->Usage = InUsage;

	VertexAttributeStreams.Add(VertexAttributeStream);
//	VertexAttributeStreams.Add(new UVertexAttributeStream(InUsage)); todo

	return VertexAttributeStreams[Num];
}

UVertexAttributeStream* USubDSurface::FindStreamByUsage(FName InUsage)
{
	for(UVertexAttributeStream* it : VertexAttributeStreams)
	{
		if(it->Usage == InUsage)
		{
			return it;
		}
	}

	return 0;
}

// -------------------------------------------------------------------

ASubDSurfaceActor::ASubDSurfaceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayMeshComponent = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("DisplayMeshComponent"));
	RootComponent = DisplayMeshComponent;

	SubDSurface = ObjectInitializer.CreateDefaultSubobject<USubDSurfaceComponent>(this, TEXT("NewSubDSurfaceComponent"));
	SubDSurface->SetDisplayMeshComponent(DisplayMeshComponent);

	SubDSurface->AttachToComponent(DisplayMeshComponent, FAttachmentTransformRules::KeepRelativeTransform);
//	bool USceneComponent::AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName)
//	SubDSurface->AttachParent = DisplayMeshComponent;

//	ProcMeshComponent = ObjectInitializer.CreateDefaultSubobject<UProceduralMeshComponent>(this, TEXT("NewProceduralMeshComponent"));

#if WITH_EDITORONLY_DATA
	SpriteComponent = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));

	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SubDSurfaceTexture;
			FConstructorStatics()
				: SubDSurfaceTexture(TEXT("/Engine/EditorResources/S_Terrain.S_Terrain"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.SubDSurfaceTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		
		SpriteComponent->AttachToComponent(SubDSurface, FAttachmentTransformRules::KeepRelativeTransform);
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->bAbsoluteScale = true;
		SpriteComponent->bReceivesDecals = false;
	}
#endif
}


// for OpenSubDiv to interpolate positions
struct FVertexPosition
{
	// Minimal required interface ----------------------
	FVertexPosition() { }
	
	FVertexPosition(int /*i*/) { }

/*
	FVertexPosition(FVertexPosition const & src)
	{
		_position[0] = src._position[0];
		_position[1] = src._position[1];
		_position[1] = src._position[1];
	}
*/

	void Clear( void * =0 )
	{
		point = FVector(0, 0, 0);
	}

	void AddWithWeight(FVertexPosition const & src, float weight)
	{
		point += weight * src.point;
	}

	void AddVaryingWithWeight(FVertexPosition const &, float) { }

	FVector point;
};

// for OpenSubDiv to interpolate UV
struct FVarVertexUV
{
    // Minimal required interface ----------------------
    void Clear()
	{
        UV = FVector2D(0.0f, 0.0f);
    }

    void AddWithWeight(FVarVertexUV const & src, float weight)
	{
        UV += weight * src.UV;
    }

    // Basic 'uv' layout channel
    FVector2D UV;
};

// for OpenSubDiv to compute the tangent space (similar to here: http://graphics.pixar.com/opensubdiv/docs/far_tutorial_6.html)
struct FLimitFrame
{
	void Clear( void * = 0 )
	{
		point = FVector(0, 0, 0);
        deriv1 = FVector(0, 0, 0);
        deriv2 = FVector(0, 0, 0);
    }

    void AddWithWeight(FVertexPosition const & src, float weight, float d1Weight, float d2Weight)
	{
        point += weight * src.point;
        deriv1 += d1Weight * src.point;
        deriv2 += d2Weight * src.point;
    }

	FVector point;
    FVector deriv1;
	FVector deriv2;
};

// ------------------------------------------------------

USubDSurfaceComponent::USubDSurfaceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
/*
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UMaterial> TextMaterial;
		FConstructorStatics()
			: TextMaterial(TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	TextMaterial = ConstructorStatics.TextMaterial.Get();
*/

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;
}


#if WITH_EDITOR
void USubDSurfaceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if( PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USubDSurfaceComponent, DebugLevel) || 
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USubDSurfaceComponent, Mesh))
	{
		SetMesh(Mesh);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR


bool USubDSurfaceComponent::SetMesh(USubDSurface* NewMesh)
{
	// Do nothing if we are already using the supplied static mesh
/*
	// good for blueprint code, not good for editor update
	if(NewMesh == Mesh)
	{
		return false;
	}
*/

/*
	// Don't allow changing static meshes if "static" and registered
	AActor* Owner = GetOwner();
	if(Mobility == EComponentMobility::Static && IsRegistered() && Owner != NULL)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetMeshOnStatic", "Calling SetStaticMesh on '{0}' but Mobility is Static."), 
			FText::FromString(GetPathName(this))));
		return false;
	}
*/
	double StartTime = FPlatformTime::Seconds();

	Mesh = NewMesh;

	// Need to send this to render thread at some point
//	MarkRenderStateDirty();

	RecreateMeshData();

	// Update physics representation right away
	RecreatePhysicsState();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	IStreamingManager::Get().NotifyPrimitiveAttached( this, DPT_Spawned );

	// Since we have new mesh, we need to update bounds
	UpdateBounds();

	UE_LOG(LogEngine, Log, TEXT("SubDSurface update: %5.2f seconds"), FPlatformTime::Seconds() - StartTime);

	return true;
}

#if USE_OPENSUBDIV
typedef OpenSubdiv::HbrMesh<FVertexPosition>      Hmesh;
typedef OpenSubdiv::HbrFace<FVertexPosition>      Hface;
typedef OpenSubdiv::HbrVertex<FVertexPosition>    Hvertex;
typedef OpenSubdiv::HbrHalfedge<FVertexPosition>  Hhalfedge;
typedef Far::TopologyDescriptor Descriptor;


void TestNonManiFold(USubDSurface* Mesh, const Descriptor& desc)
{
	FVector* PosData;
	{
		UVertexAttributeStream* Stream = Mesh->FindStreamByUsage(FName(TEXT("Position")));

		check(Stream);

		uint32 VertexCount = 0;
		PosData = Stream->GetFVector(VertexCount);
	}


	OpenSubdiv::HbrCatmarkSubdivision<FVertexPosition> * catmark = new OpenSubdiv::HbrCatmarkSubdivision<FVertexPosition>();

    Hmesh * hmesh = new Hmesh(catmark);

    for (int32 i = 0, count = Mesh->GetVertexCount(); i < count; ++i)
	{
		FVertexPosition v;

		v.point = PosData[i];

        hmesh->NewVertex(i, v);
    }

    // Create the topology
    const int32* fv = desc.vertIndicesPerFace;

    for (int32 i = 0; i < desc.numFaces; ++i)
	{
        int nv = desc.numVertsPerFace[i];

        bool bValid = true;

        for(int j = 0; j < nv; j++)
		{

            Hvertex const * origin      = hmesh->GetVertex(fv[j]),
                          * destination = hmesh->GetVertex(fv[(j+1)%nv]);

			check(destination);

            Hhalfedge const * opposite = destination->GetEdge(origin);

            // Make sure that the vertices exist in the mesh
            if (origin==NULL || destination==NULL)
			{
                printf(" An edge was specified that connected a nonexistent vertex\n");
                bValid = false;
                break;
            }

            // Check for a degenerate edge
            if (origin == destination)
			{
                printf(" An edge was specified that connected a vertex to itself\n");
                bValid = false;
                break;
            }

            // Check that no more than 2 faces are adjacent to the edge
            if (opposite && opposite->GetOpposite() )
			{
                printf(" A non-manifold edge incident to more than 2 faces was found\n");
                bValid = false;
                break;
            }

            // Check that the edge is unique and oriented properly
            if (origin->GetEdge(destination))
			{
                printf(" An edge connecting two vertices was specified more than once."
                       " It's likely that an incident face was flipped\n");
                bValid = false;
                break;
            }
        }

        if (bValid)
		{
            hmesh->NewFace(nv, fv, 0);
        }
		else
		{
            printf(" Skipped face %d\n", i);
        }

        fv += nv;
    }

    hmesh->SetInterpolateBoundaryMethod(Hmesh::k_InterpolateBoundaryEdgeOnly);

    hmesh->Finish();

    printf("Created a fan with %d faces and %d vertices.\n", hmesh->GetNumFaces(), hmesh->GetNumVertices());

    delete hmesh;
    delete catmark;
}
#endif // USE_OPENSUBDIV


// pyramid geometry from catmark_pyramid_crease0.h
static int const g_nverts = 5;
static float const g_verts[24] = { 0.0f,  0.0f, 2.0f,
                                   0.0f, -2.0f, 0.0f,
                                   2.0f,  0.0f, 0.0f,
                                   0.0f,  2.0f, 0.0f,
                                  -2.0f,  0.0f, 0.0f, };


static int const g_vertsperface[5] = { 3, 3, 3, 3, 4 };

static int const g_nfaces = 5;
static int const g_faceverts[16] = { 0, 1, 2,
                                     0, 2, 3,
                                     0, 3, 4,
                                     0, 4, 1,
                                     4, 3, 2, 1 };

static int const g_ncreases = 4;
static int const g_creaseverts[8] = { 4, 3, 3, 2, 2, 1, 1, 4 };
static float const g_creaseweights[4] = { 3.0f, 3.0f, 3.0f, 3.0f };


// compute the 3x3 tangent space (3 vectors) from 2 given delta position vectors and the UV delta change on those vectors
// The interface is more general to make this usable for more than triangles.
// for a triangle you would have to compute the input like this:
//   InDeltaPos[0]=InPos[1]-InPos[0], InDeltaPos[1]=InPos[2]-InPos[0]
//   InDeltaUV[0]=InUV[1]-InUV[0], InDeltaUV[1]=InUV[2]-InUV[0]
// degenerated cases are not handled yet
void ComputeTangents(FVector OutTangentXYZ[3], FVector InDeltaPos[2], FVector2D InDeltaUV[2])
{
	// originally from: Lengyel, Eric. “Computing Tangent Space Basis Vectors for an Arbitrary Mesh”. Terathon Software 3D Graphics Library, 2001. http://www.terathon.com/code/tangent.html

	float x1 = InDeltaPos[0].X;
    float x2 = InDeltaPos[1].X;
    float y1 = InDeltaPos[0].Y;
    float y2 = InDeltaPos[1].Y;
    float z1 = InDeltaPos[0].Z;
    float z2 = InDeltaPos[1].Z;
        
    float s1 = InDeltaUV[0].X;
    float s2 = InDeltaUV[1].X;
    float t1 = InDeltaUV[0].Y;
    float t2 = InDeltaUV[1].Y;

	float deter = s1 * t2 - s2 * t1;
        
	// no need as we normalize anyway
//    float r = 1.0f / deter;
    float r = 1.0f;

    OutTangentXYZ[0] = FVector(t2 * x1 - t1 * x2, t2 * y1 - t1 * y2, t2 * z1 - t1 * z2) * r;
    OutTangentXYZ[1] = FVector(s1 * x2 - s2 * x1, s1 * y2 - s2 * y1, s1 * z2 - s2 * z1) * r;

	OutTangentXYZ[0] = OutTangentXYZ[0].GetSafeNormal();
	OutTangentXYZ[1] = OutTangentXYZ[1].GetSafeNormal();

    OutTangentXYZ[2] = (OutTangentXYZ[0] ^ OutTangentXYZ[1]).GetSafeNormal();

	if(deter < 0)
	{
		// mirrored UV changes the handiness of the matrix
		OutTangentXYZ[2] = -OutTangentXYZ[2];
	}
}


void USubDSurfaceComponent::RecreateMeshData()
{
	if(!Mesh)
	{
		if(DisplayMeshComponent)
		{
			DisplayMeshComponent->SetStaticMesh(0);
			DisplayMeshComponent->MarkRenderStateDirty();
		}
		return;
	}

//	bool bUV = false;			/// todo expose
	bool bUV = true;			/// todo expose

	double StartTime = FPlatformTime::Seconds();

	// number of n-gons
	uint32 FaceCount = Mesh->VertexCountPerFace.Num();
	
	check(FaceCount);

#if USE_OPENSUBDIV
	// ----
	Sdc::SchemeType type = OpenSubdiv::Sdc::SCHEME_CATMARK;

	Sdc::Options options;
	options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
//    options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_NONE);				// affects how UVs are interpolated

	Descriptor desc;
	desc.numVertices  = Mesh->GetVertexCount();
	desc.numFaces     = FaceCount;
	desc.numVertsPerFace = (const int32*)&Mesh->VertexCountPerFace[0];
	desc.vertIndicesPerFace = (const Far::Index *)Mesh->IndicesPerFace.GetData();

/*
	// hack
    Descriptor desc;
    desc.numVertices = g_nverts;
    desc.numFaces = g_nfaces;
    desc.numVertsPerFace = g_vertsperface;
    desc.vertIndicesPerFace = g_faceverts;
	bUV = false;
	// hack end
*/
/*	// debug

	TArray<uint32> TestValency;
	TestValency.AddZeroed(desc.numVertices);
	{
		int32* IndexCountBuffer = (int32*)desc.numVertsPerFace;
		int32* IndexBuffer = (int32*)desc.vertIndicesPerFace;
		for (uint32 Face = 0; Face < (uint32)desc.numFaces; ++Face)
		{
			uint32 CornerCount = *IndexCountBuffer++;
			for (uint32 Corner = 0; Corner < CornerCount; ++Corner)
			{
				uint32 VertexIndex = *IndexBuffer++;

				TestValency[VertexIndex]++;
			}
		}
	}
*/

	TArray<uint32> PerCornerIndexBuffer;
    Descriptor::FVarChannel channels[1];
    int32 channelUV = 0;

	if(bUV)
	{
		PerCornerIndexBuffer.Empty(Mesh->IndicesPerFace.Num());

		uint32 UniqueIndex = 0;

		for(uint32 Face = 0; Face < FaceCount; ++Face)
		{
			uint32 Corners = Mesh->VertexCountPerFace[Face];

			for(uint32 i = 0; i < Corners; ++i)
			{
				PerCornerIndexBuffer.Add(UniqueIndex++);
			}
		}

		// Add the channel topology to the main descriptor

		// Create a face-varying channel descriptor
		channels[channelUV].numValues = PerCornerIndexBuffer.Num();			// for now we map the UV by the face
		channels[channelUV].valueIndices = (Far::Index const *)PerCornerIndexBuffer.GetData();

		desc.numFVarChannels = 1;
		desc.fvarChannels = channels;
	}

	TestNonManiFold(Mesh, desc);

	int32 maxlevel = FMath::Clamp((int32)DebugLevel, 1, 6);

	// hack
//	maxlevel = 3;

	// Instantiate a FarTopologyRefiner from the descriptor
	Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                         Far::TopologyRefinerFactory<Descriptor>::Options(type, options));

	{
		// Uniformly refine the topology up to 'maxlevel'
		Far::TopologyRefiner::UniformOptions refineOptions(maxlevel);

		// note: fullTopologyInLastLevel must be true to work with face-varying data
		refineOptions.fullTopologyInLastLevel = true;

		// simpler but we don't get good normals
		refiner->RefineUniform(refineOptions);
	}

	// cage mesh vertex count
    int CoarseVerts = desc.numVertices;
	// most refined level vertex count
	int FineVerts   = refiner->GetLevel(maxlevel).GetNumVertices();
	// vertex count of all levels [cage mesh, intermediate levels, most refined level]
    int TotalVerts  = refiner->GetNumVerticesTotal();
//    int TempVerts = TotalVerts - CoarseVerts - FineVerts;

	// intermediate position for SubDiv process, does not have the limit positions and not the last level
	// UVs for [cage mesh, intermediate levels, but not most refined level]
	TArray<FVertexPosition> TempPosBuffer;

//	TempPosBuffer.AddUninitialized(refiner->GetNumVerticesTotal());
	TempPosBuffer.AddUninitialized(TotalVerts - FineVerts);

	FVertexPosition* verts = (FVertexPosition*)TempPosBuffer.GetData();

	// setup position
	{
		FVector* PosData = 0;
		{
			UVertexAttributeStream* Stream = Mesh->FindStreamByUsage(FName(TEXT("Position")));

			check(Stream);

			uint32 VertexCount = 0;
			PosData = Stream->GetFVector(VertexCount);

			check(VertexCount == desc.numVertices);
		}

		// Initialize coarse mesh positions
		for (uint32 i = 0, Count = (uint32)desc.numVertices; i < Count; ++i)
		{
			FVector Pos = PosData[i];

			verts[i].point = Pos;
		}
	}

	// Interpolate vertex primvar data
    Far::PrimvarRefiner primvarRefiner(*refiner);

	// position
	FVertexPosition* src = verts;

	// interpolate position [not the cage mesh, intermediate levels, most refined level]
	for(int32 level = 1; level < maxlevel; ++level)
	{
		FVertexPosition* dst = src + refiner->GetLevel(level - 1).GetNumVertices();

		primvarRefiner.Interpolate(level, src, dst);

		src = dst;
	}

	// SubD Position for [most refined level]
	TArray<FVertexPosition> finePosBuffer;
	finePosBuffer.AddUninitialized(FineVerts);

	// Limit Position for [most refined level]
	TArray<FVertexPosition> fineLimitPos;
	fineLimitPos.AddUninitialized(FineVerts);

	// Patch Derivative in X for [most refined level]
    TArray<FVertexPosition> fineDu;
	fineDu.AddUninitialized(FineVerts);

	// Patch Derivative in Y for [most refined level]
    TArray<FVertexPosition> fineDv;
	fineDv.AddUninitialized(FineVerts);

	// test
	// Limit Position for [most refined level]
	TArray<FVarVertexUV> fineLimitUV;
	TArray<FVarVertexUV> fineLimitUVDu;
	TArray<FVarVertexUV> fineLimitUVDv;
	fineLimitUV.AddUninitialized(refiner->GetLevel(maxlevel).GetNumFVarValues());
	fineLimitUVDu.AddUninitialized(refiner->GetLevel(maxlevel).GetNumFVarValues());
	fineLimitUVDv.AddUninitialized(refiner->GetLevel(maxlevel).GetNumFVarValues());

	// interpolate SubD position [most refined level]
    primvarRefiner.Interpolate(maxlevel, src, finePosBuffer);
	// interpolate limit position [most refined level]
    primvarRefiner.Limit(finePosBuffer, fineLimitPos, fineDu, fineDv);


	// UVs for [cage mesh, intermediate levels, most refined level]
	TArray<FVarVertexUV> fvBufferUV;
	if(bUV)
	{
		fvBufferUV.AddUninitialized(refiner->GetNumFVarValuesTotal(channelUV));
		FVarVertexUV* fvVertsUV = fvBufferUV.GetData();

		uint32 CageUVCount = 0;

		// setup UV
		{
			FVector2D* UVData = 0;
			{
				UVertexAttributeStream* Stream = Mesh->FindStreamByUsage(FName(TEXT("UV0")));

				check(Stream);

				UVData = Stream->GetFVector2D(CageUVCount);
			}

			for(uint32 i = 0; i < CageUVCount; ++i)
			{
				fvVertsUV[i].UV = UVData[i];
			}
		}
			
		{
			FVarVertexUV* srcUV = &fvVertsUV[0];

			uint32 Level0NumVertices = refiner->GetLevel(0).GetNumFVarValues();

			check(CageUVCount == Level0NumVertices);
			
			// interpolate UV [not the cage mesh, intermediate levels, most refined level]
			for (int32 level = 1; level <= maxlevel; ++level)
			{
				FVarVertexUV* dstUV = srcUV + refiner->GetLevel(level - 1).GetNumFVarValues();

				// FaceVarying: see http://graphics.pixar.com/opensubdiv/docs/subdivision_surfaces.html#face-varying-interpolation-rules
				primvarRefiner.InterpolateFaceVarying(level, srcUV, dstUV);

				srcUV = dstUV;
			}
		}

		auto fineLimitUVPtr =&fineLimitUV[0];

		// test
		// interpolate limit position [most refined level]
//		primvarRefiner.Limit(&fvBufferUV[refiner->GetNumFVarValuesTotal(channelUV) - refiner->GetLevel(maxlevel).GetNumFVarValues()], fineLimitUV, fineLimitUVDu, fineLimitUVDv);
		primvarRefiner.LimitFaceVarying(&fvBufferUV[refiner->GetNumFVarValuesTotal(channelUV) - refiner->GetLevel(maxlevel).GetNumFVarValues()], fineLimitUVPtr, channelUV);
	}

	
    // Create a Far::PtexIndices to help find indices of ptex faces.

#if 1	// RAWMESH

    Far::PtexIndices ptexIndices(*refiner);
	FRawMesh RawMesh;

	// Output of the highest level refined -----------
    {
        Far::TopologyLevel const & refLastLevel = refiner->GetLevel(maxlevel);

        int32 nverts = refLastLevel.GetNumVertices();
        int32 nfaces = refLastLevel.GetNumFaces();

		uint32 TriangleCount = PerCornerIndexBuffer.Num();

		// reserve to avoid memory reallocations
		RawMesh.VertexPositions.Empty(nverts);
		RawMesh.WedgeIndices.Empty(TriangleCount * 3);
		RawMesh.FaceMaterialIndices.Empty(TriangleCount);
		RawMesh.FaceSmoothingMasks.Empty(TriangleCount);
		
		int32 firstOfLastUvs = 0;

		if(bUV)
		{
			int32 nuvs = refLastLevel.GetNumFVarValues(channelUV);
			RawMesh.WedgeTexCoords[0].Empty(TriangleCount * 3);		// our FRawMesh wants it like this
			firstOfLastUvs = refiner->GetNumFVarValuesTotal(channelUV) - nuvs;
		}

		int32 firstOfLastVerts = refiner->GetNumVerticesTotal() - nverts;

		for(int32 vert = 0; vert < nverts; ++vert)
		{
			FVector pos = fineLimitPos[vert].point;

			RawMesh.VertexPositions.Add(pos);
		}
	
		{
			uint32 StartIndexForHighestLevel = 0; //- refiner->GetNumVertices(maxlevel);
			FVarVertexUV* fvVertsUV = fvBufferUV.GetData();

			for (int32 face = 0; face < nfaces; ++face)
			{
				Far::ConstIndexArray fverts = refLastLevel.GetFaceVertices(face);

				// all refined Catmark faces should be quads
				uint32 CornerCount = fverts.size();

				// one n-gon
				for(uint32 Corner = 0; Corner < CornerCount; ++Corner)
				{
					// one triangle
					RawMesh.FaceMaterialIndices.Add(0);
					RawMesh.FaceSmoothingMasks.Add(0xffffffff);

					uint32 CornerIds[] = { 0, (Corner + 1) % CornerCount, (Corner + 2) % CornerCount };

					// convert to triangles by spawning 3 vertices
					for(uint32 i = 0 ; i < 3; ++i)
					{
						uint32 LocalCornerId = CornerIds[i];

						RawMesh.WedgeIndices.Add(fverts[LocalCornerId] + StartIndexForHighestLevel);
					}

					if(!bUV)
					{
						for(uint32 i = 0 ; i < 3; ++i)
						{
							RawMesh.WedgeTexCoords[0].Add(FVector2D(0,0));
						}
					}
				}
			}
			if (bUV)
			{
				for(int32 face = 0; face < nfaces; ++face)
				{
					Far::ConstIndexArray fverts = refLastLevel.GetFaceVertices(face);
					Far::ConstIndexArray fuvs = refLastLevel.GetFaceFVarValues(face, channelUV);

					// all refined Catmark faces should be quads
					uint32 CornerCount = fverts.size();

					// one n-gon
					for (uint32 Corner = 0; Corner < CornerCount; ++Corner)
					{
						uint32 CornerIds[] = { 0, (Corner + 1) % CornerCount, (Corner + 2) % CornerCount };
						
						// convert to triangles by spawning 3 vertices
						for(uint32 i = 0 ; i < 3; ++i)
						{
							uint32 LocalCornerId = CornerIds[i];
							uint32 UVIndex = fuvs[LocalCornerId];
							uint32 VertexIndex = fverts[LocalCornerId];

//							RawMesh.WedgeTexCoords[0].Add(fvVertsUV[firstOfLastUvs + UVIndex].UV);
							RawMesh.WedgeTexCoords[0].Add(fineLimitUV[UVIndex].UV);

							FVector TangentXYZ[3];
							FVector DeltaPos[2] = { fineDu[VertexIndex].point, fineDv[VertexIndex].point };
							FVector2D DeltaUV[2] = { fineLimitUVDu[UVIndex].UV, fineLimitUVDv[UVIndex].UV };
							
							ComputeTangents(TangentXYZ, DeltaPos, DeltaUV);

/*
							FVector TangentX = (fineDu[VertexIndex].point).GetSafeNormal();
							FVector TangentY = (fineDv[VertexIndex].point).GetSafeNormal();
							FVector TangentZ = (TangentY ^ TangentX).GetSafeNormal();

							RawMesh.WedgeTangentX.Add(TangentX);
							RawMesh.WedgeTangentY.Add(TangentY);
							RawMesh.WedgeTangentZ.Add(TangentZ);
*/
							RawMesh.WedgeTangentX.Add(TangentXYZ[0]);
							RawMesh.WedgeTangentY.Add(TangentXYZ[1]);
							RawMesh.WedgeTangentZ.Add(TangentXYZ[2]);
						}
					}
				}
/*

				for (int32 vert = 0; vert < nverts; ++vert)
				{
					FVector TangentX = (fineDu[vert].point).GetSafeNormal();
					FVector TangentY = (fineDv[vert].point).GetSafeNormal();
					FVector TangentZ = (TangentX ^ TangentY).GetSafeNormal();

					RawMesh.WedgeTangentX.Add(TangentX);
					RawMesh.WedgeTangentY.Add(TangentY);
					RawMesh.WedgeTangentZ.Add(TangentZ);
				}*/
			}
		}
    }

	check(RawMesh.IsValid());

	UE_LOG(LogEngine, Log, TEXT("SubDSurface OpenSubDiv: %5.2f seconds"), FPlatformTime::Seconds() - StartTime);

	UStaticMesh* GeneratedMesh = 0;

	// see UStaticMesh* FAbcImporter::ImportSingleAsStaticMesh(const int32 MeshIndex, UObject* InParent, EObjectFlags Flags)

	// Setup static mesh instance
	GeneratedMesh = NewObject<UStaticMesh>(this);

	// Generate a new lighting GUID (so its unique)
	GeneratedMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoord index exists for all LODs, etc).
	GeneratedMesh->LightMapResolution = 64;
	GeneratedMesh->LightMapCoordinateIndex = 1;

	// Material setup, since there isn't much material information in the Alembic file, 
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	check(DefaultMaterial);

	// Material list
//	GeneratedMesh->Materials.Empty();
	// If there were FaceSets available in the Alembic file use the number of unique face sets as num material entries, otherwise default to one material for the whole mesh
//	uint32 NumFaceSets = GetNumFaceSetsFromSchema(Schema);
//	uint32 NumMaterials = (NumFaceSets) ? NumFaceSets : 1;
//	for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex )
	{
		GeneratedMesh->Materials.Add(DefaultMaterial);
	}	

	// Add the first LOD ( QQQ change this )	
	new(GeneratedMesh->SourceModels) FStaticMeshSourceModel();

	// Get the first LOD for filling it up with geometry (QQ)
	FStaticMeshSourceModel& SrcModel = GeneratedMesh->SourceModels[0];
	// Set build settings for the static mesh (QQ want this from import options menu)
	SrcModel.BuildSettings.bRecomputeNormals = false;
	SrcModel.BuildSettings.bRecomputeTangents = true;
	// Generate Lightmaps uvs (no support for importing right now)
	
//	SrcModel.BuildSettings.bGenerateLightmapUVs = true;		// reconsider later
	SrcModel.BuildSettings.bGenerateLightmapUVs = false;
	SrcModel.BuildSettings.bBuildAdjacencyBuffer = false;	// reconsider later
	SrcModel.BuildSettings.bUseMikkTSpace = false;			// MikkT is much slower
	SrcModel.BuildSettings.bRemoveDegenerates = false;		// makes FindOverlappingCorners a tiny bit faster

	SrcModel.BuildSettings.bRecomputeNormals = false;
	SrcModel.BuildSettings.bRecomputeTangents = false;

	// Set lightmap UV index to 1 since we currently only import one set of UVs
	SrcModel.BuildSettings.DstLightmapIndex = 1;

	// Store the raw mesh within the RawMeshBulkData
	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
	
	// Build the static mesh (using the build setting etc.) this generates correct tangents using the extracting smoothing group along with the imported Normals data
	GeneratedMesh->Build(false);
	GeneratedMesh->MarkPackageDirty();

	// QQ No collision generation for now
	GeneratedMesh->CreateBodySetup();

	check(DisplayMeshComponent);

	DisplayMeshComponent->SetStaticMesh(GeneratedMesh);
	DisplayMeshComponent->MarkRenderStateDirty();


#endif // RAWMESH

#endif // USE_OPENSUBDIV
}

void USubDSurfaceComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
/* todo
	if( StaticMesh && StaticMesh->RenderData )
	{
		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); LODIndex++)
		{
			FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < LODResources.Sections.Num(); SectionIndex++)
			{
				// Get the material for each element at the current lod index
				OutMaterials.AddUnique(GetMaterial(LODResources.Sections[SectionIndex].MaterialIndex));
			}
		}
	}
*/
}

int32 USubDSurfaceComponent::GetNumMaterials() const
{
	// todo
	return 0;
}

void USubDSurfaceComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	// todo
}

UMaterialInterface* USubDSurfaceComponent::GetMaterial(int32 ElementIndex) const
{
	// todo
	return NULL;
}

bool USubDSurfaceComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	return true;
}

FBoxSphereBounds USubDSurfaceComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// todo

	FBox LocalBox(FVector(-100, -100, -100), FVector(100, 100, 100));

	FBoxSphereBounds Ret(LocalBox.TransformBy(LocalToWorld));

	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;

	return Ret;
}

void USubDSurfaceComponent::SetDisplayMeshComponent(UStaticMeshComponent* InDisplayMeshComponent)
{
	DisplayMeshComponent = InDisplayMeshComponent;
}
