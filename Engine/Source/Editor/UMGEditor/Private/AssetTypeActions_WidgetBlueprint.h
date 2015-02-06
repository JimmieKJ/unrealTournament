// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetTypeActions_Base.h"
//TODO Use base class FAssetTypeActions_Blueprint later
class FAssetTypeActions_WidgetBlueprint : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_WidgetBlueprint", "Widget Blueprint"); }
	virtual FColor GetTypeColor() const override { return FColor(44, 89, 180); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::UI; }
	virtual FText GetAssetDescription( const FAssetData& AssetData ) const override;
	// End IAssetTypeActions Implementation
};