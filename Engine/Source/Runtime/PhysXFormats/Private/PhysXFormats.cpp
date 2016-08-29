// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Engine.h"
#include "PhysicsPublic.h"
#include "TargetPlatform.h"
#include "IPhysXFormat.h"
#include "IPhysXFormatModule.h"
#include "PhysXFormats.h"
#include "PhysicsEngine/PhysXSupport.h"

static_assert(WITH_PHYSX, "No point in compiling PhysX cooker, if we don't have PhysX.");

static FName NAME_PhysXPC(TEXT("PhysXPC"));
static FName NAME_PhysXXboxOne(TEXT("PhysXXboxOne"));
static FName NAME_PhysXPS4(TEXT("PhysXPS4"));

/**
 * FPhysXFormats. Cooks physics data.
**/
class FPhysXFormats : public IPhysXFormat
{
	enum
	{
		/** Version for PhysX format, this becomes part of the DDC key. */
		UE_PHYSX_PC_VER = 0,
	};

	PxCooking* PhysXCooking;

	/**
	 * Validates format name and returns its PhysX enum value.
	 *
	 * @param InFormatName PhysX format name.
	 * @param OutFormat PhysX enum
	 * @return true if InFormatName is a valid and supported PhysX format
	 */
	bool GetPhysXFormat(FName InFormatName, PxPlatform::Enum& OutFormat) const
	{
		if ((InFormatName == NAME_PhysXPC) || (InFormatName == NAME_PhysXXboxOne) || (InFormatName == NAME_PhysXPS4))
		{
			OutFormat = PxPlatform::ePC;
		}
		else
		{
			return false;
		}
		return true;
	}

	/**
	 * Valudates PhysX format name.
	 */
	bool CheckPhysXFormat(FName InFormatName) const
	{
		PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
		return GetPhysXFormat(InFormatName, PhysXFormat);
	}

public:

	FPhysXFormats( PxCooking* InCooking )
		: PhysXCooking( InCooking )
	{}

	virtual bool AllowParallelBuild() const override
	{
		return false;
	}

	virtual uint16 GetVersion(FName Format) const override
	{
		check(CheckPhysXFormat(Format));
		return UE_PHYSX_PC_VER;
	}


	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_PhysXPC);
		OutFormats.Add(NAME_PhysXXboxOne);
		OutFormats.Add(NAME_PhysXPS4);
	}

	virtual EPhysXCookingResult CookConvex(FName Format, int32 RuntimeCookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer, bool bDeformableMesh = false) const override
	{
		EPhysXCookingResult CookResult = EPhysXCookingResult::Failed;

#if WITH_PHYSX
		PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
		bool bIsPhysXFormatValid = GetPhysXFormat(Format, PhysXFormat);
		check(bIsPhysXFormatValid);

		PxConvexMeshDesc PConvexMeshDesc;
		PConvexMeshDesc.points.data = SrcBuffer.GetData();
		PConvexMeshDesc.points.count = SrcBuffer.Num();
		PConvexMeshDesc.points.stride = sizeof(FVector);
		PConvexMeshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

		// Set up cooking
		const PxCookingParams Params = PhysXCooking->getParams();
		PxCookingParams NewParams = Params;
		NewParams.targetPlatform = PhysXFormat;

		if(RuntimeCookFlags & ERuntimePhysxCookOptimizationFlags::SuppressFaceRemapTable)
		{
			NewParams.suppressTriangleMeshRemapTable = true;
		}

		if (bDeformableMesh)
		{
			// Meshes which can be deformed need different cooking parameters to inhibit vertex welding and add an extra skin around the collision mesh for safety.
			// We need to set the meshWeldTolerance to zero, even when disabling 'clean mesh' as PhysX will attempt to perform mesh cleaning anyway according to this meshWeldTolerance
			// if the convex hull is not well formed.
			// Set the skin thickness as a proportion of the overall size of the mesh as PhysX's internal tolerances also use the overall size to calculate the epsilon used.
			const FBox Bounds(SrcBuffer);
			const float MaxExtent = (Bounds.Max - Bounds.Min).Size();
			NewParams.skinWidth = MaxExtent / 512.0f;
			NewParams.meshPreprocessParams = PxMeshPreprocessingFlags(PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
			NewParams.areaTestEpsilon = 0.0f;
			NewParams.meshWeldTolerance = 0.0f;
			PhysXCooking->setParams(NewParams);
		}

		// Cook the convex mesh to a temp buffer
		TArray<uint8> CookedMeshBuffer;
		FPhysXOutputStream Buffer(&CookedMeshBuffer);
		if (PhysXCooking->cookConvexMesh(PConvexMeshDesc, Buffer))
		{
			CookResult = EPhysXCookingResult::Succeeded;
		}
		else
		{
			if (!(PConvexMeshDesc.flags & PxConvexFlag::eINFLATE_CONVEX))
			{
				// We failed to cook without inflating convex. Let's try again with inflation
				//This is not ideal since it makes the collision less accurate. It's needed if given verts are extremely close.
				PConvexMeshDesc.flags |= PxConvexFlag::eINFLATE_CONVEX;
				if (PhysXCooking->cookConvexMesh(PConvexMeshDesc, Buffer))
				{
					CookResult = EPhysXCookingResult::SucceededWithInflation;
				}
			}
		}

		// Return default cooking params to normal
		if (bDeformableMesh)
		{
			PhysXCooking->setParams(Params);
		}

		if (CookedMeshBuffer.Num() == 0)
		{
			CookResult = EPhysXCookingResult::Failed;
		}

		if (CookResult != EPhysXCookingResult::Failed)
		{
			// Append the cooked data into cooked buffer
			OutBuffer.Append( CookedMeshBuffer );
		}
#endif		// WITH_PHYSX
	
		return CookResult;
	}

	virtual bool CookTriMesh(FName Format, int32 RuntimeCookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer, bool bDeformableMesh = false) const override
	{
#if WITH_PHYSX
		PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
		bool bIsPhysXFormatValid = GetPhysXFormat(Format, PhysXFormat);
		check(bIsPhysXFormatValid);

		PxTriangleMeshDesc PTriMeshDesc;
		PTriMeshDesc.points.data = SrcVertices.GetData();
		PTriMeshDesc.points.count = SrcVertices.Num();
		PTriMeshDesc.points.stride = sizeof(FVector);
		PTriMeshDesc.triangles.data = SrcIndices.GetData();
		PTriMeshDesc.triangles.count = SrcIndices.Num();
		PTriMeshDesc.triangles.stride = sizeof(FTriIndices);
		PTriMeshDesc.materialIndices.data = SrcMaterialIndices.GetData();
		PTriMeshDesc.materialIndices.stride = sizeof(PxMaterialTableIndex);
		PTriMeshDesc.flags = FlipNormals ? PxMeshFlag::eFLIPNORMALS : (PxMeshFlags)0;

		// Set up cooking
		const PxCookingParams& Params = PhysXCooking->getParams();
		PxCookingParams NewParams = Params;
		NewParams.targetPlatform = PhysXFormat;
		PxMeshPreprocessingFlags OldCookingFlags = NewParams.meshPreprocessParams;

		if (RuntimeCookFlags & ERuntimePhysxCookOptimizationFlags::SuppressFaceRemapTable)
		{
			NewParams.suppressTriangleMeshRemapTable = true;
		}

		if (bDeformableMesh)
		{
			// In the case of a deformable mesh, we have to change the cook params
			NewParams.meshPreprocessParams = PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
		}

		PhysXCooking->setParams(NewParams);


		// Cook TriMesh Data
		FPhysXOutputStream Buffer(&OutBuffer);
		bool Result = PhysXCooking->cookTriangleMesh(PTriMeshDesc, Buffer);
		
		if (bDeformableMesh)	//restore old params
		{
			NewParams.meshPreprocessParams = OldCookingFlags;
			PhysXCooking->setParams(NewParams);
		}
		return Result;
#else
		return false;
#endif		// WITH_PHYSX
	}

	virtual bool CookHeightField(FName Format, FIntPoint HFSize, float Thickness, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer) const override
	{
#if WITH_PHYSX
		PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
		bool bIsPhysXFormatValid = GetPhysXFormat(Format, PhysXFormat);
		check(bIsPhysXFormatValid);

		PxHeightFieldDesc HFDesc;
		HFDesc.format = PxHeightFieldFormat::eS16_TM;
		HFDesc.nbColumns = HFSize.X;
		HFDesc.nbRows = HFSize.Y;
		HFDesc.samples.data = Samples;
		HFDesc.samples.stride = SamplesStride;
		HFDesc.flags = PxHeightFieldFlag::eNO_BOUNDARY_EDGES;
		HFDesc.thickness = Thickness;

		// Set up cooking
		const PxCookingParams& Params = PhysXCooking->getParams();
		PxCookingParams NewParams = Params;
		NewParams.targetPlatform = PhysXFormat;
		PhysXCooking->setParams(NewParams);

		// Cook to a temp buffer
		TArray<uint8> CookedBuffer;
		FPhysXOutputStream Buffer(&CookedBuffer);

		if (PhysXCooking->cookHeightField(HFDesc, Buffer) && CookedBuffer.Num() > 0)
		{
			// Append the cooked data into cooked buffer
			OutBuffer.Append(CookedBuffer);
			return true;
		}
		return false;
#else
		return false;
#endif		// WITH_PHYSX
	}

	virtual bool SerializeActors(FName Format, const TArray<FBodyInstance*>& Bodies, const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials, TArray<uint8>& OutBuffer) const override
	{
#if WITH_PHYSX
		PxSerializationRegistry* PRegistry = PxSerialization::createSerializationRegistry(*GPhysXSDK);
		PxCollection* PCollection = PxCreateCollection();

		PxBase* PLastObject = nullptr;

		for(FBodyInstance* BodyInstance : Bodies)
		{
			if(BodyInstance->RigidActorSync)
			{
				PCollection->add(*BodyInstance->RigidActorSync, BodyInstance->RigidActorSyncId);
				PLastObject = BodyInstance->RigidActorSync;
			}

			if(BodyInstance->RigidActorAsync)
			{
				PCollection->add(*BodyInstance->RigidActorAsync,  BodyInstance->RigidActorAsyncId);
				PLastObject = BodyInstance->RigidActorAsync;
			}
		}

		PxSerialization::createSerialObjectIds(*PCollection, PxSerialObjectId(1));	//we get physx to assign an id for each actor

		//Note that rigid bodies may have assigned ids. It's important to let them go first because we rely on that id for deserialization.
		//One this is done we must find out the next available ID, and use that for naming the shared resources. We have to save this for deserialization
		uint64 BaseId = PLastObject ? (PCollection->getId(*PLastObject) + 1) : 1;

		PxCollection* PExceptFor = MakePhysXCollection(PhysicalMaterials, BodySetups, BaseId);
		
		for (FBodyInstance* BodyInstance : Bodies)	//and then we mark that id back into the bodyinstance so we can pair the two later
		{
			if (BodyInstance->RigidActorSync)
			{
				BodyInstance->RigidActorSyncId = PCollection->getId(*BodyInstance->RigidActorSync);
			}

			if (BodyInstance->RigidActorAsync)
			{
				BodyInstance->RigidActorAsyncId = PCollection->getId(*BodyInstance->RigidActorAsync);
			}
		}

		//We must store the BaseId for shared resources.
		FMemoryWriter Ar(OutBuffer);
		uint8 bIsLittleEndian = PLATFORM_LITTLE_ENDIAN; //TODO: We should pass the target platform into this function and write it. Then swap the endian on the writer so the reader doesn't have to do it at runtime
		Ar << bIsLittleEndian;
		Ar << BaseId;
		//Note that PhysX expects the binary data to be 128-byte aligned. Because of this we must pad
		int32 BytesToPad = PHYSX_SERIALIZATION_ALIGNMENT - (Ar.Tell() % PHYSX_SERIALIZATION_ALIGNMENT);
		OutBuffer.AddZeroed(BytesToPad);

		FPhysXOutputStream Buffer(&OutBuffer);
		PxSerialization::complete(*PCollection, *PRegistry, PExceptFor);
		PxSerialization::serializeCollectionToBinary(Buffer, *PCollection, *PRegistry, PExceptFor);

#if PHYSX_MEMORY_VALIDATION
		GPhysXAllocator->ValidateHeaders();
#endif
		PCollection->release();
		PExceptFor->release();
		PRegistry->release();

#if PHYSX_MEMORY_VALIDATION
		GPhysXAllocator->ValidateHeaders();
#endif
		return true;
#endif
		return false;
	}

};


/**
 * Module for PhysX cooking
 */
static IPhysXFormat* Singleton = NULL;

class FPhysXPlatformModule : public IPhysXFormatModule
{
public:
	FPhysXPlatformModule()
	{

	}

	virtual ~FPhysXPlatformModule()
	{
		ShutdownPhysXCooking();

		delete Singleton;
		Singleton = NULL;
	}
	virtual IPhysXFormat* GetPhysXFormat()
	{
		if (!Singleton)
		{
			InitPhysXCooking();
			Singleton = new FPhysXFormats(GPhysXCooking);
		}
		return Singleton;
	}

private:

	/**
	 *	Load the required modules for PhysX
	 */
	void InitPhysXCooking()
	{
		// Make sure PhysX libs are loaded
		LoadPhysXModules();
	}

	void ShutdownPhysXCooking()
	{
		// Ideally PhysX cooking should be initialized in InitPhysXCooking and released here
		// but Apex is still being setup in the engine and it also requires PhysX cooking singleton.
	}
};

IMPLEMENT_MODULE( FPhysXPlatformModule, PhysXFormats );
