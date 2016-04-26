// Copyright 2015 Kite & Lightning.  All rights reserved.

#include "StereoPanoramaPrivatePCH.h"


TSharedPtr<FStereoPanoramaManager> StereoPanoramaManager;


void FStereoPanoramaModule::StartupModule()
{
	StereoPanoramaManager = MakeShareable(new FStereoPanoramaManager());
}


void FStereoPanoramaModule::ShutdownModule()
{
	if (StereoPanoramaManager.IsValid())
	{
		StereoPanoramaManager.Reset();
	}
}


TSharedPtr<FStereoPanoramaManager> FStereoPanoramaModule::Get()
{
	check(StereoPanoramaManager.IsValid());
	return StereoPanoramaManager;
}


IMPLEMENT_MODULE(FStereoPanoramaModule, StereoPanorama)
