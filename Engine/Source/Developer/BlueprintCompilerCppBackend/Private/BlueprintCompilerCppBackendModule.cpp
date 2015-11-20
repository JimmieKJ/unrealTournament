// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "ModuleManager.h"
#include "BlueprintCompilerCppBackend.h"
#include "BlueprintCompilerCppBackendUtils.h" // for FEmitHelper::GetBaseFilename()

class FBlueprintCompilerCppBackendModule : public IBlueprintCompilerCppBackendModule
{
public:
	//~ Begin IBlueprintCompilerCppBackendModuleInterface interface
	virtual IBlueprintCompilerCppBackend* Create() override;
	//~ End IBlueprintCompilerCppBackendModuleInterface interface

	//~ Begin IBlueprintCompilerCppBackendModule interface
	FString ConstructBaseFilename(const UObject* AssetObj) override;
	virtual FPCHFilenameQuery& OnPCHFilenameQuery() override;
	//~ End IBlueprintCompilerCppBackendModule interface

private: 
	FPCHFilenameQuery PCHFilenameQuery;
};

IBlueprintCompilerCppBackend* FBlueprintCompilerCppBackendModule::Create()
{
	return new FBlueprintCompilerCppBackend();
}

FString FBlueprintCompilerCppBackendModule::ConstructBaseFilename(const UObject* AssetObj)
{
	// use the same function that the backend uses for #includes
	return FEmitHelper::GetBaseFilename(AssetObj);
}

IBlueprintCompilerCppBackendModule::FPCHFilenameQuery& FBlueprintCompilerCppBackendModule::OnPCHFilenameQuery()
{
	return PCHFilenameQuery;
}

IMPLEMENT_MODULE(FBlueprintCompilerCppBackendModule, BlueprintCompilerCppBackend)
