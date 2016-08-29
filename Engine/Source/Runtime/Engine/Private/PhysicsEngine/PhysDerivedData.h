// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX && (WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR)

#include "DerivedDataPluginInterface.h"
#include "DerivedDataCacheInterface.h"
#include "TargetPlatform.h"
#include "PhysXSupport.h"
#include "IPhysXFormat.h"

//////////////////////////////////////////////////////////////////////////
// PhysX Cooker
class FDerivedDataPhysXCooker : public FDerivedDataPluginInterface
{
private:

	UBodySetup* BodySetup;
	UObject* CollisionDataProvider;
	FName Format;
	bool bGenerateNormalMesh;
	bool bGenerateMirroredMesh;
	bool bGenerateUVInfo;
	int32 RuntimeCookFlags;
	const class IPhysXFormat* Cooker;
	FGuid DataGuid;
	FString MeshId;

public:
	FDerivedDataPhysXCooker(FName InFormat, int32 InRuntimeCookFlags, UBodySetup* InBodySetup);

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("PhysX");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("6FF6F996840F4A23995CD1B9DD0D80B7");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override
	{
		//  1 - base version
		//  2 - cook out small area trimesh triangles from BSP (see UPhysicsSettings::TriangleMeshTriangleMinAreaThreshold)
		//  3 - increase default small area threshold and force recook.
		enum { UE_PHYSX_DERIVEDDATA_VER = 3 };

		const uint16 PhysXVersion = ((PX_PHYSICS_VERSION_MAJOR  & 0xF) << 12) |
				((PX_PHYSICS_VERSION_MINOR  & 0xF) << 8) |
				((PX_PHYSICS_VERSION_BUGFIX & 0xF) << 4) |
				((UE_PHYSX_DERIVEDDATA_VER	& 0xF));

		return FString::Printf( TEXT("%s_%s_%s_%d_%d_%d_%d_%hu_%hu"),
			*Format.ToString(),
			*DataGuid.ToString(),
			*MeshId,
			(int32)bGenerateNormalMesh,
			(int32)bGenerateMirroredMesh,
			(int32)bGenerateUVInfo,
			(int32)RuntimeCookFlags,
			PhysXVersion,
			Cooker ? Cooker->GetVersion( Format ) : 0xffff
			);
	}


	virtual bool IsBuildThreadsafe() const override
	{
		return false;
	}

	virtual bool Build( TArray<uint8>& OutData ) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return !!Cooker;
	}
private:

	void InitCooker();
	int32 BuildConvex( TArray<uint8>& OutData, bool InMirrored );
	int32 BuildTriMesh( TArray<uint8>& OutData, bool InUseAllTriData, FBodySetupUVInfo* UVInfo);
	bool ShouldGenerateTriMeshData(bool InUseAllTriData);
};

//////////////////////////////////////////////////////////////////////////
// PhysX Binary Serialization
class FDerivedDataPhysXBinarySerializer : public FDerivedDataPluginInterface
{
private:

	const TArray<FBodyInstance*>& Bodies;
	const TArray<class UBodySetup*>& BodySetups;
	const TArray<class UPhysicalMaterial*>& PhysicalMaterials;
	FName Format;
	FGuid DataGuid;
	const class IPhysXFormat* Serializer;
	int64 PhysXDataStart;	//important to keep track of this for alignment requirements
	
public:
	FDerivedDataPhysXBinarySerializer(FName InFormat, const TArray<FBodyInstance*>& InBodies, const TArray<class UBodySetup*>& BodySetups, const TArray<class UPhysicalMaterial*>& PhysicalMaterials, const FGuid& InGuid);

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("PhysXSerializer");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("EB1DC7A5D72F469B8E0BF5251D90A220");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override
	{
		enum { UE_PHYSX_DERIVEDDATA_VER = 3 };

		const uint16 PhysXVersion = ((PX_PHYSICS_VERSION_MAJOR & 0xF) << 12) |
			((PX_PHYSICS_VERSION_MINOR & 0xF) << 8) |
			((PX_PHYSICS_VERSION_BUGFIX & 0xF) << 4) |
			((UE_PHYSX_DERIVEDDATA_VER & 0xF));

		return FString::Printf(TEXT("%s_%s_%hu"),
			*Format.ToString(),
			*DataGuid.ToString(),
			PhysXVersion
			);
	}


	virtual bool IsBuildThreadsafe() const override
	{
		return false;
	}

	virtual bool Build(TArray<uint8>& OutData) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return true;
	}
private:

	void SerializeRigidActors(TArray<uint8>& OutData);
	void InitSerializer();

};

#endif	//WITH_PHYSX && WITH_EDITOR