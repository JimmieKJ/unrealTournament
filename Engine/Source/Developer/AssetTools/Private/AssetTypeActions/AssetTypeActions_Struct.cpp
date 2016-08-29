// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "PackageTools.h"
#include "AssetRegistryModule.h"
#include "ISourceControlModule.h"
#include "BlueprintEditorModule.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Struct::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	if (FStructureEditorUtils::UserDefinedStructEnabled())
	{
		const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );

		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			if (UUserDefinedStruct* UDStruct = Cast<UUserDefinedStruct>(*ObjIt))
			{
				BlueprintEditorModule.CreateUserDefinedStructEditor(Mode, EditWithinLevelEditor, UDStruct);
			}
		}
	}
}

FText FAssetTypeActions_Struct::GetAssetDescription(const FAssetData& AssetData) const
{
	FString Description = AssetData.GetTagValueRef<FString>("Tooltip");
	if (!Description.IsEmpty())
	{
		Description.ReplaceInline(TEXT("\\n"), TEXT("\n"));
		return FText::FromString(MoveTemp(Description));
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
