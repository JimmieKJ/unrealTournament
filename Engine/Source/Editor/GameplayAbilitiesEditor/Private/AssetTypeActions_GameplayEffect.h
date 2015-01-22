// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetTypeActions_GameplayEffect, Warning, All);

class FAssetTypeActions_GameplayEffect : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual uint32 GetCategories() override;
	// End of IAssetTypeActions interface

protected:
	typedef TArray< TWeakObjectPtr<class UGameplayEffect> > FWeakGameplayEffectPointerArray;

	void ExecuteConvertToBlueprint(FWeakGameplayEffectPointerArray Objects);
	void FixReferencers(UPackage* Package, UGameplayEffect* OldGE, UGameplayEffect* GE);
};