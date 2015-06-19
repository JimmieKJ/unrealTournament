// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "NiagaraScript.h"

#include "Toolkits/AssetEditorManager.h"

#include "Editor/NiagaraEditor/Public/NiagaraEditorModule.h"
#include "Editor/NiagaraEditor/Public/INiagaraEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_NiagaraScript::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Script = Cast<UNiagaraScript>(*ObjIt);
		if (Script != NULL)
		{
			FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>( "NiagaraEditor" );
			TSharedRef< INiagaraEditor > NewNiagaraEditor = NiagaraEditorModule.CreateNiagaraEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Script );
		}
	}
}

UClass* FAssetTypeActions_NiagaraScript::GetSupportedClass() const
{ 
	return UNiagaraScript::StaticClass(); 
}


#undef LOCTEXT_NAMESPACE
