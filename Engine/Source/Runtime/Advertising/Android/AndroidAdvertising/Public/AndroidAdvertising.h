// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAdvertisingProvider.h"
#include "Core.h"

class FAndroidAdvertisingProvider : public IAdvertisingProvider
{
public:
	virtual void ShowAdBanner( bool bShowOnBottomOfScreen ) override;
	virtual void HideAdBanner() override;
	virtual void CloseAdBanner() override;
};
