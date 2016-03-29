// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	GGameplayTagsManager = NewObject<UGameplayTagsManager>(GetTransientPackage(), NAME_None);
	GGameplayTagsManager->AddToRoot();

	TArray<FString> GameplayTagTables;
	GConfig->GetArray(TEXT("GameplayTags"), TEXT("GameplayTagTableList"), GameplayTagTables, GEngineIni);

	GGameplayTagsManager->LoadGameplayTagTables(GameplayTagTables);
	GGameplayTagsManager->ConstructGameplayTagTree();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 GameplayTagPrintReportOnShutdown = 0;
static FAutoConsoleVariableRef CVarGameplayTagPrintReportOnShutdown(TEXT("GameplayTags.PrintReportOnShutdown"), GameplayTagPrintReportOnShutdown, TEXT("Print gameplay tag replication report on shutdown"), ECVF_Default );
#endif


void FGameplayTagsModule::ShutdownModule()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GameplayTagPrintReportOnShutdown)
	{
		UGameplayTagsManager::PrintReplicationFrequencyReport();
	}
#endif

	GGameplayTagsManager = NULL;
}
