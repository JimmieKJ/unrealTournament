// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class IClassTypeActions;

/** Base class for "class type" assets (C++ classes and Blueprints */
class ASSETTOOLS_API FAssetTypeActions_ClassTypeBase : public FAssetTypeActions_Base
{
public:
	/** Get the class type actions for this asset */
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActions(const FAssetData& AssetData) const = 0;

	// IAssetTypeActions Implementation
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
};
