//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "ISubstanceCore.h"
#include "Ticker.h"

// Settings
#include "SubstanceSettings.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "SubstanceModule"

class FSubstanceCoreModule : public ISubstanceCore
{
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	virtual unsigned int GetMaxOutputTextureSize() const override;

	virtual bool Tick(float DeltaTime);

	void RegisterSettings()
	{
#if WITH_EDITOR
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Substance",
				LOCTEXT("SubstanceSettingsName", "Substance"),
				LOCTEXT("SubstanceSettingsDescription", "Configure the Substance plugin"),
				GetMutableDefault<USubstanceSettings>()
				);
		}
#endif
	}

	void UnregisterSettings()
	{
#if WITH_EDITOR
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Substance");
		}
#endif
	}

#if WITH_EDITOR
	void OnBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);

public:
	bool isPie(){ return PIE; }
#endif //WITH_EDITOR

private:
	FTickerDelegate TickDelegate;

	static void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnLevelAdded(ULevel* Level, UWorld* World);

	void LoadSubstanceLibraries();
	
	bool PIE;
};

#undef LOCTEXT_NAMESPACE
