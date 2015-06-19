// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WindowsTargetPlatformPrivatePCH.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "FWindowsTargetPlatformModule"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Implements the Windows target platform module.
 */
class FWindowsTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Destructor. */
	~FWindowsTargetPlatformModule( )
	{
		Singleton = nullptr;
	}

public:

	// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
	void HotfixTest( void *InPayload, int PayloadSize )
	{
		check(sizeof(FTestHotFixPayload) == PayloadSize);
		
		FTestHotFixPayload* Payload = (FTestHotFixPayload*)InPayload;
		UE_LOG(LogTemp, Log, TEXT("Hotfix Test %s"), *Payload->Message);
		Payload->Result = Payload->ValueToReturn;
	}
#endif

public:

	// ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == nullptr)
		{
			Singleton = new TGenericWindowsTargetPlatform<true, false, false>();
		}

		return Singleton;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).BindRaw(this, &FWindowsTargetPlatformModule::HotfixTest);
#endif

		TargetSettings = NewObject<UWindowsTargetSettings>(GetTransientPackage(), "WindowsTargetSettings", RF_Standalone);
		
		// We need to manually load the config properties here, as this module is loaded before the UObject system is setup to do this
		GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), TargetSettings->TargetedRHIs, GEngineIni);

		// When this is initialized the UEnum for EMinimumSupportedOS hasn't been registered. 
		FString MinOSString;
		if (GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("MinimumOSVersion"), MinOSString, GEngineIni))
		{
			// We need to parse the string and compare manually.
			TargetSettings->MinimumOSVersion = MinOSString == TEXT("MSOS_XP") ? EMinimumSupportedOS::MSOS_XP : EMinimumSupportedOS::MSOS_Vista;
		}


		TargetSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Windows",
				LOCTEXT("TargetSettingsName", "Windows"),
				LOCTEXT("TargetSettingsDescription", "Settings for Windows target platform"),
				TargetSettings
			);
		}
	}

	virtual void ShutdownModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).Unbind();
#endif

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Windows");
		}

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = nullptr;
		}
	}

private:

	// Holds the target settings.
	UWindowsTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FWindowsTargetPlatformModule, WindowsTargetPlatform);
