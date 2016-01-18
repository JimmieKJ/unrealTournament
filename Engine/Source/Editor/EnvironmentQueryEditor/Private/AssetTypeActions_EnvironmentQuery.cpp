// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "Toolkits/AssetEditorManager.h"

#include "Editor/EnvironmentQueryEditor/Public/EnvironmentQueryEditorModule.h"
#include "Editor/EnvironmentQueryEditor/Public/IEnvironmentQueryEditor.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "AssetTypeActions_EnvironmentQuery.h"

#include "AIModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_EnvironmentQuery::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Script = Cast<UEnvQuery>(*ObjIt);
		if (Script != NULL)
		{
			FEnvironmentQueryEditorModule& EnvironmentQueryEditorModule = FModuleManager::LoadModuleChecked<FEnvironmentQueryEditorModule>( "EnvironmentQueryEditor" );
			TSharedRef< IEnvironmentQueryEditor > NewEditor = EnvironmentQueryEditorModule.CreateEnvironmentQueryEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Script );
		}
	}
}

UClass* FAssetTypeActions_EnvironmentQuery::GetSupportedClass() const
{ 
	return UEnvQuery::StaticClass(); 
}

uint32 FAssetTypeActions_EnvironmentQuery::GetCategories()
{
	IAIModule& AIModule = FModuleManager::GetModuleChecked<IAIModule>("AIModule").Get();
	return AIModule.GetAIAssetCategoryBit();
}
#undef LOCTEXT_NAMESPACE
