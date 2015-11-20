// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAdvertisingProvider.h"
#include "Core.h"

class FIOSAdvertisingProvider : public IAdvertisingProvider
{
public:
	virtual void ShowAdBanner( bool bShowOnBottomOfScreen, int32 AdID ) override;
	virtual void HideAdBanner() override;
	virtual void CloseAdBanner() override;
	virtual int32 GetAdIDCount() override;
};
