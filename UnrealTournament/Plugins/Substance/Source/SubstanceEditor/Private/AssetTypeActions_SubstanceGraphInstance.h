//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_SubstanceGraphInstance : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubstanceGraphInstance", "Substance Graph Instance"); }
	virtual FColor GetTypeColor() const override { return FColor(230,30,50); }
	virtual UClass* GetSupportedClass() const override { return USubstanceGraphInstance::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }

	/** Create dynamic submenu with available presets*/
	void CreatePresetsSubMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USubstanceGraphInstance>> Graphs);

protected:
	/** Returns additional tooltip information for the specified asset, if it has any (otherwise return the null widget) */
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override
	{
		USubstanceGraphInstance* GraphInstance = Cast<USubstanceGraphInstance>(AssetData.GetAsset());

		if (GraphInstance && GraphInstance->Instance && NULL == GraphInstance->Instance->Desc)
		{
			return FText::FromString(TEXT("Orphan Graph Instance, Instance Factory missing."));
		}
		else
		{
			return FText::FromString(TEXT("Substance Graph Instance."));
		}
	}

private:
	/** Handler for when Edit is selected */
	void ExecuteEdit(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);

	/** Handler for when Reimport is selected */
	void ExecuteReimport(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);

	/** Handling sbsprs import */
	void ExecuteImportPresets(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);
	/** Handling sbsprs export */
	void ExecuteExportPresets(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);
	/** Handling sbsprs export */
	void ExecuteResetDefault(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);

	/** Handling parent factory locating*/
	void ExecuteFindParentFactory(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);
	/** Handling child objects deletion*/
	void ExecuteDeleteWithOutputs(TArray<TWeakObjectPtr<USubstanceGraphInstance>> Objects);
};
