// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"


/**
 * Implements an action for UMediaPlaylist assets.
 */
class FMediaPlaylistActions
	: public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FMediaPlaylistActions(const TSharedRef<ISlateStyle>& InStyle);

public:

	//~ FAssetTypeActions_Base interface

	virtual bool CanFilter() override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

private:

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;
};
