// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"


UFbxSceneImportOptions::UFbxSceneImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateContentFolderHierarchy = false;
	bImportAsDynamic = false;
	HierarchyType = FBXSOCHT_CreateActorComponents;
	bImportStaticMeshLODs = false;
	bImportSkeletalMeshLODs = false;
	ImportTranslation = FVector(0);
	ImportRotation = FRotator(0);
	ImportUniformScale = 1.0f;
}

