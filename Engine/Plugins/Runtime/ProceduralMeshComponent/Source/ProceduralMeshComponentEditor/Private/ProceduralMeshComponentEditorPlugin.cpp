// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ProceduralMeshComponentEditorPrivatePCH.h"




class FProceduralMeshComponentEditorPlugin : public IProceduralMeshComponentEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FProceduralMeshComponentEditorPlugin, ProceduralMeshComponentEditor )



void FProceduralMeshComponentEditorPlugin::StartupModule()
{
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UProceduralMeshComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FProceduralMeshComponentDetails::MakeInstance));
	}
}


void FProceduralMeshComponentEditorPlugin::ShutdownModule()
{
	
}



