// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Implements the WidgetCarousel module.
 */
class WIDGETCAROUSEL_API FWidgetCarouselModule
	: public IModuleInterface
{
public:

	virtual void StartupModule( ) override;

	virtual void ShutdownModule( ) override;
};
