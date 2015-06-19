// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ITextureFormatModule.h: Declares the ITextureFormatModule interface.
=============================================================================*/

#pragma once


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
	 * @param SrcBuffer The source buffer
	 * @param OutBuffer The resulting cooked data
	 * @return true on success, false otherwise.
	 */
	virtual bool CookConvex( FName Format, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Cooks the source Tri-Mesh data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format.
	 * @param SrcBuffer The source buffer.
	 * @param OutBuffer The resulting cooked data.
	 * @param bPerPolySkeletalMesh This is a very special case that requires different cooking parameters set.
	 * @return true on success, false otherwise.
	 */
	virtual bool CookTriMesh( FName Format, const TArray<FVector>& SrcVertices, const TArray<struct FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer, bool bPerPolySkeletalMesh = false ) const = 0;
		
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
