// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

enum ERuntimePhysxCookOptimizationFlags
{
	SuppressFaceRemapTable = 0x1
};

// The result of a cooking operation
enum class EPhysXCookingResult : uint8
{
	// Cooking failed
	Failed,

	// Cooking succeeded with no issues
	Succeeded,

	// Cooking the exact source data failed, but succeeded after retrying with inflation enabled
	SucceededWithInflation,
};

/**
 * IPhysXFormat, PhysX cooking and serialization abstraction
**/
class IPhysXFormat
{
public:

	/**
	 * Checks whether parallel PhysX cooking is allowed.
	 *
	 * Note: This method is not currently used yet.
	 *
	 * @return true if this PhysX format can cook in parallel, false otherwise.
	 */
	virtual bool AllowParallelBuild( ) const
	{
		return false;
	}

	/**
	 * Cooks the source convex data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format
	 * @param RuntimeCookFlags Any special runtime flags we want to pass in
	 * @param SrcBuffer The source buffer
	 * @param OutBuffer The resulting cooked data
	 * @param bDeformableMesh Whether this mesh is deformable and hence needs to be cooked with different parameters
	 * @return An enum indicating full success, partial success, or failure (@see EPhysXCookingResult)
	 */
	virtual EPhysXCookingResult CookConvex( FName Format, int32 RuntimeCookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer, bool bDeformableMesh = false ) const = 0;

	/**
	 * Cooks the source Tri-Mesh data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format.
	 * @param RuntimeCookFlags Any special runtime flags we want to pass in
	 * @param SrcBuffer The source buffer.
	 * @param OutBuffer The resulting cooked data.
	 * @param bDeformableMesh This is a very special case that requires different cooking parameters set.
	 * @return true on success, false otherwise.
	 */
	virtual bool CookTriMesh( FName Format, int32 RuntimeCookFlags, const TArray<FVector>& SrcVertices, const TArray<struct FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer, bool bDeformableMesh = false ) const = 0;
		
	/**
	 * Cooks the source height field data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format
	 * @param HFSize Size of height field [NumColumns, NumRows]
	 * @param Thickness Sets how thick the height field surface is
	 * @param SrcBuffer The source buffer
	 * @param OutBuffer The resulting cooked data
	 * @return true on success, false otherwise.
	 */
	virtual bool CookHeightField( FName Format, FIntPoint HFSize, float Thickness, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Serializes the BodyInstance
	 *
	 * @param Format The desired format
	 * @param Bodies The bodies containing the needed physx actors to serialize
	 * @param BodySetups The various body setups used by Bodies (could be just 1). This is needed for keeping geometry out of the serialized data
	 * @param PhysicalMaterials The physical materials used by Bodies (could be just 1). This is needed for keeping physical materials out of the serialized data
	 * @param OutBuffer The resulting cooked data
	 * @return true on success, false otherwise.
	 */
	virtual bool SerializeActors( FName Format, const TArray<struct FBodyInstance*>& Bodies, const TArray<class UBodySetup*>& BodySetups, const TArray<class UPhysicalMaterial*>& PhysicalMaterials, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Gets the list of supported formats.
	 *
	 * @param OutFormats Will hold the list of formats.
	 */
	virtual void GetSupportedFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the current version of the specified PhysX format.
	 *
	 * @param Format The format to get the version for.
	 * @return Version number.
	 */
	virtual uint16 GetVersion( FName Format ) const = 0;


public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IPhysXFormat( ) { }
};
