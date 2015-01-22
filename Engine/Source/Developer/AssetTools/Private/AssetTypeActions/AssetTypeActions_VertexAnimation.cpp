// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Toolkits/AssetEditorManager.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_VertexAnimation::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto VertexAnim = Cast<UVertexAnimation>(*ObjIt);
		if (VertexAnim != NULL && VertexAnim->BaseSkelMesh)
		{
		}
	}
}

bool FAssetTypeActions_VertexAnimation::CanFilter()
{
	return false;
}

#undef LOCTEXT_NAMESPACE