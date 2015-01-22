// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __FbxAnimUtils_h__
#define __FbxAnimUtils_h__

class UAnimSequence;
class USkeletalMesh;

//Define interface for exporting fbx animation
namespace FbxAnimUtils
{
	//Function to export fbx animation
	UNREALED_API void ExportAnimFbx( const FString& ExportFilename, UAnimSequence* AnimSequence, USkeletalMesh* Mesh, bool bSaveSkeletalMesh );

} // namespace FbxAnimUtils

#endif //__FbxAnimUtils_h__
