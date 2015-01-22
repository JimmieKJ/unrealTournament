// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorMorphFactory.cpp: Morph target mesh factory import code.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "SkelImport.h"
#include "Engine/StaticMesh.h"

#define SET_MORPH_ERROR( ErrorType ) { if( Error ) *Error = ErrorType; }

/** 
* Constructor
* 
* @param	InSrcMesh - source skeletal mesh that the new morph mesh morphs
* @param	LODIndex - lod entry of base mesh
* @param	InWarn - for outputing warnings
*/
FMorphTargetBinaryImport::FMorphTargetBinaryImport( USkeletalMesh* InSrcMesh, int32 LODIndex, FFeedbackContext* InWarn )
:	Warn(InWarn)
,	BaseMeshRawData( InSrcMesh, LODIndex )
,	BaseLODIndex(LODIndex)
,   BaseMesh(InSrcMesh)
{	
}

/** 
* Constructor
* 
* @param	InSrcMesh - source static mesh that the new morph mesh morphs
* @param	LODIndex - lod entry of base mesh
* @param	InWarn - for outputing warnings
*/
FMorphTargetBinaryImport::FMorphTargetBinaryImport( UStaticMesh* InSrcMesh, int32 LODIndex, FFeedbackContext* InWarn )
:	Warn(InWarn)
,	BaseMeshRawData( InSrcMesh, LODIndex )
,	BaseLODIndex(LODIndex)
,   BaseMesh(InSrcMesh)
{	
}

/**
* Imports morph target data from file for a specific LOD entry
* of an existing morph target
*
* @param	MorphTarget - existing morph target to modify
* @param	SrcFilename - file to import
* @param	LODIndex - LOD entry to import to
* @param	Error - optional error value
*/
void FMorphTargetBinaryImport::ImportMorphLODModel( UMorphTarget* MorphTarget, const TCHAR* SrcFilename, int32 LODIndex, EMorphImportError* Error )
{
	check(MorphTarget);

	SET_MORPH_ERROR( MorphImport_OK );

	// check for valid LOD index 
	if( LODIndex > MorphTarget->MorphLODModels.Num() ||
		LODIndex != BaseLODIndex )
	{
		SET_MORPH_ERROR( MorphImport_InvalidLODIndex );
	}
	// check for valid meta data for the base mesh
	else if( BaseMeshRawData.Indices.Num() && !BaseMeshRawData.WedgePointIndices.Num() )
	{
		SET_MORPH_ERROR( MorphImport_ReimportBaseMesh );
	}
	else
	{
		USkeletalMesh* TargetSkelMesh = this->CreateSkeletalMesh(SrcFilename, Error);
				
		if( !TargetSkelMesh )
		{
			SET_MORPH_ERROR( MorphImport_InvalidMeshFormat );
		}		
		else
		{
			Warn->BeginSlowTask( NSLOCTEXT("MorphTargetBinaryImport", "BeginGeneratingMorphModelTask", "Generating Morph Model"), 1);

			// convert the morph target mesh to the raw vertex data
			FMorphMeshRawSource TargetMeshRawData( TargetSkelMesh );

			// check to see if the vertex data is compatible
			if( !BaseMeshRawData.IsValidTarget(TargetMeshRawData) )
			{
				SET_MORPH_ERROR( MorphImport_MismatchBaseMesh );                
			}
			else
			{
				// populate the vertex data for the morph target mesh using its base mesh
				// and the newly imported mesh				
				// @todo : bCompareNormal is not used in this function
				MorphTarget->PostProcess( TargetSkelMesh, BaseMeshRawData, TargetMeshRawData, LODIndex, false );
			}

			Warn->EndSlowTask();
		}
	}
}
