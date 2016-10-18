// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	virtual FMarkUnconvertedBlueprintAsNecessary& OnIncludingUnconvertedBP() override;
	virtual FIsFunctionUsedInADelegate& GetIsFunctionUsedInADelegateCallback() override;
	virtual TSharedPtr<FNativizationSummary>& NativizationSummary() override;
	//~ End IBlueprintCompilerCppBackendModule interface

private: 
	FPCHFilenameQuery PCHFilenameQuery;
	FIsTargetedForConversionQuery IsTargetedForConversionQuery;
	FMarkUnconvertedBlueprintAsNecessary MarkUnconvertedBlueprintAsNecessary;
	FIsFunctionUsedInADelegate IsFunctionUsedInADelegate;
	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> > OriginalClassMap;
	TSharedPtr<FNativizationSummary> NativizationSummaryPtr;
};

IBlueprintCompilerCppBackend* FBlueprintCompilerCppBackendModule::Create()
{
	return new FBlueprintCompilerCppBackend();
}

TSharedPtr<FNativizationSummary>& FBlueprintCompilerCppBackendModule::NativizationSummary()
{
	return NativizationSummaryPtr;
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

IBlueprintCompilerCppBackendModule::FMarkUnconvertedBlueprintAsNecessary& FBlueprintCompilerCppBackendModule::OnIncludingUnconvertedBP()
{
	return MarkUnconvertedBlueprintAsNecessary;
}

TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UClass> >& FBlueprintCompilerCppBackendModule::GetOriginalClassMap()
{
	return OriginalClassMap;
}

IBlueprintCompilerCppBackendModule::FIsFunctionUsedInADelegate& FBlueprintCompilerCppBackendModule::GetIsFunctionUsedInADelegateCallback()
{
	return IsFunctionUsedInADelegate;
}

IMPLEMENT_MODULE(FBlueprintCompilerCppBackendModule, BlueprintCompilerCppBackend)
