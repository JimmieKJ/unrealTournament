// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"


/**
 * Implements an action for UMediaSoundWave assets.
 */
class FMediaSoundWaveActions
	: public FAssetTypeActions_Base
{
public:

	// FAssetTypeActions_Base overrides

	virtual bool CanFilter() override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override;
};
