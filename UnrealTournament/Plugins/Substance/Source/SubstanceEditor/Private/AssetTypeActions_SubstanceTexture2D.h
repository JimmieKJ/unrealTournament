//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_SubstanceTexture2D : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubstanceTexture2D", "Substance Texture 2D"); }
	virtual FColor GetTypeColor() const override { return FColor(255,128,0); }
	virtual UClass* GetSupportedClass() const override { return USubstanceTexture2D::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }

protected:
	/** Returns additional tooltip information for the specified asset, if it has any (otherwise return the null widget) */
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override
	{
		USubstanceTexture2D* Texture = Cast<USubstanceTexture2D>(AssetData.GetAsset());

		if (Texture && NULL == Texture->ParentInstance)
		{
			return FText::FromString(TEXT("Orphan Substance Texture, Graph Instance missing, please delete."));
		}
		else if (Texture && NULL == Texture->ParentInstance->Instance->Desc)
		{
			return FText::FromString(TEXT("Substance Texture's Instance Factory missing, both the texture and its Graph Instance are orphans, please delete."));
		}
		else
		{
			return FText::FromString(TEXT("Substance Texture."));
		}
	}

private:
	void ExecuteFindParentInstance(TArray<TWeakObjectPtr<USubstanceTexture2D>> Objects);

	void virtual OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;

};
