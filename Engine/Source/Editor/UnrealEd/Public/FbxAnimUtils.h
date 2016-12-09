// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#ifndef __FbxAnimUtils_h__
#define __FbxAnimUtils_h__

#include "CoreMinimal.h"

class UAnimSequence;
class UCurveTable;
class USkeletalMesh;

//Define interface for exporting fbx animation
namespace FbxAnimUtils
{
	//Function to export fbx animation
	UNREALED_API void ExportAnimFbx( const FString& ExportFilename, UAnimSequence* AnimSequence, USkeletalMesh* Mesh, bool bSaveSkeletalMesh );

	/** 
	 * Import curves from a named node into the supplied curve table.
	 * @param	InFbxFilename	The FBX file to import from
	 * @param	InCurveNodeName	The name of the node in the FBX scene that contains our curves
	 * @param	InOutCurveTable	The curve table to fill with curves
	 * @return true if the import was successful
	 */
	UNREALED_API bool ImportCurveTableFromNode(const FString& InFbxFilename, const FString& InCurveNodeName, UCurveTable* InOutCurveTable, float& OutPreRoll);

} // namespace FbxAnimUtils

#endif //__FbxAnimUtils_h__
