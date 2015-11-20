// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"

/** Generic interface for an advertising provider. Other modules can define more and register them with this module. */
class IAdvertisingProvider : public IModuleInterface
{
public:
	virtual void ShowAdBanner( bool bShowOnBottomOfScreen, int32 adID ) = 0;
	virtual void HideAdBanner() = 0;
	virtual void CloseAdBanner() = 0;
	virtual int32 GetAdIDCount() = 0;
};

