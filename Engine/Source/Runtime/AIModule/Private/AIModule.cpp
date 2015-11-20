// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "AISystem.h"

#if WITH_EDITOR
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#if ENABLE_VISUAL_LOG
	#include "VisualLoggerExtension.h"
#endif // ENABLE_VISUAL_LOG
#endif

#include "AIModule.h"

#define LOCTEXT_NAMESPACE "AIModule"

DEFINE_LOG_CATEGORY_STATIC(LogAIModule, Log, All);

class FAIModule : public IAIModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	virtual UAISystemBase* CreateAISystemInstance(UWorld* World) override;
	// End IModuleInterface
#if WITH_EDITOR
	virtual EAssetTypeCategories::Type GetAIAssetCategoryBit() const override { return AIAssetCategoryBit; }
protected:
	EAssetTypeCategories::Type AIAssetCategoryBit;
#if ENABLE_VISUAL_LOG
	FVisualLoggerExtension	VisualLoggerExtension;
#endif // ENABLE_VISUAL_LOG
#endif
};

IMPLEMENT_MODULE(FAIModule, AIModule)

void FAIModule::StartupModule()
{ 
	// This code will execute after your module is loaded into memory and after global variables initialization. We needs some place to load GameplayDebugger module so it's best place for it now.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FModuleManager::LoadModulePtr< IModuleInterface >("GameplayDebugger");
#endif

#if WITH_EDITOR 
	FModuleManager::LoadModulePtr< IModuleInterface >("AITestSuite");

	if (GIsEditor)
	{
		// Register asset types
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		// register AI category so that AI assets can register to it
		AIAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("AI")), LOCTEXT("AIAssetCategory", "Artificial Intelligence"));
	}
#endif // WITH_EDITOR 

#if WITH_EDITOR && ENABLE_VISUAL_LOG
	FVisualLogger::Get().RegisterExtension(*EVisLogTags::TAG_EQS, &VisualLoggerExtension);
#endif
}

void FAIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
#if WITH_EDITOR && ENABLE_VISUAL_LOG
	FVisualLogger::Get().UnregisterExtension(*EVisLogTags::TAG_EQS, &VisualLoggerExtension);
#endif
}

UAISystemBase* FAIModule::CreateAISystemInstance(UWorld* World)
{
	UE_LOG(LogAIModule, Log, TEXT("Creating AISystem for world %s"), *GetNameSafe(World));
	
	TSubclassOf<UAISystemBase> AISystemClass = LoadClass<UAISystemBase>(NULL, *UAISystem::GetAISystemClassName().ToString(), NULL, LOAD_None, NULL);
	return NewObject<UAISystemBase>(World, AISystemClass);
}

#undef LOCTEXT_NAMESPACE
