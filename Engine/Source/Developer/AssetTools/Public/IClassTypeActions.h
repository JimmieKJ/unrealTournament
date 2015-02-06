// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** ClassTypeActions provide actions and other information about asset types that host classes */
class IClassTypeActions : public TSharedFromThis<IClassTypeActions>
{
public:
	/** Virtual destructor */
	virtual ~IClassTypeActions(){}

	/** Checks to see if the specified object is handled by this type. */
	virtual UClass* GetSupportedClass() const = 0;

	/** Optionally returns a custom widget to overlay on top of this assets' thumbnail */
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const class FAssetData& AssetData) const = 0;
};
