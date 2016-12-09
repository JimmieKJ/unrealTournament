// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSceneImportOptions.h"


UFbxSceneImportOptions::UFbxSceneImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTransformVertexToAbsolute = false;
	bBakePivotInVertex = false;
	bCreateContentFolderHierarchy = false;
	bImportAsDynamic = false;
	HierarchyType = FBXSOCHT_CreateBlueprint;
	bImportStaticMeshLODs = false;
	bImportSkeletalMeshLODs = false;
	bInvertNormalMaps = false;
	ImportTranslation = FVector(0);
	ImportRotation = FRotator(0);
	ImportUniformScale = 1.0f;
}

