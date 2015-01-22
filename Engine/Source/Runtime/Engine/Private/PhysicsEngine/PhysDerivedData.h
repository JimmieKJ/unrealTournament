// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	const class IPhysXFormat* Cooker;
	FGuid DataGuid;
	FString MeshId;

public:
	FDerivedDataPhysXCooker( FName InFormat, UBodySetup* InBodySetup );

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("PhysX");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		return TEXT("{1F0627AE-ABEB-4206-8D78-E16BEB5DDC7E}");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override
	{
		//  1 - base version
		//  2 - cook out small area trimesh triangles from BSP (see UPhysicsSettings::TriangleMeshTriangleMinAreaThreshold)
		enum { UE_PHYSX_DERIVEDDATA_VER = 2 };

		const uint16 PhysXVersion = ((PX_PHYSICS_VERSION_MAJOR  & 0xF) << 12) |
				((PX_PHYSICS_VERSION_MINOR  & 0xF) << 8) |
				((PX_PHYSICS_VERSION_BUGFIX & 0xF) << 4) |
				((UE_PHYSX_DERIVEDDATA_VER	& 0xF));

		return FString::Printf( TEXT("%s_%s_%s_%d_%d_%hu_%hu"),
			*Format.ToString(),
			*DataGuid.ToString(),
			*MeshId,
			(int32)bGenerateNormalMesh,
			(int32)bGenerateMirroredMesh,
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
	bool BuildTriMesh( TArray<uint8>& OutData, bool InMirrored, bool InUseAllTriData );
	bool ShouldGenerateTriMeshData(bool InUseAllTriData);
	bool ShouldGenerateNegXTriMeshData();
};

#endif	//WITH_PHYSX && WITH_EDITOR