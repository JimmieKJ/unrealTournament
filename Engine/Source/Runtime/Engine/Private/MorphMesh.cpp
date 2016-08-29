// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphMesh.cpp: Unreal morph target mesh and blending implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Animation/MorphTarget.h"

//////////////////////////////////////////////////////////////////////////

UMorphTarget::UMorphTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UMorphTarget::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsDataStrippedForServer() )
	{
		Ar << MorphLODModels;
	}
}


SIZE_T UMorphTarget::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;

	for (const auto& LODModel : MorphLODModels)
	{
		ResourceSize += LODModel.GetResourceSize();
	}
	return ResourceSize;
}


//////////////////////////////////////////////////////////////////////
SIZE_T FMorphTargetLODModel::GetResourceSize() const
{
	return Vertices.GetAllocatedSize() + sizeof(int32);
}