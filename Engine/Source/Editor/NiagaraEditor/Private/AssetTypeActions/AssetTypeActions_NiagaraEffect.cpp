// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "AssetTypeActions_NiagaraEffect.h"
#include "NiagaraEffect.h"


void FAssetTypeActions_NiagaraEffect::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Effect = Cast<UNiagaraEffect>(*ObjIt);
		if (Effect != NULL)
		{
			TSharedRef< FNiagaraEffectEditor > NewNiagaraEditor(new FNiagaraEffectEditor());
			NewNiagaraEditor->InitNiagaraEffectEditor(Mode, EditWithinLevelEditor, Effect);
		}
	}
}

UClass* FAssetTypeActions_NiagaraEffect::GetSupportedClass() const
{
	return UNiagaraEffect::StaticClass();
}
