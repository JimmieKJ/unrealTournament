//! @file SubstanceEditorPanel.cpp
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_SubstanceImageInput : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubstanceImageInput", "Substance Image Input"); }
	virtual FColor GetTypeColor() const override { return FColor(192, 128, 32); }
	virtual UClass* GetSupportedClass() const override { return USubstanceImageInput::StaticClass(); }

	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
	
protected:
	/** Returns additional tooltip information for the specified asset, if it has any (otherwise return the null widget) */
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override
	{
		return FText::FromString(TEXT("Substance Image Input."));
	}

private:
	/** Handler for when Reimport is selected */
	void ExecuteReimport(TArray<TWeakObjectPtr<USubstanceImageInput>> Objects);
};
