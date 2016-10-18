// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

/** Adapter interface which allows finding the corresponding actor node name to act on both sequencer and matinee data. */
class INodeNameAdapter
{
public:
	virtual FString GetActorNodeName(const AActor* InActor) { return InActor->GetName(); }
};

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
	virtual void ExportLevelMesh( ULevel* Level, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter ) = 0;

	/**
	 * Exports the light-specific information for a light actor.
	 */
	virtual void ExportLight( ALight* Actor, INodeNameAdapter& NodeNameAdapter ) = 0;

	/**
	 * Exports the camera-specific information for a camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter ) = 0;

	/**
	 * Exports the mesh and the actor information for a brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* Model, bool bConvertToStaticMesh, INodeNameAdapter& NodeNameAdapter ) = 0;

	/**
	 * Exports the mesh and the actor information for a static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter ) = 0;

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
		
protected:

	/** When true, a key will exported per frame at the set frames-per-second (FPS). */
	bool bBakeKeys;
	/** When true, we'll export with hierarchical relation of attachment with relative transform */
	bool bKeepHierarchy;
};

#endif // __MATINEEEXPORTER_H__
