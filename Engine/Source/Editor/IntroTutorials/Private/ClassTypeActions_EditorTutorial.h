// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClassTypeActions_Base.h"

class FClassTypeActions_EditorTutorial : public FClassTypeActions_Base
{
public:
	// IClassTypeActions Implementation
	virtual UClass* GetSupportedClass() const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
};
