// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProceduralMeshComponentPluginPrivatePCH.h"




class FProceduralMeshComponentPlugin : public IProceduralMeshComponentPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FProceduralMeshComponentPlugin, ProceduralMeshComponent )



void FProceduralMeshComponentPlugin::StartupModule()
{
	
}


void FProceduralMeshComponentPlugin::ShutdownModule()
{
	
}



