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
	virtual FString ConstructBaseFilename(const UObject* AssetObj) override;
	virtual FPCHFilenameQuery& OnPCHFilenameQuery() override;
	virtual FIsTargetedForConversionQuery& OnIsTargetedForConversionQuery() override;
	virtual TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> >& GetOriginalClassMap() override;
	//~ End IBlueprintCompilerCppBackendModule interface

private: 
	FPCHFilenameQuery PCHFilenameQuery;
	FIsTargetedForConversionQuery IsTargetedForConversionQuery;
	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> > OriginalClassMap;
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

IBlueprintCompilerCppBackendModule::FIsTargetedForConversionQuery& FBlueprintCompilerCppBackendModule::OnIsTargetedForConversionQuery()
{
	return IsTargetedForConversionQuery;
}

TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> >& FBlueprintCompilerCppBackendModule::GetOriginalClassMap()
{
	return OriginalClassMap;
}

IMPLEMENT_MODULE(FBlueprintCompilerCppBackendModule, BlueprintCompilerCppBackend)
