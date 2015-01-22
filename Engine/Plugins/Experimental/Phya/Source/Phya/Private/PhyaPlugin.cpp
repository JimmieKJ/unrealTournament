// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhyaPluginPrivatePCH.h"




class FPhyaPlugin : public IPhyaPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FPhyaPlugin, Phya )



void FPhyaPlugin::StartupModule()
{
	
}


void FPhyaPlugin::ShutdownModule()
{
	
}



