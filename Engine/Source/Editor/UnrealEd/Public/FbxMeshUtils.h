// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace FbxMeshUtils
{
	/**
	 * Writes out all StaticMeshes passed in (and all LoDs) for light map editing
	 * @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be exported
	 * @param InDirectory - The directory to attempt the export to
	 * @return If any errors occured during export
	 */
	UNREALED_API bool ExportAllLightmapModels (TArray<UStaticMesh*>& AllMeshes, const FString& Directory, bool IsFBX);

	/**
	 * Reads in all StaticMeshes passed in (and all LoDs) for light map editing
	 * @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be imported
	 * @param InDirectory - The directory to attempt the import from
	 * @return If any errors occured during import
	 */
	UNREALED_API bool ImportAllLightmapModels (TArray<UStaticMesh*>& AllMeshes, const FString& Directory, bool IsFBX);

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InStaticMesh - The mesh to be exported
	 * @param InOutputDevice - The output device to write the data to.  Current usage is a FOutputDeviceFile
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	bool ExportSingleLightmapModel (UStaticMesh* CurrentStaticMesh, FOutputDevice* Out, const int32 LODIndex);

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InCurrentStaticMesh - The mesh to be exported
	 * @param InFilename - The name of the file to write the data to.
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	bool ExportSingleLightmapModelFBX (UStaticMesh* CurrentStaticMesh, FString& Filename, const int32 LODIndex);


	/**
	 * Reads in a temp FBX formatted mesh who's texture coordinates will REPLACE that of the active static mesh's LightMapCoordinates
	 * @param InStaticMesh - The mesh to be inported
	 * @param InData- An array of bytes that contains the files contents
	 * @param InLODIndex - The LOD of the model we want imported
	 * @return If any errors occured during import
	 */
	bool ImportSingleLightmapModelFBX(UStaticMesh* CurrentStaticMesh, FString& Filename, const int32 LODIndex );

	/**
	 * Imports a mesh LOD to the given static mesh
	 *
	 * @param BaseStaticMesh	The static mesh to import the LOD to
	 * @param Filename			The filename of the FBX file containing the LOD
	 * @param LODLevel			The level of the lod to import
	 */
	UNREALED_API void ImportStaticMeshLOD( UStaticMesh* BaseStaticMesh, const FString& Filename, int32 LODLevel );

		/**
	 * Imports a skeletal mesh LOD to the given skeletal mesh
	 *
	 * @param Mesh				The skeletal mesh to import the LOD to
	 * @param Filename			The filename of the FBX file containing the LOD
	 * @param LODLevel			The level of the lod to import
	 */
	UNREALED_API void ImportSkeletalMeshLOD( class USkeletalMesh* Mesh, const FString& Filename, int32 LODLevel );

	/**
	 * Imports a skeletal mesh LOD to the given skeletal mesh
	 * Opens file dialog to do so!
	 *
	 * @param Mesh				The skeletal mesh to import the LOD to
	 * @param LODLevel			The level of the lod to import
	 */
	UNREALED_API void ImportMeshLODDialog( class UObject* Mesh, int32 LODLevel );

	/**
	 * Sets import option before importing
	 *
	 * @param ImportUI			The importUI you'd like to apply to
	 */
	UNREALED_API void SetImportOption(UFbxImportUI* ImportUI);

}  //end namespace ExportMeshUtils
