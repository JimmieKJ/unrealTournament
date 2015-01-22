// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EngineSettingsPrivatePCH.h"


/**
 * Implements the EngineSettings module.
 */
class FEngineSettingsModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


/* Class constructors
 *****************************************************************************/

UConsoleSettings::UConsoleSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameMapsSettings::UGameMapsSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bUseSplitscreen(true)
	, TwoPlayerSplitscreenLayout(ETwoPlayerSplitScreenType::Horizontal)
	, ThreePlayerSplitscreenLayout(EThreePlayerSplitScreenType::FavorTop)
{ }


UGameNetworkManagerSettings::UGameNetworkManagerSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameSessionSettings::UGameSessionSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralEngineSettings::UGeneralEngineSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralProjectSettings::UGeneralProjectSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UHudSettings::UHudSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


/* Static functions
 *****************************************************************************/

const FString& UGameMapsSettings::GetGameDefaultMap( )
{
	return IsRunningDedicatedServer()
		? GetDefault<UGameMapsSettings>()->ServerDefaultMap
		: GetDefault<UGameMapsSettings>()->GameDefaultMap;
}


const FString& UGameMapsSettings::GetGlobalDefaultGameMode( )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	return IsRunningDedicatedServer() && GameMapsSettings->GlobalDefaultServerGameMode.IsValid()
		? GameMapsSettings->GlobalDefaultServerGameMode.ToString()
		: GameMapsSettings->GlobalDefaultGameMode.ToString();
}


void UGameMapsSettings::SetGameDefaultMap( const FString& NewMap )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	if (IsRunningDedicatedServer())
	{
		GameMapsSettings->ServerDefaultMap = NewMap;
	}
	else
	{
		GameMapsSettings->GameDefaultMap = NewMap;
	}
}

void UGameMapsSettings::SetGlobalDefaultGameMode( const FString& NewGameMode )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());
	
	GameMapsSettings->GlobalDefaultGameMode = NewGameMode;
}


IMPLEMENT_MODULE(FEngineSettingsModule, EngineSettings);
