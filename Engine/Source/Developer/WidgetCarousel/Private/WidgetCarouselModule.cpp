// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WidgetCarouselPrivatePCH.h"

void FWidgetCarouselModule::StartupModule()
{
	FWidgetCarouselModuleStyle::Initialize();
}

void FWidgetCarouselModule::ShutdownModule()
{
	FWidgetCarouselModuleStyle::Shutdown();
}

IMPLEMENT_MODULE(FWidgetCarouselModule, WidgetCarousel);
