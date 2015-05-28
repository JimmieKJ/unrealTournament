// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TexAlignTools.h: Tools for aligning textures on surfaces
=============================================================================*/

#pragma once

#include "Model.h"

class FBspSurfIdx
{
public:
	FBspSurfIdx()
	{}
	FBspSurfIdx( FBspSurf* InSurf, int32 InIdx )
	{
		Surf = InSurf;
		Idx = InIdx;
	}

	FBspSurf* Surf;
	int32 Idx;
};

#include "../Classes/TexAligner/TexAligner.h"

/**
 * A helper class to store the state of the various texture alignment tools.
 */
class FTexAlignTools
{
public:

	/** Constructor */
	FTexAlignTools();

	/** Destructor */
	~FTexAlignTools();

	/** A list of all available aligners. */
	TArray<UTexAligner*> Aligners;

	/**
	 * Creates the list of aligners.
	 */
	void Init();

	/**
	 * Returns the most appropriate texture aligner based on the type passed in.
	 */
	UNREALED_API UTexAligner* GetAligner( ETexAlign InTexAlign );

private:
	/** 
	 * Delegate handlers
	 **/
	void OnEditorFitTextureToSurface(UWorld* InWorld);

};

extern UNREALED_API FTexAlignTools GTexAlignTools;
