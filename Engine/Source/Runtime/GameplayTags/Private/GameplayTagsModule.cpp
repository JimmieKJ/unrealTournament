// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

class FGameplayTagsModule : public IGameplayTagsModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};

IMPLEMENT_MODULE( FGameplayTagsModule, GameplayTags )
DEFINE_LOG_CATEGORY(LogGameplayTags);

void FGameplayTagsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	GGameplayTagsManager = NewObject<UGameplayTagsManager>(GetTransientPackage(), NAME_None, RF_RootSet);

	TArray<FString> GameplayTagTables;
	GConfig->GetArray(TEXT("GameplayTags"), TEXT("GameplayTagTableList"), GameplayTagTables, GEngineIni);

	GGameplayTagsManager->LoadGameplayTagTables(GameplayTagTables);
	GGameplayTagsManager->ConstructGameplayTagTree();
}

void FGameplayTagsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	GGameplayTagsManager = NULL;
}
