// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomMeshComponentPluginPrivatePCH.h"




class FCustomMeshComponentPlugin : public ICustomMeshComponentPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FCustomMeshComponentPlugin, CustomMeshComponent )



void FCustomMeshComponentPlugin::StartupModule()
{
	
}


void FCustomMeshComponentPlugin::ShutdownModule()
{
	
}



