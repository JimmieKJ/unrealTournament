// Copyright 2015 Kite & Lightning.  All rights reserved.

#include "StereoPanoramaPrivatePCH.h"

TSharedPtr<FStereoPanoramaManager> StereoPanoramaManager;

/** IModuleInterface - initialize the module */
void FStereoPanoramaModule::StartupModule()
{
	StereoPanoramaManager = MakeShareable( new FStereoPanoramaManager() );
}

/** IModuleInterface - shutdown the module */
void FStereoPanoramaModule::ShutdownModule()
{
	if( StereoPanoramaManager.IsValid() )
	{
		StereoPanoramaManager.Reset();
	}
}

TSharedPtr<FStereoPanoramaManager> FStereoPanoramaModule::Get()
{
	check( StereoPanoramaManager.IsValid() );
	return StereoPanoramaManager;
}

IMPLEMENT_MODULE( FStereoPanoramaModule, StereoPanorama )
