// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "PackageTools.h"
#include "AssetRegistryModule.h"
#include "ISourceControlModule.h"
#include "BlueprintEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Enum::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UUserDefinedEnum* UDEnum = Cast<UUserDefinedEnum>(*ObjIt))
		{
			BlueprintEditorModule.CreateUserDefinedEnumEditor(Mode, EditWithinLevelEditor, UDEnum);
		}
	}
}

#undef LOCTEXT_NAMESPACE
