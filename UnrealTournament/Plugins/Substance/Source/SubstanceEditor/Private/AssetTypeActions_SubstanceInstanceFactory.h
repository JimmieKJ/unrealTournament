//! @file SubstanceEditorPanel.cpp
//! @copyright Allegorithmic. All rights reserved.

#pragma once

class FAssetTypeActions_SubstanceInstanceFactory : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubstanceInstanceFactory", "Substance Instance Factory"); }
	virtual FColor GetTypeColor() const override { return FColor(32,192,32); }
	virtual UClass* GetSupportedClass() const override { return USubstanceInstanceFactory::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
	
	void CreateInstanceSubMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USubstanceInstanceFactory>> InstanceFactory);

protected:
	/** Returns additional tooltip information for the specified asset, if it has any (otherwise return the null widget) */
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override
	{
		return FText::FromString(TEXT("Substance Instance Factory."));
	}

private:
	/** Handler for when Edit is selected */
	void ExecuteEdit(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects);

	/** Handler for when Reimport is selected */
	void ExecuteReimport(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects);

	void ExecuteInstantiate(graph_desc_t* Graph);
		
	/** Handling child object deletions*/
	void ExecuteDeleteWithOutputs(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects);
};
