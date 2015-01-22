// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DestructibleFractureSettings.h"

/*=============================================================================
	ApexDestructibleAssetImport.h:

	Creation of an APEX NxDestructibleAsset from a binary buffer.
	DestructibleMesh creation from an APEX NxDestructibleAsset.

=============================================================================*/

#if WITH_APEX

// Forward declarations
class UDestructibleMesh;
class FSkelMeshOptionalImportData;
class FSkeletalMeshImportData;
namespace physx
{
	namespace apex
	{
		class NxDestructibleAsset;
	};
};



/**
 * Creates an APEX NxDestructibleAsset from a binary buffer.
 *
 * @param Buffer - pointer to the beginning of the buffer
 * @param BufferSize - the size of the buffer in bytes
 *
 * @return The newly created NxDestructibleAsset if successful, NULL otherwise
 */
physx::apex::NxDestructibleAsset* CreateApexDestructibleAssetFromBuffer(const uint8* Buffer, int32 BufferSize);


/**
 * Creates an APEX NxDestructibleAsset from a named file.
 *
 * @param Filename - name of the file to import
 *
 * @return The newly created NxDestructibleAsset if successful, NULL otherwise
 */
physx::apex::NxDestructibleAsset* CreateApexDestructibleAssetFromFile(const FString& Filename);

/**
 * Sets the NxDestructibleAsset represented by the UDestructibleMesh.  If there was an existing NxDestructibleAsset, its release method is called.
 * SkeletalMesh data is replaced or created by the render mesh data in the NxDestructibleAsset.
 *
 * N.B. The ApexDestructibleAsset will be modified.  Its collision geometry will undergo a reflection along the y-axis to match UE's left-handed convention.
 * Graphics information will get a similar transformation, and V coordnates will also be flipped using the mapping V' = 1-V, again according to UE's convention.
 * Not currently implemented, but soon to be, all graphics data in the ApexDestructibleAsset will be deleted after it is translated into the SkeletalMesh data.
 * 
 * @param DestructibleMesh - the UDestructibleMesh to which to apply the new APEX Desrtructible Asset
 * @param ApexDestructibleAsset - the NxDestructibleAsset to use
 * @param OutData - Optional import data to populate
 * @param Options - Additional options to modify the import behavior ( see EDestructibleImportOptions::Type )
 *
 * @return true if successful
 */
UNREALED_API bool SetApexDestructibleAsset(UDestructibleMesh& DestructibleMesh, physx::apex::NxDestructibleAsset& ApexDestructibleAsset, FSkeletalMeshImportData* OutData, EDestructibleImportOptions::Type Options = EDestructibleImportOptions::None);

/**
 * Creates a DestructibleMesh from an APEX Destructible Asset with the given name and flags.
 *
 * @param InParent - parent object for the UDestructibleMesh
 * @param ApexDestructibleAsset - the APEX Destructible Asset to import
 * @param Name - the Unreal name for the UDestructibleMesh
 * @param Flags - object flags for the UDestructibleMesh
 * @param OutData - Optional import data to populate
 * @param Options - Additional options to modify the import behavior ( see EDestructibleImportOptions::Type )
 *
 * @return The newly created UDestructibleMesh if successful, NULL otherwise
 */
UNREALED_API UDestructibleMesh* ImportDestructibleMeshFromApexDestructibleAsset(UObject* InParent, physx::apex::NxDestructibleAsset& ApexDestructibleAsset, FName Name, EObjectFlags Flags, FSkeletalMeshImportData* OutData, EDestructibleImportOptions::Type Options = EDestructibleImportOptions::None);

/**
 * Builds a UDestructibleMesh from its internal fracture settings
 *
 * @param DestructibleMesh - the UDestructibleMesh to which to build from its fracture settings
 * @param OutData - Optional import data to populate
 *
 * @return The true iff successful
 */
UNREALED_API bool BuildDestructibleMeshFromFractureSettings(UDestructibleMesh& DestructibleMesh, FSkeletalMeshImportData* OutData);

#endif // WITH_APEX
