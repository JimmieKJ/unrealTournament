// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

class FGameplayTagsModule : public IGameplayTagsModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	// Gets the UGameplayTagsManager manager
	virtual UGameplayTagsManager& GetGameplayTagsManager() override
	{
		check(GameplayTagsManager);
		return *GameplayTagsManager;
	}

private:

	// Stores the UGameplayTagsManager
	UGameplayTagsManager* GameplayTagsManager;
};

IMPLEMENT_MODULE( FGameplayTagsModule, GameplayTags )
DEFINE_LOG_CATEGORY(LogGameplayTags);

void FGameplayTagsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	GameplayTagsManager = NewObject<UGameplayTagsManager>(GetTransientPackage(), NAME_None, RF_RootSet);

	TArray<FString> GameplayTagTables;
	GConfig->GetArray(TEXT("GameplayTags"), TEXT("GameplayTagTableList"), GameplayTagTables, GEngineIni);

	GameplayTagsManager->LoadGameplayTagTables(GameplayTagTables);
	GameplayTagsManager->ConstructGameplayTagTree();
}

void FGameplayTagsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	GameplayTagsManager = NULL;
}
