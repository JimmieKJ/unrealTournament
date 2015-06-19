// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "NiagaraEffect.h"

#include "Toolkits/AssetEditorManager.h"

#include "Editor/NiagaraEditor/Public/NiagaraEditorModule.h"
#include "Editor/NiagaraEditor/Public/INiagaraEffectEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_NiagaraEffect::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Script = Cast<UNiagaraEffect>(*ObjIt);
		if (Script != NULL)
		{
			FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
			TSharedRef< INiagaraEffectEditor > NewNiagaraEditor = NiagaraEditorModule.CreateNiagaraEffectEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Script);
		}
	}
}

UClass* FAssetTypeActions_NiagaraEffect::GetSupportedClass() const
{
	return UNiagaraEffect::StaticClass();
}


#undef LOCTEXT_NAMESPACE
