// Copyright 2015 Kite & Lightning.  All rights reserved.

#pragma once


/**
 * Implements the StereoPanorama module.
 */
class FStereoPanoramaModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	static TSharedPtr<FStereoPanoramaManager> Get();
};
