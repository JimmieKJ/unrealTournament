// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Matinee exporter for Unreal Engine 3.
=============================================================================*/

#ifndef __MATINEEEXPORTER_H__
#define __MATINEEEXPORTER_H__

#include "Matinee/MatineeActor.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"

class UInterpData;
class UInterpTrackMove;
class UInterpTrackFloatProp;
class UInterpTrackVectorProp;
class AActor;
class AMatineeActor;
class ALight;

/**
 * Main Matinee Exporter class.
 * Except for CImporter, consider the other classes as private.
 */
class MatineeExporter
{
public:
	virtual ~MatineeExporter() {}

	/**
	 * Creates and readies an empty document for export.
	 */
	virtual void CreateDocument() = 0;
	

	void SetTrasformBaking(bool bBakeTransforms)
	{
		bBakeKeys = bBakeTransforms;
	}

	void SetKeepHierarchy(bool bInKeepHierarchy)
	{
		bKeepHierarchy = bInKeepHierarchy;
	}

	/**
	 * Exports the basic scene information to a file.
	 */
	virtual void ExportLevelMesh( ULevel* Level, AMatineeActor* InMatineeActor, bool bSelectedOnly ) = 0;

	/**
	 * Exports the light-specific information for a light actor.
	 */
	virtual void ExportLight( ALight* Actor, AMatineeActor* InMatineeActor ) = 0;

	/**
	 * Exports the camera-specific information for a camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, AMatineeActor* InMatineeActor, bool bExportComponents ) = 0;

	/**
	 * Exports the mesh and the actor information for a brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* Model, bool bConvertToStaticMesh ) = 0;

	/**
	 * Exports the mesh and the actor information for a static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, AMatineeActor* InMatineeActor ) = 0;

	/**
	 * Exports the given Matinee sequence information into a file.
	 * 
	 * @return	true, if sucessful
	 */
	virtual bool ExportMatinee(class AMatineeActor* InMatineeActor) = 0;

	/**
	 * Writes the file to disk and releases it.
	 */
	virtual void WriteToFile(const TCHAR* Filename) = 0;

	/**
	 * Closes the file, releasing its memory.
	 */
	virtual void CloseDocument() = 0;
	
	// Choose a name for this actor.
	// If the actor is bound to a Matinee sequence, we'll
	// use the Matinee group name, otherwise we'll just use the actor name.
	FString GetActorNodeName(AActor* Actor, AMatineeActor* InMatineeActor )
	{
		FString NodeName = Actor->GetName();
		if( InMatineeActor != NULL )
		{
			const UInterpGroupInst* FoundGroupInst = InMatineeActor->FindGroupInst( Actor );
			if( FoundGroupInst != NULL )
			{
				NodeName = FoundGroupInst->Group->GroupName.ToString();
			}
		}

		// Maya does not support dashes.  Change all dashes to underscores
		NodeName = NodeName.Replace(TEXT("-"), TEXT("_") );

		return NodeName;
	}
	
protected:

	/** When true, a key will exported per frame at the set frames-per-second (FPS). */
	bool bBakeKeys;
	/** When true, we'll export with hierarchical relation of attachment with relative transform */
	bool bKeepHierarchy;
};

#endif // __MATINEEEXPORTER_H__
